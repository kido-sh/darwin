/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/cc_priv.h>
#include <corecrypto/cchmac.h>
#include <prng/hmac_drbg.h>

struct hmac_drbg_state {
	const struct ccdigest_info *di;
	uint64_t reseed_counter;
	bool strict;
	uint8_t key[MAX_DIGEST_OUTPUT_SIZE];
	uint8_t v[MAX_DIGEST_OUTPUT_SIZE];
};

struct hmac_drbg_input {
	size_t len;
	const void *data;
};

/* highest_supported_security_strength of each hash, SP 800-90A table 2 */
static size_t
hmac_drbg_strength(const struct ccdigest_info *di)
{
	if (di->output_size >= 32) {
		return 32;
	}
	return di->output_size >= 28 ? 24 : 16;
}

/* K = HMAC(K, V || sep || in[0] || ... || in[n - 1]), then V = HMAC(K, V) */
static void
hmac_drbg_rekey(struct hmac_drbg_state *st, uint8_t sep, size_t n, const struct hmac_drbg_input *in)
{
	const struct ccdigest_info *di = st->di;
	size_t outlen = di->output_size;
	cchmac_di_decl(di, hmac);

	cchmac_init(di, hmac, outlen, st->key);
	cchmac_update(di, hmac, outlen, st->v);
	cchmac_update(di, hmac, 1, &sep);
	for (size_t i = 0; i < n; i++) {
		cchmac_update(di, hmac, in[i].len, in[i].data);
	}
	cchmac_final(di, hmac, st->key);
	cchmac_di_clear(di, hmac);

	cchmac(di, outlen, st->key, outlen, st->v, st->v);
}

/* HMAC_DRBG_Update (10.1.2.2), the provided data being the concatenation of in[] */
static void
hmac_drbg_update(struct hmac_drbg_state *st, size_t n, const struct hmac_drbg_input *in)
{
	size_t provided = 0;

	for (size_t i = 0; i < n; i++) {
		provided += in[i].len;
	}

	hmac_drbg_rekey(st, 0x00, n, in);
	if (provided != 0) {
		hmac_drbg_rekey(st, 0x01, n, in);
	}
}

/* HMAC_DRBG_Instantiate_algorithm (10.1.2.3) */
static ccdrbg_status_t
hmac_drbg_init(const struct ccdrbg_info *info, struct ccdrbg_state *state,
    size_t entropy_len, const void *entropy,
    size_t nonce_len, const void *nonce,
    size_t ps_len, const void *ps)
{
	const struct ccdrbg_nisthmac_custom *custom = info->custom;
	const struct ccdigest_info *di = custom->di;
	struct hmac_drbg_state *st = (void *)state;

	if (di->output_size > sizeof(st->key) ||
	    entropy_len < hmac_drbg_strength(di) ||
	    entropy_len > CCDRBG_MAX_ENTROPY_SIZE ||
	    nonce_len > CCDRBG_MAX_ENTROPY_SIZE ||
	    ps_len > CCDRBG_MAX_PSINPUT_SIZE) {
		return CCDRBG_STATUS_PARAM_ERROR;
	}

	st->di = di;
	st->strict = custom->strictFIPS;
	memset(st->key, 0x00, di->output_size);
	memset(st->v, 0x01, di->output_size);

	const struct hmac_drbg_input seed[] = {
		{ entropy_len, entropy },
		{ nonce_len, nonce },
		{ ps_len, ps },
	};
	hmac_drbg_update(st, 3, seed);
	st->reseed_counter = 1;

	return CCDRBG_STATUS_OK;
}

/* HMAC_DRBG_Reseed_algorithm (10.1.2.4) */
static ccdrbg_status_t
hmac_drbg_reseed(struct ccdrbg_state *state,
    size_t entropy_len, const void *entropy,
    size_t ad_len, const void *ad)
{
	struct hmac_drbg_state *st = (void *)state;

	if (entropy_len < hmac_drbg_strength(st->di) ||
	    entropy_len > CCDRBG_MAX_ENTROPY_SIZE ||
	    ad_len > CCDRBG_MAX_ADDITIONALINPUT_SIZE) {
		return CCDRBG_STATUS_PARAM_ERROR;
	}

	const struct hmac_drbg_input seed[] = {
		{ entropy_len, entropy },
		{ ad_len, ad },
	};
	hmac_drbg_update(st, 2, seed);
	st->reseed_counter = 1;

	return CCDRBG_STATUS_OK;
}

/* HMAC_DRBG_Generate_algorithm (10.1.2.5); only a strict instance asks for a reseed */
static ccdrbg_status_t
hmac_drbg_generate(struct ccdrbg_state *state,
    size_t out_len, void *out,
    size_t ad_len, const void *ad)
{
	struct hmac_drbg_state *st = (void *)state;
	const struct ccdigest_info *di = st->di;
	size_t outlen = di->output_size;
	const struct hmac_drbg_input adin = { ad_len, ad };
	uint8_t *p = out;

	if (out_len > CCDRBG_MAX_REQUEST_SIZE || ad_len > CCDRBG_MAX_ADDITIONALINPUT_SIZE) {
		return CCDRBG_STATUS_PARAM_ERROR;
	}
	if (st->strict && st->reseed_counter > CCDRBG_RESEED_INTERVAL) {
		return CCDRBG_STATUS_NEED_RESEED;
	}

	if (ad_len != 0) {
		hmac_drbg_update(st, 1, &adin);
	}
	while (out_len > 0) {
		size_t n = CC_MIN(out_len, outlen);

		cchmac(di, outlen, st->key, outlen, st->v, st->v);
		memcpy(p, st->v, n);
		p += n;
		out_len -= n;
	}
	hmac_drbg_update(st, 1, &adin);
	st->reseed_counter++;

	return CCDRBG_STATUS_OK;
}

static void
hmac_drbg_done(struct ccdrbg_state *state)
{
	cc_clear(sizeof(struct hmac_drbg_state), state);
}

void
ccdrbg_factory_nisthmac(struct ccdrbg_info *info, const struct ccdrbg_nisthmac_custom *custom)
{
	info->size = sizeof(struct hmac_drbg_state);
	info->init = hmac_drbg_init;
	info->reseed = hmac_drbg_reseed;
	info->generate = hmac_drbg_generate;
	info->done = hmac_drbg_done;
	info->custom = custom;
}

/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/ccrsa_priv.h>
#include <corecrypto/ccder.h>
#include <corecrypto/cc_error.h>
#include <stdlib.h>
#include <string.h>

/* RFC 8017 B.2.1: buf ^= MGF1(seed, len) */
static void mgf1_xor(const struct ccdigest_info *di, const uint8_t *seed, size_t seed_len,
                     uint8_t *buf, size_t len)
{
    uint8_t block[MAX_DIGEST_OUTPUT_SIZE];
    ccdigest_di_decl(di, ctx);

    for (uint32_t counter = 0; len > 0; counter++) {
        uint8_t c[4] = { counter >> 24, counter >> 16, counter >> 8, counter };
        ccdigest_init(di, ctx);
        ccdigest_update(di, ctx, seed_len, seed);
        ccdigest_update(di, ctx, sizeof(c), c);
        ccdigest_final(di, ctx, block);
        size_t take = len < di->output_size ? len : di->output_size;
        for (size_t i = 0; i < take; i++)
            buf[i] ^= block[i];
        buf += take;
        len -= take;
    }
    ccdigest_di_clear(di, ctx);
    cc_clear(sizeof(block), block);
}

/* H = Hash(0x00 * 8 || mhash || salt), RFC 8017 9.1.1 step 5 */
static void pss_hash(const struct ccdigest_info *di, size_t hlen, const uint8_t *mhash,
                     size_t saltlen, const uint8_t *salt, uint8_t *h)
{
    static const uint8_t zeros[8];
    ccdigest_di_decl(di, ctx);
    ccdigest_init(di, ctx);
    ccdigest_update(di, ctx, sizeof(zeros), zeros);
    ccdigest_update(di, ctx, hlen, mhash);
    ccdigest_update(di, ctx, saltlen, salt);
    ccdigest_final(di, ctx, h);
    ccdigest_di_clear(di, ctx);
}

static size_t digest_info_size(size_t dgstlen, const uint8_t *oid)
{
    size_t alg = ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, ccoid_size(oid) + ccder_sizeof(CCDER_NULL, 0));
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, alg + ccder_sizeof_raw_octet_string(dgstlen));
}

/* T of RFC 8017 9.2 step 2, the DigestInfo, written to the end of [out, out_end) */
static uint8_t *encode_digest_info(size_t dgstlen, const uint8_t *dgst, const uint8_t *oid,
                                   uint8_t *out, uint8_t *out_end)
{
    uint8_t *p = ccder_encode_raw_octet_string(dgstlen, dgst, out, out_end);
    uint8_t *alg_end = p;
    p = ccder_encode_tl(CCDER_NULL, 0, out, p);
    p = ccder_encode_body(ccoid_size(oid), oid, out, p);
    p = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, alg_end, out, p);
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, out_end, out, p);
}

int ccrsa_emsa_pkcs1v15_encode(size_t emlen, uint8_t *em,
                               size_t dgstlen, const uint8_t *dgst, const uint8_t *oid)
{
    size_t tlen = oid != NULL ? digest_info_size(dgstlen, oid) : dgstlen;

    if (emlen < tlen + 11)
        return CCERR_PARAMETER;
    uint8_t *t = em + emlen - tlen;
    if (oid != NULL) {
        if (encode_digest_info(dgstlen, dgst, oid, t, em + emlen) != t)
            return CCERR_INTERNAL;
    } else {
        memmove(t, dgst, dgstlen);
    }
    em[0] = 0x00;
    em[1] = 0x01;
    memset(em + 2, 0xff, emlen - tlen - 3);
    t[-1] = 0x00;
    return CCERR_OK;
}

int ccrsa_emsa_pkcs1v15_verify(size_t emlen, const uint8_t *em,
                               size_t dgstlen, const uint8_t *dgst, const uint8_t *oid)
{
    uint8_t *expected = malloc(emlen);
    int rv = CCERR_PARAMETER;

    if (expected != NULL && ccrsa_emsa_pkcs1v15_encode(emlen, expected, dgstlen, dgst, oid) == CCERR_OK)
        rv = cc_cmp_safe(emlen, expected, em) == 0 ? CCERR_OK : CCERR_INTEGRITY;
    free(expected);
    return rv;
}

int ccrsa_emsa_pss_encode(const struct ccdigest_info *di, const struct ccdigest_info *mgfdi,
                          size_t saltlen, const uint8_t *salt,
                          size_t hlen, const uint8_t *mhash,
                          size_t embits, uint8_t *em)
{
    size_t emlen = (embits + 7) / 8;
    size_t dblen;

    if (hlen != di->output_size || emlen < hlen + saltlen + 2)
        return CCERR_PARAMETER;
    dblen = emlen - di->output_size - 1;

    uint8_t *h = em + dblen;
    pss_hash(di, hlen, mhash, saltlen, salt, h);
    memset(em, 0, dblen - saltlen - 1);
    em[dblen - saltlen - 1] = 0x01;
    memmove(em + dblen - saltlen, salt, saltlen);
    mgf1_xor(mgfdi, h, di->output_size, em, dblen);
    em[0] &= 0xff >> (8 * emlen - embits);
    em[emlen - 1] = 0xbc;
    return CCERR_OK;
}

int ccrsa_emsa_pss_decode(const struct ccdigest_info *di, const struct ccdigest_info *mgfdi,
                          size_t saltlen,
                          size_t hlen, const uint8_t *mhash,
                          size_t embits, const uint8_t *em)
{
    size_t emlen = (embits + 7) / 8;
    uint8_t top_mask = 0xff >> (8 * emlen - embits);
    uint8_t h[MAX_DIGEST_OUTPUT_SIZE];
    int rv = CCERR_INTEGRITY;

    if (hlen != di->output_size || emlen < hlen + saltlen + 2)
        return CCERR_PARAMETER;
    if (em[emlen - 1] != 0xbc || (em[0] & ~top_mask) != 0)
        return CCERR_INTEGRITY;

    size_t dblen = emlen - hlen - 1;
    uint8_t *db = malloc(dblen);
    if (db == NULL)
        return CCERR_MEMORY;
    memcpy(db, em, dblen);
    mgf1_xor(mgfdi, em + dblen, hlen, db, dblen);
    db[0] &= top_mask;

    uint8_t bad = 0;
    size_t pslen = dblen - saltlen - 1;
    for (size_t i = 0; i < pslen; i++)
        bad |= db[i];
    bad |= db[pslen] ^ 0x01;
    if (bad == 0) {
        pss_hash(di, hlen, mhash, saltlen, db + dblen - saltlen, h);
        if (cc_cmp_safe(hlen, h, em + dblen) == 0)
            rv = CCERR_OK;
    }
    cc_clear(dblen, db);
    free(db);
    return rv;
}

/* the leading 0x00 of an encoded block sits above size - 1 bytes, so it fits ccn_nof_size(size) units */
static int write_block_ccn(size_t size, const uint8_t *em, cc_unit *r)
{
    return ccn_read_uint(ccn_nof_size(size), r, size, em);
}

/* 1 when b is 0, without a branch */
static unsigned ct_is_zero(uint8_t b)
{
    return ((unsigned)b - 1) >> 31;
}

/* all ones when bit is 1, all zeros when it is 0 */
static size_t ct_mask(unsigned bit)
{
    return (size_t)0 - (size_t)bit;
}

static int oaep_encode(const struct ccdigest_info *di, struct ccrng_state *rng,
                       size_t r_size, cc_unit *r, size_t message_len, const uint8_t *message,
                       size_t label_len, const uint8_t *label)
{
    size_t hlen = di->output_size;

    if (r_size < 2 * hlen + 2 || message_len > r_size - 2 * hlen - 2)
        return CCERR_PARAMETER;

    uint8_t *em = malloc(r_size);
    if (em == NULL)
        return CCERR_MEMORY;
    uint8_t *seed = em + 1, *db = em + 1 + hlen;
    size_t dblen = r_size - hlen - 1;

    em[0] = 0x00;
    ccdigest(di, label_len, label_len ? (const void *)label : "", db);
    memset(db + hlen, 0, dblen - hlen - message_len - 1);
    db[dblen - message_len - 1] = 0x01;
    memmove(db + dblen - message_len, message, message_len);

    int rv = ccrng_generate(rng, hlen, seed);
    if (rv == CCERR_OK) {
        mgf1_xor(di, seed, hlen, db, dblen);
        mgf1_xor(di, db, dblen, seed, hlen);
        rv = write_block_ccn(r_size, em, r);
    }
    cc_clear(r_size, em);
    free(em);
    return rv;
}

int ccrsa_oaep_encode(const struct ccdigest_info *di, struct ccrng_state *rng,
                      size_t r_size, cc_unit *r,
                      size_t message_len, const uint8_t *message)
{
    return oaep_encode(di, rng, r_size, r, message_len, message, 0, NULL);
}

static int oaep_decode(const struct ccdigest_info *di, size_t *r_len, uint8_t *r,
                       size_t s_size, const cc_unit *s, size_t label_len, const uint8_t *label)
{
    size_t hlen = di->output_size;
    uint8_t lhash[MAX_DIGEST_OUTPUT_SIZE];

    if (s_size < 2 * hlen + 2)
        return CCERR_PARAMETER;
    uint8_t *em = malloc(s_size);
    if (em == NULL)
        return CCERR_MEMORY;
    int rv = CCERR_PARAMETER;
    if (ccn_write_uint_padded(ccn_nof_size(s_size), s, s_size, em) != CCERR_OK)
        goto out;

    uint8_t *seed = em + 1, *db = em + 1 + hlen;
    size_t dblen = s_size - hlen - 1;
    mgf1_xor(di, db, dblen, seed, hlen);
    mgf1_xor(di, seed, hlen, db, dblen);
    ccdigest(di, label_len, label_len ? (const void *)label : "", lhash);

    /* RFC 8017 7.1.2 step 3g, without branching on where the padding fails */
    unsigned bad = em[0] | (unsigned)cc_cmp_safe(hlen, lhash, db);
    size_t sep = 0;
    unsigned found = 0;
    for (size_t i = hlen; i < dblen; i++) {
        unsigned is_one = ct_is_zero(db[i] ^ 0x01);
        unsigned is_zero = ct_is_zero(db[i]);
        unsigned first = is_one & ~found & 1;
        sep |= ct_mask(first) & i;
        found |= first;
        bad |= ~found & ~is_zero & 1;
    }
    bad |= ~found & 1;
    if (bad != 0) {
        rv = CCERR_INTEGRITY;
        goto out;
    }

    size_t mlen = dblen - sep - 1;
    if (*r_len < mlen) {
        rv = CCERR_PARAMETER;
        goto out;
    }
    memcpy(r, db + sep + 1, mlen);
    *r_len = mlen;
    rv = CCERR_OK;
out:
    cc_clear(s_size, em);
    free(em);
    return rv;
}

int ccrsa_oaep_decode(const struct ccdigest_info *di,
                      size_t *r_len, uint8_t *r,
                      size_t s_size, const cc_unit *s)
{
    return oaep_decode(di, r_len, r, s_size, s, 0, NULL);
}

int ccrsa_eme_pkcs1v15_encode(struct ccrng_state *rng,
                              size_t r_size, cc_unit *r,
                              size_t s_size, const uint8_t *s)
{
    if (r_size < 11 || s_size > r_size - 11)
        return CCERR_PARAMETER;
    uint8_t *em = malloc(r_size);
    if (em == NULL)
        return CCERR_MEMORY;

    size_t pslen = r_size - s_size - 3;
    uint8_t *ps = em + 2;
    int rv = ccrng_generate(rng, pslen, ps);
    for (size_t i = 0; rv == CCERR_OK && i < pslen; i++) {
        while (rv == CCERR_OK && ps[i] == 0)
            rv = ccrng_generate(rng, 1, ps + i);
    }
    if (rv == CCERR_OK) {
        em[0] = 0x00;
        em[1] = 0x02;
        em[2 + pslen] = 0x00;
        memmove(em + 3 + pslen, s, s_size);
        rv = write_block_ccn(r_size, em, r);
    }
    cc_clear(r_size, em);
    free(em);
    return rv;
}

int ccrsa_eme_pkcs1v15_decode(size_t *r_size, uint8_t *r,
                              size_t s_size, const cc_unit *s)
{
    if (s_size < 11)
        return CCERR_PARAMETER;
    uint8_t *em = malloc(s_size);
    if (em == NULL)
        return CCERR_MEMORY;
    int rv = CCERR_PARAMETER;
    if (ccn_write_uint_padded(ccn_nof_size(s_size), s, s_size, em) != CCERR_OK)
        goto out;

    /* RFC 8017 7.2.2 step 3, scanning every byte */
    unsigned bad = em[0] | (em[1] ^ 0x02);
    size_t sep = 0;
    unsigned found = 0;
    for (size_t i = 2; i < s_size; i++) {
        unsigned first = ct_is_zero(em[i]) & ~found & 1;
        sep |= ct_mask(first) & i;
        found |= first;
    }
    bad |= ~found & 1;
    bad |= sep < 10;
    if (bad != 0) {
        rv = CCERR_INTEGRITY;
        goto out;
    }

    size_t mlen = s_size - sep - 1;
    if (*r_size < mlen) {
        rv = CCERR_PARAMETER;
        goto out;
    }
    memcpy(r, em + sep + 1, mlen);
    *r_size = mlen;
    rv = CCERR_OK;
out:
    cc_clear(s_size, em);
    free(em);
    return rv;
}

/* a key block of k big-endian bytes as n units, NULL unless in is exactly k bytes */
static cc_unit *block_in(cc_size n, size_t k, size_t in_len, const uint8_t *in)
{
    cc_unit *x = in_len == k ? calloc(n, sizeof(cc_unit)) : NULL;
    if (x != NULL && ccn_read_uint(n, x, in_len, in) != CCERR_OK) {
        free(x);
        x = NULL;
    }
    return x;
}

static void free_block(cc_size n, cc_unit *x)
{
    if (x != NULL) {
        cc_clear(ccn_sizeof_n(n), x);
        free(x);
    }
}

/* out = OS2IP(em) raised by the key, written as k bytes */
static int crypt_block(ccrsa_full_ctx_t full, ccrsa_pub_ctx_t pub, size_t em_len, const uint8_t *em,
                       size_t k, uint8_t *out)
{
    cc_size n = ccrsa_ctx_n(pub);
    cc_unit *x = calloc(n, sizeof(cc_unit));
    int rv = x != NULL ? ccn_read_uint(n, x, em_len, em) : CCERR_MEMORY;
    if (rv == CCERR_OK)
        rv = full != NULL ? ccrsa_priv_crypt(full, x, x) : ccrsa_pub_crypt(pub, x, x);
    if (rv == CCERR_OK)
        rv = ccn_write_uint_padded(n, x, k, out);
    free_block(n, x);
    return rv;
}

int ccrsa_sign_pkcs1v15(ccrsa_full_ctx_t key, const uint8_t *oid, size_t digest_len, const uint8_t *digest,
                        size_t *sig_size, uint8_t *sig)
{
    size_t k = ccrsa_block_size(ccrsa_ctx_public(key));
    if (*sig_size < k)
        return CCERR_PARAMETER;
    uint8_t *em = malloc(k);
    int rv = em != NULL ? ccrsa_emsa_pkcs1v15_encode(k, em, digest_len, digest, oid) : CCERR_MEMORY;
    if (rv == CCERR_OK)
        rv = crypt_block(key, ccrsa_ctx_public(key), k, em, k, sig);
    if (rv == CCERR_OK)
        *sig_size = k;
    free(em);
    return rv;
}

int ccrsa_sign_pss(ccrsa_full_ctx_t key, const struct ccdigest_info *di, const struct ccdigest_info *mgfdi,
                   size_t salt_size, struct ccrng_state *rng, size_t digest_len, const uint8_t *digest,
                   size_t *sig_size, uint8_t *sig)
{
    size_t modbits = ccn_bitlen(ccrsa_ctx_n(key), ccrsa_ctx_m(key));
    size_t k = (modbits + 7) / 8, embits = modbits - 1, em_len = (embits + 7) / 8;
    if (modbits < 2 || *sig_size < k)
        return CCERR_PARAMETER;
    uint8_t *em = malloc(em_len), *salt = malloc(salt_size + 1);
    int rv = em != NULL && salt != NULL ? CCERR_OK : CCERR_MEMORY;
    if (rv == CCERR_OK && salt_size != 0)
        rv = ccrng_generate(rng, salt_size, salt);
    if (rv == CCERR_OK)
        rv = ccrsa_emsa_pss_encode(di, mgfdi, salt_size, salt, digest_len, digest, embits, em);
    if (rv == CCERR_OK)
        rv = crypt_block(key, ccrsa_ctx_public(key), em_len, em, k, sig);
    if (rv == CCERR_OK)
        *sig_size = k;
    free(em);
    free(salt);
    return rv;
}

int ccrsa_verify_pss(ccrsa_pub_ctx_t key, const struct ccdigest_info *di, const struct ccdigest_info *mgfdi,
                     size_t digest_len, const uint8_t *digest, size_t sig_size, const uint8_t *sig,
                     size_t salt_size, bool *valid)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t modbits = ccn_bitlen(n, ccrsa_ctx_m(key));
    size_t k = (modbits + 7) / 8, embits = modbits - 1, em_len = (embits + 7) / 8;

    *valid = false;
    if (modbits < 2)
        return CCERR_PARAMETER;
    cc_unit *x = block_in(n, k, sig_size, sig);
    uint8_t *em = malloc(k);
    int rv = x != NULL && em != NULL ? CCERR_OK : CCERR_PARAMETER;
    if (rv == CCERR_OK && ccrsa_pub_crypt(key, x, x) == CCERR_OK
        && ccn_write_uint_padded(n, x, k, em) == CCERR_OK && (k == em_len || em[0] == 0))
        *valid = ccrsa_emsa_pss_decode(di, mgfdi, salt_size, digest_len, digest, embits, em + (k - em_len)) == CCERR_OK;
    free_block(n, x);
    free(em);
    return rv;
}

int ccrsa_encrypt_eme_pkcs1v15(ccrsa_pub_ctx_t key, struct ccrng_state *rng, size_t *r_size, uint8_t *r,
                               size_t s_size, const uint8_t *s)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t k = ccrsa_block_size(key);
    if (*r_size < k)
        return CCERR_PARAMETER;
    cc_unit *x = calloc(n, sizeof(cc_unit));
    int rv = x != NULL ? ccrsa_eme_pkcs1v15_encode(rng, k, x, s_size, s) : CCERR_MEMORY;
    if (rv == CCERR_OK)
        rv = ccrsa_pub_crypt(key, x, x);
    if (rv == CCERR_OK)
        rv = ccn_write_uint_padded(n, x, k, r);
    if (rv == CCERR_OK)
        *r_size = k;
    free_block(n, x);
    return rv;
}

int ccrsa_decrypt_eme_pkcs1v15(ccrsa_full_ctx_t key, size_t *r_size, uint8_t *r, size_t s_size, const uint8_t *s)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t k = ccrsa_block_size(ccrsa_ctx_public(key));
    cc_unit *x = block_in(n, k, s_size, s);
    int rv = x != NULL ? ccrsa_priv_crypt(key, x, x) : CCERR_PARAMETER;
    if (rv == CCERR_OK)
        rv = ccrsa_eme_pkcs1v15_decode(r_size, r, k, x);
    free_block(n, x);
    return rv;
}

int ccrsa_encrypt_oaep(ccrsa_pub_ctx_t key, const struct ccdigest_info *di, struct ccrng_state *rng,
                       size_t *r_size, uint8_t *r, size_t s_size, const uint8_t *s,
                       size_t label_size, const uint8_t *label)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t k = ccrsa_block_size(key);
    if (*r_size < k)
        return CCERR_PARAMETER;
    cc_unit *x = calloc(n, sizeof(cc_unit));
    int rv = x != NULL ? oaep_encode(di, rng, k, x, s_size, s, label_size, label) : CCERR_MEMORY;
    if (rv == CCERR_OK)
        rv = ccrsa_pub_crypt(key, x, x);
    if (rv == CCERR_OK)
        rv = ccn_write_uint_padded(n, x, k, r);
    if (rv == CCERR_OK)
        *r_size = k;
    free_block(n, x);
    return rv;
}

int ccrsa_decrypt_oaep(ccrsa_full_ctx_t key, const struct ccdigest_info *di, size_t *r_size, uint8_t *r,
                       size_t c_size, const uint8_t *c, size_t label_size, const uint8_t *label)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t k = ccrsa_block_size(ccrsa_ctx_public(key));
    cc_unit *x = block_in(n, k, c_size, c);
    int rv = x != NULL ? ccrsa_priv_crypt(key, x, x) : CCERR_PARAMETER;
    if (rv == CCERR_OK)
        rv = oaep_decode(di, r_size, r, k, x, label_size, label);
    free_block(n, x);
    return rv;
}

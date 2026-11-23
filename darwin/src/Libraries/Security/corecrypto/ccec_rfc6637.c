/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/ccec.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccwrap_priv.h>
#include <corecrypto/cc_error.h>
#include <stdlib.h>
#include <string.h>

/* RFC 6637 section 9 and the OpenPGP registries: ECDH, SHA2-256/512, AES-128/256 */
#define RFC6637_ECDH 18

const struct ccec_rfc6637_curve ccec_rfc6637_dh_curve_p256 = { CC_EC_OID_SECP256R1, RFC6637_ECDH };
const struct ccec_rfc6637_curve ccec_rfc6637_dh_curve_p521 = { CC_EC_OID_SECP521R1, RFC6637_ECDH };
const struct ccec_rfc6637_wrap ccec_rfc6637_wrap_sha256_kek_aes128 = { ccsha256_di, 8, 7, 16 };
const struct ccec_rfc6637_wrap ccec_rfc6637_wrap_sha512_kek_aes256 = { ccsha512_di, 10, 9, 32 };
const struct ccec_rfc6637_unwrap ccec_rfc6637_unwrap_sha256_kek_aes128 = { ccsha256_di, 8, 7, 16 };
const struct ccec_rfc6637_unwrap ccec_rfc6637_unwrap_sha512_kek_aes256 = { ccsha512_di, 10, 9, 32 };

#define RFC6637_FINGERPRINT_SIZE 20
#define RFC6637_MAX_SHARED_SIZE 66
#define RFC6637_MAX_KEK_SIZE 32

/* RFC 6637 section 7: KEK = Hash(00 00 00 01 || S || Param), truncated */
static int rfc6637_kek(const struct ccdigest_info *di, uint8_t kdf_id, uint8_t kek_id, size_t kek_size,
                       const struct ccec_rfc6637_curve *curve, const uint8_t *fingerprint,
                       size_t shared_len, const uint8_t *shared, uint8_t *kek)
{
    static const uint8_t counter[4] = { 0, 0, 0, 1 };
    static const uint8_t sender[20] = "Anonymous Sender    ";
    uint8_t oid_len = curve->curve_oid[1];
    uint8_t alg[5] = { curve->public_key_alg, 0x03, 0x01, kdf_id, kek_id };
    uint8_t h[MAX_DIGEST_OUTPUT_SIZE];

    if (kek_size > di->output_size)
        return CCERR_PARAMETER;
    ccdigest_di_decl(di, ctx);
    ccdigest_init(di, ctx);
    ccdigest_update(di, ctx, sizeof(counter), counter);
    ccdigest_update(di, ctx, shared_len, shared);
    ccdigest_update(di, ctx, 1, &oid_len);
    ccdigest_update(di, ctx, oid_len, curve->curve_oid + 2);
    ccdigest_update(di, ctx, sizeof(alg), alg);
    ccdigest_update(di, ctx, sizeof(sender), sender);
    ccdigest_update(di, ctx, RFC6637_FINGERPRINT_SIZE, fingerprint);
    ccdigest_final(di, ctx, h);
    ccdigest_di_clear(di, ctx);
    memcpy(kek, h, kek_size);
    cc_clear(sizeof(h), h);
    return CCERR_OK;
}

/* symm_alg_id || key || checksum, padded to a multiple of 8 as in PKCS #5 */
static size_t rfc6637_padded_size(size_t key_len)
{
    return (1 + key_len + 2) / 8 * 8 + 8;
}

size_t ccec_rfc6637_wrap_key_size(ccec_pub_ctx_t public_key, unsigned long flags, size_t key_len)
{
    size_t wrapped = rfc6637_padded_size(key_len) + 8;
    if (flags != 0 || wrapped > 0xff)
        return 0;
    return 2 + ccec_export_pub_size(public_key) + 1 + wrapped;
}

static int rfc6637_aes_wrap(bool encrypt, size_t kek_size, const uint8_t *kek,
                            size_t in_len, const uint8_t *in, size_t *out_len, uint8_t *out)
{
    const struct ccmode_ecb *mode = encrypt ? ccaes_ecb_encrypt_mode() : ccaes_ecb_decrypt_mode();
    ccecb_ctx_decl(ccecb_context_size(mode), ctx);
    int rv = ccecb_init(mode, ctx, kek_size, kek);
    if (rv == CCERR_OK) {
        rv = encrypt ? ccwrap_auth_encrypt(mode, ctx, in_len, in, out_len, out)
                     : ccwrap_auth_decrypt(mode, ctx, in_len, in, out_len, out);
    }
    ccecb_ctx_clear(ccecb_context_size(mode), ctx);
    return rv;
}

int ccec_rfc6637_wrap_key(ccec_pub_ctx_t public_key, void *wrapped_key, unsigned long flags,
                          uint8_t symm_alg_id, size_t key_len, const void *key,
                          const struct ccec_rfc6637_curve *curve,
                          const struct ccec_rfc6637_wrap *wrap,
                          const uint8_t *fingerprint,
                          struct ccrng_state *rng)
{
    ccec_const_cp_t cp = ccec_ctx_cp(public_key);
    uint8_t *out = wrapped_key;
    uint8_t shared[RFC6637_MAX_SHARED_SIZE], kek[RFC6637_MAX_KEK_SIZE];
    size_t shared_len = sizeof(shared);

    if (ccec_rfc6637_wrap_key_size(public_key, flags, key_len) == 0 || curve == NULL || wrap == NULL
        || fingerprint == NULL || wrap->kek_size > sizeof(kek))
        return CCERR_PARAMETER;

    size_t padded = rfc6637_padded_size(key_len);
    uint8_t *m = malloc(padded);
    if (m == NULL)
        return CCERR_MEMORY;
    unsigned checksum = 0;
    for (size_t i = 0; i < key_len; i++)
        checksum += ((const uint8_t *)key)[i];
    m[0] = symm_alg_id;
    memcpy(m + 1, key, key_len);
    m[1 + key_len] = (uint8_t)(checksum >> 8);
    m[2 + key_len] = (uint8_t)checksum;
    memset(m + 3 + key_len, (int)(padded - 3 - key_len), padded - 3 - key_len);

    ccec_full_ctx_decl_cp(cp, ephemeral);
    int rv = ccec_generate_key_fips(cp, rng, ephemeral);
    if (rv == CCERR_OK)
        rv = ccecdh_compute_shared_secret(ephemeral, public_key, &shared_len, shared, rng);
    if (rv == CCERR_OK)
        rv = rfc6637_kek(wrap->difun(), wrap->kdfhash_id, wrap->kek_id, wrap->kek_size,
                         curve, fingerprint, shared_len, shared, kek);
    if (rv == CCERR_OK) {
        /* the MPI of 04 || x || y: its bit count, the top octet 0x04 holding three bits */
        size_t point_len = ccec_export_pub_size(ccec_ctx_pub(ephemeral));
        size_t bits = (point_len - 1) * 8 + 3;
        size_t wrapped_len = padded + 8;
        out[0] = (uint8_t)(bits >> 8);
        out[1] = (uint8_t)bits;
        ccec_export_pub(ccec_ctx_pub(ephemeral), out + 2);
        out[2 + point_len] = (uint8_t)wrapped_len;
        rv = rfc6637_aes_wrap(true, wrap->kek_size, kek, padded, m, &wrapped_len, out + 3 + point_len);
    }

    ccec_full_ctx_clear_cp(cp, ephemeral);
    cc_clear(sizeof(shared), shared);
    cc_clear(sizeof(kek), kek);
    cc_clear(padded, m);
    free(m);
    return rv;
}

int ccec_rfc6637_unwrap_key(ccec_full_ctx_t private_key, size_t *key_len, void *key,
                            unsigned long flags, uint8_t *symm_key_alg,
                            const struct ccec_rfc6637_curve *curve,
                            const struct ccec_rfc6637_unwrap *unwrap,
                            const uint8_t *fingerprint,
                            size_t wrapped_key_len, const void *wrapped_key)
{
    ccec_const_cp_t cp = ccec_ctx_cp(private_key);
    const uint8_t *in = wrapped_key;
    uint8_t shared[RFC6637_MAX_SHARED_SIZE], kek[RFC6637_MAX_KEK_SIZE];
    size_t shared_len = sizeof(shared);

    if (flags != 0 || cp == NULL || curve == NULL || unwrap == NULL || fingerprint == NULL
        || unwrap->kek_size > sizeof(kek))
        return CCERR_PARAMETER;

    size_t point_len = ccec_x963_export_size(0, ccec_ctx_pub(private_key));
    if (wrapped_key_len < 3 + point_len || (((size_t)in[0] << 8 | in[1]) + 7) / 8 != point_len)
        return CCERR_PARAMETER;
    size_t c_len = in[2 + point_len];
    if (c_len != wrapped_key_len - 3 - point_len || c_len < 24 || c_len % 8 != 0)
        return CCERR_PARAMETER;

    ccec_pub_ctx_decl_cp(cp, ephemeral);
    int rv = ccec_import_pub(cp, point_len, in + 2, ephemeral);
    if (rv == CCERR_OK)
        rv = ccecdh_compute_shared_secret(private_key, ephemeral, &shared_len, shared, NULL);
    if (rv == CCERR_OK)
        rv = rfc6637_kek(unwrap->difun(), unwrap->kdfhash_id, unwrap->kek_id, unwrap->kek_size,
                         curve, fingerprint, shared_len, shared, kek);
    cc_clear(sizeof(shared), shared);
    if (rv != CCERR_OK) {
        cc_clear(sizeof(kek), kek);
        return rv;
    }

    size_t m_len = c_len - 8;
    uint8_t *m = malloc(m_len);
    if (m == NULL) {
        cc_clear(sizeof(kek), kek);
        return CCERR_MEMORY;
    }
    rv = rfc6637_aes_wrap(false, unwrap->kek_size, kek, c_len, in + 3 + point_len, &m_len, m);
    cc_clear(sizeof(kek), kek);

    /* senders pad to 8-byte granularity, some (Apple's) past the next multiple of 8 */
    size_t pad = rv == CCERR_OK ? m[m_len - 1] : 0;
    if (rv == CCERR_OK && (pad == 0 || m_len < pad + 3))
        rv = CCERR_INTEGRITY;
    for (size_t i = 0; rv == CCERR_OK && i < pad; i++) {
        if (m[m_len - 1 - i] != pad)
            rv = CCERR_INTEGRITY;
    }
    if (rv == CCERR_OK) {
        size_t klen = m_len - pad - 3;
        unsigned checksum = 0;
        for (size_t i = 0; i < klen; i++)
            checksum += m[1 + i];
        if (((m[1 + klen] << 8 | m[2 + klen]) & 0xffff) != (checksum & 0xffff))
            rv = CCERR_INTEGRITY;
        else if (*key_len < klen)
            rv = CCERR_PARAMETER;
        else {
            memcpy(key, m + 1, klen);
            *key_len = klen;
            if (symm_key_alg != NULL)
                *symm_key_alg = m[0];
        }
    }
    cc_clear(c_len - 8, m);
    free(m);
    return rv;
}

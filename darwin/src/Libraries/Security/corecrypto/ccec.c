/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/ccec.h>
#include <corecrypto/ccder.h>
#include "cc_openssl.h"

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#include <stdarg.h>
#include <string.h>

#define EC_MAX_PRIME_SIZE 66
#define EC_MAX_POINT_SIZE (1 + 2 * EC_MAX_PRIME_SIZE)

static const struct ccec_cp cp192 = { ccn_nof(192), 192, "prime192v1", NID_X9_62_prime192v1 };
static const struct ccec_cp cp224 = { ccn_nof(224), 224, "secp224r1", NID_secp224r1 };
static const struct ccec_cp cp256 = { ccn_nof(256), 256, "prime256v1", NID_X9_62_prime256v1 };
static const struct ccec_cp cp384 = { ccn_nof(384), 384, "secp384r1", NID_secp384r1 };
static const struct ccec_cp cp521 = { ccn_nof(521), 521, "secp521r1", NID_secp521r1 };
static const ccec_const_cp_t curves[] = { &cp192, &cp224, &cp256, &cp384, &cp521 };
#define NCURVES (sizeof(curves) / sizeof(curves[0]))

ccec_const_cp_t ccec_cp_192(void) { return &cp192; }
ccec_const_cp_t ccec_cp_224(void) { return &cp224; }
ccec_const_cp_t ccec_cp_256(void) { return &cp256; }
ccec_const_cp_t ccec_cp_384(void) { return &cp384; }
ccec_const_cp_t ccec_cp_521(void) { return &cp521; }

ccec_const_cp_t ccec_get_cp(size_t keysize)
{
    for (size_t i = 0; i < NCURVES; i++) {
        if (curves[i]->bitlen == keysize)
            return curves[i];
    }
    return NULL;
}

bool ccec_keysize_is_supported(size_t keysize)
{
    return ccec_get_cp(keysize) != NULL;
}

ccec_const_cp_t ccec_curve_for_length_lookup(size_t keylen, ...)
{
    ccec_const_cp_t cp, found = NULL;
    va_list ap;

    va_start(ap, keylen);
    while (found == NULL && (cp = va_arg(ap, ccec_const_cp_t)) != NULL) {
        if (keylen <= 8 * ccec_cp_prime_size(cp))
            found = cp;
    }
    va_end(ap);
    return found;
}

static size_t keysize_for_prime_size(size_t prime_size)
{
    for (size_t i = 0; i < NCURVES; i++) {
        if (ccec_cp_prime_size(curves[i]) == prime_size)
            return curves[i]->bitlen;
    }
    return 0;
}

size_t ccec_x963_import_pub_size(size_t len)
{
    return (len > 1 && (len - 1) % 2 == 0) ? keysize_for_prime_size((len - 1) / 2) : 0;
}

size_t ccec_x963_import_priv_size(size_t len)
{
    return (len > 1 && (len - 1) % 3 == 0) ? keysize_for_prime_size((len - 1) / 3) : 0;
}

size_t ccec_x963_export_size(int fullkey, ccec_pub_ctx_t key)
{
    return 1 + (fullkey ? 3 : 2) * ccec_ctx_size(key);
}

size_t ccec_export_pub_size(ccec_pub_ctx_t key)
{
    return ccec_x963_export_size(0, key);
}

void ccec_export_pub(ccec_pub_ctx_t key, void *out)
{
    uint8_t *o = out;
    size_t size = ccec_ctx_size(key);

    o[0] = 0x04;
    ccn_write_uint_padded(ccec_ctx_n(key), ccec_ctx_x(key), size, o + 1);
    ccn_write_uint_padded(ccec_ctx_n(key), ccec_ctx_y(key), size, o + 1 + size);
}

static EVP_PKEY *ec_pkey(ccec_const_cp_t cp, const uint8_t *point, size_t point_len, const cc_unit *k)
{
    OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
    BIGNUM *priv = NULL;
    OSSL_PARAM *params = NULL;
    EVP_PKEY_CTX *ctx = NULL;
    EVP_PKEY *pkey = NULL;
    int ok = bld != NULL
        && OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_PKEY_PARAM_GROUP_NAME, cp->name, 0)
        && OSSL_PARAM_BLD_push_octet_string(bld, OSSL_PKEY_PARAM_PUB_KEY, point, point_len);

    if (ok && k != NULL) {
        ok = (priv = cc_bn_from_secret_ccn(ccec_cp_n(cp), k)) != NULL
            && OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_PRIV_KEY, priv);
    }
    ok = ok && (params = OSSL_PARAM_BLD_to_param(bld)) != NULL
        && (ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL)) != NULL
        && EVP_PKEY_fromdata_init(ctx) > 0
        && EVP_PKEY_fromdata(ctx, &pkey, k != NULL ? EVP_PKEY_KEYPAIR : EVP_PKEY_PUBLIC_KEY, params) > 0;
    if (!ok) {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

    EVP_PKEY_CTX_free(ctx);
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(bld);
    BN_clear_free(priv);
    return pkey;
}

static EVP_PKEY *ec_pkey_from_key(ccec_pub_ctx_t key, const cc_unit *k)
{
    uint8_t point[EC_MAX_POINT_SIZE];
    size_t len;

    if (ccec_ctx_cp(key) == NULL || (len = ccec_export_pub_size(key)) > sizeof(point))
        return NULL;
    ccec_export_pub(key, point);
    return ec_pkey(ccec_ctx_cp(key), point, len, k);
}

int ccec_import_pub(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_pub_ctx_t key)
{
    if (cp == NULL || in == NULL || key == NULL)
        return CCERR_PARAMETER;
    size_t size = ccec_cp_prime_size(cp);
    if (in_len != 1 + 2 * size || in[0] != 0x04)
        return CCERR_PARAMETER;

    EVP_PKEY *pkey = ec_pkey(cp, in, in_len, NULL);
    if (pkey == NULL)
        return CCERR_PARAMETER;
    EVP_PKEY_free(pkey);

    ccec_ctx_init(cp, key);
    if (ccn_read_uint(ccec_cp_n(cp), ccec_ctx_x(key), size, in + 1) != CCERR_OK
        || ccn_read_uint(ccec_cp_n(cp), ccec_ctx_y(key), size, in + 1 + size) != CCERR_OK)
        return CCERR_PARAMETER;
    return CCERR_OK;
}

/* checks 0 < k < order and sets the public point of key to k * G */
static int ec_set_public(const EC_GROUP *group, ccec_full_ctx_t key)
{
    cc_size n = ccec_ctx_n(key);
    BIGNUM *k = cc_bn_from_secret_ccn(n, ccec_ctx_k(key));
    BIGNUM *x = BN_new(), *y = BN_new();
    EC_POINT *point = EC_POINT_new(group);
    BN_CTX *bnctx = BN_CTX_new();
    int ok = k != NULL && x != NULL && y != NULL && point != NULL && bnctx != NULL
        && !BN_is_zero(k) && BN_cmp(k, EC_GROUP_get0_order(group)) < 0
        && EC_POINT_mul(group, point, k, NULL, NULL, bnctx)
        && EC_POINT_get_affine_coordinates(group, point, x, y, bnctx)
        && cc_bn_to_ccn(x, n, ccec_ctx_x(key)) == CCERR_OK
        && cc_bn_to_ccn(y, n, ccec_ctx_y(key)) == CCERR_OK;

    BN_CTX_free(bnctx);
    EC_POINT_clear_free(point);
    BN_free(x);
    BN_free(y);
    BN_clear_free(k);
    return ok ? CCERR_OK : CCERR_PARAMETER;
}

int ccec_generate_key_fips(ccec_const_cp_t cp, struct ccrng_state *rng, ccec_full_ctx_t key)
{
    EC_GROUP *group = cp != NULL ? EC_GROUP_new_by_curve_name(cp->nid) : NULL;
    BIGNUM *k = BN_secure_new();
    uint8_t buf[EC_MAX_PRIME_SIZE];
    int rv = CCERR_INTERNAL;

    if (group == NULL || k == NULL)
        goto out;
    const BIGNUM *order = EC_GROUP_get0_order(group);
    int bits = BN_num_bits(order);
    size_t nbytes = (size_t)(bits + 7) / 8;
    if (nbytes > sizeof(buf))
        goto out;
    BN_set_flags(k, BN_FLG_CONSTTIME);

    /* FIPS 186-4 B.4.2: draw candidates until one lies in [1, order - 1] */
    for (;;) {
        if ((rv = ccrng_generate(rng, nbytes, buf)) != CCERR_OK)
            goto out;
        if (bits % 8 != 0)
            buf[0] &= 0xff >> (8 - bits % 8);
        if (BN_bin2bn(buf, (int)nbytes, k) == NULL) {
            rv = CCERR_INTERNAL;
            goto out;
        }
        if (!BN_is_zero(k) && BN_cmp(k, order) < 0)
            break;
    }

    ccec_ctx_init(cp, key);
    if ((rv = cc_bn_to_ccn(k, ccec_cp_n(cp), ccec_ctx_k(key))) == CCERR_OK)
        rv = ec_set_public(group, key);

out:
    OPENSSL_cleanse(buf, sizeof(buf));
    BN_clear_free(k);
    EC_GROUP_free(group);
    return rv;
}

/* ECPrivateKey (SEC 1 C.4): the scalar, the namedCurve OID and the public key, both optional */
struct ec_der_priv {
    const uint8_t *k;
    size_t k_len;
    const uint8_t *oid;
    const uint8_t *pub;
    size_t pub_len;
};

static int ec_der_decode_priv(struct ec_der_priv *out, size_t len, const uint8_t *der)
{
    const uint8_t *der_end = der != NULL ? der + len : NULL;
    const uint8_t *seq_end = NULL, *field_end = NULL;
    size_t field_len;

    memset(out, 0, sizeof(*out));
    der = ccder_decode_sequence_tl(&seq_end, der, der_end);
    der = ccder_decode_tl(CCDER_INTEGER, &field_len, der, seq_end);
    if (der == NULL || field_len != 1 || der[0] != 1)
        return CCERR_PARAMETER;
    der = ccder_decode_tl(CCDER_OCTET_STRING, &out->k_len, der + 1, seq_end);
    if (der == NULL)
        return CCERR_PARAMETER;
    out->k = der;
    der += out->k_len;

    if (der < seq_end && der[0] == (CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED | 0)) {
        der = ccder_decode_constructed_tl(CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED | 0, &field_end, der, seq_end);
        if (ccder_decode_tl(CCDER_OBJECT_IDENTIFIER, &field_len, der, field_end) == NULL)
            return CCERR_PARAMETER;
        out->oid = der;
        der = field_end;
    }
    if (der != NULL && der < seq_end && der[0] == (CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED | 1)) {
        der = ccder_decode_constructed_tl(CCDER_CONTEXT_SPECIFIC | CCDER_CONSTRUCTED | 1, &field_end, der, seq_end);
        const uint8_t *bits = ccder_decode_tl(CCDER_BIT_STRING, &field_len, der, field_end);
        if (bits == NULL || field_len < 2 || bits[0] != 0)
            return CCERR_PARAMETER;
        out->pub = bits + 1;
        out->pub_len = field_len - 1;
        der = field_end;
    }
    return (der != NULL && der == seq_end) ? CCERR_OK : CCERR_PARAMETER;
}

int ccec_der_import_priv_keytype(size_t len, const uint8_t *der, ccoid_t *oid, size_t *n)
{
    struct ec_der_priv priv;
    int rv = ec_der_decode_priv(&priv, len, der);
    if (rv != CCERR_OK)
        return rv;
    if (oid != NULL)
        *oid = priv.oid;
    if (n != NULL)
        *n = priv.k_len;
    return CCERR_OK;
}

int ccec_der_import_priv(ccec_const_cp_t cp, size_t len, const uint8_t *der, ccec_full_ctx_t key)
{
    struct ec_der_priv priv;
    int rv = ec_der_decode_priv(&priv, len, der);

    if (rv != CCERR_OK || cp == NULL || priv.k_len == 0 || priv.k_len > ccec_cp_prime_size(cp))
        return CCERR_PARAMETER;
    ccec_ctx_init(cp, key);
    if (ccn_read_uint(ccec_cp_n(cp), ccec_ctx_k(key), priv.k_len, priv.k) != CCERR_OK)
        return CCERR_PARAMETER;

    EC_GROUP *group = EC_GROUP_new_by_curve_name(cp->nid);
    if (group == NULL)
        return CCERR_INTERNAL;
    rv = ec_set_public(group, key);
    EC_GROUP_free(group);
    if (rv == CCERR_OK && priv.pub != NULL && priv.pub[0] == 0x04) {
        uint8_t point[EC_MAX_POINT_SIZE];
        ccec_export_pub(ccec_ctx_pub(key), point);
        if (priv.pub_len != ccec_export_pub_size(ccec_ctx_pub(key)) || memcmp(point, priv.pub, priv.pub_len) != 0)
            rv = CCERR_PARAMETER;
    }
    if (rv != CCERR_OK)
        cc_clear(ccec_full_ctx_size(ccn_sizeof_n(ccec_cp_n(cp))), key);
    return rv;
}

size_t ccec_sign_max_size(ccec_const_cp_t cp)
{
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, 2 * ccder_sizeof(CCDER_INTEGER, ccec_cp_prime_size(cp) + 1));
}

int ccec_sign(ccec_full_ctx_t key, size_t digest_len, const uint8_t *digest,
              size_t *sig_len, uint8_t *sig, struct ccrng_state *rng)
{
    (void)rng;
    EVP_PKEY *pkey = ec_pkey_from_key(ccec_ctx_pub(key), ccec_ctx_k(key));
    EVP_PKEY_CTX *ctx = pkey != NULL ? EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL) : NULL;
    size_t len = *sig_len;
    int rv = (ctx != NULL
              && EVP_PKEY_sign_init(ctx) > 0
              && EVP_PKEY_sign(ctx, sig, &len, digest, digest_len) > 0) ? CCERR_OK : CCERR_INTERNAL;

    if (rv == CCERR_OK)
        *sig_len = len;
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return rv;
}

int ccec_verify(ccec_pub_ctx_t key, size_t digest_len, const uint8_t *digest,
                size_t sig_len, const uint8_t *sig, bool *valid)
{
    *valid = false;
    EVP_PKEY *pkey = ec_pkey_from_key(key, NULL);
    EVP_PKEY_CTX *ctx = pkey != NULL ? EVP_PKEY_CTX_new_from_pkey(NULL, pkey, NULL) : NULL;
    int rv = CCERR_PARAMETER;

    if (ctx != NULL && EVP_PKEY_verify_init(ctx) > 0) {
        int result = EVP_PKEY_verify(ctx, sig, sig_len, digest, digest_len);
        if (result >= 0) {
            *valid = result == 1;
            rv = CCERR_OK;
        }
    }
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return rv;
}

int ccecdh_compute_shared_secret(ccec_full_ctx_t private_key, ccec_pub_ctx_t public_key,
                                 size_t *computed_shared_secret_len, uint8_t *computed_shared_secret,
                                 struct ccrng_state *masking_rng)
{
    (void)masking_rng;
    ccec_const_cp_t cp = ccec_ctx_cp(private_key);

    if (cp == NULL || ccec_ctx_cp(public_key) != cp || *computed_shared_secret_len < ccec_cp_prime_size(cp))
        return CCERR_PARAMETER;

    EVP_PKEY *mine = ec_pkey_from_key(ccec_ctx_pub(private_key), ccec_ctx_k(private_key));
    EVP_PKEY *peer = ec_pkey_from_key(public_key, NULL);
    EVP_PKEY_CTX *ctx = mine != NULL ? EVP_PKEY_CTX_new_from_pkey(NULL, mine, NULL) : NULL;
    size_t len = *computed_shared_secret_len;
    int rv = (ctx != NULL && peer != NULL
              && EVP_PKEY_derive_init(ctx) > 0
              && EVP_PKEY_derive_set_peer(ctx, peer) > 0
              && EVP_PKEY_derive(ctx, computed_shared_secret, &len) > 0
              && len == ccec_cp_prime_size(cp)) ? CCERR_OK : CCERR_INTERNAL;

    if (rv == CCERR_OK)
        *computed_shared_secret_len = len;
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(peer);
    EVP_PKEY_free(mine);
    return rv;
}

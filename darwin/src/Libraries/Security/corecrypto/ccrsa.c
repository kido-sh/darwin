/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/ccrsa.h>
#include <corecrypto/ccder.h>
#include <corecrypto/cczp_priv.h>
#include "cc_libressl.h"

#include <limits.h>
#include <openssl/rsa.h>

enum { RSA_D, RSA_P, RSA_Q, RSA_DP, RSA_DQ, RSA_QINV, RSA_NPRIV };

static cc_unit *rsa_priv_field(ccrsa_full_ctx_t key, int i)
{
    switch (i) {
    case RSA_D: return ccrsa_ctx_d(key);
    case RSA_P: return cczp_prime(ccrsa_ctx_private_zp(key));
    case RSA_Q: return cczp_prime(ccrsa_ctx_private_zq(key));
    case RSA_DP: return ccrsa_ctx_private_dp(key);
    case RSA_DQ: return ccrsa_ctx_private_dq(key);
    default: return ccrsa_ctx_private_qinv(key);
    }
}
#define RSA_PRIV(_fk_, _i_) rsa_priv_field(_fk_, _i_)

static void rsa_set_prime_headers(ccrsa_full_ctx_t key)
{
    cczp_t zp[2] = { ccrsa_ctx_private_zp(key), ccrsa_ctx_private_zq(key) };
    for (int i = 0; i < 2; i++) {
        zp[i]->n = ccrsa_ctx_n(key);
        zp[i]->bitlen = ccn_bitlen(ccrsa_ctx_n(key), cczp_prime(zp[i]));
        zp[i]->funcs = NULL;
    }
}

static const uint8_t rsa_encryption_oid[] = {
    0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01
};

size_t ccrsa_block_size(ccrsa_pub_ctx_t key)
{
    return ccn_write_uint_size(ccrsa_ctx_n(key), ccrsa_ctx_m(key));
}

int ccrsa_pub_crypt(ccrsa_pub_ctx_t key, cc_unit *out, const cc_unit *in)
{
    cc_size n = ccrsa_ctx_n(key);
    const cc_unit *m = ccrsa_ctx_m(key);

    if (n == 0 || (m[0] & 1) == 0 || ccn_cmp(n, in, m) >= 0)
        return CCERR_PARAMETER;
    cc_unit *ws = malloc(ccn_sizeof_n(3 * n + 2));
    if (ws == NULL)
        return CCERR_MEMORY;
    ccn_mont_powmod(n, out, in, n, ccrsa_ctx_e(key), m, *ccrsa_ctx_m0inv(key), ccrsa_ctx_mr2(key), ws);
    free(ws);
    return CCERR_OK;
}

static RSA *rsa_from_full(ccrsa_full_ctx_t key)
{
    cc_size n = ccrsa_ctx_n(key);
    BIGNUM *pub[2] = { cc_bn_from_ccn(n, ccrsa_ctx_m(key)), cc_bn_from_ccn(n, ccrsa_ctx_e(key)) };
    BIGNUM *priv[RSA_NPRIV] = { NULL };
    RSA *rsa = RSA_new();
    bool ok = rsa != NULL && pub[0] != NULL && pub[1] != NULL;

    for (int i = 0; ok && i < RSA_NPRIV; i++)
        ok = (priv[i] = cc_bn_from_secret_ccn(n, RSA_PRIV(key, i))) != NULL;
    if (ok && RSA_set0_key(rsa, pub[0], pub[1], priv[RSA_D]))
        pub[0] = pub[1] = priv[RSA_D] = NULL;
    else
        ok = false;
    if (ok && RSA_set0_factors(rsa, priv[RSA_P], priv[RSA_Q]))
        priv[RSA_P] = priv[RSA_Q] = NULL;
    else
        ok = false;
    if (ok && RSA_set0_crt_params(rsa, priv[RSA_DP], priv[RSA_DQ], priv[RSA_QINV]))
        priv[RSA_DP] = priv[RSA_DQ] = priv[RSA_QINV] = NULL;
    else
        ok = false;

    BN_free(pub[0]);
    BN_free(pub[1]);
    for (int i = 0; i < RSA_NPRIV; i++)
        BN_clear_free(priv[i]);
    if (!ok) {
        RSA_free(rsa);
        return NULL;
    }
    return rsa;
}

int ccrsa_priv_crypt(ccrsa_full_ctx_t key, cc_unit *out, const cc_unit *in)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t k = ccrsa_block_size(ccrsa_ctx_public(key));
    int rv = CCERR_INTERNAL;

    if (n == 0 || ccn_cmp(n, in, ccrsa_ctx_m(key)) >= 0)
        return CCERR_PARAMETER;

    uint8_t *buf = malloc(2 * k);
    RSA *rsa = rsa_from_full(key);
    int outlen;

    if (buf != NULL && rsa != NULL && k <= INT_MAX && (size_t)RSA_size(rsa) == k
        && ccn_write_uint_padded(n, in, k, buf) == CCERR_OK
        && (outlen = RSA_private_decrypt((int)k, buf, buf + k, rsa, RSA_NO_PADDING)) > 0
        && ccn_read_uint(n, out, (size_t)outlen, buf + k) == CCERR_OK)
        rv = CCERR_OK;

    if (buf != NULL) {
        OPENSSL_cleanse(buf, 2 * k);
        free(buf);
    }
    RSA_free(rsa);
    return rv;
}

/* the INTEGER bodies of RSAPublicKey ::= SEQUENCE { modulus, publicExponent } */
struct rsa_der_int {
    const uint8_t *body;
    size_t len;
};

static const uint8_t *rsa_decode_int(struct rsa_der_int *v, const uint8_t *der, const uint8_t *der_end)
{
    size_t len;
    der = ccder_decode_tl(CCDER_INTEGER, &len, der, der_end);
    if (der == NULL || len == 0 || (der[0] & 0x80) != 0)
        return NULL;
    v->body = der;
    v->len = len;
    while (v->len > 0 && *v->body == 0) {
        v->body++;
        v->len--;
    }
    return der + len;
}

static int rsa_decode_pkcs1_pub(struct rsa_der_int v[2], const uint8_t *der, const uint8_t *der_end)
{
    const uint8_t *body_end = NULL;
    der = ccder_decode_sequence_tl(&body_end, der, der_end);
    if (der == NULL || body_end != der_end)
        return CCERR_PARAMETER;
    der = rsa_decode_int(&v[0], der, body_end);
    der = rsa_decode_int(&v[1], der, body_end);
    return der == body_end ? CCERR_OK : CCERR_PARAMETER;
}

/* SubjectPublicKeyInfo with rsaEncryption around an RSAPublicKey */
static int rsa_decode_spki_pub(struct rsa_der_int v[2], const uint8_t *der, const uint8_t *der_end)
{
    const uint8_t *spki_end = NULL, *alg_end = NULL;
    size_t len;

    der = ccder_decode_sequence_tl(&spki_end, der, der_end);
    if (der == NULL || spki_end != der_end)
        return CCERR_PARAMETER;
    der = ccder_decode_sequence_tl(&alg_end, der, spki_end);
    if (der == NULL || (size_t)(alg_end - der) < sizeof(rsa_encryption_oid)
        || memcmp(der, rsa_encryption_oid, sizeof(rsa_encryption_oid)) != 0)
        return CCERR_PARAMETER;
    der = ccder_decode_tl(CCDER_BIT_STRING, &len, alg_end, spki_end);
    if (der == NULL || len < 1 || der[0] != 0 || der + len != spki_end)
        return CCERR_PARAMETER;
    return rsa_decode_pkcs1_pub(v, der + 1, spki_end);
}

static int rsa_decode_pub(struct rsa_der_int v[2], size_t inlen, const uint8_t *der)
{
    if (der == NULL)
        return CCERR_PARAMETER;
    if (rsa_decode_pkcs1_pub(v, der, der + inlen) == CCERR_OK)
        return CCERR_OK;
    return rsa_decode_spki_pub(v, der, der + inlen);
}

static int rsa_set_pub(ccrsa_pub_ctx_t key, const struct rsa_der_int *m, const struct rsa_der_int *e)
{
    cc_size n = ccn_nof_size(m->len);

    if (n == 0 || n > ccrsa_ctx_n(key) || ccn_nof_size(e->len) > n)
        return CCERR_PARAMETER;
    ccrsa_ctx_n(key) = n;
    if (ccn_read_uint(n, ccrsa_ctx_m(key), m->len, m->body) != CCERR_OK
        || cczp_init(ccrsa_ctx_zm(key)) != CCERR_OK
        || ccn_read_uint(n, ccrsa_ctx_e(key), e->len, e->body) != CCERR_OK)
        return CCERR_PARAMETER;
    const cc_unit *ev = ccrsa_ctx_e(key);
    if ((ev[0] & 1) == 0 || ccn_bitlen(n, ev) < 2 || ccn_cmp(n, ev, ccrsa_ctx_m(key)) >= 0)
        return CCERR_PARAMETER;
    return CCERR_OK;
}

cc_size ccrsa_import_pub_n(size_t inlen, const uint8_t *der)
{
    struct rsa_der_int v[2];
    if (rsa_decode_pub(v, inlen, der) != CCERR_OK)
        return 0;
    return ccn_nof_size(v[0].len);
}

int ccrsa_import_pub(ccrsa_pub_ctx_t key, size_t inlen, const uint8_t *der)
{
    struct rsa_der_int v[2];
    int rv = rsa_decode_pub(v, inlen, der);
    return rv != CCERR_OK ? rv : rsa_set_pub(key, &v[0], &v[1]);
}

/* RSAPrivateKey ::= SEQUENCE { version 0, n, e, d, p, q, dp, dq, qinv } */
static int rsa_decode_priv(struct rsa_der_int v[2 + RSA_NPRIV], size_t inlen, const uint8_t *der)
{
    const uint8_t *der_end = der != NULL ? der + inlen : NULL;
    const uint8_t *body_end = NULL;
    struct rsa_der_int version;

    der = ccder_decode_sequence_tl(&body_end, der, der_end);
    if (der == NULL || body_end != der_end)
        return CCERR_PARAMETER;
    der = rsa_decode_int(&version, der, body_end);
    if (der == NULL || version.len != 0)
        return CCERR_PARAMETER;
    for (int i = 0; i < 2 + RSA_NPRIV; i++)
        der = rsa_decode_int(&v[i], der, body_end);
    return der == body_end ? CCERR_OK : CCERR_PARAMETER;
}

cc_size ccrsa_import_priv_n(size_t inlen, const uint8_t *der)
{
    struct rsa_der_int v[2 + RSA_NPRIV];
    if (rsa_decode_priv(v, inlen, der) != CCERR_OK)
        return 0;
    return ccn_nof_size(v[0].len);
}

int ccrsa_import_priv(ccrsa_full_ctx_t key, size_t inlen, const uint8_t *der)
{
    struct rsa_der_int v[2 + RSA_NPRIV];
    int rv = rsa_decode_priv(v, inlen, der);
    if (rv == CCERR_OK)
        rv = rsa_set_pub(ccrsa_ctx_public(key), &v[0], &v[1]);
    for (int i = 0; rv == CCERR_OK && i < RSA_NPRIV; i++) {
        if (ccn_nof_size(v[2 + i].len) > ccrsa_ctx_n(key)
            || ccn_read_uint(ccrsa_ctx_n(key), RSA_PRIV(key, i), v[2 + i].len, v[2 + i].body) != CCERR_OK)
            rv = CCERR_PARAMETER;
    }
    if (rv == CCERR_OK)
        rsa_set_prime_headers(key);
    return rv;
}

static const cc_unit rsa_zero = 0;

static size_t rsa_priv_body_size(ccrsa_full_ctx_t key)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t size = ccder_sizeof_integer(1, &rsa_zero)
        + ccder_sizeof_integer(n, ccrsa_ctx_m(key))
        + ccder_sizeof_integer(n, ccrsa_ctx_e(key));
    for (int i = 0; i < RSA_NPRIV; i++)
        size += ccder_sizeof_integer(n, RSA_PRIV(key, i));
    return size;
}

size_t ccrsa_export_priv_size(ccrsa_full_ctx_t key)
{
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, rsa_priv_body_size(key));
}

int ccrsa_export_priv(ccrsa_full_ctx_t key, size_t outlen, uint8_t *out)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t size = ccrsa_export_priv_size(key);

    if (out == NULL || outlen < size)
        return CCERR_PARAMETER;
    uint8_t *der_end = out + size;
    uint8_t *p = der_end;
    for (int i = RSA_NPRIV - 1; i >= 0; i--)
        p = ccder_encode_integer(n, RSA_PRIV(key, i), out, p);
    p = ccder_encode_integer(n, ccrsa_ctx_e(key), out, p);
    p = ccder_encode_integer(n, ccrsa_ctx_m(key), out, p);
    p = ccder_encode_integer(1, &rsa_zero, out, p);
    p = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, out, p);
    return p == out ? CCERR_OK : CCERR_INTERNAL;
}

/* n, e, then the private components in RSA_PRIV order */
enum { RSA_BN_N, RSA_BN_E, RSA_BN_PRIV, RSA_NBN = RSA_BN_PRIV + RSA_NPRIV };

static void rsa_free_bns(BIGNUM *v[RSA_NBN])
{
    for (int i = 0; i < RSA_NBN; i++)
        BN_clear_free(v[i]);
}

/* stores the components with p > q, as CommonCrypto's CRT export expects, within the capacity ccrsa_ctx_n */
static int rsa_set_full(ccrsa_full_ctx_t key, BIGNUM *v[RSA_NBN])
{
    BIGNUM **p = &v[RSA_BN_PRIV + RSA_P], **q = &v[RSA_BN_PRIV + RSA_Q];

    if (BN_cmp(*p, *q) < 0) {
        BIGNUM *t = *p;
        *p = *q;
        *q = t;
        t = v[RSA_BN_PRIV + RSA_DP];
        v[RSA_BN_PRIV + RSA_DP] = v[RSA_BN_PRIV + RSA_DQ];
        v[RSA_BN_PRIV + RSA_DQ] = t;
        BN_CTX *ctx = BN_CTX_new();
        BN_set_flags(*q, BN_FLG_CONSTTIME);
        bool ok = ctx != NULL && BN_mod_inverse(v[RSA_BN_PRIV + RSA_QINV], *q, *p, ctx) != NULL;
        BN_CTX_free(ctx);
        if (!ok)
            return CCERR_INTERNAL;
    }

    cc_size n = ccn_nof_size((size_t)BN_num_bytes(v[RSA_BN_N]));
    if (n == 0 || n > ccrsa_ctx_n(key))
        return CCERR_PARAMETER;
    ccrsa_ctx_n(key) = n;
    bool ok = cc_bn_to_ccn(v[RSA_BN_N], n, ccrsa_ctx_m(key)) == CCERR_OK
        && cc_bn_to_ccn(v[RSA_BN_E], n, ccrsa_ctx_e(key)) == CCERR_OK;
    for (int i = 0; ok && i < RSA_NPRIV; i++)
        ok = cc_bn_to_ccn(v[RSA_BN_PRIV + i], n, RSA_PRIV(key, i)) == CCERR_OK;
    if (!ok || cczp_init(ccrsa_ctx_zm(key)) != CCERR_OK) {
        ccrsa_full_ctx_clear(ccn_sizeof_n(n), key);
        return CCERR_PARAMETER;
    }
    rsa_set_prime_headers(key);
    return CCERR_OK;
}

int ccrsa_generate_fips186_key(size_t nbits, ccrsa_full_ctx_t key,
                               size_t e_size, const void *e,
                               struct ccrng_state *rng1, struct ccrng_state *rng2)
{
    (void)rng1;
    (void)rng2;
    RSA *rsa = NULL;
    BIGNUM *bn_e = NULL;
    BIGNUM *v[RSA_NBN] = { NULL };
    const BIGNUM *got[RSA_NBN] = { NULL };
    int rv = CCERR_PARAMETER;

    if (nbits < 512 || nbits > 16384 || ccn_nof(nbits) > ccrsa_ctx_n(key) || e == NULL || e_size == 0)
        return CCERR_PARAMETER;
    if ((bn_e = BN_bin2bn(e, (int)e_size, NULL)) == NULL
        || (rsa = RSA_new()) == NULL
        || RSA_generate_key_ex(rsa, (int)nbits, bn_e, NULL) != 1)
        goto out;

    RSA_get0_key(rsa, &got[RSA_BN_N], &got[RSA_BN_E], &got[RSA_BN_PRIV + RSA_D]);
    RSA_get0_factors(rsa, &got[RSA_BN_PRIV + RSA_P], &got[RSA_BN_PRIV + RSA_Q]);
    RSA_get0_crt_params(rsa, &got[RSA_BN_PRIV + RSA_DP], &got[RSA_BN_PRIV + RSA_DQ], &got[RSA_BN_PRIV + RSA_QINV]);
    rv = CCERR_OK;
    for (int i = 0; rv == CCERR_OK && i < RSA_NBN; i++)
        rv = got[i] != NULL && (v[i] = BN_dup(got[i])) != NULL ? CCERR_OK : CCERR_INTERNAL;
    if (rv == CCERR_OK)
        rv = rsa_set_full(key, v);

out:
    rsa_free_bns(v);
    RSA_free(rsa);
    BN_free(bn_e);
    return rv;
}

/* d = e^-1 mod lcm(p - 1, q - 1), as RFC 8017 3.2 and SP 800-56B have it */
int ccrsa_make_priv(ccrsa_full_ctx_t key, size_t e_size, const uint8_t *e,
                    size_t p_size, const uint8_t *p, size_t q_size, const uint8_t *q)
{
    BIGNUM *v[RSA_NBN] = { NULL };
    BIGNUM *p1 = BN_new(), *q1 = BN_new(), *g = BN_new(), *lambda = BN_new();
    BN_CTX *ctx = BN_CTX_new();
    bool ok = p1 != NULL && q1 != NULL && g != NULL && lambda != NULL && ctx != NULL;

    v[RSA_BN_N] = BN_new();
    v[RSA_BN_E] = e != NULL ? BN_bin2bn(e, (int)e_size, NULL) : NULL;
    for (int i = RSA_BN_PRIV; i < RSA_NBN; i++) {
        if ((v[i] = BN_new()) != NULL)
            BN_set_flags(v[i], BN_FLG_CONSTTIME);
        ok = ok && v[i] != NULL;
    }
    BIGNUM *P = v[RSA_BN_PRIV + RSA_P], *Q = v[RSA_BN_PRIV + RSA_Q], *D = v[RSA_BN_PRIV + RSA_D];
    ok = ok && v[RSA_BN_N] != NULL && v[RSA_BN_E] != NULL && p != NULL && q != NULL
        && BN_bin2bn(p, (int)p_size, P) != NULL && BN_bin2bn(q, (int)q_size, Q) != NULL
        && BN_is_odd(P) && BN_is_odd(Q) && BN_cmp(P, BN_value_one()) > 0 && BN_cmp(Q, BN_value_one()) > 0
        && BN_mul(v[RSA_BN_N], P, Q, ctx)
        && BN_sub(p1, P, BN_value_one()) && BN_sub(q1, Q, BN_value_one())
        && BN_gcd(g, p1, q1, ctx) && BN_mul(lambda, p1, q1, ctx) && BN_div(lambda, NULL, lambda, g, ctx);
    if (ok) {
        BN_set_flags(lambda, BN_FLG_CONSTTIME);
        ok = BN_mod_inverse(D, v[RSA_BN_E], lambda, ctx) != NULL
            && BN_mod(v[RSA_BN_PRIV + RSA_DP], D, p1, ctx) && BN_mod(v[RSA_BN_PRIV + RSA_DQ], D, q1, ctx)
            && BN_mod_inverse(v[RSA_BN_PRIV + RSA_QINV], Q, P, ctx) != NULL;
    }
    int rv = ok ? rsa_set_full(key, v) : CCERR_PARAMETER;

    rsa_free_bns(v);
    BN_clear_free(p1);
    BN_clear_free(q1);
    BN_clear_free(g);
    BN_clear_free(lambda);
    BN_CTX_free(ctx);
    return rv;
}

int ccrsa_init_pub(ccrsa_pub_ctx_t key, const cc_unit *m, const cc_unit *e)
{
    cc_size n = ccrsa_ctx_n(key);
    if (n == 0)
        return CCERR_PARAMETER;
    ccn_set(n, ccrsa_ctx_m(key), m);
    ccn_set(n, ccrsa_ctx_e(key), e);
    return cczp_init(ccrsa_ctx_zm(key));
}

static int rsa_put(cc_size n, const cc_unit *s, uint8_t *out, size_t *out_size)
{
    size_t need = ccn_write_uint_size(n, s);
    if (out == NULL || out_size == NULL || *out_size < need)
        return CCERR_PARAMETER;
    ccn_write_uint(n, s, need, out);
    *out_size = need;
    return CCERR_OK;
}

int ccrsa_get_pubkey_components(ccrsa_pub_ctx_t key, uint8_t *m, size_t *m_size, uint8_t *e, size_t *e_size)
{
    cc_size n = ccrsa_ctx_n(key);
    int rv = rsa_put(n, ccrsa_ctx_m(key), m, m_size);
    return rv != CCERR_OK ? rv : rsa_put(n, ccrsa_ctx_e(key), e, e_size);
}

int ccrsa_get_fullkey_components(ccrsa_full_ctx_t key, uint8_t *m, size_t *m_size, uint8_t *d, size_t *d_size,
                                 uint8_t *p, size_t *p_size, uint8_t *q, size_t *q_size)
{
    cc_size n = ccrsa_ctx_n(key);
    int rv = rsa_put(n, ccrsa_ctx_m(key), m, m_size);
    if (rv == CCERR_OK)
        rv = rsa_put(n, RSA_PRIV(key, RSA_D), d, d_size);
    if (rv == CCERR_OK)
        rv = rsa_put(n, RSA_PRIV(key, RSA_P), p, p_size);
    return rv != CCERR_OK ? rv : rsa_put(n, RSA_PRIV(key, RSA_Q), q, q_size);
}

size_t ccrsa_export_pub_size(ccrsa_pub_ctx_t key)
{
    cc_size n = ccrsa_ctx_n(key);
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        ccder_sizeof_integer(n, ccrsa_ctx_m(key)) + ccder_sizeof_integer(n, ccrsa_ctx_e(key)));
}

int ccrsa_export_pub(ccrsa_pub_ctx_t key, size_t outlen, uint8_t *out)
{
    cc_size n = ccrsa_ctx_n(key);
    size_t size = ccrsa_export_pub_size(key);

    if (out == NULL || outlen < size)
        return CCERR_PARAMETER;
    uint8_t *der_end = out + size;
    uint8_t *p = ccder_encode_integer(n, ccrsa_ctx_e(key), out, der_end);
    p = ccder_encode_integer(n, ccrsa_ctx_m(key), out, p);
    p = ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, out, p);
    return p == out ? CCERR_OK : CCERR_INTERNAL;
}

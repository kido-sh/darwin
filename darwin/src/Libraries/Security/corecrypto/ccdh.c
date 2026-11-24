/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/ccdh.h>
#include "cc_libressl.h"

#include <stdlib.h>
#include <string.h>

static bool dh_in_range(ccdh_const_gp_t gp, const BIGNUM *v)
{
    BIGNUM *p = cc_bn_from_ccn(ccdh_gp_n(gp), CCDH_GP_PRIME(gp));
    bool ok = p != NULL && BN_sub_word(p, 1) && BN_cmp(v, BN_value_one()) > 0 && BN_cmp(v, p) < 0;
    BN_free(p);
    return ok;
}

/* r = base^x mod p, in constant time in x; fails when r is out of range */
static int dh_modexp(ccdh_const_gp_t gp, const cc_unit *base, const cc_unit *x, cc_unit *r)
{
    cc_size n = ccdh_gp_n(gp);
    BIGNUM *p = cc_bn_from_ccn(n, CCDH_GP_PRIME(gp));
    BIGNUM *b = cc_bn_from_ccn(n, base);
    BIGNUM *e = cc_bn_from_secret_ccn(n, x);
    BIGNUM *result = BN_new();
    BN_CTX *ctx = BN_CTX_new();
    int ok = p != NULL && b != NULL && e != NULL && result != NULL && ctx != NULL
        && BN_mod_exp_mont_consttime(result, b, e, p, ctx, NULL)
        && dh_in_range(gp, result)
        && cc_bn_to_ccn(result, n, r) == CCERR_OK;

    BN_CTX_free(ctx);
    BN_clear_free(result);
    BN_clear_free(e);
    BN_free(b);
    BN_free(p);
    return ok ? CCERR_OK : CCERR_PARAMETER;
}

/* x of l bits with the top one set when the group sets l, otherwise x in [1, p - 2] */
static int dh_draw_private(ccdh_const_gp_t gp, struct ccrng_state *rng, cc_unit *x)
{
    cc_size n = ccdh_gp_n(gp);
    size_t pbits = ccn_bitlen(n, CCDH_GP_PRIME(gp));
    size_t l = CCDH_GP_L(gp);
    bool fixed_length = l != 0 && l < pbits;
    size_t bits = fixed_length ? l : pbits;
    size_t nbytes = (bits + 7) / 8;
    uint8_t *buf = malloc(nbytes);
    BIGNUM *candidate = BN_new();
    int rv = CCERR_INTERNAL;

    if (buf == NULL || candidate == NULL || pbits < 3)
        goto out;
    BN_set_flags(candidate, BN_FLG_CONSTTIME);
    for (;;) {
        if ((rv = ccrng_generate(rng, nbytes, buf)) != CCERR_OK)
            goto out;
        if (bits % 8 != 0)
            buf[0] &= 0xff >> (8 - bits % 8);
        if (fixed_length)
            buf[0] |= 1 << ((bits - 1) % 8);
        if (BN_bin2bn(buf, (int)nbytes, candidate) == NULL) {
            rv = CCERR_INTERNAL;
            goto out;
        }
        if (fixed_length || dh_in_range(gp, candidate) || BN_is_one(candidate))
            break;
    }
    rv = ccn_read_uint(n, x, nbytes, buf);
out:
    if (buf != NULL) {
        OPENSSL_cleanse(buf, nbytes);
        free(buf);
    }
    BN_clear_free(candidate);
    return rv;
}

int ccdh_generate_key(ccdh_const_gp_t gp, struct ccrng_state *rng, ccdh_full_ctx_t key)
{
    if (gp == NULL || ccdh_gp_n(gp) == 0)
        return CCERR_PARAMETER;
    key->gp = gp;
    int rv = dh_draw_private(gp, rng, ccdh_ctx_x(key));
    if (rv == CCERR_OK)
        rv = dh_modexp(gp, CCDH_GP_G(gp), ccdh_ctx_x(key), ccdh_ctx_y(key));
    if (rv != CCERR_OK)
        cc_clear(ccdh_full_ctx_size(ccn_sizeof_n(ccdh_gp_n(gp))), key);
    return rv;
}

int ccdh_import_pub(ccdh_const_gp_t gp, size_t in_len, const uint8_t *in, ccdh_pub_ctx_t key)
{
    if (gp == NULL || in == NULL)
        return CCERR_PARAMETER;
    key->gp = gp;
    if (ccn_read_uint(ccdh_gp_n(gp), ccdh_ctx_y(key), in_len, in) != CCERR_OK)
        return CCERR_PARAMETER;
    BIGNUM *y = cc_bn_from_ccn(ccdh_gp_n(gp), ccdh_ctx_y(key));
    bool ok = y != NULL && dh_in_range(gp, y);
    BN_free(y);
    return ok ? CCERR_OK : CCERR_PARAMETER;
}

int ccdh_compute_key(ccdh_full_ctx_t private_key, ccdh_pub_ctx_t public_key, cc_unit *r)
{
    ccdh_const_gp_t gp = ccdh_ctx_gp(private_key);
    ccdh_const_gp_t peer = ccdh_ctx_gp(public_key);

    if (gp == NULL || peer == NULL || ccdh_gp_n(gp) != ccdh_gp_n(peer)
        || ccn_cmp(ccdh_gp_n(gp), CCDH_GP_PRIME(gp), CCDH_GP_PRIME(peer)) != 0)
        return CCERR_PARAMETER;
    BIGNUM *y = cc_bn_from_ccn(ccdh_gp_n(gp), ccdh_ctx_y(public_key));
    bool ok = y != NULL && dh_in_range(gp, y);
    BN_free(y);
    if (!ok)
        return CCERR_PARAMETER;
    return dh_modexp(gp, ccdh_ctx_y(public_key), ccdh_ctx_x(private_key), r);
}

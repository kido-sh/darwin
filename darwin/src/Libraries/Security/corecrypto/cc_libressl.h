/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CC_LIBRESSL_H
#define KIDO_SECURITY_CC_LIBRESSL_H

#include <corecrypto/ccn.h>
#include <corecrypto/cc_error.h>
#include <openssl/bn.h>
#include <openssl/crypto.h>
#include <stdlib.h>
#include <string.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "the ccn conversions below read units as little-endian bytes"
#endif

#define CCERR_INVALID_INPUT CCERR_PARAMETER

static inline void cc_reverse_bytes(unsigned char *p, size_t len)
{
    for (size_t i = 0; i < len / 2; i++) {
        unsigned char t = p[i];
        p[i] = p[len - 1 - i];
        p[len - 1 - i] = t;
    }
}

static inline BIGNUM *cc_bn_from_le(const unsigned char *le, size_t len, BIGNUM *b)
{
    unsigned char *be = malloc(len ? len : 1);
    BIGNUM *r = NULL;

    if (be == NULL)
        return NULL;
    memcpy(be, le, len);
    cc_reverse_bytes(be, len);
    r = BN_bin2bn(be, (int)len, b);
    OPENSSL_cleanse(be, len);
    free(be);
    return r;
}

static inline BIGNUM *cc_bn_from_ccn(cc_size n, const cc_unit *s)
{
    return cc_bn_from_le((const unsigned char *)s, ccn_sizeof_n(n), NULL);
}

static inline BIGNUM *cc_bn_from_secret_ccn(cc_size n, const cc_unit *s)
{
    BIGNUM *b = BN_new();

    if (b == NULL)
        return NULL;
    BN_set_flags(b, BN_FLG_CONSTTIME);
    if (cc_bn_from_le((const unsigned char *)s, ccn_sizeof_n(n), b) == NULL) {
        BN_clear_free(b);
        return NULL;
    }
    return b;
}

static inline int cc_bn_to_ccn(const BIGNUM *b, cc_size n, cc_unit *r)
{
    size_t len = ccn_sizeof_n(n);
    int nbytes = BN_num_bytes(b);
    unsigned char *out = (unsigned char *)r;

    if (BN_is_negative(b) || nbytes < 0 || (size_t)nbytes > len)
        return CCERR_PARAMETER;
    memset(out, 0, len);
    BN_bn2bin(b, out + len - (size_t)nbytes);
    cc_reverse_bytes(out, len);
    return CCERR_OK;
}

#endif

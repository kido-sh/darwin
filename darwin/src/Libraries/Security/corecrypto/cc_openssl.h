/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CC_OPENSSL_H
#define KIDO_SECURITY_CC_OPENSSL_H

#include <corecrypto/ccn.h>
#include <corecrypto/cc_error.h>
#include <openssl/bn.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "the ccn conversions below read units as little-endian bytes"
#endif

#define CCERR_INVALID_INPUT CCERR_PARAMETER

static inline BIGNUM *cc_bn_from_ccn(cc_size n, const cc_unit *s)
{
    return BN_lebin2bn((const unsigned char *)s, (int)ccn_sizeof_n(n), NULL);
}

static inline BIGNUM *cc_bn_from_secret_ccn(cc_size n, const cc_unit *s)
{
    BIGNUM *b = BN_secure_new();
    if (b == NULL)
        return NULL;
    BN_set_flags(b, BN_FLG_CONSTTIME);
    if (BN_lebin2bn((const unsigned char *)s, (int)ccn_sizeof_n(n), b) == NULL) {
        BN_clear_free(b);
        return NULL;
    }
    return b;
}

static inline int cc_bn_to_ccn(const BIGNUM *b, cc_size n, cc_unit *r)
{
    return BN_bn2lebinpad(b, (unsigned char *)r, (int)ccn_sizeof_n(n)) < 0 ? CCERR_PARAMETER : CCERR_OK;
}

#endif

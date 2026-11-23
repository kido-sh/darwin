/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCZP_H
#define KIDO_SECURITY_CORECRYPTO_CCZP_H

#include_next <corecrypto/cczp.h>

CC_BEGIN_DECLS

/*
 * A zp is the cczp header, the n-unit prime and n + 1 units the fork's Montgomery code reads:
 * m0inv, then R^2 mod p.
 */
#define cczp_size(_nbytes_)  (sizeof(struct cczp) + ccn_sizeof_n(1) + 2 * ccn_sizeof_size(_nbytes_))
#define cczp_n(_zp_)         ((_zp_)->n)
#define CCZP_N(_zp_)         ((_zp_)->n)
#define cczp_bitlen(_zp_)    ((_zp_)->bitlen)
#define cczp_prime(_zp_)     ((_zp_)->u)
#define cczp_m0inv(_zp_)     ((_zp_)->u + cczp_n(_zp_))
#define cczp_r2(_zp_)        ((_zp_)->u + cczp_n(_zp_) + 1)

/* fills in the Montgomery constants of the odd prime set in cczp_prime(zp) */
int cczp_init(cczp_t zp);

/* the reciprocal is not used, this is cczp_init */
void cczp_init_with_recip(cczp_t zp, const cc_unit *recip);

CC_END_DECLS

#endif

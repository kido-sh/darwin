/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/cczp.h>
#include <corecrypto/cczp_priv.h>
#include <corecrypto/cc_error.h>

int cczp_init(cczp_t zp)
{
    cc_size n = cczp_n(zp);
    const cc_unit *p = cczp_prime(zp);

    if (n == 0 || (p[0] & 1) == 0)
        return CCERR_PARAMETER;
    cczp_bitlen(zp) = ccn_bitlen(n, p);
    zp->funcs = NULL;
    *cczp_m0inv(zp) = ccn_mont_m0inv(p[0]);
    ccn_mont_r2(n, cczp_r2(zp), p);
    return CCERR_OK;
}

void cczp_init_with_recip(cczp_t zp, const cc_unit *recip)
{
    (void)recip;
    cczp_init(zp);
}

/* SPDX-License-Identifier: Apache-2.0 */

#ifndef _PRNG_HMAC_DRBG_H_
#define _PRNG_HMAC_DRBG_H_

#include <corecrypto/ccdrbg.h>

CC_BEGIN_DECLS

/*
 * HMAC_DRBG, NIST SP 800-90A Rev. 1 section 10.1.2, behind the ccdrbg
 * interface: external/corecrypto declares struct ccdrbg_nisthmac_custom
 * but implements only the CTR_DRBG.
 */
void ccdrbg_factory_nisthmac(struct ccdrbg_info *info, const struct ccdrbg_nisthmac_custom *custom);

CC_END_DECLS

#endif /* _PRNG_HMAC_DRBG_H_ */

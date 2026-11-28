/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * RFC 1321 MD5 compatibility bridge for libresolv hmac_link.c over
 * CommonCrypto's CommonDigest API.
 */
#ifndef _PD_LIBRESOLV_MD5_H_
#define _PD_LIBRESOLV_MD5_H_

#include <CommonCrypto/CommonDigest.h>

typedef CC_MD5_CTX MD5_CTX;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static inline void
MD5Init(MD5_CTX *ctx)
{
	(void)CC_MD5_Init(ctx);
}

static inline void
MD5Update(MD5_CTX *ctx, const void *data, unsigned int len)
{
	(void)CC_MD5_Update(ctx, data, (CC_LONG)len);
}

static inline void
MD5Final(unsigned char digest[16], MD5_CTX *ctx)
{
	(void)CC_MD5_Final(digest, ctx);
}
#pragma clang diagnostic pop

#endif /* _PD_LIBRESOLV_MD5_H_ */

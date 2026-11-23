/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCRSA_PRIV_H
#define KIDO_SECURITY_CORECRYPTO_CCRSA_PRIV_H

#include <corecrypto/ccrsa.h>
#include <corecrypto/ccdigest.h>

CC_BEGIN_DECLS

/*
 * RFC 8017 encodings. The signature encodings work on big-endian bytes; the encryption encodings
 * read and write the encoded block as a number of ccn_nof_size(size) units, as the raw RSA
 * operations take it.
 */

/* EMSA-PKCS1-v1_5 with a DigestInfo for oid, or the bare digest when oid is NULL */
int ccrsa_emsa_pkcs1v15_encode(size_t emlen, uint8_t *em,
                               size_t dgstlen, const uint8_t *dgst, const uint8_t *oid);
/* 0 when em is the encoding of dgst */
int ccrsa_emsa_pkcs1v15_verify(size_t emlen, const uint8_t *em,
                               size_t dgstlen, const uint8_t *dgst, const uint8_t *oid);

/* EMSA-PSS with MGF1 over mgfdi; em is (embits + 7) / 8 bytes */
int ccrsa_emsa_pss_encode(const struct ccdigest_info *di, const struct ccdigest_info *mgfdi,
                          size_t saltlen, const uint8_t *salt,
                          size_t hlen, const uint8_t *mhash,
                          size_t embits, uint8_t *em);
/* 0 when em is consistent with mhash and a salt of saltlen bytes */
int ccrsa_emsa_pss_decode(const struct ccdigest_info *di, const struct ccdigest_info *mgfdi,
                          size_t saltlen,
                          size_t hlen, const uint8_t *mhash,
                          size_t embits, const uint8_t *em);

/* EME-OAEP with MGF1 over di and an empty label */
int ccrsa_oaep_encode(const struct ccdigest_info *di, struct ccrng_state *rng,
                      size_t r_size, cc_unit *r,
                      size_t message_len, const uint8_t *message);
int ccrsa_oaep_decode(const struct ccdigest_info *di,
                      size_t *r_len, uint8_t *r,
                      size_t s_size, const cc_unit *s);

/* EME-PKCS1-v1_5 */
int ccrsa_eme_pkcs1v15_encode(struct ccrng_state *rng,
                              size_t r_size, cc_unit *r,
                              size_t s_size, const uint8_t *s);
int ccrsa_eme_pkcs1v15_decode(size_t *r_size, uint8_t *r,
                              size_t s_size, const cc_unit *s);

CC_END_DECLS

#endif

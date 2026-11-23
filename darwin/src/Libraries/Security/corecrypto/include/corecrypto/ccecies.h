/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCECIES_H
#define KIDO_SECURITY_CORECRYPTO_CCECIES_H

#include <corecrypto/ccec.h>
#include <corecrypto/ccmode.h>

CC_BEGIN_DECLS

/*
 * ECIES with an ephemeral key, the ANSI X9.63 KDF and AES-GCM. The output is the ephemeral
 * public key (X9.63), the ciphertext and the tag. Supported options: the ephemeral key in
 * uncompressed form, used as the KDF's shared info, and a 16-byte all-zero IV.
 */
#define ECIES_EXPORT_PUB_STANDARD       0x01
#define ECIES_EPH_PUBKEY_IN_SHAREDINFO1 0x02
#define ECIES_LEGACY_IV                 0x04

struct ccecies_gcm {
    const struct ccdigest_info *di;
    struct ccrng_state *rng;
    const struct ccmode_gcm *gcm;
    uint32_t key_length;
    uint32_t mac_length;
    uint32_t options;
};
typedef struct ccecies_gcm *ccecies_gcm_t;

void ccecies_encrypt_gcm_setup(ccecies_gcm_t ecies, const struct ccdigest_info *di,
                               struct ccrng_state *rng, const struct ccmode_gcm *gcm,
                               uint32_t cipher_key_size, uint32_t mac_tag_size, uint32_t options);

size_t ccecies_encrypt_gcm_ciphertext_size(ccec_pub_ctx_t public_key, ccecies_gcm_t ecies,
                                           size_t plaintext_len);
int ccecies_encrypt_gcm(ccec_pub_ctx_t public_key, const ccecies_gcm_t ecies,
                        size_t plaintext_len, const uint8_t *plaintext,
                        size_t sharedinfo1_len, const void *sharedinfo1,
                        size_t sharedinfo2_len, const void *sharedinfo2,
                        size_t *encrypted_blob_len, uint8_t *encrypted_blob);

size_t ccecies_decrypt_gcm_plaintext_size(ccec_full_ctx_t full_key, ccecies_gcm_t ecies,
                                          size_t ciphertext_len);
int ccecies_decrypt_gcm(ccec_full_ctx_t full_key, const ccecies_gcm_t ecies,
                        size_t encrypted_blob_len, const uint8_t *encrypted_blob,
                        size_t sharedinfo1_len, const void *sharedinfo1,
                        size_t sharedinfo2_len, const void *sharedinfo2,
                        size_t *plaintext_len, uint8_t *plaintext);

CC_END_DECLS

#endif

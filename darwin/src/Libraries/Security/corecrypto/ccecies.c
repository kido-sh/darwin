/* SPDX-License-Identifier: Apache-2.0 */

#include <corecrypto/ccecies.h>
#include <corecrypto/ccansikdf.h>
#include <corecrypto/cc_error.h>
#include <string.h>

#define ECIES_MAX_SHARED_SIZE 66
#define ECIES_MAX_KEY_SIZE 32
#define ECIES_TAG_SIZE 16
#define ECIES_LEGACY_IV_SIZE 16

void ccecies_encrypt_gcm_setup(ccecies_gcm_t ecies, const struct ccdigest_info *di,
                               struct ccrng_state *rng, const struct ccmode_gcm *gcm,
                               uint32_t cipher_key_size, uint32_t mac_tag_size, uint32_t options)
{
    ecies->di = di;
    ecies->rng = rng;
    ecies->gcm = gcm;
    ecies->key_length = cipher_key_size;
    ecies->mac_length = mac_tag_size;
    ecies->options = options;
}

static bool ecies_supported(const struct ccecies_gcm *ecies)
{
    uint32_t required = ECIES_EXPORT_PUB_STANDARD | ECIES_LEGACY_IV;
    return ecies->di != NULL && ecies->gcm != NULL
        && (ecies->options & ~ECIES_EPH_PUBKEY_IN_SHAREDINFO1) == required
        && (ecies->key_length == 16 || ecies->key_length == 24 || ecies->key_length == 32)
        && ecies->mac_length > 0 && ecies->mac_length <= ECIES_TAG_SIZE;
}

/* the X9.63 KDF over the ECDH secret, its shared info the ephemeral key or sharedinfo1 */
static int ecies_key(const struct ccecies_gcm *ecies, ccec_full_ctx_t private_key, ccec_pub_ctx_t public_key,
                     const uint8_t *ephemeral, size_t ephemeral_len,
                     size_t sharedinfo1_len, const void *sharedinfo1, uint8_t *key)
{
    uint8_t shared[ECIES_MAX_SHARED_SIZE];
    size_t shared_len = sizeof(shared);

    if (ecies->options & ECIES_EPH_PUBKEY_IN_SHAREDINFO1) {
        if (sharedinfo1_len != 0)
            return CCERR_PARAMETER;
        sharedinfo1 = ephemeral;
        sharedinfo1_len = ephemeral_len;
    }
    int rv = ccecdh_compute_shared_secret(private_key, public_key, &shared_len, shared, ecies->rng);
    if (rv == CCERR_OK)
        rv = ccansikdf_x963(ecies->di, shared_len, shared, sharedinfo1_len, sharedinfo1, ecies->key_length, key);
    cc_clear(sizeof(shared), shared);
    return rv;
}

size_t ccecies_encrypt_gcm_ciphertext_size(ccec_pub_ctx_t public_key, ccecies_gcm_t ecies,
                                           size_t plaintext_len)
{
    return ccec_export_pub_size(public_key) + plaintext_len + ecies->mac_length;
}

int ccecies_encrypt_gcm(ccec_pub_ctx_t public_key, const ccecies_gcm_t ecies,
                        size_t plaintext_len, const uint8_t *plaintext,
                        size_t sharedinfo1_len, const void *sharedinfo1,
                        size_t sharedinfo2_len, const void *sharedinfo2,
                        size_t *encrypted_blob_len, uint8_t *encrypted_blob)
{
    static const uint8_t iv[ECIES_LEGACY_IV_SIZE];
    ccec_const_cp_t cp = ccec_ctx_cp(public_key);
    size_t size = ccecies_encrypt_gcm_ciphertext_size(public_key, ecies, plaintext_len);
    uint8_t key[ECIES_MAX_KEY_SIZE];

    if (!ecies_supported(ecies) || cp == NULL || *encrypted_blob_len < size)
        return CCERR_PARAMETER;

    size_t point_len = ccec_export_pub_size(public_key);
    ccec_full_ctx_decl_cp(cp, ephemeral);
    int rv = ccec_generate_key_fips(cp, ecies->rng, ephemeral);
    if (rv == CCERR_OK) {
        ccec_export_pub(ccec_ctx_pub(ephemeral), encrypted_blob);
        rv = ecies_key(ecies, ephemeral, public_key, encrypted_blob, point_len,
                       sharedinfo1_len, sharedinfo1, key);
    }
    if (rv == CCERR_OK)
        rv = ccgcm_one_shot_legacy(ecies->gcm, ecies->key_length, key, sizeof(iv), iv,
                                   sharedinfo2_len, sharedinfo2,
                                   plaintext_len, plaintext, encrypted_blob + point_len,
                                   ecies->mac_length, encrypted_blob + point_len + plaintext_len);
    if (rv == CCERR_OK)
        *encrypted_blob_len = size;

    ccec_full_ctx_clear_cp(cp, ephemeral);
    cc_clear(sizeof(key), key);
    return rv;
}

size_t ccecies_decrypt_gcm_plaintext_size(ccec_full_ctx_t full_key, ccecies_gcm_t ecies,
                                          size_t ciphertext_len)
{
    size_t overhead = ccec_export_pub_size(ccec_ctx_pub(full_key)) + ecies->mac_length;
    return ciphertext_len >= overhead ? ciphertext_len - overhead : 0;
}

int ccecies_decrypt_gcm(ccec_full_ctx_t full_key, const ccecies_gcm_t ecies,
                        size_t encrypted_blob_len, const uint8_t *encrypted_blob,
                        size_t sharedinfo1_len, const void *sharedinfo1,
                        size_t sharedinfo2_len, const void *sharedinfo2,
                        size_t *plaintext_len, uint8_t *plaintext)
{
    static const uint8_t iv[ECIES_LEGACY_IV_SIZE];
    ccec_const_cp_t cp = ccec_ctx_cp(full_key);
    uint8_t key[ECIES_MAX_KEY_SIZE], tag[ECIES_TAG_SIZE];

    if (!ecies_supported(ecies) || cp == NULL)
        return CCERR_PARAMETER;
    size_t point_len = ccec_export_pub_size(ccec_ctx_pub(full_key));
    if (encrypted_blob_len < point_len + ecies->mac_length)
        return CCERR_PARAMETER;
    size_t size = encrypted_blob_len - point_len - ecies->mac_length;
    if (*plaintext_len < size)
        return CCERR_PARAMETER;

    ccec_pub_ctx_decl_cp(cp, ephemeral);
    int rv = ccec_import_pub(cp, point_len, encrypted_blob, ephemeral);
    if (rv == CCERR_OK)
        rv = ecies_key(ecies, full_key, ephemeral, encrypted_blob, point_len,
                       sharedinfo1_len, sharedinfo1, key);
    if (rv == CCERR_OK) {
        memcpy(tag, encrypted_blob + point_len + size, ecies->mac_length);
        rv = ccgcm_one_shot_legacy(ecies->gcm, ecies->key_length, key, sizeof(iv), iv,
                                   sharedinfo2_len, sharedinfo2,
                                   size, encrypted_blob + point_len, plaintext,
                                   ecies->mac_length, tag);
        if (rv != CCERR_OK)
            cc_clear(size, plaintext);
    }
    if (rv == CCERR_OK)
        *plaintext_len = size;

    cc_clear(sizeof(key), key);
    cc_clear(sizeof(tag), tag);
    return rv;
}

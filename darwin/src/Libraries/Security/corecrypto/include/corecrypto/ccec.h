/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCEC_H
#define KIDO_SECURITY_CORECRYPTO_CCEC_H

#include <corecrypto/ccn.h>
#include <corecrypto/ccasn1.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccdigest.h>

CC_BEGIN_DECLS

/* the NIST prime curves P-192, P-224, P-256, P-384 and P-521 */
struct ccec_cp {
    cc_size n;
    size_t bitlen;
    const char *name;
    int nid;
};
typedef const struct ccec_cp *ccec_const_cp_t;

#define ccec_cp_n(_cp_)            ((_cp_)->n)
#define ccec_cp_prime_bitlen(_cp_) ((_cp_)->bitlen)
#define ccec_cp_prime_size(_cp_)   (((_cp_)->bitlen + 7) / 8)
#define ccec_ccn_size(_cp_)        ccn_sizeof_n(ccec_cp_n(_cp_))

ccec_const_cp_t ccec_cp_192(void);
ccec_const_cp_t ccec_cp_224(void);
ccec_const_cp_t ccec_cp_256(void);
ccec_const_cp_t ccec_cp_384(void);
ccec_const_cp_t ccec_cp_521(void);

#define CC_EC_OID_SECP192R1 ((ccoid_t)"\x06\x08\x2a\x86\x48\xce\x3d\x03\x01\x01")
#define CC_EC_OID_SECP224R1 ((ccoid_t)"\x06\x05\x2b\x81\x04\x00\x21")
#define CC_EC_OID_SECP256R1 ((ccoid_t)"\x06\x08\x2a\x86\x48\xce\x3d\x03\x01\x07")
#define CC_EC_OID_SECP384R1 ((ccoid_t)"\x06\x05\x2b\x81\x04\x00\x22")
#define CC_EC_OID_SECP521R1 ((ccoid_t)"\x06\x05\x2b\x81\x04\x00\x23")

/*
 * A public key is its curve and the affine x and y, n units each; a full key adds the private
 * scalar k, so its leading ccec_pub_ctx_size bytes are its public key.
 */
struct ccec_pub_ctx {
    ccec_const_cp_t cp;
    cc_unit xyk[];
};
struct ccec_full_ctx {
    ccec_const_cp_t cp;
    cc_unit xyk[];
};
typedef struct ccec_pub_ctx *ccec_pub_ctx_t;
typedef struct ccec_full_ctx *ccec_full_ctx_t;

#define ccec_pub_ctx_size(_nbytes_)   (sizeof(struct ccec_pub_ctx) + 2 * ccn_sizeof_size(_nbytes_))
#define ccec_full_ctx_size(_nbytes_)  (sizeof(struct ccec_full_ctx) + 3 * ccn_sizeof_size(_nbytes_))
#define ccec_pub_ctx_decl(_nbytes_, _name_) \
    cc_ctx_decl(struct ccec_pub_ctx, ccec_pub_ctx_size(_nbytes_), _name_)
#define ccec_full_ctx_decl(_nbytes_, _name_) \
    cc_ctx_decl(struct ccec_full_ctx, ccec_full_ctx_size(_nbytes_), _name_)
#define ccec_pub_ctx_decl_cp(_cp_, _name_)  ccec_pub_ctx_decl(ccn_sizeof_n(ccec_cp_n(_cp_)), _name_)
#define ccec_full_ctx_decl_cp(_cp_, _name_) ccec_full_ctx_decl(ccn_sizeof_n(ccec_cp_n(_cp_)), _name_)
#define ccec_pub_ctx_clear_cp(_cp_, _name_)  cc_clear(ccec_pub_ctx_size(ccn_sizeof_n(ccec_cp_n(_cp_))), _name_)
#define ccec_full_ctx_clear_cp(_cp_, _name_) cc_clear(ccec_full_ctx_size(ccn_sizeof_n(ccec_cp_n(_cp_))), _name_)

#define ccec_ctx_cp(_key_)    ((_key_)->cp)
#define ccec_ctx_init(_cp_, _key_) ((_key_)->cp = (_cp_))
#define ccec_ctx_n(_key_)     ccec_cp_n(ccec_ctx_cp(_key_))
#define ccec_ctx_x(_key_)     ((_key_)->xyk)
#define ccec_ctx_y(_key_)     ((_key_)->xyk + ccec_ctx_n(_key_))
#define ccec_ctx_k(_key_)     ((_key_)->xyk + 2 * ccec_ctx_n(_key_))
#define ccec_ctx_pub(_key_)   ((ccec_pub_ctx_t)(_key_))
/* prime size in bytes */
#define ccec_ctx_size(_key_)  ccec_cp_prime_size(ccec_ctx_cp(_key_))

/* key size in bits (192, 224, 256, 384 or 521), the curve for it, or NULL */
bool ccec_keysize_is_supported(size_t keysize);
ccec_const_cp_t ccec_get_cp(size_t keysize);
/* the first curve whose prime bytes hold keylen bits, from a NULL-terminated list */
ccec_const_cp_t ccec_curve_for_length_lookup(size_t keylen, ...);

/* key size in bits for an X9.63 public (04 x y) or private (04 x y k) encoding of len bytes */
size_t ccec_x963_import_pub_size(size_t len);
size_t ccec_x963_import_priv_size(size_t len);
/* bytes of the X9.63 public (fullkey 0) or private (fullkey 1) encoding */
size_t ccec_x963_export_size(int fullkey, ccec_pub_ctx_t key);

/* X9.63 uncompressed point, checked to be on the curve */
int ccec_import_pub(ccec_const_cp_t cp, size_t in_len, const uint8_t *in, ccec_pub_ctx_t key);
size_t ccec_export_pub_size(ccec_pub_ctx_t key);
void ccec_export_pub(ccec_pub_ctx_t key, void *out);

/* SEC 1 ECPrivateKey: its curve OID (pointing into der, NULL without one) and key length */
int ccec_der_import_priv_keytype(size_t len, const uint8_t *der, ccoid_t *oid, size_t *n);
int ccec_der_import_priv(ccec_const_cp_t cp, size_t len, const uint8_t *der, ccec_full_ctx_t key);

int ccec_generate_key_fips(ccec_const_cp_t cp, struct ccrng_state *rng, ccec_full_ctx_t key);

/* ECDSA over a digest, the signature DER-encoded */
size_t ccec_sign_max_size(ccec_const_cp_t cp);
int ccec_sign(ccec_full_ctx_t key, size_t digest_len, const uint8_t *digest,
              size_t *sig_len, uint8_t *sig, struct ccrng_state *rng);
/* returns 0 with *valid set when the inputs are well formed */
int ccec_verify(ccec_pub_ctx_t key, size_t digest_len, const uint8_t *digest,
                size_t sig_len, const uint8_t *sig, bool *valid);

/* ECDH: the x coordinate of k * P, ccec_cp_prime_size bytes */
int ccecdh_compute_shared_secret(ccec_full_ctx_t private_key, ccec_pub_ctx_t public_key,
                                 size_t *computed_shared_secret_len, uint8_t *computed_shared_secret,
                                 struct ccrng_state *masking_rng);

/* RFC 6637: ECDH key wrapping for OpenPGP */
struct ccec_rfc6637_curve {
    const uint8_t *curve_oid;
    uint8_t public_key_alg;
};
struct ccec_rfc6637_wrap {
    const struct ccdigest_info *(*difun)(void);
    uint8_t kdfhash_id;
    uint8_t kek_id;
    size_t kek_size;
};
struct ccec_rfc6637_unwrap {
    const struct ccdigest_info *(*difun)(void);
    uint8_t kdfhash_id;
    uint8_t kek_id;
    size_t kek_size;
};

extern const struct ccec_rfc6637_curve ccec_rfc6637_dh_curve_p256;
extern const struct ccec_rfc6637_curve ccec_rfc6637_dh_curve_p521;
extern const struct ccec_rfc6637_wrap ccec_rfc6637_wrap_sha256_kek_aes128;
extern const struct ccec_rfc6637_wrap ccec_rfc6637_wrap_sha512_kek_aes256;
extern const struct ccec_rfc6637_unwrap ccec_rfc6637_unwrap_sha256_kek_aes128;
extern const struct ccec_rfc6637_unwrap ccec_rfc6637_unwrap_sha512_kek_aes256;

/*
 * The output is the ECDH fields of a public-key encrypted session key packet: the ephemeral
 * public key as an MPI, one length octet and the wrapped key. The only flag value is 0.
 */
size_t ccec_rfc6637_wrap_key_size(ccec_pub_ctx_t public_key, unsigned long flags, size_t key_len);
int ccec_rfc6637_wrap_key(ccec_pub_ctx_t public_key, void *wrapped_key, unsigned long flags,
                          uint8_t symm_alg_id, size_t key_len, const void *key,
                          const struct ccec_rfc6637_curve *curve,
                          const struct ccec_rfc6637_wrap *wrap,
                          const uint8_t *fingerprint,
                          struct ccrng_state *rng);
int ccec_rfc6637_unwrap_key(ccec_full_ctx_t private_key, size_t *key_len, void *key,
                            unsigned long flags, uint8_t *symm_key_alg,
                            const struct ccec_rfc6637_curve *curve,
                            const struct ccec_rfc6637_unwrap *unwrap,
                            const uint8_t *fingerprint,
                            size_t wrapped_key_len, const void *wrapped_key);

CC_END_DECLS

#endif

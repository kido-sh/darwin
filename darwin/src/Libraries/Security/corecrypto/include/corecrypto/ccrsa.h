/* SPDX-License-Identifier: Apache-2.0 */

#ifndef KIDO_SECURITY_CORECRYPTO_CCRSA_H
#define KIDO_SECURITY_CORECRYPTO_CCRSA_H

#include <corecrypto/cczp.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccdigest.h>
#include_next <corecrypto/ccrsa.h>

CC_BEGIN_DECLS

/*
 * A full key is the fork's public key followed by d, p and q (each behind a cczp header),
 * d mod (p - 1), d mod (q - 1) and q^-1 mod p, all of n units.
 */
#define ccrsa_full_ctx_size(_nbytes_) \
    (ccrsa_pub_ctx_size(_nbytes_) + 6 * ccn_sizeof_size(_nbytes_) + 2 * ccn_sizeof_n(ccn_nof_sizeof(struct cczp)))
#define ccrsa_full_ctx_decl(_nbytes_, _name_) \
    cc_ctx_decl(struct ccrsa_full_ctx, ccrsa_full_ctx_size(_nbytes_), _name_)
#define ccrsa_full_ctx_clear(_nbytes_, _name_)  cc_clear(ccrsa_full_ctx_size(_nbytes_), _name_)
#define ccrsa_ctx_public(_fk_)  ((ccrsa_pub_ctx_t)(_fk_))

#define ccrsa_ctx_d(_fk_)              ((_fk_)->u + 3 * ccrsa_ctx_n(_fk_) + 1)
#define ccrsa_ctx_private_zp(_fk_)     ((cczp_t)(ccrsa_ctx_d(_fk_) + ccrsa_ctx_n(_fk_)))
#define ccrsa_ctx_private_zq(_fk_) \
    ((cczp_t)(ccrsa_ctx_d(_fk_) + 2 * ccrsa_ctx_n(_fk_) + ccn_nof_sizeof(struct cczp)))
#define ccrsa_ctx_private_dp(_fk_) \
    (ccrsa_ctx_d(_fk_) + 3 * ccrsa_ctx_n(_fk_) + 2 * ccn_nof_sizeof(struct cczp))
#define ccrsa_ctx_private_dq(_fk_)     (ccrsa_ctx_private_dp(_fk_) + ccrsa_ctx_n(_fk_))
#define ccrsa_ctx_private_qinv(_fk_)   (ccrsa_ctx_private_dq(_fk_) + ccrsa_ctx_n(_fk_))

/* modulus size in bytes */
size_t ccrsa_block_size(ccrsa_pub_ctx_t key);

/* out = in^e mod m, in < m, both of ccrsa_ctx_n(key) units */
int ccrsa_pub_crypt(ccrsa_pub_ctx_t key, cc_unit *out, const cc_unit *in);

/* out = in^d mod m, in < m, both of ccrsa_ctx_n(key) units */
int ccrsa_priv_crypt(ccrsa_full_ctx_t key, cc_unit *out, const cc_unit *in);

/* units an RSAPublicKey (PKCS #1) or a SubjectPublicKeyInfo needs, 0 when it is not one */
cc_size ccrsa_import_pub_n(size_t inlen, const uint8_t *der);
int ccrsa_import_pub(ccrsa_pub_ctx_t key, size_t inlen, const uint8_t *der);

/* units an RSAPrivateKey (PKCS #1) needs, 0 when it is not one */
cc_size ccrsa_import_priv_n(size_t inlen, const uint8_t *der);
int ccrsa_import_priv(ccrsa_full_ctx_t key, size_t inlen, const uint8_t *der);

size_t ccrsa_export_priv_size(ccrsa_full_ctx_t key);
int ccrsa_export_priv(ccrsa_full_ctx_t key, size_t outlen, uint8_t *out);

/* key of nbits bits with exponent e (big endian); OpenSSL's generator draws the randomness */
int ccrsa_generate_fips186_key(size_t nbits, ccrsa_full_ctx_t key,
                               size_t e_size, const void *e,
                               struct ccrng_state *rng1, struct ccrng_state *rng2);

/* public key from m and e of ccrsa_ctx_n(key) units */
int ccrsa_init_pub(ccrsa_pub_ctx_t key, const cc_unit *m, const cc_unit *e);

/* full key from the primes p and q and the exponent e, big endian; ccrsa_ctx_n(key) is the capacity */
int ccrsa_make_priv(ccrsa_full_ctx_t key, size_t e_size, const uint8_t *e,
                    size_t p_size, const uint8_t *p, size_t q_size, const uint8_t *q);

/* big-endian components; each length is the capacity on entry and the size written on return */
int ccrsa_get_pubkey_components(ccrsa_pub_ctx_t key, uint8_t *m, size_t *m_size, uint8_t *e, size_t *e_size);
/* m, d, p and q */
int ccrsa_get_fullkey_components(ccrsa_full_ctx_t key, uint8_t *m, size_t *m_size, uint8_t *d, size_t *d_size,
                                 uint8_t *p, size_t *p_size, uint8_t *q, size_t *q_size);

/* RSAPublicKey (PKCS #1) */
size_t ccrsa_export_pub_size(ccrsa_pub_ctx_t key);
int ccrsa_export_pub(ccrsa_pub_ctx_t key, size_t outlen, uint8_t *out);

/*
 * RFC 8017 schemes over big-endian blocks of ccrsa_block_size bytes; *sig_size and *r_size are the
 * capacity on entry and the size written on return. The verifiers return 0 with *valid set.
 */
int ccrsa_sign_pkcs1v15(ccrsa_full_ctx_t key, const uint8_t *oid, size_t digest_len, const uint8_t *digest,
                        size_t *sig_size, uint8_t *sig);
int ccrsa_sign_pss(ccrsa_full_ctx_t key, const struct ccdigest_info *di, const struct ccdigest_info *mgfdi,
                   size_t salt_size, struct ccrng_state *rng, size_t digest_len, const uint8_t *digest,
                   size_t *sig_size, uint8_t *sig);
int ccrsa_verify_pss(ccrsa_pub_ctx_t key, const struct ccdigest_info *di, const struct ccdigest_info *mgfdi,
                     size_t digest_len, const uint8_t *digest, size_t sig_size, const uint8_t *sig,
                     size_t salt_size, bool *valid);
int ccrsa_encrypt_eme_pkcs1v15(ccrsa_pub_ctx_t key, struct ccrng_state *rng, size_t *r_size, uint8_t *r,
                               size_t s_size, const uint8_t *s);
int ccrsa_decrypt_eme_pkcs1v15(ccrsa_full_ctx_t key, size_t *r_size, uint8_t *r, size_t s_size, const uint8_t *s);
int ccrsa_encrypt_oaep(ccrsa_pub_ctx_t key, const struct ccdigest_info *di, struct ccrng_state *rng,
                       size_t *r_size, uint8_t *r, size_t s_size, const uint8_t *s,
                       size_t label_size, const uint8_t *label);
int ccrsa_decrypt_oaep(ccrsa_full_ctx_t key, const struct ccdigest_info *di, size_t *r_size, uint8_t *r,
                       size_t c_size, const uint8_t *c, size_t label_size, const uint8_t *label);

CC_END_DECLS

#endif

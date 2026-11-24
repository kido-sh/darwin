/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../libsystem/check.h"

typedef struct env_md_st EVP_MD;
typedef struct evp_cipher_st EVP_CIPHER;
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;
typedef struct bignum_st BIGNUM;
typedef struct rsa_st RSA;
typedef struct ec_key_st EC_KEY;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct x509_st X509;
typedef struct X509_name_st X509_NAME;
typedef struct X509_extension_st X509_EXTENSION;
typedef struct asn1_string_st ASN1_STRING;
typedef struct x509_store_st X509_STORE;
typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct X509_VERIFY_PARAM_st X509_VERIFY_PARAM;
typedef struct stack_st STACK;

const char *OpenSSL_version(int);
unsigned long OpenSSL_version_num(void);
const char *X509_get_default_cert_file(void);
const char *X509_get_default_cert_dir(void);
const EVP_MD *EVP_md5(void);
const EVP_MD *EVP_sha1(void);
const EVP_MD *EVP_sha256(void);
const EVP_MD *EVP_sha512(void);
int EVP_Digest(const void *, size_t, unsigned char *, unsigned int *, const EVP_MD *, void *);
unsigned char *HMAC(const EVP_MD *, const void *, int, const unsigned char *, size_t, unsigned char *,
    unsigned int *);
int SHA256_Init(void *);
int SHA256_Update(void *, const void *, size_t);
int SHA256_Final(unsigned char *, void *);
int SHA256_Init_legacy(void *);
void SHA256_Transform_legacy(void *, const unsigned char *);
const EVP_CIPHER *EVP_aes_128_ecb(void);
const EVP_CIPHER *EVP_aes_128_cbc(void);
const EVP_CIPHER *EVP_aes_256_gcm(void);
const EVP_CIPHER *EVP_get_cipherbyname(const char *);
EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void);
void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *);
int EVP_CipherInit_ex(EVP_CIPHER_CTX *, const EVP_CIPHER *, void *, const unsigned char *,
    const unsigned char *, int);
int EVP_CipherUpdate(EVP_CIPHER_CTX *, unsigned char *, int *, const unsigned char *, int);
int EVP_CipherFinal_ex(EVP_CIPHER_CTX *, unsigned char *, int *);
int EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *, int);
int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *, int, int, void *);
int RAND_status(void);
int RAND_bytes(unsigned char *, int);
BIGNUM *BN_new(void);
int BN_set_word(BIGNUM *, unsigned long);
void BN_free(BIGNUM *);
RSA *RSA_new(void);
int RSA_generate_key_ex(RSA *, int, BIGNUM *, void *);
int RSA_size(const RSA *);
int RSA_sign(int, const unsigned char *, unsigned int, unsigned char *, unsigned int *, RSA *);
int RSA_verify(int, const unsigned char *, unsigned int, const unsigned char *, unsigned int, RSA *);
void RSA_free(RSA *);
EC_KEY *EC_KEY_new_by_curve_name(int);
int EC_KEY_generate_key(EC_KEY *);
int ECDSA_size(const EC_KEY *);
int ECDSA_sign(int, const unsigned char *, int, unsigned char *, unsigned int *, EC_KEY *);
int ECDSA_verify(int, const unsigned char *, int, const unsigned char *, int, EC_KEY *);
void EC_KEY_free(EC_KEY *);
EVP_PKEY *EVP_PKEY_new(void);
int EVP_PKEY_set1_EC_KEY(EVP_PKEY *, EC_KEY *);
void EVP_PKEY_free(EVP_PKEY *);
X509 *X509_new(void);
void X509_free(X509 *);
int X509_set_version(X509 *, long);
ASN1_STRING *X509_get_serialNumber(X509 *);
int ASN1_INTEGER_set(ASN1_STRING *, long);
ASN1_STRING *X509_getm_notBefore(const X509 *);
ASN1_STRING *X509_getm_notAfter(const X509 *);
ASN1_STRING *X509_gmtime_adj(ASN1_STRING *, long);
X509_NAME *X509_get_subject_name(const X509 *);
int X509_NAME_add_entry_by_txt(X509_NAME *, const char *, int, const unsigned char *, int, int, int);
int X509_set_issuer_name(X509 *, X509_NAME *);
int X509_set_pubkey(X509 *, EVP_PKEY *);
X509_EXTENSION *X509V3_EXT_conf_nid(void *, void *, int, const char *);
int X509_add_ext(X509 *, X509_EXTENSION *, int);
void X509_EXTENSION_free(X509_EXTENSION *);
int X509_sign(X509 *, EVP_PKEY *, const EVP_MD *);
X509_STORE *X509_STORE_new(void);
int X509_STORE_add_cert(X509_STORE *, X509 *);
void X509_STORE_free(X509_STORE *);
X509_STORE_CTX *X509_STORE_CTX_new(void);
int X509_STORE_CTX_init(X509_STORE_CTX *, X509_STORE *, X509 *, STACK *);
void X509_STORE_CTX_free(X509_STORE_CTX *);
int X509_STORE_CTX_get_error(X509_STORE_CTX *);
X509_VERIFY_PARAM *X509_STORE_CTX_get0_param(X509_STORE_CTX *);
unsigned long X509_VERIFY_PARAM_get_flags(X509_VERIFY_PARAM *);
int X509_VERIFY_PARAM_clear_flags(X509_VERIFY_PARAM *, unsigned long);
int X509_VERIFY_PARAM_set1_host(X509_VERIFY_PARAM *, const char *, size_t);
void X509_STORE_CTX_set_verify_cb(X509_STORE_CTX *, int (*)(int, X509_STORE_CTX *));
int X509_verify_cert(X509_STORE_CTX *);
STACK *sk_new_null(void);
int sk_push(STACK *, void *);
void sk_free(STACK *);

enum {
    OPENSSL_DIR = 4,
    EVP_CTRL_GCM_GET_TAG = 0x10,
    EVP_CTRL_GCM_SET_TAG = 0x11,
    NID_sha256 = 672,
    NID_X9_62_prime256v1 = 415,
    NID_basic_constraints = 87,
    MBSTRING_ASC = 0x1001,
    X509_V_ERR_CERT_CHAIN_TOO_LONG = 22,
    X509_V_ERR_HOSTNAME_MISMATCH = 62,
};

#define X509_V_FLAG_TRUSTED_FIRST 0x8000UL
#define X509_V_FLAG_LEGACY_VERIFY 0x400000UL

static const char *
hex(const unsigned char *p, size_t n)
{
    static char buf[2 * 256 + 1];
    size_t i;

    for (i = 0; i < n && i < 256; i++)
        snprintf(buf + 2 * i, 3, "%02x", p[i]);
    buf[2 * i] = '\0';
    return buf;
}

static void
unhex(const char *s, unsigned char *out)
{
    unsigned int b;

    for (; *s; s += 2, out++) {
        sscanf(s, "%2x", &b);
        *out = (unsigned char)b;
    }
}

static int
digest(const EVP_MD *md, const void *data, size_t n, const char *want)
{
    unsigned char out[64];
    unsigned int len = 0;

    return EVP_Digest(data, n, out, &len, md, NULL) == 1 && strcmp(hex(out, len), want) == 0;
}

static int
hmac(const EVP_MD *md, const void *key, int keylen, const char *data, const char *want)
{
    unsigned char out[64];
    unsigned int len = 0;

    return HMAC(md, key, keylen, (const unsigned char *)data, strlen(data), out, &len) != NULL &&
        strcmp(hex(out, len), want) == 0;
}

static int
crypt_block(const EVP_CIPHER *c, const char *key, const char *iv, const char *in, const char *want)
{
    unsigned char k[32], v[16], i[64], o[80];
    int n = 0, m = 0, ok;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    unhex(key, k);
    if (iv)
        unhex(iv, v);
    unhex(in, i);
    ok = EVP_CipherInit_ex(ctx, c, NULL, k, iv ? v : NULL, 1) == 1 &&
        EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
        EVP_CipherUpdate(ctx, o, &n, i, (int)strlen(in) / 2) == 1 &&
        EVP_CipherFinal_ex(ctx, o + n, &m) == 1 && strcmp(hex(o, n + m), want) == 0;
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

static int
gcm(int tamper)
{
    unsigned char key[32] = { 0 }, iv[12] = { 0 }, pt[16] = { 0 }, ct[16], back[16], tag[16];
    int n = 0, m = 0, ok;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    ok = EVP_CipherInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv, 1) == 1 &&
        EVP_CipherUpdate(ctx, ct, &n, pt, sizeof(pt)) == 1 && EVP_CipherFinal_ex(ctx, ct + n, &m) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) == 1 &&
        strcmp(hex(ct, 16), "cea7403d4d606b6e074ec5d3baf39d18") == 0 &&
        strcmp(hex(tag, 16), "d0d1c8a799996bf0265b98b5d48ab919") == 0;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        return 0;
    tag[0] ^= tamper;
    ctx = EVP_CIPHER_CTX_new();
    ok = EVP_CipherInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, iv, 0) == 1 &&
        EVP_CipherUpdate(ctx, back, &n, ct, sizeof(ct)) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) == 1 &&
        EVP_CipherFinal_ex(ctx, back + n, &m) == 1 && memcmp(back, pt, sizeof(pt)) == 0;
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

static int
sha256_million(void)
{
    unsigned char ctx[112], block[1000], out[32];
    int i, ok;

    memset(block, 'a', sizeof(block));
    ok = SHA256_Init(ctx) == 1;
    for (i = 0; i < 1000; i++)
        ok &= SHA256_Update(ctx, block, sizeof(block)) == 1;
    ok &= SHA256_Final(out, ctx) == 1;
    return ok && strcmp(hex(out, 32), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0;
}

static int
sha256_legacy(void)
{
    static const char init[] = "67e6096a85ae67bb72f36e3c3af54fa57f520e518c68059babd9831f19cde05b";
    static const char after[] = "15966c17e6471e3a49596554a04fa3a243d2620a18ce4651d2d9513c5f1a0e85";
    unsigned char ctx[128], block[64], zero[76] = { 0 }, len[4] = { 0x20, 0, 0, 0 };
    int i;

    memset(ctx, 0xaa, sizeof(ctx));
    for (i = 0; i < 64; i++)
        block[i] = (unsigned char)(i * 7 + 1);
    if (SHA256_Init_legacy(ctx) != 1 || strcmp(hex(ctx, 32), init) != 0 || memcmp(ctx + 32, zero, 76) != 0 ||
        memcmp(ctx + 108, len, 4) != 0 || ctx[112] != 0xaa)
        return 0;
    SHA256_Transform_legacy(ctx, block);
    return strcmp(hex(ctx, 32), after) == 0 && memcmp(ctx + 32, zero, 76) == 0 && memcmp(ctx + 108, len, 4) == 0;
}

static int
rsa_round_trip(void)
{
    unsigned char md[32], sig[512];
    unsigned int siglen = 0;
    BIGNUM *e = BN_new();
    RSA *rsa = RSA_new();
    int ok;

    memset(md, 0x5a, sizeof(md));
    ok = BN_set_word(e, 65537) == 1 && RSA_generate_key_ex(rsa, 2048, e, NULL) == 1 && RSA_size(rsa) == 256 &&
        RSA_sign(NID_sha256, md, sizeof(md), sig, &siglen, rsa) == 1 && siglen == 256 &&
        RSA_verify(NID_sha256, md, sizeof(md), sig, siglen, rsa) == 1;
    md[0] ^= 1;
    ok = ok && RSA_verify(NID_sha256, md, sizeof(md), sig, siglen, rsa) != 1;
    RSA_free(rsa);
    BN_free(e);
    return ok;
}

static int
ecdsa_round_trip(void)
{
    unsigned char md[32], sig[128];
    unsigned int siglen = 0;
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    int ok;

    memset(md, 0xa5, sizeof(md));
    ok = key != NULL && EC_KEY_generate_key(key) == 1 && ECDSA_size(key) <= (int)sizeof(sig) &&
        ECDSA_sign(0, md, sizeof(md), sig, &siglen, key) == 1 &&
        ECDSA_verify(0, md, sizeof(md), sig, (int)siglen, key) == 1;
    md[31] ^= 1;
    ok = ok && ECDSA_verify(0, md, sizeof(md), sig, (int)siglen, key) != 1;
    EC_KEY_free(key);
    return ok;
}

static X509 *
certificate(EVP_PKEY *key, int n, X509 *issuer, int ca)
{
    X509 *x = X509_new();
    X509_EXTENSION *ext;
    char cn[32];

    snprintf(cn, sizeof(cn), "kido chain %d", n);
    X509_set_version(x, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(x), n + 1);
    X509_gmtime_adj(X509_getm_notBefore(x), -3600);
    X509_gmtime_adj(X509_getm_notAfter(x), 86400);
    X509_NAME_add_entry_by_txt(X509_get_subject_name(x), "CN", MBSTRING_ASC, (const unsigned char *)cn, -1, -1, 0);
    X509_set_issuer_name(x, X509_get_subject_name(issuer ? issuer : x));
    X509_set_pubkey(x, key);
    ext = X509V3_EXT_conf_nid(NULL, NULL, NID_basic_constraints, ca ? "critical,CA:TRUE" : "critical,CA:FALSE");
    X509_add_ext(x, ext, -1);
    X509_EXTENSION_free(ext);
    X509_sign(x, key, EVP_sha256());
    return x;
}

static int
accept_all(int ok, X509_STORE_CTX *ctx)
{
    (void)ok;
    (void)ctx;
    return 1;
}

static int
verify_chain(int length, int legacy, const char *host, unsigned long *flags, int *error)
{
    EC_KEY *ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    EVP_PKEY *key = EVP_PKEY_new();
    X509_STORE *store = X509_STORE_new();
    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    STACK *untrusted = sk_new_null();
    X509 *certs[40];
    int i, ok;

    EC_KEY_generate_key(ec);
    EVP_PKEY_set1_EC_KEY(key, ec);
    for (i = 0; i < length; i++)
        certs[i] = certificate(key, i, i ? certs[i - 1] : NULL, i < length - 1);
    X509_STORE_add_cert(store, certs[0]);
    for (i = 1; i < length - 1; i++)
        sk_push(untrusted, certs[i]);
    X509_STORE_CTX_init(ctx, store, certs[length - 1], untrusted);
    *flags = X509_VERIFY_PARAM_get_flags(X509_STORE_CTX_get0_param(ctx));
    if (!legacy)
        X509_VERIFY_PARAM_clear_flags(X509_STORE_CTX_get0_param(ctx), X509_V_FLAG_LEGACY_VERIFY);
    if (host) {
        X509_VERIFY_PARAM_set1_host(X509_STORE_CTX_get0_param(ctx), host, 0);
        X509_STORE_CTX_set_verify_cb(ctx, accept_all);
    }
    ok = X509_verify_cert(ctx) == 1;
    *error = X509_STORE_CTX_get_error(ctx);
    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);
    sk_free(untrusted);
    for (i = 0; i < length; i++)
        X509_free(certs[i]);
    EVP_PKEY_free(key);
    EC_KEY_free(ec);
    return ok;
}

int
main(void)
{
    unsigned long flags;
    int error;
    static const char abc[] = "abc";
    unsigned char key[20], r1[32], r2[32], zero[32] = { 0 };

    setvbuf(stdout, NULL, _IOLBF, 0);
    CHECK(strcmp(OpenSSL_version(0), "LibreSSL 3.3.6") == 0);
    CHECK(strcmp(OpenSSL_version(OPENSSL_DIR), "OPENSSLDIR: \"/private/etc/ssl\"") == 0);
    CHECK(OpenSSL_version_num() == 0x20000000UL);
    CHECK(strcmp(X509_get_default_cert_file(), "/private/etc/ssl/cert.pem") == 0);
    CHECK(strcmp(X509_get_default_cert_dir(), "/private/etc/ssl/certs") == 0);

    CHECK(digest(EVP_md5(), abc, 3, "900150983cd24fb0d6963f7d28e17f72"));
    CHECK(digest(EVP_sha1(), abc, 3, "a9993e364706816aba3e25717850c26c9cd0d89d"));
    CHECK(digest(EVP_sha256(), abc, 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(digest(EVP_sha512(), abc, 3,
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"));
    CHECK(sha256_million());
    CHECK(sha256_legacy());

    memset(key, 0x0b, sizeof(key));
    CHECK(hmac(EVP_sha256(), key, 20, "Hi There",
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
    CHECK(hmac(EVP_sha256(), "Jefe", 4, "what do ya want for nothing?",
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
    CHECK(hmac(EVP_sha1(), "Jefe", 4, "what do ya want for nothing?",
        "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79"));

    CHECK(crypt_block(EVP_aes_128_ecb(), "000102030405060708090a0b0c0d0e0f", NULL,
        "00112233445566778899aabbccddeeff", "69c4e0d86a7b0430d8cdb78070b4c55a"));
    CHECK(crypt_block(EVP_aes_128_cbc(), "2b7e151628aed2a6abf7158809cf4f3c", "000102030405060708090a0b0c0d0e0f",
        "6bc1bee22e409f96e93d7e117393172a", "7649abac8119b246cee98e9b12e9197d"));
    CHECK(gcm(0));
    CHECK(!gcm(1));

    CHECK(EVP_get_cipherbyname("aes-256-gcm") != NULL);
    CHECK(EVP_get_cipherbyname("idea-cbc") == NULL);

    CHECK(RAND_status() == 1);
    CHECK(RAND_bytes(r1, sizeof(r1)) == 1 && RAND_bytes(r2, sizeof(r2)) == 1);
    CHECK(memcmp(r1, r2, sizeof(r1)) != 0 && memcmp(r1, zero, sizeof(r1)) != 0);

    CHECK(rsa_round_trip());
    CHECK(ecdsa_round_trip());

    CHECK(verify_chain(33, 1, NULL, &flags, &error) && flags == (X509_V_FLAG_LEGACY_VERIFY | X509_V_FLAG_TRUSTED_FIRST));
    CHECK(verify_chain(32, 0, NULL, &flags, &error));
    CHECK(!verify_chain(33, 0, NULL, &flags, &error) && error == X509_V_ERR_CERT_CHAIN_TOO_LONG);
    CHECK(verify_chain(2, 1, "wrong.example", &flags, &error) && error == X509_V_ERR_HOSTNAME_MISMATCH);
    CHECK(verify_chain(2, 0, "wrong.example", &flags, &error) && error == X509_V_ERR_HOSTNAME_MISMATCH);
    return checks_done("libcrypto");
}

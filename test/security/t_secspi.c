/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <CommonCrypto/CommonDigest.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libsystem/check.h"

typedef struct bignum_st BIGNUM;
typedef struct dh_st DH;

BIGNUM *BN_bin2bn(const unsigned char *, int, BIGNUM *);
int BN_bn2bin(const BIGNUM *, unsigned char *);
BIGNUM *BN_new(void);
int BN_set_word(BIGNUM *, unsigned long);
void BN_free(BIGNUM *);
DH *DH_new(void);
void DH_free(DH *);
int DH_set0_pqg(DH *, BIGNUM *, BIGNUM *, BIGNUM *);
int DH_generate_key(DH *);
void DH_get0_key(const DH *, const BIGNUM **, const BIGNUM **);
int DH_compute_key(unsigned char *, const BIGNUM *, DH *);

typedef struct OpaqueSecDHContext *SecDHContext;
typedef struct _CCRSACryptor *CCRSACryptorRef;

enum { ccRSAKeyPrivate = 1, ccPKCS1Padding = 1001, ccOAEPPadding = 1002, kCCDigestSHA256 = 10, kCCDigestSHA512 = 12 };

static int32_t (*dh_create)(uint32_t, const uint8_t *, size_t, uint32_t, const uint8_t *, size_t, SecDHContext *);
static int32_t (*dh_generate)(SecDHContext, uint8_t *, size_t *);
static int32_t (*dh_compute)(SecDHContext, const uint8_t *, size_t, uint8_t *, size_t *);
static void (*dh_destroy)(SecDHContext);
static int32_t (*rsa_generate)(size_t, uint32_t, CCRSACryptorRef *, CCRSACryptorRef *);
static int32_t (*rsa_from_data)(int, const uint8_t *, size_t, const uint8_t *, size_t, const uint8_t *, size_t, const uint8_t *,
                                size_t, CCRSACryptorRef *);
static int32_t (*rsa_components)(CCRSACryptorRef, uint8_t *, size_t *, uint8_t *, size_t *, uint8_t *, size_t *, uint8_t *,
                                 size_t *);
static int32_t (*rsa_sign)(CCRSACryptorRef, int, const void *, size_t, int, size_t, void *, size_t *);
static int32_t (*rsa_verify)(CCRSACryptorRef, int, const void *, size_t, int, size_t, const void *, size_t);
static int32_t (*rsa_decrypt)(CCRSACryptorRef, int, const void *, size_t, void *, size_t *, const void *, size_t, int);
static void (*rsa_release)(CCRSACryptorRef);

static const char rfc3526_group14[] =
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B139B22514A08798E3404DD"
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
    "83655D23DCA3AD961C62F356208552BB9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
    "15728E5A8AACAA68FFFFFFFFFFFFFFFF";
static const char msg_text[] = "kido security known-answer message";

static struct {
    char name[32];
    uint8_t *data;
    size_t len;
} vectors[64];
static int nvectors;

static int
load(const char *path)
{
    static char line[1 << 16];
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return 0;
    while (nvectors < 64 && fgets(line, sizeof(line), f)) {
        char *hex = strchr(line, ' ');

        if (hex == NULL)
            continue;
        *hex++ = '\0';
        snprintf(vectors[nvectors].name, sizeof(vectors[nvectors].name), "%s", line);
        vectors[nvectors].len = strcspn(hex, "\r\n") / 2;
        vectors[nvectors].data = malloc(vectors[nvectors].len + 1);
        for (size_t i = 0; i < vectors[nvectors].len; i++)
            sscanf(hex + 2 * i, "%2hhx", &vectors[nvectors].data[i]);
        nvectors++;
    }
    fclose(f);
    return 1;
}

static const uint8_t *
get(const char *name, size_t *len)
{
    for (int i = 0; i < nvectors; i++)
        if (strcmp(vectors[i].name, name) == 0) {
            *len = vectors[i].len;
            return vectors[i].data;
        }
    *len = 0;
    return NULL;
}

static size_t
der_length(const uint8_t **p, const uint8_t *end)
{
    size_t n;
    int k;

    if (*p >= end)
        return SIZE_MAX;
    n = *(*p)++;
    if (n < 0x80)
        return n;
    for (k = (int)(n & 0x7f), n = 0; k > 0; k--) {
        if (*p >= end)
            return SIZE_MAX;
        n = n << 8 | *(*p)++;
    }
    return n;
}

static const uint8_t *
der_integer(const uint8_t **p, const uint8_t *end, size_t *len)
{
    const uint8_t *v;
    size_t n;

    if (*p >= end || *(*p)++ != 0x02 || (n = der_length(p, end)) > (size_t)(end - *p))
        return NULL;
    v = *p;
    *p += n;
    while (n > 1 && *v == 0) {
        v++;
        n--;
    }
    *len = n;
    return v;
}

static CCRSACryptorRef
rsa_from_pkcs1(const char *name)
{
    size_t len, nl, el, pl, ql, skip;
    const uint8_t *der = get(name, &len), *end = der + len, *p = der, *n, *e, *pp, *qq;
    CCRSACryptorRef key = NULL;

    if (der == NULL || *p++ != 0x30 || der_length(&p, end) != (size_t)(end - p) || der_integer(&p, end, &skip) == NULL ||
        (n = der_integer(&p, end, &nl)) == NULL || (e = der_integer(&p, end, &el)) == NULL ||
        der_integer(&p, end, &skip) == NULL || (pp = der_integer(&p, end, &pl)) == NULL ||
        (qq = der_integer(&p, end, &ql)) == NULL)
        return NULL;
    return rsa_from_data(ccRSAKeyPrivate, n, nl, e, el, pp, pl, qq, ql, &key) == 0 ? key : NULL;
}

static int
signs_like(CCRSACryptorRef key, int digest, const char *want)
{
    uint8_t md[CC_SHA512_DIGEST_LENGTH], sig[512];
    size_t siglen = sizeof(sig), wantlen, mdlen = digest == kCCDigestSHA256 ? CC_SHA256_DIGEST_LENGTH : CC_SHA512_DIGEST_LENGTH;
    const uint8_t *expected = get(want, &wantlen);

    if (digest == kCCDigestSHA256)
        CC_SHA256(msg_text, sizeof(msg_text) - 1, md);
    else
        CC_SHA512(msg_text, sizeof(msg_text) - 1, md);
    return key && expected && rsa_sign(key, ccPKCS1Padding, md, mdlen, digest, 0, sig, &siglen) == 0 && siglen == wantlen &&
           memcmp(sig, expected, siglen) == 0;
}

static void
rsa_tests(void)
{
    static const uint8_t e[] = { 1, 0, 1 };
    uint8_t md[CC_SHA256_DIGEST_LENGTH], plain[512], mod[512], d[512], p[512], q[512], s1[512], s2[512];
    size_t ctlen, plainlen = sizeof(plain), ml = sizeof(mod), dl = sizeof(d), pl = sizeof(p), ql = sizeof(q);
    size_t l1 = sizeof(s1), l2 = sizeof(s2);
    const uint8_t *ct = get("rsa_oaep_sha256", &ctlen);
    CCRSACryptorRef key = rsa_from_pkcs1("rsa_priv"), pub = NULL, priv = NULL, rebuilt = NULL;

    CHECK(key != NULL);
    CHECK(signs_like(key, kCCDigestSHA256, "rsa_pkcs1_sha256"));
    CHECK(key && ct && rsa_decrypt(key, ccOAEPPadding, ct, ctlen, plain, &plainlen, NULL, 0, kCCDigestSHA256) == 0 &&
          plainlen == sizeof(msg_text) - 1 && memcmp(plain, msg_text, plainlen) == 0);
    CHECK(signs_like(rsa_from_pkcs1("rsa4096_priv"), kCCDigestSHA512, "rsa4096_pkcs1_sha512"));

    CC_SHA256(msg_text, sizeof(msg_text) - 1, md);
    CHECK(rsa_generate(2048, 65537, &pub, &priv) == 0);
    CHECK(priv && rsa_components(priv, mod, &ml, d, &dl, p, &pl, q, &ql) == 0 && ml == 256 && pl == 128 && ql == 128);
    CHECK(rsa_from_data(ccRSAKeyPrivate, mod, ml, e, sizeof(e), p, pl, q, ql, &rebuilt) == 0);
    CHECK(rebuilt && rsa_sign(priv, ccPKCS1Padding, md, sizeof(md), kCCDigestSHA256, 0, s1, &l1) == 0 &&
          rsa_sign(rebuilt, ccPKCS1Padding, md, sizeof(md), kCCDigestSHA256, 0, s2, &l2) == 0 && l1 == l2 &&
          memcmp(s1, s2, l1) == 0);
    CHECK(pub && rsa_verify(pub, ccPKCS1Padding, md, sizeof(md), kCCDigestSHA256, 0, s2, l2) == 0);
    s2[l2 / 2] ^= 1;
    CHECK(pub && rsa_verify(pub, ccPKCS1Padding, md, sizeof(md), kCCDigestSHA256, 0, s2, l2) != 0);
    for (CCRSACryptorRef *k = (CCRSACryptorRef[]){ key, pub, priv, rebuilt }, *last = k + 4; k < last; k++)
        if (*k)
            rsa_release(*k);
}

static size_t
strip(const uint8_t *b, size_t n, const uint8_t **out)
{
    while (n > 0 && *b == 0) {
        b++;
        n--;
    }
    *out = b;
    return n;
}

static void
dh_tests(void)
{
    uint8_t p[256], mine[256], theirs[256], k1[256], k2[256], bad[256];
    size_t minelen = sizeof(mine), k1len = sizeof(k1), badlen;
    SecDHContext ctx = NULL;
    DH *dh = DH_new();
    BIGNUM *g = BN_new(), *peer;
    const BIGNUM *pub = NULL;
    const uint8_t *a, *b;
    int theirslen, k2len;

    for (int i = 0; i < 256; i++)
        sscanf(rfc3526_group14 + 2 * i, "%2hhx", &p[i]);
    CHECK(dh_create(2, p, sizeof(p), 0, NULL, 0, &ctx) == 0);
    CHECK(ctx && dh_generate(ctx, mine, &minelen) == 0 && minelen > 200);
    BN_set_word(g, 2);
    CHECK(DH_set0_pqg(dh, BN_bin2bn(p, sizeof(p), NULL), NULL, g) == 1 && DH_generate_key(dh) == 1);
    DH_get0_key(dh, &pub, NULL);
    theirslen = BN_bn2bin(pub, theirs);
    CHECK(ctx && dh_compute(ctx, theirs, (size_t)theirslen, k1, &k1len) == 0);
    peer = BN_bin2bn(mine, (int)minelen, NULL);
    k2len = DH_compute_key(k2, peer, dh);
    CHECK(k2len > 200 && strip(k1, k1len, &a) == strip(k2, (size_t)k2len, &b) && memcmp(a, b, (size_t)k2len) == 0);
    memset(bad, 0, sizeof(bad));
    bad[255] = 1;
    badlen = sizeof(k1);
    CHECK(ctx && dh_compute(ctx, bad, sizeof(bad), k1, &badlen) != 0);
    memcpy(bad, p, sizeof(p));
    bad[255] -= 1;
    badlen = sizeof(k1);
    CHECK(ctx && dh_compute(ctx, bad, sizeof(bad), k1, &badlen) != 0);
    if (ctx)
        dh_destroy(ctx);
    BN_free(peer);
    DH_free(dh);
}

int
main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc != 2 || !load(argv[1])) {
        fprintf(stderr, "usage: t_secspi VECTORS\n");
        return 2;
    }
    dh_create = dlsym(RTLD_DEFAULT, "SecDHCreate");
    dh_generate = dlsym(RTLD_DEFAULT, "SecDHGenerateKeypair");
    dh_compute = dlsym(RTLD_DEFAULT, "SecDHComputeKey");
    dh_destroy = dlsym(RTLD_DEFAULT, "SecDHDestroy");
    rsa_generate = dlsym(RTLD_DEFAULT, "CCRSACryptorGeneratePair");
    rsa_from_data = dlsym(RTLD_DEFAULT, "CCRSACryptorCreateFromData");
    rsa_components = dlsym(RTLD_DEFAULT, "CCRSAGetKeyComponents");
    rsa_sign = dlsym(RTLD_DEFAULT, "CCRSACryptorSign");
    rsa_verify = dlsym(RTLD_DEFAULT, "CCRSACryptorVerify");
    rsa_decrypt = dlsym(RTLD_DEFAULT, "CCRSACryptorDecrypt");
    rsa_release = dlsym(RTLD_DEFAULT, "CCRSACryptorRelease");
    if (!dh_create || !dh_generate || !dh_compute || !dh_destroy || !rsa_generate || !rsa_from_data || !rsa_components ||
        !rsa_sign || !rsa_verify || !rsa_decrypt || !rsa_release) {
        printf("FAIL the DH and RSA cryptor interfaces are not all in Security\n");
        return 1;
    }
    rsa_tests();
    dh_tests();
    return checks_done("Security's RSA from p and q and DH");
}

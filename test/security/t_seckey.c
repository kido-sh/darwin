/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXV 64

static int checks_run, checks_failed;
static const char msg_text[] = "kido security known-answer message";
static char *vname[MAXV];
static CFDataRef vdata[MAXV];
static int nv;
static const int curves[] = { 256, 384, 521 };

static void
check(bool ok, CFErrorRef err, const char *fmt, ...)
{
    char name[256], why[256] = "";
    va_list ap;

    checks_run++;
    if (ok)
        return;
    checks_failed++;
    va_start(ap, fmt);
    vsnprintf(name, sizeof(name), fmt, ap);
    va_end(ap);
    if (err) {
        CFStringRef d = CFErrorCopyDescription(err);
        if (d) {
            CFStringGetCString(d, why, sizeof(why), kCFStringEncodingUTF8);
            CFRelease(d);
        }
    }
    printf("FAIL %s %s\n", name, why);
}

static CFDataRef
msg(void)
{
    return CFDataCreate(NULL, (const UInt8 *)msg_text, sizeof(msg_text) - 1);
}

static bool
data_eq(CFDataRef a, CFDataRef b)
{
    return a && b && CFEqual(a, b);
}

static CFDictionaryRef
dict(int n, const void **k, const void **v)
{
    return CFDictionaryCreate(NULL, k, v, n, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static SecKeyRef
make_key(CFStringRef type, int bits, CFErrorRef *err)
{
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &bits);
    const void *k[] = { kSecAttrKeyType, kSecAttrKeySizeInBits, kSecAttrIsPermanent };
    const void *v[] = { type, n, kCFBooleanFalse };
    CFDictionaryRef a = dict(3, k, v);
    SecKeyRef key = SecKeyCreateRandomKey(a, err);

    CFRelease(a);
    CFRelease(n);
    return key;
}

static SecKeyRef
import_key(CFDataRef data, CFStringRef type, CFStringRef cls, CFErrorRef *err)
{
    const void *k[] = { kSecAttrKeyType, kSecAttrKeyClass };
    const void *v[] = { type, cls };
    CFDictionaryRef a;
    SecKeyRef key;

    if (data == NULL)
        return NULL;
    a = dict(2, k, v);
    key = SecKeyCreateWithData(data, a, err);
    CFRelease(a);
    return key;
}

static CFDataRef
tamper(CFDataRef d)
{
    CFMutableDataRef m;

    if (d == NULL)
        return NULL;
    m = CFDataCreateMutableCopy(NULL, 0, d);
    CFDataGetMutableBytePtr(m)[CFDataGetLength(m) / 2] ^= 0x01;
    return m;
}

static CFDictionaryRef
no_parameters(void)
{
    return dict(0, NULL, NULL);
}

static SecKeyAlgorithm
ecdsa(int bits)
{
    return bits == 256 ? kSecKeyAlgorithmECDSASignatureMessageX962SHA256
        : bits == 384  ? kSecKeyAlgorithmECDSASignatureMessageX962SHA384
                       : kSecKeyAlgorithmECDSASignatureMessageX962SHA512;
}

static void
put(const char *name, CFDataRef d)
{
    if (d == NULL) {
        check(false, NULL, "make %s", name);
        return;
    }
    printf("%s ", name);
    for (CFIndex i = 0; i < CFDataGetLength(d); i++)
        printf("%02x", CFDataGetBytePtr(d)[i]);
    printf("\n");
}

static CFDataRef
get(const char *name)
{
    for (int i = 0; i < nv; i++)
        if (strcmp(vname[i], name) == 0)
            return vdata[i];
    return NULL;
}

static bool
load(const char *path)
{
    static char line[1 << 16];
    FILE *f = fopen(path, "r");

    if (f == NULL)
        return false;
    while (nv < MAXV && fgets(line, sizeof(line), f)) {
        char *hex = strchr(line, ' ');
        size_t len;
        UInt8 *buf;

        if (hex == NULL)
            continue;
        *hex++ = '\0';
        len = strcspn(hex, "\r\n") / 2;
        buf = malloc(len ? len : 1);
        for (size_t i = 0; i < len; i++)
            sscanf(hex + 2 * i, "%2hhx", &buf[i]);
        vname[nv] = strdup(line);
        vdata[nv++] = CFDataCreate(NULL, buf, (CFIndex)len);
        free(buf);
    }
    fclose(f);
    return true;
}

static void
gen(void)
{
    CFErrorRef err = NULL;
    CFDataRef m = msg();
    SecKeyRef rsa = make_key(kSecAttrKeyTypeRSA, 2048, &err), rsa4k = make_key(kSecAttrKeyTypeRSA, 4096, &err), rsapub;
    char name[64];

    if (rsa == NULL || rsa4k == NULL) {
        check(false, err, "generate the RSA keys");
        return;
    }
    rsapub = SecKeyCopyPublicKey(rsa);
    put("rsa_priv", SecKeyCopyExternalRepresentation(rsa, &err));
    put("rsa_pkcs1_sha256", SecKeyCreateSignature(rsa, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, m, &err));
    put("rsa_pss_sha256", SecKeyCreateSignature(rsa, kSecKeyAlgorithmRSASignatureMessagePSSSHA256, m, &err));
    put("rsa_oaep_sha256", SecKeyCreateEncryptedData(rsapub, kSecKeyAlgorithmRSAEncryptionOAEPSHA256, m, &err));
    put("rsa_pkcs1_ct", SecKeyCreateEncryptedData(rsapub, kSecKeyAlgorithmRSAEncryptionPKCS1, m, &err));
    put("rsa4096_priv", SecKeyCopyExternalRepresentation(rsa4k, &err));
    put("rsa4096_pkcs1_sha512", SecKeyCreateSignature(rsa4k, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512, m, &err));
    for (int i = 0; i < 3; i++) {
        int bits = curves[i];
        SecKeyRef ec = make_key(kSecAttrKeyTypeECSECPrimeRandom, bits, &err);
        SecKeyRef peer = make_key(kSecAttrKeyTypeECSECPrimeRandom, bits, &err);
        SecKeyRef ecpub = ec ? SecKeyCopyPublicKey(ec) : NULL, peerpub = peer ? SecKeyCopyPublicKey(peer) : NULL;

        if (ecpub == NULL || peerpub == NULL) {
            check(false, err, "generate the P-%d keys", bits);
            continue;
        }
        snprintf(name, sizeof(name), "ec%d_priv", bits);
        put(name, SecKeyCopyExternalRepresentation(ec, &err));
        snprintf(name, sizeof(name), "ec%d_sig", bits);
        put(name, SecKeyCreateSignature(ec, ecdsa(bits), m, &err));
        snprintf(name, sizeof(name), "ec%d_peer", bits);
        put(name, SecKeyCopyExternalRepresentation(peerpub, &err));
        snprintf(name, sizeof(name), "ec%d_shared", bits);
        put(name, SecKeyCopyKeyExchangeResult(ec, kSecKeyAlgorithmECDHKeyExchangeStandard, peerpub, no_parameters(), &err));
        snprintf(name, sizeof(name), "ec%d_ecies", bits);
        put(name, SecKeyCreateEncryptedData(ecpub, kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM, m, &err));
    }
}

static void
check_vectors(void)
{
    CFErrorRef err = NULL;
    CFDataRef m = msg();
    SecKeyRef rsa = import_key(get("rsa_priv"), kSecAttrKeyTypeRSA, kSecAttrKeyClassPrivate, &err), rsa4k, pub;

    check(rsa != NULL, err, "RSA-2048 import the PKCS#1 private key");
    if (rsa) {
        pub = SecKeyCopyPublicKey(rsa);
        check(data_eq(SecKeyCreateSignature(rsa, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, m, &err),
                      get("rsa_pkcs1_sha256")),
              err, "RSA-2048 PKCS#1 v1.5 SHA-256 signature equals the vector's");
        check(SecKeyVerifySignature(pub, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, m, get("rsa_pkcs1_sha256"), &err),
              err, "RSA-2048 verify the PKCS#1 v1.5 signature");
        check(SecKeyVerifySignature(pub, kSecKeyAlgorithmRSASignatureMessagePSSSHA256, m, get("rsa_pss_sha256"), &err), err,
              "RSA-2048 verify the PSS signature");
        check(!SecKeyVerifySignature(pub, kSecKeyAlgorithmRSASignatureMessagePSSSHA256, m, tamper(get("rsa_pss_sha256")), NULL),
              NULL, "RSA-2048 refuse a tampered PSS signature");
        check(data_eq(SecKeyCreateDecryptedData(rsa, kSecKeyAlgorithmRSAEncryptionOAEPSHA256, get("rsa_oaep_sha256"), &err), m),
              err, "RSA-2048 decrypt the OAEP SHA-256 ciphertext");
        check(data_eq(SecKeyCreateDecryptedData(rsa, kSecKeyAlgorithmRSAEncryptionPKCS1, get("rsa_pkcs1_ct"), &err), m), err,
              "RSA-2048 decrypt the PKCS#1 v1.5 ciphertext");
    }
    rsa4k = import_key(get("rsa4096_priv"), kSecAttrKeyTypeRSA, kSecAttrKeyClassPrivate, &err);
    check(rsa4k && data_eq(SecKeyCreateSignature(rsa4k, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512, m, &err),
                           get("rsa4096_pkcs1_sha512")),
          err, "RSA-4096 PKCS#1 v1.5 SHA-512 signature equals the vector's");
    for (int i = 0; i < 3; i++) {
        int bits = curves[i];
        char name[64];
        SecKeyRef ec, peer;

        snprintf(name, sizeof(name), "ec%d_priv", bits);
        ec = import_key(get(name), kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPrivate, &err);
        check(ec != NULL, err, "P-%d import the X9.63 private key", bits);
        if (ec == NULL)
            continue;
        pub = SecKeyCopyPublicKey(ec);
        snprintf(name, sizeof(name), "ec%d_sig", bits);
        check(SecKeyVerifySignature(pub, ecdsa(bits), m, get(name), &err), err, "P-%d verify the ECDSA signature", bits);
        check(!SecKeyVerifySignature(pub, ecdsa(bits), m, tamper(get(name)), NULL), NULL,
              "P-%d refuse a tampered ECDSA signature", bits);
        snprintf(name, sizeof(name), "ec%d_peer", bits);
        peer = import_key(get(name), kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPublic, &err);
        snprintf(name, sizeof(name), "ec%d_shared", bits);
        check(peer && data_eq(SecKeyCopyKeyExchangeResult(ec, kSecKeyAlgorithmECDHKeyExchangeStandard, peer, no_parameters(), &err),
                              get(name)),
              err, "P-%d ECDH secret equals the vector's", bits);
        snprintf(name, sizeof(name), "ec%d_ecies", bits);
        check(data_eq(SecKeyCreateDecryptedData(ec, kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM, get(name),
                                                &err),
                      m),
              err, "P-%d decrypt the ECIES ciphertext", bits);
    }
}

static void
self_rsa(int bits)
{
    const char *signames[] = { "PKCS#1 v1.5 SHA-256", "PSS SHA-256", "PKCS#1 v1.5 SHA-1" };
    const char *encnames[] = { "OAEP SHA-256", "OAEP SHA-1", "PKCS#1 v1.5" };
    SecKeyAlgorithm sigs[] = { kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, kSecKeyAlgorithmRSASignatureMessagePSSSHA256,
                               kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1 };
    SecKeyAlgorithm encs[] = { kSecKeyAlgorithmRSAEncryptionOAEPSHA256, kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
                               kSecKeyAlgorithmRSAEncryptionPKCS1 };
    CFErrorRef err = NULL;
    CFDataRef m = msg(), s1, s2;
    SecKeyRef priv = make_key(kSecAttrKeyTypeRSA, bits, &err), pub, again, pub2;

    check(priv != NULL, err, "RSA-%d generate", bits);
    if (priv == NULL)
        return;
    pub = SecKeyCopyPublicKey(priv);
    for (int i = 0; i < 3; i++) {
        CFDataRef sig = SecKeyCreateSignature(priv, sigs[i], m, &err);

        check(sig && SecKeyVerifySignature(pub, sigs[i], m, sig, &err), err, "RSA-%d sign and verify %s", bits, signames[i]);
        check(sig && !SecKeyVerifySignature(pub, sigs[i], m, tamper(sig), NULL), NULL, "RSA-%d refuse a tampered %s signature",
              bits, signames[i]);
    }
    for (int i = 0; i < 3; i++) {
        CFDataRef ct = SecKeyCreateEncryptedData(pub, encs[i], m, &err);

        check(ct && data_eq(SecKeyCreateDecryptedData(priv, encs[i], ct, &err), m), err, "RSA-%d encrypt and decrypt %s", bits,
              encnames[i]);
    }
    again = import_key(SecKeyCopyExternalRepresentation(priv, &err), kSecAttrKeyTypeRSA, kSecAttrKeyClassPrivate, &err);
    s1 = SecKeyCreateSignature(priv, sigs[0], m, NULL);
    s2 = again ? SecKeyCreateSignature(again, sigs[0], m, &err) : NULL;
    check(data_eq(s1, s2), err, "RSA-%d export, import, same signature", bits);
    pub2 = import_key(SecKeyCopyExternalRepresentation(pub, &err), kSecAttrKeyTypeRSA, kSecAttrKeyClassPublic, &err);
    check(pub2 && SecKeyVerifySignature(pub2, sigs[0], m, s1, &err), err, "RSA-%d export and import the public key, verify",
          bits);
}

static void
self_ec(int bits)
{
    CFErrorRef err = NULL;
    CFDataRef m = msg(), sig, ab, ba, ct, sig2;
    SecKeyRef a = make_key(kSecAttrKeyTypeECSECPrimeRandom, bits, &err), b = make_key(kSecAttrKeyTypeECSECPrimeRandom, bits, &err);
    SecKeyRef apub, bpub, again;

    check(a && b, err, "P-%d generate", bits);
    if (a == NULL || b == NULL)
        return;
    apub = SecKeyCopyPublicKey(a);
    bpub = SecKeyCopyPublicKey(b);
    sig = SecKeyCreateSignature(a, ecdsa(bits), m, &err);
    check(sig && SecKeyVerifySignature(apub, ecdsa(bits), m, sig, &err), err, "P-%d ECDSA sign and verify", bits);
    check(sig && !SecKeyVerifySignature(bpub, ecdsa(bits), m, sig, NULL), NULL, "P-%d ECDSA refuse another key's signature", bits);
    ab = SecKeyCopyKeyExchangeResult(a, kSecKeyAlgorithmECDHKeyExchangeStandard, bpub, no_parameters(), &err);
    ba = SecKeyCopyKeyExchangeResult(b, kSecKeyAlgorithmECDHKeyExchangeStandard, apub, no_parameters(), &err);
    check(data_eq(ab, ba) && CFDataGetLength(ab) == (bits + 7) / 8, err, "P-%d ECDH both sides agree", bits);
    ct = SecKeyCreateEncryptedData(apub, kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM, m, &err);
    check(ct && data_eq(SecKeyCreateDecryptedData(a, kSecKeyAlgorithmECIESEncryptionStandardVariableIVX963SHA256AESGCM, ct, &err), m),
          err, "P-%d ECIES encrypt and decrypt", bits);
    again = import_key(SecKeyCopyExternalRepresentation(a, &err), kSecAttrKeyTypeECSECPrimeRandom, kSecAttrKeyClassPrivate, &err);
    sig2 = again ? SecKeyCreateSignature(again, ecdsa(bits), m, &err) : NULL;
    check(sig2 && SecKeyVerifySignature(apub, ecdsa(bits), m, sig2, &err), err, "P-%d X9.63 export, import, verify", bits);
}

int
main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc == 2 && strcmp(argv[1], "gen") == 0) {
        gen();
        return checks_failed != 0;
    }
    if (argc == 3 && strcmp(argv[1], "check") == 0) {
        if (!load(argv[2])) {
            fprintf(stderr, "t_seckey: cannot read %s\n", argv[2]);
            return 2;
        }
        check_vectors();
    } else if (argc == 2 && strcmp(argv[1], "self") == 0) {
        self_rsa(2048);
        self_rsa(4096);
        for (int i = 0; i < 3; i++)
            self_ec(curves[i]);
    } else {
        fprintf(stderr, "usage: t_seckey gen | check FILE | self\n");
        return 2;
    }
    printf("SecKey %s: %d/%d passed\n", argv[1], checks_run - checks_failed, checks_run);
    return checks_failed != 0;
}

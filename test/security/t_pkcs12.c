/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CFDataRef
slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    UInt8 buf[65536];
    size_t n;

    if (f == NULL)
        return NULL;
    n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    return CFDataCreate(NULL, buf, (CFIndex)n);
}

static void
print_hex(const char *prefix, CFDataRef d, CFIndex max)
{
    CFIndex n = d ? CFDataGetLength(d) : 0;

    printf("%s", prefix);
    for (CFIndex i = 0; i < n && i < max; i++)
        printf("%02x", CFDataGetBytePtr(d)[i]);
    printf(n ? "" : "none");
}

static void
print_string(const char *prefix, CFStringRef s)
{
    char buf[256] = "";

    if (s)
        CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8);
    printf("%s\"%s\"", prefix, buf);
}

static int
by_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static void
print_sorted(CFArrayRef certs)
{
    char names[16][256];
    char *sorted[16];
    CFIndex n = certs ? CFArrayGetCount(certs) : 0;

    if (n > 16)
        n = 16;
    for (CFIndex i = 0; i < n; i++) {
        CFStringRef s = SecCertificateCopySubjectSummary((SecCertificateRef)CFArrayGetValueAtIndex(certs, i));

        names[i][0] = '\0';
        if (s)
            CFStringGetCString(s, names[i], sizeof(names[i]), kCFStringEncodingUTF8);
        sorted[i] = names[i];
    }
    qsort(sorted, (size_t)n, sizeof(sorted[0]), by_name);
    for (CFIndex i = 0; i < n; i++)
        printf(" \"%s\"", sorted[i]);
}

static CFDataRef
sha256(CFDataRef d)
{
    UInt8 md[CC_SHA256_DIGEST_LENGTH];

    CC_SHA256(CFDataGetBytePtr(d), (CC_LONG)CFDataGetLength(d), md);
    return CFDataCreate(NULL, md, sizeof(md));
}

static void
print_identity(SecIdentityRef identity)
{
    SecCertificateRef cert = NULL;
    SecKeyRef key = NULL, pub, certkey;
    CFDictionaryRef attrs;
    CFDataRef der, a, b, sig = NULL, m = CFDataCreate(NULL, (const UInt8 *)"kido", 4);
    CFStringRef type;
    SecKeyAlgorithm alg;
    int bits = 0;

    if (SecIdentityCopyCertificate(identity, &cert) != errSecSuccess || SecIdentityCopyPrivateKey(identity, &key) != errSecSuccess) {
        printf(" identity without certificate or key");
        return;
    }
    der = SecCertificateCopyData(cert);
    print_hex(" cert sha256 ", sha256(der), 8);
    print_string(" subject ", SecCertificateCopySubjectSummary(cert));
    attrs = SecKeyCopyAttributes(key);
    type = attrs ? CFDictionaryGetValue(attrs, kSecAttrKeyType) : NULL;
    if (attrs && CFDictionaryGetValue(attrs, kSecAttrKeySizeInBits))
        CFNumberGetValue(CFDictionaryGetValue(attrs, kSecAttrKeySizeInBits), kCFNumberIntType, &bits);
    print_string(" key type ", type);
    printf(" bits %d", bits);
    pub = SecKeyCopyPublicKey(key);
    certkey = SecCertificateCopyKey(cert);
    a = pub ? SecKeyCopyExternalRepresentation(pub, NULL) : NULL;
    b = certkey ? SecKeyCopyExternalRepresentation(certkey, NULL) : NULL;
    printf(" matches cert %s", a && b && CFEqual(a, b) ? "yes" : "no");
    alg = type && CFEqual(type, kSecAttrKeyTypeRSA) ? kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256
                                                    : kSecKeyAlgorithmECDSASignatureMessageX962SHA256;
    sig = SecKeyCreateSignature(key, alg, m, NULL);
    printf(" signs %s", sig && certkey && SecKeyVerifySignature(certkey, alg, m, sig, NULL) ? "yes" : "no");
}

int
main(int argc, char **argv)
{
    CFDataRef data;
    CFStringRef password;
    CFMutableDictionaryRef options;
    CFArrayRef items = NULL;
    OSStatus status;

    if (argc != 4) {
        fprintf(stderr, "usage: t_pkcs12 NAME FILE PASSWORD\n");
        return 2;
    }
    if ((data = slurp(argv[2])) == NULL) {
        printf("%s: cannot read %s\n", argv[1], argv[2]);
        return 1;
    }
    password = CFStringCreateWithCString(NULL, argv[3], kCFStringEncodingUTF8);
    options = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(options, kSecImportExportPassphrase, password);
    if (&kSecImportToMemoryOnly != NULL)
        CFDictionarySetValue(options, kSecImportToMemoryOnly, kCFBooleanTrue);
    status = SecPKCS12Import(data, options, &items);
    printf("%s: status %d, items %ld\n", argv[1], (int)status, items ? (long)CFArrayGetCount(items) : 0L);
    for (CFIndex i = 0; items && i < CFArrayGetCount(items); i++) {
        CFDictionaryRef item = CFArrayGetValueAtIndex(items, i);
        CFArrayRef chain = CFDictionaryGetValue(item, kSecImportItemCertChain);
        CFTypeRef identity = CFDictionaryGetValue(item, kSecImportItemIdentity);

        printf("%s item %ld:", argv[1], (long)i);
        print_string(" label ", CFDictionaryGetValue(item, kSecImportItemLabel));
        print_hex(" keyid ", CFDictionaryGetValue(item, kSecImportItemKeyID), 64);
        printf(" trust %s", CFDictionaryGetValue(item, kSecImportItemTrust) ? "yes" : "no");
        if (identity) {
            print_identity((SecIdentityRef)identity);
            printf(" chain");
            for (CFIndex j = 0; chain && j < CFArrayGetCount(chain); j++)
                print_string(" ", SecCertificateCopySubjectSummary((SecCertificateRef)CFArrayGetValueAtIndex(chain, j)));
        } else {
            printf(" certificates");
            print_sorted(chain);
        }
        printf("\n");
    }
    return 0;
}

/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "../libsystem/check.h"

static CFDataRef
load(const char *path)
{
    unsigned char buf[16384];
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    return CFDataCreate(NULL, buf, (CFIndex)n);
}

static void
describe(const char *tag, SecKeyRef key, CFDataRef reference)
{
    if (key == NULL) {
        printf("result certlegacy %s key none\n", tag);
        return;
    }
    CFDictionaryRef a = SecKeyCopyAttributes(key);
    CFStringRef type = a ? CFDictionaryGetValue(a, kSecAttrKeyType) : NULL;
    CFNumberRef size = a ? CFDictionaryGetValue(a, kSecAttrKeySizeInBits) : NULL;
    int bits = 0;
    char t[32] = "-";
    if (size != NULL) {
        CFNumberGetValue(size, kCFNumberIntType, &bits);
    }
    if (type != NULL) {
        CFStringGetCString(type, t, sizeof(t), kCFStringEncodingUTF8);
    }
    CFDataRef ext = SecKeyCopyExternalRepresentation(key, NULL);
    printf("result certlegacy %s key type %s bits %d same as SecCertificateCopyKey %d\n", tag, t, bits,
           ext != NULL && reference != NULL && CFEqual(ext, reference));
    printf("result certlegacy blocksize %s %zu\n", tag, SecKeyGetBlockSize(key));
    if (ext != NULL) {
        CFRelease(ext);
    }
    if (a != NULL) {
        CFRelease(a);
    }
}

static void
serial(const char *tag, CFDataRef s)
{
    printf("result certlegacy %s serial", tag);
    if (s == NULL) {
        printf(" none\n");
        return;
    }
    for (CFIndex i = 0; i < CFDataGetLength(s); i++) {
        printf("%s%02x", i ? "" : " ", CFDataGetBytePtr(s)[i]);
    }
    printf("\n");
}

static void
certificate(const char *name, const char *path)
{
    CFDataRef der = load(path);
    SecCertificateRef cert = der ? SecCertificateCreateWithData(NULL, der) : NULL;
    CHECK(cert != NULL);
    if (cert == NULL) {
        return;
    }
    SecKeyRef modern = SecCertificateCopyKey(cert);
    CFDataRef reference = modern ? SecKeyCopyExternalRepresentation(modern, NULL) : NULL;

    SecKeyRef key = NULL;
    OSStatus st = SecCertificateCopyPublicKey(cert, &key);
    printf("result certlegacy %s SecCertificateCopyPublicKey status %d\n", name, (int)st);
    CHECK(st == errSecSuccess && key != NULL);
    describe(name, key, reference);

    SecKeyRef (*ios)(SecCertificateRef) = dlsym(RTLD_DEFAULT, "SecCertificateCopyPublicKey_ios");
    CHECK(ios != NULL);
    SecKeyRef k2 = ios ? ios(cert) : NULL;
    describe("ios", k2, reference);

    CFErrorRef error = NULL;
    CFDataRef s1 = SecCertificateCopySerialNumber(cert, &error);
    serial(name, s1);
    CHECK(s1 != NULL && error == NULL);
    CFDataRef (*sios)(SecCertificateRef) = dlsym(RTLD_DEFAULT, "SecCertificateCopySerialNumber_ios");
    CHECK(sios != NULL);
    CFDataRef s2 = sios ? sios(cert) : NULL;
    CHECK(s1 != NULL && s2 != NULL && CFEqual(s1, s2));
    serial("ios", s2);

    st = SecCertificateCopyPublicKey(cert, NULL);
    printf("result certlegacy %s no key pointer status %d\n", name, (int)st);
    CHECK(st == 4);
}

int
main(int argc, char **argv)
{
    for (int i = 1; i + 1 < argc; i += 2) {
        certificate(argv[i], argv[i + 1]);
    }
    SecKeyRef key = (SecKeyRef)1;
    OSStatus st = SecCertificateCopyPublicKey(NULL, &key);
    printf("result certlegacy no certificate status %d key untouched %d\n", (int)st, key == (SecKeyRef)1);
    CHECK(st == errSecInvalidItemRef && key == (SecKeyRef)1);
    CFErrorRef error = NULL;
    CFDataRef s = SecCertificateCopySerialNumber(NULL, &error);
    printf("result certlegacy no certificate serial %s error %ld\n", s ? "data" : "none",
           error ? (long)CFErrorGetCode(error) : 0L);
    CHECK(s == NULL && error != NULL && CFErrorGetCode(error) == errSecInvalidCertificateRef);
    return checks_done("certlegacy");
}

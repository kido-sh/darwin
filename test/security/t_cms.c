/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/CMSDecoder.h>
#include <Security/Security.h>
#include <stdio.h>
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
print_string(const char *prefix, CFStringRef s)
{
    char buf[256] = "";

    if (s)
        CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8);
    printf("%s\"%s\"", prefix, buf);
}

static long
anchored(SecTrustRef trust, CFArrayRef roots)
{
    CFErrorRef err = NULL;
    long code;

    if (trust == NULL)
        return 1;
    SecTrustSetAnchorCertificates(trust, roots);
    SecTrustSetAnchorCertificatesOnly(trust, true);
    code = SecTrustEvaluateWithError(trust, &err) ? 0 : (err ? CFErrorGetCode(err) : 2);
    if (err)
        CFRelease(err);
    return code;
}

int
main(int argc, char **argv)
{
    CFDataRef message, ca, detached = NULL;
    SecCertificateRef root;
    CFArrayRef roots, certs = NULL;
    CMSDecoderRef decoder = NULL;
    OSStatus created, updated, attached = noErr, finalized;
    size_t signers = 0;

    if (argc != 4 && !(argc == 6 && (strcmp(argv[4], "detached") == 0 || strcmp(argv[4], "changed") == 0))) {
        fprintf(stderr, "usage: t_cms NAME FILE CA_DER [detached CONTENT | changed CONTENT]\n");
        return 2;
    }
    if ((message = slurp(argv[2])) == NULL || (ca = slurp(argv[3])) == NULL ||
        (argc == 6 && (detached = slurp(argv[5])) == NULL) || (root = SecCertificateCreateWithData(NULL, ca)) == NULL) {
        printf("%s: cannot read the inputs\n", argv[1]);
        return 1;
    }
    if (argc == 6 && strcmp(argv[4], "changed") == 0) {
        CFMutableDataRef changed = CFDataCreateMutableCopy(NULL, 0, message);
        UInt8 *found = memmem(CFDataGetMutableBytePtr(changed), (size_t)CFDataGetLength(changed), CFDataGetBytePtr(detached),
                              (size_t)CFDataGetLength(detached));

        if (found == NULL) {
            printf("%s: the content is not in the message\n", argv[1]);
            return 1;
        }
        found[0] ^= 0x20;
        CFRelease(message);
        CFRelease(detached);
        message = changed;
        detached = NULL;
    }
    roots = CFArrayCreate(NULL, (const void **)&root, 1, &kCFTypeArrayCallBacks);
    created = CMSDecoderCreate(&decoder);
    updated = CMSDecoderUpdateMessage(decoder, CFDataGetBytePtr(message), (size_t)CFDataGetLength(message));
    if (detached)
        attached = CMSDecoderSetDetachedContent(decoder, detached);
    finalized = CMSDecoderFinalizeMessage(decoder);
    CMSDecoderGetNumSigners(decoder, &signers);
    CMSDecoderCopyAllCerts(decoder, &certs);
    printf("%s: create %d, update %d, detached %d, finalize %d, signers %zu, certs", argv[1], (int)created, (int)updated,
           (int)attached, (int)finalized, signers);
    for (CFIndex i = 0; certs && i < CFArrayGetCount(certs); i++)
        print_string(" ", SecCertificateCopySubjectSummary((SecCertificateRef)CFArrayGetValueAtIndex(certs, i)));
    printf("\n");
    for (size_t i = 0; i < signers; i++) {
        SecPolicyRef policy = SecPolicyCreateBasicX509();
        CMSSignerStatus status = kCMSSignerUnsigned;
        SecTrustRef trust = NULL;
        OSStatus verify = 0, rc;

        rc = CMSDecoderCopySignerStatus(decoder, i, policy, true, &status, &trust, &verify);
        printf("%s signer %zu: rc %d, status %d, verify %d, with the test CA %ld\n", argv[1], i, (int)rc, (int)status,
               (int)verify, anchored(trust, roots));
        if (trust)
            CFRelease(trust);
        CFRelease(policy);
    }
    return 0;
}

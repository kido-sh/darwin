/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include "../apple/OSX/libsecurity_cssm/lib/cssmerr.h"

SecKeyRef SecCertificateCopyPublicKey_ios(SecCertificateRef certificate);
CFDataRef SecCertificateCopySerialNumber_ios(SecCertificateRef certificate);

SecKeyRef
SecCertificateCopyPublicKey_ios(SecCertificateRef certificate)
{
    return SecCertificateCopyKey(certificate);
}

CFDataRef
SecCertificateCopySerialNumber_ios(SecCertificateRef certificate)
{
    return SecCertificateCopySerialNumberData(certificate, NULL);
}

static bool
legacyKeyType(SecKeyRef key)
{
    CFDictionaryRef attributes = SecKeyCopyAttributes(key);
    CFTypeRef type = attributes ? CFDictionaryGetValue(attributes, kSecAttrKeyType) : NULL;
    bool known = type != NULL &&
        (CFEqual(type, kSecAttrKeyTypeRSA) || CFEqual(type, kSecAttrKeyTypeECSECPrimeRandom));

    if (attributes != NULL) {
        CFRelease(attributes);
    }
    return known;
}

OSStatus
SecCertificateCopyPublicKey(SecCertificateRef certificate, SecKeyRef *key)
{
    if (certificate == NULL) {
        return errSecInvalidItemRef;
    }
    if (key == NULL) {
        return CSSM_ERRCODE_INVALID_POINTER;
    }
    SecKeyRef copy = SecCertificateCopyKey(certificate);
    if (copy == NULL) {
        return errSecUnknownFormat;
    }
    if (!legacyKeyType(copy)) {
        CFRelease(copy);
        return errSecUnknownFormat;
    }
    *key = copy;
    return errSecSuccess;
}

CFDataRef
SecCertificateCopySerialNumber(SecCertificateRef certificate, CFErrorRef *error)
{
    if (certificate == NULL) {
        if (error != NULL) {
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificateRef, NULL);
        }
        return NULL;
    }
    return SecCertificateCopySerialNumberData(certificate, error);
}

// pd_security_macos_trust.c - the macOS keychain's side of trust evaluation
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// no keychains or stored trust settings, so these answer as an empty keychain and trust comes from the root store
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include "../apple/OSX/libsecurity_codesigning/lib/CSCommon.h"
#include <stdbool.h>

CFArrayRef SecItemCopyParentCertificates_osx(SecCertificateRef certificate, void *context)
{
    return NULL;
}

SecCertificateRef SecItemCopyStoredCertificate(SecCertificateRef certificate, void *context)
{
    return NULL;
}

// keychain change notifications invalidate trustd's caches, no keychain sends any
void SecTrustLegacySourcesListenForKeychainEvents(void)
{
}

// macOS 12's replacement for the index getters, which Go's crypto/x509 binds at launch
CFArrayRef SecTrustCopyCertificateChain(SecTrustRef trust)
{
    CFIndex count = SecTrustGetCertificateCount(trust);
    CFMutableArrayRef chain;

    if (count <= 0)
        return NULL;
    chain = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0; chain != NULL && i < count; i++) {
        SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust, i);
        if (cert != NULL)
            CFArrayAppendValue(chain, cert);
    }
    return chain;
}

OSStatus SecTrustSettingsCopyCertificatesForUserAdminDomains(CFArrayRef *certArray)
{
    if (certArray)
        *certArray = NULL;
    return errSecNoTrustSettings;
}

OSStatus SecTrustSettingsCopyTrustSettings_Cached(SecCertificateRef certRef, uint32_t domain, CFArrayRef *trustSettings)
{
    if (trustSettings)
        *trustSettings = NULL;
    return errSecItemNotFound;
}

bool SecTrustSettingsUserAdminDomainsContain(SecCertificateRef certRef)
{
    return false;
}

// code requirements are not evaluated here, so none is ever met, as SecStaticCodeCheckValidityWithErrors says
OSStatus SecTaskValidateForRequirement(void *task, CFStringRef requirement)
{
    return errSecCSReqFailed;
}

// PowerLog records energy events, which nothing collects here
void SecPLLogRegisteredEvent(CFStringRef eventName, CFDictionaryRef eventDictionary)
{
}

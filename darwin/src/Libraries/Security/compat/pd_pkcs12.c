// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// OpenSSL first: libDER, under Security's headers, defines ASN1_* macros that clash with OpenSSL's type names
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stdlib.h>
#include <string.h>

const CFStringRef kSecImportExportPassphrase = CFSTR("passphrase");
const CFStringRef kSecImportToMemoryOnly = CFSTR("memory");
const CFStringRef kSecImportItemLabel = CFSTR("label");
const CFStringRef kSecImportItemKeyID = CFSTR("keyid");
const CFStringRef kSecImportItemTrust = CFSTR("trust");
const CFStringRef kSecImportItemCertChain = CFSTR("chain");
const CFStringRef kSecImportItemIdentity = CFSTR("identity");

extern SecIdentityRef SecIdentityCreate(CFAllocatorRef allocator, SecCertificateRef certificate, SecKeyRef privateKey);

static SecCertificateRef createCertificate(X509 *x509)
{
    unsigned char *der = NULL;
    int length = i2d_X509(x509, &der);
    SecCertificateRef certificate = NULL;

    if (length > 0) {
        CFDataRef data = CFDataCreate(kCFAllocatorDefault, der, length);

        certificate = SecCertificateCreateWithData(kCFAllocatorDefault, data);
        CFRelease(data);
    }
    OPENSSL_free(der);
    return certificate;
}

// SecKey takes RSA keys as PKCS#1 and EC keys as 04 || X || Y || K, as SecKeyCopyExternalRepresentation gives them
static CFDataRef copyKeyData(EVP_PKEY *pkey, CFStringRef *keyType)
{
    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_RSA) {
        unsigned char *der = NULL;
        int length = i2d_PrivateKey(pkey, &der);
        CFDataRef data = length > 0 ? CFDataCreate(kCFAllocatorDefault, der, length) : NULL;

        OPENSSL_free(der);
        *keyType = kSecAttrKeyTypeRSA;
        return data;
    }
    if (EVP_PKEY_base_id(pkey) == EVP_PKEY_EC) {
        EC_KEY *ec = EVP_PKEY_get0_EC_KEY(pkey);
        const EC_GROUP *group = ec ? EC_KEY_get0_group(ec) : NULL;
        const EC_POINT *pub = ec ? EC_KEY_get0_public_key(ec) : NULL;
        const BIGNUM *secret = ec ? EC_KEY_get0_private_key(ec) : NULL;
        unsigned char point[133];
        size_t pointLength = 0;
        CFMutableDataRef data = NULL;
        int fieldBytes = group ? (EC_GROUP_get_degree(group) + 7) / 8 : 0;
        int secretBytes = secret ? BN_num_bytes(secret) : 0;

        if (pub)
            pointLength = EC_POINT_point2oct(group, pub, POINT_CONVERSION_UNCOMPRESSED, point, sizeof(point), NULL);
        if (pointLength > 0 && secret && point[0] == 0x04 && secretBytes <= fieldBytes) {
            data = CFDataCreateMutable(kCFAllocatorDefault, 0);
            CFDataAppendBytes(data, point, pointLength);
            CFDataIncreaseLength(data, fieldBytes);
            BN_bn2bin(secret, CFDataGetMutableBytePtr(data) + pointLength + fieldBytes - secretBytes);
        }
        *keyType = kSecAttrKeyTypeECSECPrimeRandom;
        return data;
    }
    return NULL;
}

static void appendCertificates(CFMutableArrayRef array, STACK_OF(X509) *certs)
{
    for (int i = 0; certs && i < sk_X509_num(certs); i++) {
        SecCertificateRef certificate = createCertificate(sk_X509_value(certs, i));

        if (certificate) {
            CFArrayAppendValue(array, certificate);
            CFRelease(certificate);
        }
    }
}

static SecKeyRef createPrivateKey(EVP_PKEY *pkey)
{
    CFStringRef keyType = NULL;
    CFDataRef data = copyKeyData(pkey, &keyType);
    CFMutableDictionaryRef attributes;
    SecKeyRef key;

    if (data == NULL)
        return NULL;
    attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attributes, kSecAttrKeyType, keyType);
    CFDictionarySetValue(attributes, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    key = SecKeyCreateWithData(data, attributes, NULL);
    CFRelease(attributes);
    CFRelease(data);
    return key;
}

OSStatus SecPKCS12Import(CFDataRef pkcs12_data, CFDictionaryRef options, CFArrayRef *items)
{
    CFStringRef passphrase = options ? CFDictionaryGetValue(options, kSecImportExportPassphrase) : NULL;
    const unsigned char *bytes;
    char *password = NULL;
    PKCS12 *p12 = NULL;
    EVP_PKEY *pkey = NULL;
    X509 *x509 = NULL;
    STACK_OF(X509) *ca = NULL;
    STACK_OF(PKCS7) *safes;
    OSStatus status = errSecDecode;

    if (pkcs12_data == NULL || items == NULL)
        return errSecParam;
    *items = NULL;
    if (passphrase && CFGetTypeID(passphrase) == CFStringGetTypeID()) {
        CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(passphrase), kCFStringEncodingUTF8) + 1;

        password = malloc(size);
        if (password == NULL || !CFStringGetCString(passphrase, password, size, kCFStringEncodingUTF8)) {
            free(password);
            return errSecParam;
        }
    }
    bytes = CFDataGetBytePtr(pkcs12_data);
    p12 = d2i_PKCS12(NULL, &bytes, CFDataGetLength(pkcs12_data));
    if (p12 == NULL || (safes = PKCS12_unpack_authsafes(p12)) == NULL)
        goto out;
    sk_PKCS7_pop_free(safes, PKCS7_free);
    if (!PKCS12_verify_mac(p12, password, -1)) {
        status = errSecAuthFailed;
        goto out;
    }
    if (!PKCS12_parse(p12, password, &pkey, &x509, &ca))
        goto out;

    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    SecCertificateRef certificate = x509 ? createCertificate(x509) : NULL;
    SecKeyRef key = pkey ? createPrivateKey(pkey) : NULL;

    if (certificate && key) {
        SecIdentityRef identity = SecIdentityCreate(kCFAllocatorDefault, certificate, key);
        CFMutableArrayRef chain = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFMutableDictionaryRef item = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

        SecPolicyRef policy = SecPolicyCreateBasicX509();
        SecTrustRef trust = NULL;

        CFArrayAppendValue(chain, certificate);
        appendCertificates(chain, ca);
        if (identity) {
            CFDictionarySetValue(item, kSecImportItemIdentity, identity);
            CFRelease(identity);
        }
        if (policy && SecTrustCreateWithCertificates(chain, policy, &trust) == errSecSuccess) {
            CFDictionarySetValue(item, kSecImportItemTrust, trust);
            CFRelease(trust);
        }
        if (policy)
            CFRelease(policy);
        CFDictionarySetValue(item, kSecImportItemCertChain, chain);
        CFArrayAppendValue(result, item);
        CFRelease(item);
        CFRelease(chain);
        status = identity ? errSecSuccess : errSecDecode;
    } else if (pkey == NULL && x509 == NULL && ca && sk_X509_num(ca) > 0) {
        CFMutableArrayRef certificates = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFMutableDictionaryRef item = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

        appendCertificates(certificates, ca);
        CFDictionarySetValue(item, kSecImportItemCertChain, certificates);
        CFArrayAppendValue(result, item);
        CFRelease(item);
        CFRelease(certificates);
        status = errSecSuccess;
    } else
        status = key == NULL && pkey != NULL ? errSecUnsupportedFormat : errSecDecode;
    if (key)
        CFRelease(key);
    if (certificate)
        CFRelease(certificate);
    if (status == errSecSuccess)
        *items = result;
    else
        CFRelease(result);
out:
    if (ca)
        sk_X509_pop_free(ca, X509_free);
    if (x509)
        X509_free(x509);
    if (pkey)
        EVP_PKEY_free(pkey);
    if (p12)
        PKCS12_free(p12);
    if (password) {
        OPENSSL_cleanse(password, strlen(password));
        free(password);
    }
    return status;
}

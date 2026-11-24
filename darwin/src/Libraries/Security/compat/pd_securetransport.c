// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// SecTrust judges peer certificates, and a caller that asked to break on peer authentication is suspended there

// OpenSSL first: libDER, under Security's headers, defines ASN1_* macros that clash with OpenSSL's type names
#include <openssl/dh.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <Security/Security.h>
#include "../apple/OSX/libsecurity_ssl/lib/SecureTransport.h"
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static const SSLCipherSuite defaultCiphers[] = {
    TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
    TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
    TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA, TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
    TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA, TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
    TLS_RSA_WITH_AES_256_GCM_SHA384, TLS_RSA_WITH_AES_128_GCM_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA256, TLS_RSA_WITH_AES_128_CBC_SHA256,
    TLS_RSA_WITH_AES_256_CBC_SHA, TLS_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_3DES_EDE_CBC_SHA,
};

struct SSLContext {
    CFRuntimeBase base;
    SSL_CTX *sslContext;
    SSL *ssl;
    SSLProtocolSide side;
    SSLReadFunc readFunc;
    SSLWriteFunc writeFunc;
    SSLConnectionRef connection;
    char *peerName;
    SSLSessionState state;
    SSLAuthenticate clientAuth;
    Boolean breakOnServerAuth;
    Boolean breakOnClientAuth;
    Boolean peerAuthReported;
    Boolean breakPending;
    Boolean writesHeld;
    Boolean trustFailed;
    OSStatus ioError;
    CFArrayRef peerCertificates;
};

static CFTypeID sslContextTypeID;
static BIO_METHOD *bioMethod;

static void contextFinalize(CFTypeRef cf)
{
    SSLContextRef context = (SSLContextRef)cf;

    if (context->ssl)
        SSL_free(context->ssl);
    if (context->sslContext)
        SSL_CTX_free(context->sslContext);
    if (context->peerCertificates)
        CFRelease(context->peerCertificates);
    free(context->peerName);
}

static const CFRuntimeClass contextClass = {
    0, "SSLContext", NULL, NULL, contextFinalize, NULL, NULL, NULL, NULL
};

// the transport is the caller's: a would-block with nothing moved is a retry, an error is kept for the status
static int bioWrite(BIO *bio, const char *data, int length)
{
    SSLContextRef context = BIO_get_data(bio);
    size_t moved = (size_t)length;
    OSStatus status;

    BIO_clear_retry_flags(bio);
    if (context->writesHeld) {
        BIO_set_retry_write(bio);
        return -1;
    }
    status = context->writeFunc(context->connection, data, &moved);
    if (status == noErr || (status == errSSLWouldBlock && moved > 0))
        return (int)moved;
    if (status == errSSLWouldBlock) {
        BIO_set_retry_write(bio);
        return -1;
    }
    context->ioError = status;
    return -1;
}

static int bioRead(BIO *bio, char *data, int length)
{
    SSLContextRef context = BIO_get_data(bio);
    size_t moved = (size_t)length;
    OSStatus status;

    BIO_clear_retry_flags(bio);
    status = context->readFunc(context->connection, data, &moved);
    if ((status == noErr || status == errSSLWouldBlock) && moved > 0)
        return (int)moved;
    if (status == errSSLWouldBlock) {
        BIO_set_retry_read(bio);
        return -1;
    }
    if (status == errSSLClosedGraceful || status == errSSLClosedNoNotify || status == noErr)
        return 0;
    context->ioError = status;
    return -1;
}

static long bioControl(BIO *bio, int command, long number, void *pointer)
{
    return command == BIO_CTRL_FLUSH ? 1 : 0;
}

static void setup(void)
{
    sslContextTypeID = _CFRuntimeRegisterClass(&contextClass);
    OPENSSL_init_ssl(0, NULL);
    bioMethod = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "SecureTransport");
    BIO_meth_set_write(bioMethod, bioWrite);
    BIO_meth_set_read(bioMethod, bioRead);
    BIO_meth_set_ctrl(bioMethod, bioControl);
}

static void ensureSetup(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    pthread_once(&once, setup);
}

CFTypeID SSLContextGetTypeID(void)
{
    ensureSetup();
    return sslContextTypeID;
}

// the certificates the peer sent, leaf first, as SecCertificates
static CFArrayRef copyPeerChain(X509_STORE_CTX *store)
{
    CFMutableArrayRef chain = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    STACK_OF(X509) *sent = X509_STORE_CTX_get0_untrusted(store);
    X509 *leaf = X509_STORE_CTX_get0_cert(store);
    int count = sent ? sk_X509_num(sent) : 0;

    for (int i = -1; i < count; i++) {
        X509 *x509 = i < 0 ? leaf : sk_X509_value(sent, i);
        unsigned char *der = NULL;
        int length;

        if (x509 == NULL || (i >= 0 && X509_cmp(x509, leaf) == 0))
            continue;
        length = i2d_X509(x509, &der);
        if (length > 0) {
            CFDataRef data = CFDataCreate(kCFAllocatorDefault, der, length);
            SecCertificateRef certificate = SecCertificateCreateWithData(kCFAllocatorDefault, data);

            if (certificate) {
                CFArrayAppendValue(chain, certificate);
                CFRelease(certificate);
            }
            CFRelease(data);
        }
        OPENSSL_free(der);
    }
    return chain;
}

static SecTrustRef createTrust(SSLContextRef context)
{
    CFStringRef host = context->peerName ?
        CFStringCreateWithCString(kCFAllocatorDefault, context->peerName, kCFStringEncodingUTF8) : NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(context->side == kSSLClientSide, host);
    SecTrustRef trust = NULL;

    if (context->peerCertificates && CFArrayGetCount(context->peerCertificates) > 0)
        SecTrustCreateWithCertificates(context->peerCertificates, policy, &trust);
    if (policy)
        CFRelease(policy);
    if (host)
        CFRelease(host);
    return trust;
}

// replaces OpenSSL's own chain building, trust is SecTrust's decision
static int verifyPeer(X509_STORE_CTX *store, void *argument)
{
    SSL *ssl = X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx());
    SSLContextRef context = SSL_get_app_data(ssl);
    Boolean breakHere = context->side == kSSLClientSide ? context->breakOnServerAuth : context->breakOnClientAuth;
    SecTrustRef trust;
    bool trusted;

    if (context->peerCertificates == NULL)
        context->peerCertificates = copyPeerChain(store);
    // the caller judges the peer between the two handshake calls
    if (breakHere) {
        if (!context->peerAuthReported)
            context->breakPending = context->writesHeld = true;
        return 1;
    }
    trust = createTrust(context);
    trusted = trust && SecTrustEvaluateWithError(trust, NULL);
    if (trust)
        CFRelease(trust);
    context->trustFailed = !trusted;
    return trusted ? 1 : 0;
}

static const struct {
    int alert;
    OSStatus status;
} alertStatuses[] = {
    { SSL_AD_UNEXPECTED_MESSAGE, errSSLPeerUnexpectedMsg }, { SSL_AD_BAD_RECORD_MAC, errSSLPeerBadRecordMac },
    { SSL_AD_DECRYPTION_FAILED, errSSLPeerDecryptionFail }, { SSL_AD_RECORD_OVERFLOW, errSSLPeerRecordOverflow },
    { SSL_AD_DECOMPRESSION_FAILURE, errSSLPeerDecompressFail }, { SSL_AD_HANDSHAKE_FAILURE, errSSLPeerHandshakeFail },
    { SSL_AD_BAD_CERTIFICATE, errSSLPeerBadCert }, { SSL_AD_UNSUPPORTED_CERTIFICATE, errSSLPeerUnsupportedCert },
    { SSL_AD_CERTIFICATE_REVOKED, errSSLPeerCertRevoked }, { SSL_AD_CERTIFICATE_EXPIRED, errSSLPeerCertExpired },
    { SSL_AD_CERTIFICATE_UNKNOWN, errSSLPeerCertUnknown }, { SSL_AD_ILLEGAL_PARAMETER, errSSLIllegalParam },
    { SSL_AD_UNKNOWN_CA, errSSLPeerUnknownCA }, { SSL_AD_ACCESS_DENIED, errSSLPeerAccessDenied },
    { SSL_AD_DECODE_ERROR, errSSLPeerDecodeError }, { SSL_AD_DECRYPT_ERROR, errSSLPeerDecryptError },
    { SSL_AD_EXPORT_RESTRICTION, errSSLPeerExportRestriction }, { SSL_AD_PROTOCOL_VERSION, errSSLPeerProtocolVersion },
    { SSL_AD_INSUFFICIENT_SECURITY, errSSLPeerInsufficientSecurity }, { SSL_AD_INTERNAL_ERROR, errSSLPeerInternalError },
    { SSL_AD_USER_CANCELLED, errSSLPeerUserCancelled }, { SSL_AD_NO_RENEGOTIATION, errSSLPeerNoRenegotiation },
};

static OSStatus failureStatus(SSLContextRef context)
{
    unsigned long error;
    int alert = -1;

    while ((error = ERR_get_error()) != 0)
        if (ERR_GET_LIB(error) == ERR_LIB_SSL && ERR_GET_REASON(error) > SSL_AD_REASON_OFFSET)
            alert = ERR_GET_REASON(error) - SSL_AD_REASON_OFFSET;
    if (context->trustFailed)
        return errSSLXCertChainInvalid;
    for (size_t i = 0; i < sizeof(alertStatuses) / sizeof(alertStatuses[0]); i++)
        if (alertStatuses[i].alert == alert)
            return alertStatuses[i].status;
    return errSSLProtocol;
}

static OSStatus statusFor(SSLContextRef context, int result)
{
    switch (SSL_get_error(context->ssl, result)) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        return errSSLWouldBlock;
    case SSL_ERROR_ZERO_RETURN:
        context->state = kSSLClosed;
        return errSSLClosedGraceful;
    case SSL_ERROR_SYSCALL:
        context->state = kSSLAborted;
        return context->ioError ? context->ioError : errSSLClosedNoNotify;
    default:
        context->state = kSSLAborted;
        return failureStatus(context);
    }
}

SSLContextRef SSLCreateContext(CFAllocatorRef alloc, SSLProtocolSide protocolSide, SSLConnectionType connectionType)
{
    SSLContextRef context;
    BIO *bio;

    // datagram TLS is not offered
    if (connectionType != kSSLStreamType)
        return NULL;
    ensureSetup();
    context = (SSLContextRef)_CFRuntimeCreateInstance(alloc, sslContextTypeID,
        sizeof(struct SSLContext) - sizeof(CFRuntimeBase), NULL);
    if (context == NULL)
        return NULL;
    context->side = protocolSide;
    context->state = kSSLIdle;
    context->sslContext = SSL_CTX_new(protocolSide == kSSLClientSide ? TLS_client_method() : TLS_server_method());
    context->ssl = context->sslContext ? SSL_new(context->sslContext) : NULL;
    bio = BIO_new(bioMethod);
    if (context->ssl == NULL || bio == NULL) {
        if (bio)
            BIO_free(bio);
        CFRelease(context);
        return NULL;
    }
    BIO_set_data(bio, context);
    BIO_set_init(bio, 1);
    SSL_set_bio(context->ssl, bio, bio);
    SSL_set_app_data(context->ssl, context);
    SSL_set_mode(context->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_CTX_set_cert_verify_callback(context->sslContext, verifyPeer, NULL);
    if (protocolSide == kSSLClientSide) {
        SSL_set_verify(context->ssl, SSL_VERIFY_PEER, NULL);
        SSL_set_connect_state(context->ssl);
    } else
        SSL_set_accept_state(context->ssl);
    SSL_set_min_proto_version(context->ssl, TLS1_VERSION);
    SSL_set_max_proto_version(context->ssl, TLS1_2_VERSION);
    if (SSLSetEnabledCiphers(context, defaultCiphers, sizeof(defaultCiphers) / sizeof(defaultCiphers[0])) != noErr) {
        CFRelease(context);
        return NULL;
    }
    return context;
}

OSStatus SSLSetIOFuncs(SSLContextRef context, SSLReadFunc readFunc, SSLWriteFunc writeFunc)
{
    if (context == NULL || context->state != kSSLIdle)
        return errSecParam;
    context->readFunc = readFunc;
    context->writeFunc = writeFunc;
    return noErr;
}

OSStatus SSLSetConnection(SSLContextRef context, SSLConnectionRef connection)
{
    if (context == NULL)
        return errSecParam;
    context->connection = connection;
    return noErr;
}

OSStatus SSLSetPeerDomainName(SSLContextRef context, const char *peerName, size_t peerNameLen)
{
    if (context == NULL || context->state != kSSLIdle)
        return errSecParam;
    free(context->peerName);
    context->peerName = peerName ? strndup(peerName, peerNameLen) : NULL;
    // the name also goes out as SNI, which servers hosting several sites need
    if (context->side == kSSLClientSide && context->peerName)
        SSL_set_tlsext_host_name(context->ssl, context->peerName);
    return noErr;
}

OSStatus SSLSetSessionOption(SSLContextRef context, SSLSessionOption option, Boolean value)
{
    if (context == NULL)
        return errSecParam;
    if (option == kSSLSessionOptionBreakOnServerAuth)
        context->breakOnServerAuth = value;
    else if (option == kSSLSessionOptionBreakOnClientAuth)
        context->breakOnClientAuth = value;
    return noErr;
}

OSStatus SSLGetSessionState(SSLContextRef context, SSLSessionState *state)
{
    if (context == NULL || state == NULL)
        return errSecParam;
    *state = context->state;
    return noErr;
}

static int opensslVersion(SSLProtocol protocol)
{
    switch (protocol) {
    case kSSLProtocol3:
        return SSL3_VERSION;
    case kTLSProtocol1:
        return TLS1_VERSION;
    case kTLSProtocol11:
        return TLS1_1_VERSION;
    case kTLSProtocol12:
    case kTLSProtocolMaxSupported:
        return TLS1_2_VERSION;
    default:
        return -1;
    }
}

OSStatus SSLSetProtocolVersionMin(SSLContextRef context, SSLProtocol minVersion)
{
    int version = opensslVersion(minVersion);

    if (context == NULL)
        return errSecParam;
    if (version < 0)
        return errSSLIllegalParam;
    return SSL_set_min_proto_version(context->ssl, version) ? noErr : errSecParam;
}

OSStatus SSLSetProtocolVersionMax(SSLContextRef context, SSLProtocol maxVersion)
{
    int version = opensslVersion(maxVersion);

    if (context == NULL)
        return errSecParam;
    if (version < 0)
        return errSSLIllegalParam;
    return SSL_set_max_proto_version(context->ssl, version) ? noErr : errSecParam;
}

OSStatus SSLGetNegotiatedProtocolVersion(SSLContextRef context, SSLProtocol *protocol)
{
    if (context == NULL || protocol == NULL)
        return errSecParam;
    switch (context->state == kSSLConnected ? SSL_version(context->ssl) : 0) {
    case SSL3_VERSION:
        *protocol = kSSLProtocol3;
        break;
    case TLS1_VERSION:
        *protocol = kTLSProtocol1;
        break;
    case TLS1_1_VERSION:
        *protocol = kTLSProtocol11;
        break;
    case TLS1_2_VERSION:
        *protocol = kTLSProtocol12;
        break;
    default:
        *protocol = kSSLProtocolUnknown;
        break;
    }
    return noErr;
}

OSStatus SSLGetNumberSupportedCiphers(SSLContextRef context, size_t *numCiphers)
{
    STACK_OF(SSL_CIPHER) *ciphers = context ? SSL_get_ciphers(context->ssl) : NULL;

    if (context == NULL || numCiphers == NULL)
        return errSecParam;
    *numCiphers = ciphers ? (size_t)sk_SSL_CIPHER_num(ciphers) : 0;
    return noErr;
}

OSStatus SSLGetSupportedCiphers(SSLContextRef context, SSLCipherSuite *ciphers, size_t *numCiphers)
{
    STACK_OF(SSL_CIPHER) *available = context ? SSL_get_ciphers(context->ssl) : NULL;
    size_t count = available ? (size_t)sk_SSL_CIPHER_num(available) : 0;

    if (context == NULL || ciphers == NULL || numCiphers == NULL)
        return errSecParam;
    if (*numCiphers < count)
        return errSSLBufferOverflow;
    for (size_t i = 0; i < count; i++)
        ciphers[i] = SSL_CIPHER_get_value(sk_SSL_CIPHER_value(available, (int)i));
    *numCiphers = count;
    return noErr;
}

OSStatus SSLSetEnabledCiphers(SSLContextRef context, const SSLCipherSuite *ciphers, size_t numCiphers)
{
    size_t size = numCiphers * 64 + 1;
    char *list = calloc(1, size);
    OSStatus status = noErr;

    if (context == NULL || (ciphers == NULL && numCiphers > 0) || list == NULL) {
        free(list);
        return errSecParam;
    }
    for (size_t i = 0; i < numCiphers; i++) {
        const SSL_CIPHER *cipher = SSL_CIPHER_get_by_value(ciphers[i]);

        if (cipher == NULL)
            continue;
        if (*list)
            strlcat(list, ":", size);
        strlcat(list, SSL_CIPHER_get_name(cipher), size);
    }
    if (*list && !SSL_set_cipher_list(context->ssl, list))
        status = errSSLBadCipherSuite;
    free(list);
    return status;
}

OSStatus SSLGetNegotiatedCipher(SSLContextRef context, SSLCipherSuite *cipherSuite)
{
    const SSL_CIPHER *cipher = context ? SSL_get_current_cipher(context->ssl) : NULL;

    if (context == NULL || cipherSuite == NULL)
        return errSecParam;
    if (cipher == NULL)
        return errSSLSessionNotFound;
    *cipherSuite = SSL_CIPHER_get_value(cipher);
    return noErr;
}

OSStatus SSLSetALPNProtocols(SSLContextRef context, CFArrayRef protocols)
{
    unsigned char wire[1024];
    size_t length = 0;
    CFIndex count = protocols ? CFArrayGetCount(protocols) : 0;

    if (context == NULL)
        return errSecParam;
    for (CFIndex i = 0; i < count; i++) {
        CFStringRef protocol = CFArrayGetValueAtIndex(protocols, i);
        char name[256];
        size_t nameLength;

        if (!CFStringGetCString(protocol, name, sizeof(name), kCFStringEncodingUTF8))
            return errSecParam;
        nameLength = strlen(name);
        if (nameLength == 0 || nameLength > 255 || length + 1 + nameLength > sizeof(wire))
            return errSecParam;
        wire[length++] = (unsigned char)nameLength;
        memcpy(wire + length, name, nameLength);
        length += nameLength;
    }
    return SSL_set_alpn_protos(context->ssl, wire, (unsigned int)length) == 0 ? noErr : errSecParam;
}

OSStatus SSLCopyALPNProtocols(SSLContextRef context, CFArrayRef *protocols)
{
    const unsigned char *selected = NULL;
    unsigned int length = 0;

    if (context == NULL || protocols == NULL)
        return errSecParam;
    *protocols = NULL;
    SSL_get0_alpn_selected(context->ssl, &selected, &length);
    if (selected && length) {
        CFStringRef protocol = CFStringCreateWithBytes(kCFAllocatorDefault, selected, length, kCFStringEncodingUTF8, false);

        if (protocol) {
            *protocols = CFArrayCreate(kCFAllocatorDefault, (const void **)&protocol, 1, &kCFTypeArrayCallBacks);
            CFRelease(protocol);
        }
    }
    return noErr;
}

static X509 *copyX509(SecCertificateRef certificate)
{
    CFDataRef data = SecCertificateCopyData(certificate);
    const unsigned char *bytes;
    X509 *x509;

    if (data == NULL)
        return NULL;
    bytes = CFDataGetBytePtr(data);
    x509 = d2i_X509(NULL, &bytes, CFDataGetLength(data));
    CFRelease(data);
    return x509;
}

OSStatus SSLSetCertificate(SSLContextRef context, CFArrayRef certRefs)
{
    SecIdentityRef identity;
    SecCertificateRef certificate = NULL;
    SecKeyRef key = NULL;
    CFDataRef keyData = NULL;
    X509 *x509 = NULL;
    EVP_PKEY *pkey = NULL;
    OSStatus status = errSSLBadCert;

    if (context == NULL)
        return errSecParam;
    if (certRefs == NULL || CFArrayGetCount(certRefs) == 0)
        return noErr;
    identity = (SecIdentityRef)CFArrayGetValueAtIndex(certRefs, 0);
    if (CFGetTypeID(identity) != SecIdentityGetTypeID() ||
        SecIdentityCopyCertificate(identity, &certificate) != errSecSuccess ||
        SecIdentityCopyPrivateKey(identity, &key) != errSecSuccess)
        goto out;
    keyData = SecKeyCopyExternalRepresentation(key, NULL);
    x509 = copyX509(certificate);
    if (keyData && x509) {
        const unsigned char *bytes = CFDataGetBytePtr(keyData);

        pkey = d2i_PrivateKey(EVP_PKEY_RSA, NULL, &bytes, CFDataGetLength(keyData));
    }
    if (pkey && SSL_use_certificate(context->ssl, x509) == 1 && SSL_use_PrivateKey(context->ssl, pkey) == 1) {
        status = noErr;
        for (CFIndex i = 1; i < CFArrayGetCount(certRefs); i++) {
            X509 *intermediate = copyX509((SecCertificateRef)CFArrayGetValueAtIndex(certRefs, i));

            if (intermediate) {
                SSL_add1_chain_cert(context->ssl, intermediate);
                X509_free(intermediate);
            }
        }
    }
out:
    if (pkey)
        EVP_PKEY_free(pkey);
    if (x509)
        X509_free(x509);
    if (keyData)
        CFRelease(keyData);
    if (key)
        CFRelease(key);
    if (certificate)
        CFRelease(certificate);
    return status;
}

OSStatus SSLSetClientSideAuthenticate(SSLContextRef context, SSLAuthenticate auth)
{
    if (context == NULL || context->side != kSSLServerSide)
        return errSecParam;
    context->clientAuth = auth;
    switch (auth) {
    case kAlwaysAuthenticate:
        SSL_set_verify(context->ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
        break;
    case kTryAuthenticate:
        SSL_set_verify(context->ssl, SSL_VERIFY_PEER, NULL);
        break;
    default:
        SSL_set_verify(context->ssl, SSL_VERIFY_NONE, NULL);
        break;
    }
    return noErr;
}

OSStatus SSLSetDiffieHellmanParams(SSLContextRef context, const void *dhParams, size_t dhParamsLen)
{
    const unsigned char *bytes = dhParams;
    DH *parameters;

    if (context == NULL || context->side != kSSLServerSide)
        return errSecParam;
    parameters = dhParams ? d2i_DHparams(NULL, &bytes, (long)dhParamsLen) : NULL;
    if (parameters && SSL_set_tmp_dh(context->ssl, parameters)) {
        DH_free(parameters);
        return noErr;
    }
    DH_free(parameters);
    SSL_set_dh_auto(context->ssl, 1);
    return noErr;
}

OSStatus SSLHandshake(SSLContextRef context)
{
    int result;

    if (context == NULL || context->readFunc == NULL || context->writeFunc == NULL)
        return errSecParam;
    if (context->state == kSSLConnected)
        return noErr;
    if (context->state == kSSLClosed || context->state == kSSLAborted)
        return errSSLClosedAbort;
    context->state = kSSLHandshake;
    context->ioError = noErr;
    if (context->peerAuthReported)
        context->writesHeld = false;
    result = SSL_do_handshake(context->ssl);
    if (context->breakPending && (result == 1 || SSL_get_error(context->ssl, result) == SSL_ERROR_WANT_WRITE)) {
        context->breakPending = false;
        context->peerAuthReported = true;
        if (result == 1)
            context->state = kSSLConnected;
        return errSSLPeerAuthCompleted;
    }
    if (result == 1) {
        context->state = kSSLConnected;
        return noErr;
    }
    return statusFor(context, result);
}

OSStatus SSLCopyPeerTrust(SSLContextRef context, SecTrustRef *trust)
{
    if (context == NULL || trust == NULL)
        return errSecParam;
    *trust = createTrust(context);
    return noErr;
}

OSStatus SSLRead(SSLContextRef context, void *data, size_t dataLength, size_t *processed)
{
    int result;

    if (processed)
        *processed = 0;
    if (context == NULL || processed == NULL || (data == NULL && dataLength > 0))
        return errSecParam;
    if (context->state != kSSLConnected)
        return context->state == kSSLClosed ? errSSLClosedGraceful : errSSLClosedAbort;
    if (dataLength == 0)
        return noErr;
    context->ioError = noErr;
    result = SSL_read(context->ssl, data, dataLength > INT_MAX ? INT_MAX : (int)dataLength);
    if (result > 0) {
        *processed = (size_t)result;
        return noErr;
    }
    return statusFor(context, result);
}

OSStatus SSLWrite(SSLContextRef context, const void *data, size_t dataLength, size_t *processed)
{
    int result;

    if (processed)
        *processed = 0;
    if (context == NULL || processed == NULL || (data == NULL && dataLength > 0))
        return errSecParam;
    if (context->state != kSSLConnected)
        return errSSLClosedAbort;
    if (dataLength == 0)
        return noErr;
    context->ioError = noErr;
    result = SSL_write(context->ssl, data, dataLength > INT_MAX ? INT_MAX : (int)dataLength);
    if (result > 0) {
        *processed = (size_t)result;
        return noErr;
    }
    return statusFor(context, result);
}

// the close alert is sent if the transport takes it, closing never waits for the peer's
OSStatus SSLClose(SSLContextRef context)
{
    if (context == NULL)
        return errSecParam;
    if (context->state == kSSLConnected) {
        context->ioError = noErr;
        SSL_shutdown(context->ssl);
    }
    context->state = kSSLClosed;
    return noErr;
}

// keychains are macOS's file-based stores, which this system does not keep.
// SecPKCS12Import works without one and returns the identities directly
const CFStringRef kSecImportExportKeychain = CFSTR("keychain");

OSStatus SecKeychainCreate(const char *pathName, UInt32 passwordLength, const void *password, Boolean promptUser,
    SecAccessRef initialAccess, SecKeychainRef *keychain)
{
    if (keychain)
        *keychain = NULL;
    return errSecUnimplemented;
}

OSStatus SecKeychainDelete(SecKeychainRef keychainOrArray)
{
    return errSecUnimplemented;
}

OSStatus SecKeychainSetSettings(SecKeychainRef keychain, const SecKeychainSettings *newSettings)
{
    return errSecUnimplemented;
}

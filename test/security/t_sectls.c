/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Security/SecureTransport.h>
#include <Security/Security.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static OSStatus
sock_read(SSLConnectionRef conn, void *data, size_t *len)
{
    int fd = *(const int *)conn;
    size_t got = 0;

    while (got < *len) {
        ssize_t n = read(fd, (char *)data + got, *len - got);
        if (n > 0) {
            got += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        *len = got;
        return n == 0 ? errSSLClosedGraceful : errSSLClosedAbort;
    }
    return noErr;
}

static OSStatus
sock_write(SSLConnectionRef conn, const void *data, size_t *len)
{
    int fd = *(const int *)conn;
    size_t put = 0;

    while (put < *len) {
        ssize_t n = write(fd, (const char *)data + put, *len - put);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            *len = put;
            return errSSLClosedAbort;
        }
        put += (size_t)n;
    }
    return noErr;
}

static int
tcp(int port)
{
    struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons((uint16_t)port) };
    struct timeval tv = { .tv_sec = 20 };
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static CFArrayRef
anchors(const char *path)
{
    FILE *f = fopen(path, "rb");
    UInt8 buf[16384];
    size_t n;
    CFDataRef der;
    SecCertificateRef cert;
    CFArrayRef array;

    if (f == NULL)
        return NULL;
    n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    der = CFDataCreate(NULL, buf, (CFIndex)n);
    cert = SecCertificateCreateWithData(NULL, der);
    CFRelease(der);
    if (cert == NULL)
        return NULL;
    array = CFArrayCreate(NULL, (const void **)&cert, 1, &kCFTypeArrayCallBacks);
    CFRelease(cert);
    return array;
}

static long
evaluate(SSLContextRef ctx, CFArrayRef roots)
{
    SecTrustRef trust = NULL;
    CFErrorRef err = NULL;
    long code;

    if (SSLCopyPeerTrust(ctx, &trust) != noErr || trust == NULL)
        return 1;
    SecTrustSetAnchorCertificates(trust, roots);
    SecTrustSetAnchorCertificatesOnly(trust, true);
    code = SecTrustEvaluateWithError(trust, &err) ? 0 : (err ? CFErrorGetCode(err) : 2);
    if (err)
        CFRelease(err);
    CFRelease(trust);
    return code;
}

int
main(int argc, char **argv)
{
    const char *mode = argc > 5 ? argv[5] : "anchors";
    int anchored = strcmp(mode, "system") != 0;
    CFArrayRef roots;
    SSLContextRef ctx;
    SSLProtocol version = kSSLProtocolUnknown;
    SSLCipherSuite cipher = 0;
    OSStatus hs, setmax = noErr;
    long trust = -1;
    char body[256] = "";
    size_t got = 0;
    int fd;

    if (argc < 5) {
        fprintf(stderr, "usage: t_sectls NAME CA_DER PORT HOST [anchors|system|tls12|tls13]\n");
        return 2;
    }
    alarm(120);
    if ((roots = anchors(argv[2])) == NULL || (fd = tcp(atoi(argv[3]))) < 0) {
        printf("%s: no anchors or no connection\n", argv[1]);
        return 1;
    }
    ctx = SSLCreateContext(NULL, kSSLClientSide, kSSLStreamType);
    SSLSetIOFuncs(ctx, sock_read, sock_write);
    SSLSetConnection(ctx, &fd);
    SSLSetPeerDomainName(ctx, argv[4], strlen(argv[4]));
    if (anchored)
        SSLSetSessionOption(ctx, kSSLSessionOptionBreakOnServerAuth, true);
    if (strcmp(mode, "tls12") == 0)
        setmax = SSLSetProtocolVersionMax(ctx, kTLSProtocol12);
    else if (strcmp(mode, "tls13") == 0)
        setmax = SSLSetProtocolVersionMax(ctx, kTLSProtocol13);
    while ((hs = SSLHandshake(ctx)) == errSSLWouldBlock || hs == errSSLPeerAuthCompleted) {
        if (hs == errSSLPeerAuthCompleted && (trust = evaluate(ctx, roots)) != 0)
            break;
    }
    if (hs == noErr) {
        static const char request[] = "GET /ok.txt HTTP/1.0\r\n\r\n";
        size_t n = 0;
        char *last;

        SSLGetNegotiatedProtocolVersion(ctx, &version);
        SSLGetNegotiatedCipher(ctx, &cipher);
        SSLWrite(ctx, request, sizeof(request) - 1, &n);
        while (got < sizeof(body) - 1 && SSLRead(ctx, body + got, sizeof(body) - 1 - got, &n) == noErr && n > 0)
            got += n;
        body[got] = '\0';
        while (got > 0 && (body[got - 1] == '\n' || body[got - 1] == '\r'))
            body[--got] = '\0';
        last = strrchr(body, '\n');
        memmove(body, last ? last + 1 : body, strlen(last ? last + 1 : body) + 1);
    }
    printf("%s: set max %d, handshake %d, trust %ld, protocol %d, cipher 0x%04x, body \"%s\"\n", argv[1], (int)setmax,
           (int)hs, trust, (int)version, (unsigned)cipher, body);
    SSLClose(ctx);
    CFRelease(ctx);
    close(fd);
    CFRelease(roots);
    return 0;
}

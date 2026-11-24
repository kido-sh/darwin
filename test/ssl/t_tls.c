/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "../libsystem/check.h"

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
typedef struct ssl_cipher_st SSL_CIPHER;
typedef struct x509_store_ctx_st X509_STORE_CTX;
struct tls;
struct tls_config;

const SSL_METHOD *TLS_client_method(void);
SSL_CTX *SSL_CTX_new(const SSL_METHOD *);
void SSL_CTX_free(SSL_CTX *);
int SSL_CTX_load_verify_locations(SSL_CTX *, const char *, const char *);
int SSL_CTX_set_max_proto_version(SSL_CTX *, uint16_t);
SSL *SSL_new(SSL_CTX *);
int SSL_set_fd(SSL *, int);
long SSL_ctrl(SSL *, int, long, void *);
int SSL_set1_host(SSL *, const char *);
void SSL_set_verify(SSL *, int, int (*)(int, X509_STORE_CTX *));
int SSL_connect(SSL *);
long SSL_get_verify_result(const SSL *);
const char *SSL_get_version(const SSL *);
const SSL_CIPHER *SSL_get_current_cipher(const SSL *);
const char *SSL_CIPHER_get_name(const SSL_CIPHER *);
int SSL_shutdown(SSL *);
void SSL_free(SSL *);

int tls_init(void);
const char *tls_default_ca_cert_file(void);
struct tls_config *tls_config_new(void);
int tls_config_set_ca_file(struct tls_config *, const char *);
void tls_config_free(struct tls_config *);
struct tls *tls_client(void);
int tls_configure(struct tls *, struct tls_config *);
int tls_connect_servername(struct tls *, const char *, const char *, const char *);
int tls_handshake(struct tls *);
const char *tls_conn_version(struct tls *);
const char *tls_error(struct tls *);
int tls_close(struct tls *);
void tls_free(struct tls *);

enum {
    SSL_VERIFY_PEER = 1,
    SSL_CTRL_SET_TLSEXT_HOSTNAME = 55,
    TLSEXT_NAMETYPE_host_name = 0,
    TLS1_2_VERSION = 0x0303,
    X509_V_ERR_CERT_HAS_EXPIRED = 10,
    X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY = 20,
    X509_V_ERR_HOSTNAME_MISMATCH = 62,
};

struct result {
    int connected;
    long verify;
    char version[16];
    char cipher[64];
};

static const char *dir;
static int good_port, expired_port;

static int
tcp(int port)
{
    struct sockaddr_in sin = { .sin_family = AF_INET, .sin_port = htons((uint16_t)port) };
    struct timeval tv = { .tv_sec = 10 };
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static const char *
path(const char *name)
{
    static char buf[4][512];
    static int i;

    i = (i + 1) % 4;
    snprintf(buf[i], sizeof(buf[i]), "%s/%s", dir, name);
    return buf[i];
}

static int
accept_all(int ok, X509_STORE_CTX *store)
{
    (void)ok;
    (void)store;
    return 1;
}

static struct result
handshake(const char *ca, int port, uint16_t max, const char *host, int accept_everything)
{
    struct result r = { 0 };
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    SSL *ssl;
    int fd;

    r.verify = -1;
    if (ctx == NULL || SSL_CTX_load_verify_locations(ctx, path(ca), NULL) != 1 ||
        (max && SSL_CTX_set_max_proto_version(ctx, max) != 1) || (fd = tcp(port)) < 0) {
        SSL_CTX_free(ctx);
        return r;
    }
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    SSL_ctrl(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME, TLSEXT_NAMETYPE_host_name, "localhost");
    SSL_set1_host(ssl, host);
    SSL_set_verify(ssl, SSL_VERIFY_PEER, accept_everything ? accept_all : NULL);
    r.connected = SSL_connect(ssl) == 1;
    r.verify = SSL_get_verify_result(ssl);
    if (r.connected) {
        snprintf(r.version, sizeof(r.version), "%s", SSL_get_version(ssl));
        snprintf(r.cipher, sizeof(r.cipher), "%s", SSL_CIPHER_get_name(SSL_get_current_cipher(ssl)));
        SSL_shutdown(ssl);
    }
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(fd);
    return r;
}

static int
libtls(const char *ca, int port, const char *want_version, const char *want_error)
{
    struct tls_config *cfg = tls_config_new();
    struct tls *t = tls_client();
    char portstr[8];
    int hs, ok;

    snprintf(portstr, sizeof(portstr), "%d", port);
    if (cfg == NULL || t == NULL || tls_config_set_ca_file(cfg, path(ca)) != 0 || tls_configure(t, cfg) != 0 ||
        tls_connect_servername(t, "127.0.0.1", portstr, "localhost") != 0)
        return 0;
    hs = tls_handshake(t);
    if (want_version)
        ok = hs == 0 && strcmp(tls_conn_version(t), want_version) == 0;
    else
        ok = hs == -1 && tls_error(t) != NULL && strstr(tls_error(t), want_error) != NULL;
    if (!ok)
        printf("libtls %d: %d %s\n", port, hs, tls_error(t) ? tls_error(t) : "");
    tls_close(t);
    tls_free(t);
    tls_config_free(cfg);
    return ok;
}

static int
many_sigalgs(int port)
{
    unsigned char hello[512], reply[6], *p = hello + 9, *ext;
    size_t got = 0;
    ssize_t n;
    int fd, i;

    *p++ = 3;
    *p++ = 3;
    for (i = 0; i < 32; i++)
        *p++ = (unsigned char)i;
    *p++ = 0;
    *p++ = 0, *p++ = 4, *p++ = 0xc0, *p++ = 0x2b, *p++ = 0xc0, *p++ = 0x2c;
    *p++ = 1, *p++ = 0;
    ext = p;
    p += 2;
    *p++ = 0, *p++ = 0x0a, *p++ = 0, *p++ = 4, *p++ = 0, *p++ = 2, *p++ = 0, *p++ = 0x17;
    *p++ = 0, *p++ = 0x0b, *p++ = 0, *p++ = 2, *p++ = 1, *p++ = 0;
    *p++ = 0, *p++ = 0x0d, *p++ = 0, *p++ = 82, *p++ = 0, *p++ = 80;
    for (i = 0; i < 39; i++)
        *p++ = 0xfe, *p++ = (unsigned char)i;
    *p++ = 0x04, *p++ = 0x03;
    ext[0] = (unsigned char)((p - ext - 2) >> 8);
    ext[1] = (unsigned char)(p - ext - 2);
    hello[0] = 0x16, hello[1] = 3, hello[2] = 1;
    hello[3] = (unsigned char)((p - hello - 5) >> 8);
    hello[4] = (unsigned char)(p - hello - 5);
    hello[5] = 1, hello[6] = 0;
    hello[7] = (unsigned char)((p - hello - 9) >> 8);
    hello[8] = (unsigned char)(p - hello - 9);
    if ((fd = tcp(port)) < 0)
        return 0;
    if (write(fd, hello, (size_t)(p - hello)) != p - hello) {
        close(fd);
        return 0;
    }
    while (got < sizeof(reply) && (n = read(fd, reply + got, sizeof(reply) - got)) > 0)
        got += (size_t)n;
    close(fd);
    if (got == sizeof(reply) && reply[0] != 0x16)
        printf("40 signature algorithms: record type %u, alert %u\n", reply[0], reply[5]);
    return got == sizeof(reply) && reply[0] == 0x16 && reply[5] == 2;
}

int
main(int argc, char **argv)
{
    struct result r;

    if (argc != 4) {
        fprintf(stderr, "usage: t_tls PKI_DIR GOOD_PORT EXPIRED_PORT\n");
        return 2;
    }
    dir = argv[1];
    good_port = atoi(argv[2]);
    expired_port = atoi(argv[3]);
    setvbuf(stdout, NULL, _IOLBF, 0);
    alarm(300);

    r = handshake("ca.pem", good_port, 0, "localhost", 0);
    CHECK(r.connected && r.verify == 0 && strcmp(r.version, "TLSv1.3") == 0);
    r = handshake("ca.pem", good_port, TLS1_2_VERSION, "localhost", 0);
    CHECK(r.connected && r.verify == 0 && strcmp(r.version, "TLSv1.2") == 0 &&
          strncmp(r.cipher, "ECDHE-ECDSA-", 12) == 0);
    r = handshake("ca.pem", good_port, 0, "wrong.example", 0);
    CHECK(!r.connected && r.verify == X509_V_ERR_HOSTNAME_MISMATCH);
    r = handshake("ca.pem", good_port, 0, "wrong.example", 1);
    CHECK(r.connected && r.verify == X509_V_ERR_HOSTNAME_MISMATCH);
    r = handshake("other.pem", good_port, 0, "localhost", 0);
    CHECK(!r.connected && r.verify == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY);
    r = handshake("ca.pem", expired_port, 0, "localhost", 0);
    CHECK(!r.connected && r.verify == X509_V_ERR_CERT_HAS_EXPIRED);

    CHECK(tls_init() == 0);
    CHECK(strcmp(tls_default_ca_cert_file(), "/private/etc/ssl/cert.pem") == 0);
    CHECK(libtls("ca.pem", good_port, "TLSv1.3", NULL));
    CHECK(libtls("ca.pem", expired_port, NULL, "certificate has expired"));
    CHECK(libtls("other.pem", good_port, NULL, "unable to get local issuer certificate"));

    CHECK(many_sigalgs(good_port));
    return checks_done("libssl and libtls");
}

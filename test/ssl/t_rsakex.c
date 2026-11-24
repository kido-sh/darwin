/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../libsystem/check.h"

typedef struct rsa_st RSA;
typedef struct rsa_meth_st RSA_METHOD;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct x509_st X509;
typedef struct bio_st BIO;
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
typedef int (*rsa_crypt)(int, const unsigned char *, unsigned char *, RSA *, int);

const RSA_METHOD *RSA_PKCS1_OpenSSL(void);
RSA_METHOD *RSA_meth_dup(const RSA_METHOD *);
int RSA_meth_set_priv_dec(RSA_METHOD *, rsa_crypt);
int RSA_meth_set_pub_enc(RSA_METHOD *, rsa_crypt);
rsa_crypt RSA_meth_get_priv_dec(const RSA_METHOD *);
rsa_crypt RSA_meth_get_pub_enc(const RSA_METHOD *);
int RSA_set_method(RSA *, const RSA_METHOD *);
void RSA_set_default_method(const RSA_METHOD *);
int RSA_size(const RSA *);
RSA *EVP_PKEY_get0_RSA(EVP_PKEY *);
EVP_PKEY *PEM_read_PrivateKey(FILE *, EVP_PKEY **, void *, void *);
X509 *PEM_read_X509(FILE *, X509 **, void *, void *);
int BIO_new_bio_pair(BIO **, size_t, BIO **, size_t);
unsigned long ERR_get_error(void);
const SSL_METHOD *TLS_server_method(void);
const SSL_METHOD *TLS_client_method(void);
SSL_CTX *SSL_CTX_new(const SSL_METHOD *);
void SSL_CTX_free(SSL_CTX *);
int SSL_CTX_use_certificate(SSL_CTX *, X509 *);
int SSL_CTX_use_PrivateKey(SSL_CTX *, EVP_PKEY *);
int SSL_CTX_set_cipher_list(SSL_CTX *, const char *);
int SSL_CTX_set_max_proto_version(SSL_CTX *, uint16_t);
SSL *SSL_new(SSL_CTX *);
void SSL_free(SSL *);
void SSL_set_bio(SSL *, BIO *, BIO *);
void SSL_set_connect_state(SSL *);
void SSL_set_accept_state(SSL *);
int SSL_do_handshake(SSL *);
int SSL_get_error(const SSL *, int);

enum {
    RSA_PKCS1_PADDING = 1,
    RSA_NO_PADDING = 3,
    SSL_ERROR_WANT_READ = 2,
    SSL_ERROR_WANT_WRITE = 3,
    ERR_LIB_SSL = 20,
    SSL_AD_REASON_OFFSET = 1000,
    TLS1_2_VERSION = 0x0303,
    PMS_LEN = 48,
    ROUNDS = 200,
};

enum mode { HONEST, WRONG_SECRET, BAD_PADDING, BAD_VERSION };

struct outcome {
    int client;
    int server;
    int alert;
    int server_reason;
    int decrypts;
    int padding;
    uint64_t decrypt_ns;
};

static rsa_crypt default_priv_dec, default_pub_enc;
static enum mode mode;
static struct outcome *current;

static uint64_t
now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static int
probe_priv_dec(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding)
{
    uint64_t start = now_ns();
    int ret = default_priv_dec(flen, from, to, rsa, padding);

    current->decrypt_ns = now_ns() - start;
    current->padding = padding;
    current->decrypts++;
    return ret;
}

static int
tamper_pub_enc(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding)
{
    unsigned char pms[PMS_LEN], block[1024];
    int k = RSA_size(rsa);

    if (mode == HONEST || flen != PMS_LEN || padding != RSA_PKCS1_PADDING || k > (int)sizeof(block))
        return default_pub_enc(flen, from, to, rsa, padding);
    memcpy(pms, from, PMS_LEN);
    if (mode == WRONG_SECRET)
        pms[PMS_LEN - 1] ^= 0x01;
    if (mode == BAD_VERSION)
        pms[1] ^= 0x01;
    if (mode != BAD_PADDING)
        return default_pub_enc(PMS_LEN, pms, to, rsa, RSA_PKCS1_PADDING);
    block[0] = 0x00;
    block[1] = 0x03;
    memset(block + 2, 0x5a, (size_t)(k - 3 - PMS_LEN));
    block[k - PMS_LEN - 1] = 0x00;
    memcpy(block + k - PMS_LEN, pms, PMS_LEN);
    return default_pub_enc(k, block, to, rsa, RSA_NO_PADDING);
}

static int
last_reason(int *alert)
{
    unsigned long e, last = 0;

    while ((e = ERR_get_error()) != 0) {
        last = e;
        if (alert && (int)((e >> 24) & 0xff) == ERR_LIB_SSL && (int)(e & 0xfff) > SSL_AD_REASON_OFFSET)
            *alert = (int)(e & 0xfff) - SSL_AD_REASON_OFFSET;
    }
    return (int)(last & 0xfff);
}

static void
step(SSL *ssl, int *state, int *reason, int *alert)
{
    int ret, err;

    if (*state != 0)
        return;
    ret = SSL_do_handshake(ssl);
    if (ret == 1) {
        *state = 1;
        return;
    }
    err = SSL_get_error(ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
        return;
    *state = -1;
    *reason = last_reason(alert);
}

static struct outcome
handshake(SSL_CTX *server_ctx, SSL_CTX *client_ctx, enum mode m)
{
    struct outcome o = { 0 };
    SSL *client = SSL_new(client_ctx), *server = SSL_new(server_ctx);
    BIO *cbio = NULL, *sbio = NULL;
    int client_reason = 0;

    o.alert = -1;
    o.padding = -1;
    mode = m;
    current = &o;
    last_reason(NULL);
    if (client == NULL || server == NULL || !BIO_new_bio_pair(&cbio, 0, &sbio, 0)) {
        o.client = o.server = -2;
        return o;
    }
    SSL_set_bio(client, cbio, cbio);
    SSL_set_bio(server, sbio, sbio);
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);
    for (int i = 0; i < 64 && (o.client == 0 || o.server == 0); i++) {
        step(client, &o.client, &client_reason, &o.alert);
        step(server, &o.server, &o.server_reason, NULL);
    }
    SSL_free(client);
    SSL_free(server);
    current = NULL;
    return o;
}

static int
by_value(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;

    return x < y ? -1 : x > y;
}

static uint64_t
median_decrypt_ns(SSL_CTX *server_ctx, SSL_CTX *client_ctx, enum mode m)
{
    static uint64_t samples[ROUNDS];

    for (int i = 0; i < ROUNDS; i++)
        samples[i] = handshake(server_ctx, client_ctx, m).decrypt_ns;
    qsort(samples, ROUNDS, sizeof(samples[0]), by_value);
    return samples[ROUNDS / 2];
}

int
main(int argc, char **argv)
{
    static const char *names[] = { "honest", "wrong secret", "bad padding", "bad version" };
    struct outcome o[4];
    RSA_METHOD *probe, *tamper;
    SSL_CTX *server_ctx, *client_ctx;
    EVP_PKEY *key;
    X509 *cert;
    FILE *f;

    if (argc != 3) {
        fprintf(stderr, "usage: t_rsakex KEY CERT\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    default_priv_dec = RSA_meth_get_priv_dec(RSA_PKCS1_OpenSSL());
    default_pub_enc = RSA_meth_get_pub_enc(RSA_PKCS1_OpenSSL());
    probe = RSA_meth_dup(RSA_PKCS1_OpenSSL());
    tamper = RSA_meth_dup(RSA_PKCS1_OpenSSL());
    CHECK(probe && tamper && RSA_meth_set_priv_dec(probe, probe_priv_dec) && RSA_meth_set_pub_enc(tamper, tamper_pub_enc));
    key = (f = fopen(argv[1], "r")) ? PEM_read_PrivateKey(f, NULL, NULL, NULL) : NULL;
    if (f)
        fclose(f);
    cert = (f = fopen(argv[2], "r")) ? PEM_read_X509(f, NULL, NULL, NULL) : NULL;
    if (f)
        fclose(f);
    CHECK(key && cert && EVP_PKEY_get0_RSA(key) && RSA_set_method(EVP_PKEY_get0_RSA(key), probe));
    server_ctx = SSL_CTX_new(TLS_server_method());
    client_ctx = SSL_CTX_new(TLS_client_method());
    CHECK(server_ctx && client_ctx && SSL_CTX_use_certificate(server_ctx, cert) == 1 &&
          SSL_CTX_use_PrivateKey(server_ctx, key) == 1 && SSL_CTX_set_cipher_list(server_ctx, "AES128-SHA") == 1 &&
          SSL_CTX_set_cipher_list(client_ctx, "AES128-SHA") == 1 &&
          SSL_CTX_set_max_proto_version(client_ctx, TLS1_2_VERSION) == 1);
    if (checks_failed)
        return checks_done("RSA key exchange");
    RSA_set_default_method(tamper);

    for (int m = HONEST; m <= BAD_VERSION; m++) {
        o[m] = handshake(server_ctx, client_ctx, m);
        printf("%s: client %d, server %d, alert %d, server reason %d, decryptions %d, padding %d\n", names[m],
               o[m].client, o[m].server, o[m].alert, o[m].server_reason, o[m].decrypts, o[m].padding);
    }
    CHECK(o[HONEST].client == 1 && o[HONEST].server == 1);
    for (int m = WRONG_SECRET; m <= BAD_VERSION; m++) {
        CHECK(o[m].client == -1 && o[m].server == -1 && o[m].alert > 0);
        CHECK(o[m].alert == o[WRONG_SECRET].alert && o[m].server_reason == o[WRONG_SECRET].server_reason);
    }
    for (int m = HONEST; m <= BAD_VERSION; m++)
        CHECK(o[m].decrypts == 1 && o[m].padding == RSA_NO_PADDING);
    printf("info: median RSA decryption in the server over %d handshakes: wrong secret %llu ns, bad padding %llu ns\n",
           ROUNDS, (unsigned long long)median_decrypt_ns(server_ctx, client_ctx, WRONG_SECRET),
           (unsigned long long)median_decrypt_ns(server_ctx, client_ctx, BAD_PADDING));
    SSL_CTX_free(server_ctx);
    SSL_CTX_free(client_ctx);
    return checks_done("RSA key exchange");
}

/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <curl/curl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"

struct body {
    char data[8192];
    size_t len;
    uint64_t hash;
    size_t total;
};

static size_t
collect(char *ptr, size_t size, size_t n, void *arg)
{
    struct body *b = arg;
    size_t len = size * n;
    for (size_t i = 0; i < len; i++) {
        b->hash = (b->hash ^ (unsigned char)ptr[i]) * 0x100000001b3ULL;
    }
    size_t take = len < sizeof(b->data) - 1 - b->len ? len : sizeof(b->data) - 1 - b->len;
    memcpy(b->data + b->len, ptr, take);
    b->len += take;
    b->data[b->len] = '\0';
    b->total += len;
    return len;
}

static size_t
headers(char *ptr, size_t size, size_t n, void *arg)
{
    (void)arg;
    if (strncasecmp(ptr, "x-checksum:", 11) == 0) {
        printf("  trailer %.*s", (int)(size * n), ptr);
    }
    return size * n;
}

static CURL *
easy(const char *url, const char *ca, long version, struct body *b)
{
    CURL *h = curl_easy_init();
    memset(b, 0, sizeof(*b));
    b->hash = 0xcbf29ce484222325ULL;
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_CAINFO, ca);
    curl_easy_setopt(h, CURLOPT_HTTP_VERSION, version);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, b);
    curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, headers);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 60L);
    return h;
}

static const char *
version_name(long v)
{
    switch (v) {
    case CURL_HTTP_VERSION_1_0: return "1.0";
    case CURL_HTTP_VERSION_1_1: return "1.1";
    case CURL_HTTP_VERSION_2_0: return "2";
    case CURL_HTTP_VERSION_3: return "3";
    default: return "none";
    }
}

static long
fetch(const char *tag, const char *base, const char *path, const char *ca, long version)
{
    char url[512];
    struct body b;
    long status = 0, used = 0;
    snprintf(url, sizeof(url), "%s%s", base, path);
    CURL *h = easy(url, ca, version, &b);
    CURLcode rc = curl_easy_perform(h);
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_getinfo(h, CURLINFO_HTTP_VERSION, &used);
    printf("%s: rc %d status %ld http %s bytes %zu hash %016llx", tag, rc, status, version_name(used), b.total,
           (unsigned long long)b.hash);
    printf(b.total < 200 ? " body %s" : "\n", b.data);
    curl_easy_cleanup(h);
    return rc == CURLE_OK ? used : -1;
}

static void
upload(const char *base, const char *ca)
{
    char url[512];
    struct body b;
    long used = 0;
    size_t n = 300 * 1024;
    char *data = malloc(n);
    for (size_t i = 0; i < n; i++) {
        data[i] = (char)(i % 251);
    }
    snprintf(url, sizeof(url), "%s/upload", base);
    CURL *h = easy(url, ca, CURL_HTTP_VERSION_2TLS, &b);
    curl_easy_setopt(h, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)n);
    CURLcode rc = curl_easy_perform(h);
    curl_easy_getinfo(h, CURLINFO_HTTP_VERSION, &used);
    printf("upload: rc %d http %s %s", rc, version_name(used), b.data);
    CHECK(rc == CURLE_OK && used == CURL_HTTP_VERSION_2_0 &&
          strstr(b.data, "received 307200 sha256 ") != NULL);
    curl_easy_cleanup(h);
    free(data);
}

static void
multiplex(const char *base, const char *ca)
{
    enum { N = 8 };
    CURLM *m = curl_multi_init();
    CURL *h[N];
    struct body b[N];
    char url[512];
    curl_multi_setopt(m, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
    for (int i = 0; i < N; i++) {
        snprintf(url, sizeof(url), "%s/stream/%d", base, i);
        h[i] = easy(url, ca, CURL_HTTP_VERSION_2TLS, &b[i]);
        curl_easy_setopt(h[i], CURLOPT_PIPEWAIT, 1L);
        curl_multi_add_handle(m, h[i]);
    }
    int running = 1;
    while (running) {
        curl_multi_perform(m, &running);
        if (running) {
            curl_multi_poll(m, NULL, 0, 1000, NULL);
        }
    }
    long connects = 0, ok = 0;
    for (int i = 0; i < N; i++) {
        long c = 0, used = 0;
        curl_easy_getinfo(h[i], CURLINFO_NUM_CONNECTS, &c);
        curl_easy_getinfo(h[i], CURLINFO_HTTP_VERSION, &used);
        connects += c;
        ok += used == CURL_HTTP_VERSION_2_0 && strstr(b[i].data, "proto=HTTP/2.0") != NULL;
        curl_multi_remove_handle(m, h[i]);
        curl_easy_cleanup(h[i]);
    }
    curl_multi_cleanup(m);
    printf("multiplex: %d transfers, %ld over HTTP/2, %ld connection(s)\n", N, ok, connects);
    CHECK(ok == N && connects == 1);
}

int
main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: t_http2 TLS_BASE TLS11_BASE H2C_BASE CA_FILE\n");
        return 2;
    }
    const char *tls = argv[1], *tls11 = argv[2], *h2c = argv[3], *ca = argv[4];
    curl_global_init(CURL_GLOBAL_ALL);
    curl_version_info_data *v = curl_version_info(CURLVERSION_NOW);
    printf("libcurl %s nghttp2 %s\n", v->version, v->nghttp2_version ? v->nghttp2_version : "none");
    printf("feature HTTP2 %d\n", (v->features & CURL_VERSION_HTTP2) != 0);
    CHECK((v->features & CURL_VERSION_HTTP2) != 0 && v->nghttp2_version != NULL);
    CHECK(fetch("h2", tls, "/", ca, CURL_HTTP_VERSION_2TLS) == CURL_HTTP_VERSION_2_0);
    CHECK(fetch("h2 forced", tls, "/", ca, CURL_HTTP_VERSION_2_0) == CURL_HTTP_VERSION_2_0);
    CHECK(fetch("http1.1 asked", tls, "/", ca, CURL_HTTP_VERSION_1_1) == CURL_HTTP_VERSION_1_1);
    CHECK(fetch("http1.1 server", tls11, "/", ca, CURL_HTTP_VERSION_2TLS) == CURL_HTTP_VERSION_1_1);
    CHECK(fetch("h2c prior knowledge", h2c, "/", ca, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE) == CURL_HTTP_VERSION_2_0);
    CHECK(fetch("h2c upgrade", h2c, "/", ca, CURL_HTTP_VERSION_2_0) == CURL_HTTP_VERSION_1_1);
    CHECK(fetch("big", tls, "/big", ca, CURL_HTTP_VERSION_2TLS) == CURL_HTTP_VERSION_2_0);
    CHECK(fetch("trailer", tls, "/trailer", ca, CURL_HTTP_VERSION_2TLS) == CURL_HTTP_VERSION_2_0);
    upload(tls, ca);
    multiplex(tls, ca);
    curl_global_cleanup();
    return checks_done("http2");
}

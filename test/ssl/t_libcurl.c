/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../libsystem/check.h"

enum { X509_V_ERR_CERT_HAS_EXPIRED = 10 };

static const char *dir;

static size_t
discard(char *ptr, size_t size, size_t n, void *arg)
{
    (void)ptr;
    (void)arg;
    return size * n;
}

static CURLcode
fetch(const char *name, const char *ca, const char *host, int port, long version, long *status, long *verify)
{
    char url[128], resolve[128], cainfo[512], error[CURL_ERROR_SIZE] = "";
    struct curl_slist *names = NULL;
    CURL *h = curl_easy_init();
    CURLcode rc;

    snprintf(url, sizeof(url), "https://%s:%d/ok.txt", host, port);
    snprintf(resolve, sizeof(resolve), "%s:%d:127.0.0.1", host, port);
    snprintf(cainfo, sizeof(cainfo), "%s/%s", dir, ca);
    names = curl_slist_append(names, resolve);
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_RESOLVE, names);
    curl_easy_setopt(h, CURLOPT_CAINFO, cainfo);
    curl_easy_setopt(h, CURLOPT_SSLVERSION, version);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, discard);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(h, CURLOPT_ERRORBUFFER, error);
    rc = curl_easy_perform(h);
    *status = 0;
    *verify = -1;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, status);
    curl_easy_getinfo(h, CURLINFO_SSL_VERIFYRESULT, verify);
    printf("result curl %s: code %d, status %ld, verify %ld\n", name, rc, *status, *verify);
    if (rc != CURLE_OK)
        printf("  %s\n", error);
    curl_easy_cleanup(h);
    curl_slist_free_all(names);
    return rc;
}

int
main(int argc, char **argv)
{
    int good, expired;
    long status, verify;

    if (argc != 4) {
        fprintf(stderr, "usage: t_libcurl PKI_DIR GOOD_PORT EXPIRED_PORT\n");
        return 2;
    }
    dir = argv[1];
    good = atoi(argv[2]);
    expired = atoi(argv[3]);
    setvbuf(stdout, NULL, _IOLBF, 0);
    alarm(300);
    CHECK(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    printf("result curl backend: %s\n", curl_version_info(CURLVERSION_NOW)->ssl_version);
    CHECK(fetch("default", "ca.pem", "localhost", good, CURL_SSLVERSION_DEFAULT, &status, &verify) == CURLE_OK &&
          status == 200 && verify == 0);
    CHECK(fetch("TLS 1.3 only", "ca.pem", "localhost", good, CURL_SSLVERSION_TLSv1_3, &status, &verify) ==
          CURLE_OK && status == 200);
    CHECK(fetch("TLS 1.2 only", "ca.pem", "localhost", good, CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_2,
          &status, &verify) == CURLE_OK && status == 200);
    CHECK(fetch("expired", "ca.pem", "localhost", expired, CURL_SSLVERSION_DEFAULT, &status, &verify) ==
          CURLE_PEER_FAILED_VERIFICATION && verify == X509_V_ERR_CERT_HAS_EXPIRED);
    CHECK(fetch("untrusted", "other.pem", "localhost", good, CURL_SSLVERSION_DEFAULT, &status, &verify) ==
          CURLE_PEER_FAILED_VERIFICATION);
    CHECK(fetch("wrong name", "ca.pem", "wrong.example", good, CURL_SSLVERSION_DEFAULT, &status, &verify) ==
          CURLE_PEER_FAILED_VERIFICATION);
    curl_global_cleanup();
    return checks_done("libcurl over LibreSSL");
}

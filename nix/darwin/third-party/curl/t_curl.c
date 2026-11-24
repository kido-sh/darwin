/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "../libsystem/check.h"

/* a root with no key kept, so it can have signed nothing */
static const char unrelated_ca[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBsDCCAVWgAwIBAgIUH3Cr6rzligk+ZTxFUHSYmTY6I8MwCgYIKoZIzj0EAwIw\n"
    "LDEqMCgGA1UEAwwha2lkbyB0ZXN0IHJvb3QgdGhhdCBzaWducyBub3RoaW5nMCAX\n"
    "DTI2MTEyMzE5NTU1MFoYDzIxMjYxMDMwMTk1NTUwWjAsMSowKAYDVQQDDCFraWRv\n"
    "IHRlc3Qgcm9vdCB0aGF0IHNpZ25zIG5vdGhpbmcwWTATBgcqhkjOPQIBBggqhkjO\n"
    "PQMBBwNCAATQnXZ2fJoMl8QhoiGphLMWPjZZj2yVEu5P+CxS7mijetV1MFulIo+e\n"
    "yo0ZGsEppfAkSFBRq5tRfuTyCiOv3io6o1MwUTAdBgNVHQ4EFgQU4V3uparxgt1H\n"
    "oEVgdb4859RLP1owHwYDVR0jBBgwFoAU4V3uparxgt1HoEVgdb4859RLP1owDwYD\n"
    "VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNJADBGAiEA9PYh1uvsIfuia7fQ4W6Q\n"
    "ibP85gLxb5vS5E0wiaBEfaQCIQDHti4BxZlYcdM4NFkD/p/s8ftvqemb3NWP9xUc\n"
    "4FyoIA==\n"
    "-----END CERTIFICATE-----\n";

struct body {
    char data[65536];
    size_t len;
};

static size_t
collect(char *ptr, size_t size, size_t n, void *arg)
{
    struct body *b = arg;
    size_t take = size * n;

    if (take > sizeof(b->data) - 1 - b->len)
        take = sizeof(b->data) - 1 - b->len;
    memcpy(b->data + b->len, ptr, take);
    b->len += take;
    b->data[b->len] = '\0';
    return size * n;
}

static CURL *
easy(const char *url, struct body *b)
{
    CURL *h = curl_easy_init();

    memset(b, 0, sizeof(*b));
    curl_easy_setopt(h, CURLOPT_URL, url);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, b);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 20L);
    curl_easy_setopt(h, CURLOPT_TIMEOUT, 60L);
    return h;
}

static CURLcode
fetch(const char *url, long *status, struct body *b)
{
    CURL *h = easy(url, b);
    CURLcode rc = curl_easy_perform(h);

    *status = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, status);
    curl_easy_cleanup(h);
    return rc;
}

static void *
fetch_thread(void *arg)
{
    struct body b;
    long status;

    if (fetch("https://example.com/", &status, &b) != CURLE_OK || status != 200)
        return NULL;
    return strstr(b.data, "Example Domain") ? arg : NULL;
}

static int
global_init_quiet(double *seconds)
{
    struct timeval t0, t1;
    char path[] = "/tmp/kido-curl.XXXXXX", buf[256];
    int fd = mkstemp(path), saved = dup(2), ok;
    CURLcode rc;

    dup2(fd, 2);
    gettimeofday(&t0, NULL);
    rc = curl_global_init(CURL_GLOBAL_ALL);
    gettimeofday(&t1, NULL);
    dup2(saved, 2);
    close(saved);
    ok = rc == CURLE_OK && pread(fd, buf, sizeof(buf), 0) == 0;
    close(fd);
    unlink(path);
    *seconds = (t1.tv_sec - t0.tv_sec) + (t1.tv_usec - t0.tv_usec) / 1e6;
    return ok;
}

int
main(void)
{
    const curl_version_info_data *v;
    struct body b;
    long status = 0, verify = -1;
    char *cainfo = NULL, *s;
    struct curl_blob blob;
    double init_seconds;
    pthread_t threads[4];
    void *ret;
    CURL *h;
    CURLM *m;
    CURLMcode mc;
    CURLU *u;
    int still, done, i, ok;

    alarm(300);
    CHECK(global_init_quiet(&init_seconds));
    CHECK(init_seconds < 2.0);

    v = curl_version_info(CURLVERSION_NOW);
    CHECK(v != NULL && v->age >= CURLVERSION_TENTH);
    CHECK(v->version_num >= 0x080700);
    CHECK(v->ssl_version != NULL && strcmp(v->ssl_version, "LibreSSL/3.3.6") == 0);
    CHECK(v->libz_version != NULL);
    CHECK((v->features & CURL_VERSION_SSL) && (v->features & CURL_VERSION_IPV6) &&
          (v->features & CURL_VERSION_LIBZ) && (v->features & CURL_VERSION_ASYNCHDNS) &&
          (v->features & CURL_VERSION_THREADSAFE));
    CHECK(!(v->features & CURL_VERSION_GSSAPI));
    for (ok = 0, i = 0; v->protocols[i] != NULL; i++)
        ok += strcmp(v->protocols[i], "https") == 0 || strcmp(v->protocols[i], "file") == 0;
    CHECK(ok == 2);
    printf("libcurl %s, %s, zlib %s\n", v->version, v->ssl_version, v->libz_version);

    h = easy("https://example.com/", &b);
    CHECK(curl_easy_perform(h) == CURLE_OK);
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_getinfo(h, CURLINFO_SSL_VERIFYRESULT, &verify);
    curl_easy_getinfo(h, CURLINFO_CAINFO, &cainfo);
    CHECK(status == 200 && verify == 0);
    CHECK(strstr(b.data, "Example Domain") != NULL);
    CHECK(cainfo != NULL && strcmp(cainfo, "/etc/ssl/cert.pem") == 0);
    curl_easy_cleanup(h);

    h = easy("https://example.com/", &b);
    curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, "");
    CHECK(curl_easy_perform(h) == CURLE_OK && strstr(b.data, "Example Domain") != NULL);
    curl_easy_cleanup(h);

    CHECK(fetch("http://example.com/", &status, &b) == CURLE_OK && status == 200);

    h = easy("https://example.com/", &b);
    curl_easy_setopt(h, CURLOPT_CAINFO, "/dev/null");
    CHECK(curl_easy_perform(h) != CURLE_OK);
    curl_easy_cleanup(h);

    blob.data = (void *)unrelated_ca;
    blob.len = strlen(unrelated_ca);
    blob.flags = CURL_BLOB_COPY;
    h = easy("https://example.com/", &b);
    curl_easy_setopt(h, CURLOPT_CAINFO_BLOB, &blob);
    CHECK(curl_easy_perform(h) == CURLE_PEER_FAILED_VERIFICATION);
    CHECK(curl_easy_getinfo(h, CURLINFO_SSL_VERIFYRESULT, &verify) == CURLE_OK && verify != 0);
    curl_easy_cleanup(h);

    CHECK(fetch("file:///etc/hosts", &status, &b) == CURLE_OK && strstr(b.data, "localhost") != NULL);

    m = curl_multi_init();
    {
        struct body b1, b2;
        CURL *h1 = easy("https://example.com/", &b1), *h2 = easy("https://example.com/?2", &b2);

        curl_multi_add_handle(m, h1);
        curl_multi_add_handle(m, h2);
        do {
            mc = curl_multi_perform(m, &still);
            if (mc == CURLM_OK && still)
                mc = curl_multi_poll(m, NULL, 0, 1000, NULL);
        } while (mc == CURLM_OK && still);
        CHECK(mc == CURLM_OK);
        for (done = 0;;) {
            CURLMsg *msg = curl_multi_info_read(m, &i);

            if (msg == NULL)
                break;
            done += msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK;
        }
        CHECK(done == 2);
        CHECK(strstr(b1.data, "Example Domain") && strstr(b2.data, "Example Domain"));
        curl_multi_remove_handle(m, h1);
        curl_multi_remove_handle(m, h2);
        curl_easy_cleanup(h1);
        curl_easy_cleanup(h2);
    }
    curl_multi_cleanup(m);

    for (i = 0; i < 4; i++)
        pthread_create(&threads[i], NULL, fetch_thread, &threads[i]);
    for (ok = 0, i = 0; i < 4; i++) {
        pthread_join(threads[i], &ret);
        ok += ret == &threads[i];
    }
    CHECK(ok == 4);

    s = curl_easy_escape(NULL, "a b&c", 0);
    CHECK(s != NULL && strcmp(s, "a%20b%26c") == 0);
    curl_free(s);
    u = curl_url();
    CHECK(curl_url_set(u, CURLUPART_URL, "https://user@example.com:8443/p?q=1#f", 0) == CURLUE_OK);
    CHECK(curl_url_get(u, CURLUPART_PORT, &s, 0) == CURLUE_OK && strcmp(s, "8443") == 0);
    curl_free(s);
    curl_url_cleanup(u);
    CHECK(curl_getdate("Sun, 06 Nov 1994 08:49:37 GMT", NULL) == 784111777);

    curl_global_cleanup();
    return checks_done("curl");
}

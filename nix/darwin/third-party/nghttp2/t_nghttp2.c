/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "check.h"

typedef ptrdiff_t nghttp2_ssize;
typedef struct nghttp2_session nghttp2_session;
typedef struct nghttp2_session_callbacks nghttp2_session_callbacks;
typedef struct nghttp2_hd_deflater nghttp2_hd_deflater;
typedef struct nghttp2_hd_inflater nghttp2_hd_inflater;
typedef struct {
    uint8_t *name;
    uint8_t *value;
    size_t namelen;
    size_t valuelen;
    uint8_t flags;
} nghttp2_nv;
typedef struct {
    int32_t settings_id;
    uint32_t value;
} nghttp2_settings_entry;
typedef struct {
    int age;
    int version_num;
    const char *version_str;
    const char *proto_str;
} nghttp2_info;
typedef struct {
    uint32_t urgency;
    int inc;
} nghttp2_extpri;
typedef struct {
    size_t length;
    int32_t stream_id;
    uint8_t type;
    uint8_t flags;
    uint8_t reserved;
} nghttp2_frame_hd;
typedef union {
    int fd;
    void *ptr;
} nghttp2_data_source;
typedef nghttp2_ssize (*nghttp2_data_source_read_callback2)(nghttp2_session *, int32_t, uint8_t *, size_t,
                                                             uint32_t *, nghttp2_data_source *, void *);
typedef struct {
    nghttp2_data_source source;
    nghttp2_data_source_read_callback2 read_callback;
} nghttp2_data_provider2;

nghttp2_info *nghttp2_version(int);
const char *nghttp2_strerror(int);
const char *nghttp2_http2_strerror(uint32_t);
int nghttp2_is_fatal(int);
int nghttp2_check_header_name(const uint8_t *, size_t);
int nghttp2_check_header_value(const uint8_t *, size_t);
int nghttp2_check_header_value_rfc9113(const uint8_t *, size_t);
int nghttp2_check_method(const uint8_t *, size_t);
int nghttp2_check_path(const uint8_t *, size_t);
int nghttp2_check_authority(const uint8_t *, size_t);
nghttp2_ssize nghttp2_pack_settings_payload2(uint8_t *, size_t, const nghttp2_settings_entry *, size_t);
int nghttp2_extpri_parse_priority(nghttp2_extpri *, const uint8_t *, size_t);
int nghttp2_hd_deflate_new(nghttp2_hd_deflater **, size_t);
nghttp2_ssize nghttp2_hd_deflate_hd2(nghttp2_hd_deflater *, uint8_t *, size_t, const nghttp2_nv *, size_t);
void nghttp2_hd_deflate_del(nghttp2_hd_deflater *);
int nghttp2_hd_inflate_new(nghttp2_hd_inflater **);
nghttp2_ssize nghttp2_hd_inflate_hd3(nghttp2_hd_inflater *, nghttp2_nv *, int *, const uint8_t *, size_t, int);
int nghttp2_hd_inflate_end_headers(nghttp2_hd_inflater *);
void nghttp2_hd_inflate_del(nghttp2_hd_inflater *);
int nghttp2_session_callbacks_new(nghttp2_session_callbacks **);
void nghttp2_session_callbacks_del(nghttp2_session_callbacks *);
void nghttp2_session_callbacks_set_on_frame_recv_callback(nghttp2_session_callbacks *,
    int (*)(nghttp2_session *, const void *, void *));
void nghttp2_session_callbacks_set_on_header_callback(nghttp2_session_callbacks *,
    int (*)(nghttp2_session *, const void *, const uint8_t *, size_t, const uint8_t *, size_t, uint8_t, void *));
void nghttp2_session_callbacks_set_on_data_chunk_recv_callback(nghttp2_session_callbacks *,
    int (*)(nghttp2_session *, uint8_t, int32_t, const uint8_t *, size_t, void *));
void nghttp2_session_callbacks_set_on_stream_close_callback(nghttp2_session_callbacks *,
    int (*)(nghttp2_session *, int32_t, uint32_t, void *));
int nghttp2_session_client_new(nghttp2_session **, const nghttp2_session_callbacks *, void *);
int nghttp2_session_server_new(nghttp2_session **, const nghttp2_session_callbacks *, void *);
void nghttp2_session_del(nghttp2_session *);
int nghttp2_submit_settings(nghttp2_session *, uint8_t, const nghttp2_settings_entry *, size_t);
int32_t nghttp2_submit_request2(nghttp2_session *, const void *, const nghttp2_nv *, size_t,
                                const nghttp2_data_provider2 *, void *);
int nghttp2_submit_response2(nghttp2_session *, int32_t, const nghttp2_nv *, size_t, const nghttp2_data_provider2 *);
nghttp2_ssize nghttp2_session_mem_send2(nghttp2_session *, const uint8_t **);
nghttp2_ssize nghttp2_session_mem_recv2(nghttp2_session *, const uint8_t *, size_t);

#define NV(n, v) { (uint8_t *)(n), (uint8_t *)(v), sizeof(n) - 1, sizeof(v) - 1, 0 }
#define CSTR(s) (const uint8_t *)(s), sizeof(s) - 1

static void
hex(const char *tag, const uint8_t *p, size_t n)
{
    printf("%s", tag);
    for (size_t i = 0; i < n; i++) {
        printf("%02x", p[i]);
    }
    printf("\n");
}

static void
check_helpers(void)
{
    printf("check name %d %d %d\n", nghttp2_check_header_name(CSTR("content-type")),
           nghttp2_check_header_name(CSTR("Content-Type")), nghttp2_check_header_name(CSTR(":path")));
    printf("check value %d %d %d\n", nghttp2_check_header_value(CSTR("ok value")),
           nghttp2_check_header_value(CSTR("bad\r\nvalue")), nghttp2_check_header_value_rfc9113(CSTR(" lead")));
    printf("check method %d %d path %d %d %d authority %d %d\n", nghttp2_check_method(CSTR("GET")),
           nghttp2_check_method(CSTR("G T")), nghttp2_check_path(CSTR("/a?b")), nghttp2_check_path(CSTR("*")),
           nghttp2_check_path(CSTR("a b")), nghttp2_check_authority(CSTR("example.com:443")),
           nghttp2_check_authority(CSTR("bad host")));
    for (int e = -505; e <= -500; e++) {
        printf("strerror %d %s fatal %d\n", e, nghttp2_strerror(e), nghttp2_is_fatal(e));
    }
    printf("strerror %d %s\n", -902, nghttp2_strerror(-902));
    for (uint32_t c = 0; c <= 0xd; c++) {
        printf("http2 error %u %s\n", c, nghttp2_http2_strerror(c));
    }
    nghttp2_settings_entry iv[] = { { 3, 100 }, { 4, 65535 }, { 9, 1 } };
    uint8_t buf[64];
    nghttp2_ssize n = nghttp2_pack_settings_payload2(buf, sizeof(buf), iv, 3);
    CHECK(n == 18);
    hex("settings ", buf, n > 0 ? (size_t)n : 0);
    nghttp2_extpri pri = { 0, 0 };
    int r = nghttp2_extpri_parse_priority(&pri, CSTR("u=2, i"));
    printf("extpri %d urgency %u inc %d\n", r, pri.urgency, pri.inc);
    r = nghttp2_extpri_parse_priority(&pri, CSTR("u=9"));
    printf("extpri bad %d\n", r);
}

static void
check_hpack(void)
{
    nghttp2_nv headers[] = {
        NV(":method", "GET"), NV(":scheme", "https"), NV(":path", "/index.html"), NV(":authority", "kido.sh"),
        NV("accept", "*/*"), NV("user-agent", "t_nghttp2"), NV("x-custom", "value with spaces"),
    };
    size_t n = sizeof(headers) / sizeof(headers[0]);
    nghttp2_hd_deflater *def = NULL;
    nghttp2_hd_inflater *inf = NULL;
    CHECK(nghttp2_hd_deflate_new(&def, 4096) == 0 && nghttp2_hd_inflate_new(&inf) == 0);
    for (int round = 0; round < 2; round++) {
        uint8_t block[512];
        nghttp2_ssize len = nghttp2_hd_deflate_hd2(def, block, sizeof(block), headers, n);
        CHECK(len > 0);
        printf("hpack round %d length %ld\n", round, (long)len);
        hex("hpack ", block, len > 0 ? (size_t)len : 0);
        const uint8_t *in = block;
        size_t left = (size_t)len, seen = 0;
        for (;;) {
            nghttp2_nv nv;
            int flags = 0;
            nghttp2_ssize used = nghttp2_hd_inflate_hd3(inf, &nv, &flags, in, left, 1);
            CHECK(used >= 0);
            if (used < 0) {
                break;
            }
            in += used;
            left -= (size_t)used;
            if (flags & 0x02) {
                CHECK(seen < n && nv.namelen == headers[seen].namelen &&
                      memcmp(nv.name, headers[seen].name, nv.namelen) == 0 && nv.valuelen == headers[seen].valuelen &&
                      memcmp(nv.value, headers[seen].value, nv.valuelen) == 0);
                seen++;
            }
            if (flags & 0x01) {
                nghttp2_hd_inflate_end_headers(inf);
                break;
            }
        }
        CHECK(seen == n);
    }
    nghttp2_hd_deflate_del(def);
    nghttp2_hd_inflate_del(inf);
}

static const char body[] = "hello over HTTP/2";
static int responded;

static void
frame(const char *side, const void *f)
{
    const nghttp2_frame_hd *hd = f;
    printf("%s frame type %u flags %#x stream %d length %zu\n", side, hd->type, hd->flags, hd->stream_id, hd->length);
}

static nghttp2_ssize
read_body(nghttp2_session *s, int32_t stream, uint8_t *buf, size_t length, uint32_t *flags,
          nghttp2_data_source *src, void *u)
{
    (void)s;
    (void)stream;
    (void)src;
    (void)u;
    size_t n = sizeof(body) - 1 < length ? sizeof(body) - 1 : length;
    memcpy(buf, body, n);
    *flags |= 0x01;
    return (nghttp2_ssize)n;
}

static int
server_frame(nghttp2_session *s, const void *f, void *u)
{
    const nghttp2_frame_hd *hd = f;
    (void)u;
    frame("server", f);
    if (hd->type == 1 && (hd->flags & 0x01) && !responded) {
        nghttp2_nv res[] = { NV(":status", "200"), NV("content-type", "text/plain"), NV("server", "t_nghttp2") };
        nghttp2_data_provider2 prd = { { 0 }, read_body };
        CHECK(nghttp2_submit_response2(s, hd->stream_id, res, 3, &prd) == 0);
        responded = 1;
    }
    return 0;
}

static int
client_frame(nghttp2_session *s, const void *f, void *u)
{
    (void)s;
    (void)u;
    frame("client", f);
    return 0;
}

static int
header(nghttp2_session *s, const void *f, const uint8_t *name, size_t namelen, const uint8_t *value,
       size_t valuelen, uint8_t flags, void *u)
{
    (void)s;
    (void)f;
    (void)flags;
    printf("%s header %.*s: %.*s\n", (const char *)u, (int)namelen, name, (int)valuelen, value);
    return 0;
}

static char received[256];

static int
data(nghttp2_session *s, uint8_t flags, int32_t stream, const uint8_t *p, size_t len, void *u)
{
    (void)s;
    (void)flags;
    printf("%s data stream %d %.*s\n", (const char *)u, stream, (int)len, p);
    strncat(received, (const char *)p, len < sizeof(received) - strlen(received) - 1 ? len : 0);
    return 0;
}

static int
closed(nghttp2_session *s, int32_t stream, uint32_t error, void *u)
{
    (void)s;
    printf("%s close stream %d error %u\n", (const char *)u, stream, error);
    return 0;
}

static size_t
pump(nghttp2_session *from, nghttp2_session *to, const char *dir)
{
    size_t total = 0;
    for (;;) {
        const uint8_t *p = NULL;
        nghttp2_ssize n = nghttp2_session_mem_send2(from, &p);
        if (n <= 0) {
            break;
        }
        printf("%s %ld bytes\n", dir, (long)n);
        CHECK(nghttp2_session_mem_recv2(to, p, (size_t)n) == n);
        total += (size_t)n;
    }
    return total;
}

static void
check_sessions(void)
{
    nghttp2_session_callbacks *cb = NULL;
    nghttp2_session *client = NULL, *server = NULL;
    CHECK(nghttp2_session_callbacks_new(&cb) == 0);
    nghttp2_session_callbacks_set_on_header_callback(cb, header);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(cb, data);
    nghttp2_session_callbacks_set_on_stream_close_callback(cb, closed);
    nghttp2_session_callbacks_set_on_frame_recv_callback(cb, client_frame);
    CHECK(nghttp2_session_client_new(&client, cb, "client") == 0);
    nghttp2_session_callbacks_set_on_frame_recv_callback(cb, server_frame);
    CHECK(nghttp2_session_server_new(&server, cb, "server") == 0);
    nghttp2_session_callbacks_del(cb);
    nghttp2_settings_entry iv[] = { { 3, 100 } };
    CHECK(nghttp2_submit_settings(client, 0, iv, 1) == 0 && nghttp2_submit_settings(server, 0, iv, 1) == 0);
    nghttp2_nv req[] = { NV(":method", "GET"), NV(":scheme", "https"), NV(":path", "/"), NV(":authority", "kido.sh") };
    int32_t stream = nghttp2_submit_request2(client, NULL, req, 4, NULL, NULL);
    printf("request stream %d\n", stream);
    CHECK(stream == 1);
    for (int i = 0; i < 6; i++) {
        size_t moved = pump(client, server, "client->server") + pump(server, client, "server->client");
        if (moved == 0) {
            break;
        }
    }
    CHECK(strcmp(received, body) == 0);
    nghttp2_session_del(client);
    nghttp2_session_del(server);
}

int
main(void)
{
    nghttp2_info *v = nghttp2_version(0);
    printf("version %s %s age %d\n", v->version_str, v->proto_str, v->age);
    CHECK(v->version_num >= 0x014500 && strcmp(v->proto_str, "h2") == 0);
    check_helpers();
    check_hpack();
    check_sessions();
    return checks_done("nghttp2");
}

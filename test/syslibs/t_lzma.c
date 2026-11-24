/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <mach-o/loader.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libsystem/check.h"

#define LZMA_OK 0
#define LZMA_CHECK_CRC64 4

int lzma_easy_buffer_encode(uint32_t preset, int check, const void *allocator, const uint8_t *in,
    size_t in_size, uint8_t *out, size_t *out_pos, size_t out_size);
int lzma_stream_buffer_decode(uint64_t *memlimit, uint32_t flags, const void *allocator,
    const uint8_t *in, size_t *in_pos, size_t in_size, uint8_t *out, size_t *out_pos, size_t out_size);
const char *lzma_version_string(void);

static uint32_t
current_version(const void *symbol)
{
    Dl_info info;

    if (dladdr(symbol, &info) == 0 || info.dli_fbase == NULL) {
        return 0;
    }
    const struct mach_header_64 *mh = info.dli_fbase;
    const char *p = (const char *)(mh + 1);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        const struct load_command *lc = (const struct load_command *)p;
        if (lc->cmd == LC_ID_DYLIB) {
            return ((const struct dylib_command *)p)->dylib.current_version;
        }
        p += lc->cmdsize;
    }
    return 0;
}

int
main(void)
{
    size_t n = 1 << 16, packed_pos = 0, in_pos = 0, out_pos = 0;
    uint8_t *in = malloc(n), *packed = malloc(n + 1024), *unpacked = malloc(n);
    uint64_t memlimit = UINT64_MAX;

    for (size_t i = 0; i < n; i++) {
        in[i] = (uint8_t)("kido"[i % 4] + (i / 4096));
    }
    CHECK(lzma_easy_buffer_encode(6, LZMA_CHECK_CRC64, NULL, in, n, packed, &packed_pos, n + 1024) == LZMA_OK);
    CHECK(packed_pos > 0 && packed_pos < n / 10);
    CHECK(lzma_stream_buffer_decode(&memlimit, 0, NULL, packed, &in_pos, packed_pos, unpacked, &out_pos, n) ==
        LZMA_OK);
    CHECK(in_pos == packed_pos && out_pos == n && memcmp(in, unpacked, n) == 0);
    CHECK(strncmp(lzma_version_string(), "5.", 2) == 0);
    CHECK(current_version((const void *)lzma_version_string) == 0x60300);

    void *h = dlopen("/usr/lib/liblzma.5.dylib", RTLD_NOW);
    CHECK(h != NULL);
    if (h != NULL) {
        CHECK(dlsym(h, "lzma_stream_decoder_mt") != NULL);
        CHECK(dlsym(h, "lzma_mt_block_size") == NULL);
        CHECK(dlsym(h, "lzma_bcj_arm64_encode") == NULL);
        CHECK(dlsym(h, "lzma_bcj_x86_decode") == NULL);
    }
    free(in);
    free(packed);
    free(unpacked);
    return checks_done("liblzma");
}

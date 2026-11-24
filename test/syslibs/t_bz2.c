/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bzlib.h>
#include <dlfcn.h>
#include <mach-o/loader.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libsystem/check.h"

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
    unsigned int n = 1 << 16, packed_len = n + n / 100 + 600, unpacked_len = n;
    char *in = malloc(n), *packed = malloc(packed_len), *unpacked = malloc(n);

    for (unsigned int i = 0; i < n; i++) {
        in[i] = (char)("kido"[i % 4] + (i / 4096));
    }
    CHECK(BZ2_bzBuffToBuffCompress(packed, &packed_len, in, n, 9, 0, 0) == BZ_OK);
    CHECK(packed_len < n / 10);
    CHECK(BZ2_bzBuffToBuffDecompress(unpacked, &unpacked_len, packed, packed_len, 0, 0) == BZ_OK);
    CHECK(unpacked_len == n && memcmp(in, unpacked, n) == 0);
    CHECK(strncmp(BZ2_bzlibVersion(), "1.0.8", 5) == 0);
    CHECK(current_version((const void *)BZ2_bzlibVersion) == 0x10008);

    void *h = dlopen("/usr/lib/libbz2.1.0.dylib", RTLD_NOW);
    CHECK(h != NULL);
    if (h != NULL) {
        CHECK(dlsym(h, "BZ2_bzCompressInit") != NULL);
        CHECK(dlsym(h, "BZ2_compressBlock") == NULL);
        CHECK(dlsym(h, "BZ2_crc32Table") == NULL);
        CHECK(dlsym(h, "BZ2_hbMakeCodeLengths") == NULL);
    }
    free(in);
    free(packed);
    free(unpacked);
    return checks_done("libbz2");
}

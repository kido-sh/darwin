/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "fuse_proto.h"

static uint64_t aligned[65536 / sizeof(uint64_t)];

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    volatile uint32_t sink = 0;

    if (size > sizeof(aligned))
        return 0;
    memcpy(aligned, data, size);
    const uint8_t *buf = (const uint8_t *)aligned;

    for (int plus = 0; plus < 2; plus++) {
        struct virtiofs_dirent d;
        uint32_t pos = 0;
        while (virtiofs_dirent_next(buf, (uint32_t)size, &pos, plus, &d) == 1) {
            sink += (uint8_t)d.name[d.namelen - 1];
            if (d.entry)
                sink += (uint32_t)virtiofs_entry_ok(d.entry);
        }
    }
    if (size >= sizeof(int32_t)) {
        int32_t error;
        memcpy(&error, buf, sizeof(error));
        sink += (uint32_t)virtiofs_errno(error);
    }
    if (size >= sizeof(struct fuse_attr))
        sink += (uint32_t)virtiofs_attr_ok((const struct fuse_attr *)buf);
    sink += (uint32_t)virtiofs_xattr_list_ok((const char *)buf, (uint32_t)size);
    return 0;
}

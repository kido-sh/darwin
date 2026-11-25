/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <sys/cdefs.h>
#include "fuse.h"

#define VIRTIOFS_NAME_MAX 1023

struct virtiofs_dirent {
    uint64_t ino;
    uint64_t off;
    uint32_t type;
    uint32_t namelen;
    const char *name;
    const struct fuse_entry_out *entry;
};

__BEGIN_DECLS

int virtiofs_errno(int32_t fuse_error);
int virtiofs_attr_ok(const struct fuse_attr *attr);
int virtiofs_entry_ok(const struct fuse_entry_out *entry);
int virtiofs_name_ok(const char *name, uint32_t len);
int virtiofs_dirent_next(const uint8_t *buf, uint32_t len, uint32_t *pos, int plus, struct virtiofs_dirent *d);

__END_DECLS

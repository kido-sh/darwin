/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <sys/cdefs.h>
#include "fuse.h"

#define VIRTIOFS_NAME_MAX 1023
#define VIRTIOFS_SIZE_MAX (INT64_MAX - (1LL << 20))

#define VIRTIOFS_O_WRONLY  0x1
#define VIRTIOFS_O_CREAT   0x40
#define VIRTIOFS_O_EXCL    0x80

#define VIRTIOFS_XATTR_CREATE  0x1
#define VIRTIOFS_XATTR_REPLACE 0x2

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
int virtiofs_xattr_list_ok(const char *list, uint32_t len);

__END_DECLS

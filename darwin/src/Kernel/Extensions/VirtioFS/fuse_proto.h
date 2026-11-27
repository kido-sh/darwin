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

/*
 * A notification batch from the kido agent (VIRTIOFS_IOC_NOTIFY) is a sequence of records:
 *   u16 record length, u8 kind, u8 state, u8 flags, u8 zero, u16 path length, u32 mode, u32 nlink,
 *   u64 size, i64 mtime sec, i64 ctime sec, u32 mtime nsec, u32 ctime nsec, path
 * little-endian, the path relative to the mount's root. VIRTIOFS_NOTIFY_NAME says the name may have been
 * created, removed or renamed, not only what it names; VIRTIOFS_NOTIFY_REPLACED says it now names a
 * different file than before.
 */
#define VIRTIOFS_NOTIFY_HEADER    48
#define VIRTIOFS_NOTIFY_MAX_BATCH 32768
#define VIRTIOFS_NOTIFY_MAX_PATH  4096
#define VIRTIOFS_NOTIFY_MAX_DEPTH 64

enum { VIRTIOFS_NOTIFY_PATH = 1, VIRTIOFS_NOTIFY_FLUSH = 2 };
enum { VIRTIOFS_NOTIFY_UNKNOWN = 0, VIRTIOFS_NOTIFY_ABSENT = 1, VIRTIOFS_NOTIFY_PRESENT = 2 };
enum { VIRTIOFS_NOTIFY_NAME = 1, VIRTIOFS_NOTIFY_REPLACED = 2 };

struct virtiofs_notify_rec {
    uint8_t kind;
    uint8_t state;
    uint8_t flags;
    uint32_t mode;
    uint32_t nlink;
    uint64_t size;
    int64_t mtime;
    uint32_t mtimensec;
    int64_t ctime;
    uint32_t ctimensec;
    const char *path;
    uint32_t pathlen;
};

__BEGIN_DECLS

int virtiofs_errno(int32_t fuse_error);
int virtiofs_attr_ok(const struct fuse_attr *attr);
int virtiofs_entry_ok(const struct fuse_entry_out *entry);
int virtiofs_name_ok(const char *name, uint32_t len);
int virtiofs_dirent_next(const uint8_t *buf, uint32_t len, uint32_t *pos, int plus, struct virtiofs_dirent *d);
int virtiofs_xattr_list_ok(const char *list, uint32_t len);
int virtiofs_notify_path_ok(const char *path, uint32_t len);
int virtiofs_notify_next(const uint8_t *buf, uint32_t len, uint32_t *pos, struct virtiofs_notify_rec *rec);
uint64_t virtiofs_name_hash(const uint64_t key[2], uint64_t parent, const char *name, uint32_t len);

__END_DECLS

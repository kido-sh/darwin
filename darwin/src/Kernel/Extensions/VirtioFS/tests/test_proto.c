/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "fuse_proto.h"

static int failures;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            failures++;                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                               \
    } while (0)

static void
test_errno(void)
{
    static const struct {
        int32_t linux_errno;
        int want;
    } same[] = {
        { 1, EPERM }, { 2, ENOENT }, { 5, EIO }, { 9, EBADF }, { 12, ENOMEM }, { 13, EACCES }, { 16, EBUSY },
        { 17, EEXIST }, { 18, EXDEV }, { 20, ENOTDIR }, { 21, EISDIR }, { 22, EINVAL }, { 26, ETXTBSY },
        { 27, EFBIG }, { 28, ENOSPC }, { 30, EROFS }, { 31, EMLINK }, { 34, ERANGE }, { 11, EAGAIN },
        { 36, ENAMETOOLONG }, { 38, ENOSYS }, { 39, ENOTEMPTY }, { 40, ELOOP }, { 61, ENOATTR },
        { 95, ENOTSUP }, { 110, ETIMEDOUT }, { 116, ESTALE }, { 122, EDQUOT }, { 131, ENOTRECOVERABLE },
    };

    EXPECT(virtiofs_errno(0) == 0);
    for (size_t i = 0; i < sizeof(same) / sizeof(*same); i++)
        EXPECT(virtiofs_errno(-same[i].linux_errno) == same[i].want);
    EXPECT(virtiofs_errno(1) == EIO);
    EXPECT(virtiofs_errno(-41) == EIO);
    EXPECT(virtiofs_errno(-132) == EIO);
    EXPECT(virtiofs_errno(-100000) == EIO);
    EXPECT(virtiofs_errno(INT32_MIN) == EIO);
}

static void
test_attr(void)
{
    static const uint32_t types[] = { S_IFREG, S_IFDIR, S_IFLNK, S_IFIFO, S_IFCHR, S_IFBLK, S_IFSOCK };
    struct fuse_attr attr = { .mode = S_IFREG | 0644, .size = VIRTIOFS_SIZE_MAX };
    struct fuse_entry_out entry = { .nodeid = 2, .attr = attr };

    for (size_t i = 0; i < sizeof(types) / sizeof(*types); i++) {
        attr.mode = types[i] | 0755;
        EXPECT(virtiofs_attr_ok(&attr));
    }
    attr.mode = 0644;
    EXPECT(!virtiofs_attr_ok(&attr));
    attr.mode = S_IFMT;
    EXPECT(!virtiofs_attr_ok(&attr));
    attr.mode = S_IFREG;
    attr.size = VIRTIOFS_SIZE_MAX + 1;
    EXPECT(!virtiofs_attr_ok(&attr));
    attr.size = (uint64_t)INT64_MAX + 1;
    EXPECT(!virtiofs_attr_ok(&attr));

    EXPECT(virtiofs_entry_ok(&entry));
    entry.nodeid = 0;
    EXPECT(!virtiofs_entry_ok(&entry));
    entry.nodeid = 2;
    entry.attr.mode = 0;
    EXPECT(!virtiofs_entry_ok(&entry));
}

static void
test_names(void)
{
    char longest[VIRTIOFS_NAME_MAX + 2];

    memset(longest, 'n', sizeof(longest));
    EXPECT(virtiofs_name_ok("a", 1));
    EXPECT(virtiofs_name_ok(".", 1));
    EXPECT(virtiofs_name_ok(longest, VIRTIOFS_NAME_MAX));
    EXPECT(!virtiofs_name_ok(longest, VIRTIOFS_NAME_MAX + 1));
    EXPECT(!virtiofs_name_ok("", 0));
    EXPECT(!virtiofs_name_ok("a/b", 3));
    EXPECT(!virtiofs_name_ok("a\0b", 3));
}

static void
test_xattr_list(void)
{
    EXPECT(virtiofs_xattr_list_ok("", 0));
    EXPECT(virtiofs_xattr_list_ok("a", 2));
    EXPECT(virtiofs_xattr_list_ok("ab\0c", 5));
    EXPECT(!virtiofs_xattr_list_ok("a", 1));
    EXPECT(!virtiofs_xattr_list_ok("", 1));
    EXPECT(!virtiofs_xattr_list_ok("a\0", 3));
    EXPECT(!virtiofs_xattr_list_ok("a\0b", 3));
}

static uint32_t
put_dirent(uint8_t *buf, uint32_t at, uint64_t ino, const char *name, uint32_t namelen)
{
    struct fuse_dirent *d = (struct fuse_dirent *)(buf + at);
    d->ino = ino;
    d->off = at + 1;
    d->namelen = namelen;
    d->type = 8;
    memcpy(d->name, name, namelen);
    return at + (uint32_t)FUSE_DIRENT_SIZE(d);
}

static void
test_dirents(void)
{
    static uint64_t storage[512];
    uint8_t *buf = (uint8_t *)storage;
    struct virtiofs_dirent d;
    uint32_t pos = 0;

    uint32_t len = put_dirent(buf, 0, 7, "x", 1);
    len = put_dirent(buf, len, 9, "yz", 2);
    EXPECT(virtiofs_dirent_next(buf, len, &pos, 0, &d) == 1);
    EXPECT(d.ino == 7 && d.namelen == 1 && d.name[0] == 'x' && d.entry == NULL && d.type == 8);
    EXPECT(virtiofs_dirent_next(buf, len, &pos, 0, &d) == 1);
    EXPECT(d.ino == 9 && d.namelen == 2 && !memcmp(d.name, "yz", 2));
    EXPECT(pos == len);
    EXPECT(virtiofs_dirent_next(buf, len, &pos, 0, &d) == 0);

    pos = 0;
    EXPECT(virtiofs_dirent_next(buf, (uint32_t)FUSE_NAME_OFFSET - 1, &pos, 0, &d) == -1);
    pos = 0;
    EXPECT(virtiofs_dirent_next(buf, (uint32_t)FUSE_NAME_OFFSET + 1, &pos, 0, &d) == -1);
    pos = 0;
    EXPECT(virtiofs_dirent_next(buf, (uint32_t)FUSE_DIRENT_ALIGN(FUSE_NAME_OFFSET + 1), &pos, 0, &d) == 1);

    memset(storage, 0, sizeof(storage));
    len = put_dirent(buf, 0, 3, "a/b", 3);
    pos = 0;
    EXPECT(virtiofs_dirent_next(buf, len, &pos, 0, &d) == -1);
    len = put_dirent(buf, 0, 3, "", 0);
    pos = 0;
    EXPECT(virtiofs_dirent_next(buf, len, &pos, 0, &d) == -1);
    ((struct fuse_dirent *)buf)->namelen = VIRTIOFS_NAME_MAX + 1;
    pos = 0;
    EXPECT(virtiofs_dirent_next(buf, sizeof(storage), &pos, 0, &d) == -1);

    memset(storage, 0, sizeof(storage));
    struct fuse_direntplus *dp = (struct fuse_direntplus *)buf;
    dp->entry_out.nodeid = 5;
    dp->entry_out.attr.mode = S_IFREG | 0644;
    dp->dirent.ino = 11;
    dp->dirent.namelen = 4;
    memcpy(dp->dirent.name, "file", 4);
    len = (uint32_t)FUSE_DIRENTPLUS_SIZE(dp);
    pos = 0;
    EXPECT(virtiofs_dirent_next(buf, len, &pos, 1, &d) == 1);
    EXPECT(d.ino == 11 && d.namelen == 4 && d.entry == &dp->entry_out && pos == len);
    EXPECT(d.entry && virtiofs_entry_ok(d.entry));
    pos = 0;
    EXPECT(virtiofs_dirent_next(buf, len - 1, &pos, 1, &d) == -1);
}

static void
test_name_hash(void)
{
    /* SipHash-2-4 reference vectors: key 00..0f, messages 00..07 and 00..0e. */
    const uint64_t key[2] = {0x0706050403020100ULL, 0x0f0e0d0c0b0a0908ULL};
    const char tail[] = {8, 9, 10, 11, 12, 13, 14};
    EXPECT(virtiofs_name_hash(key, 0x0706050403020100ULL, "", 0) == 0x93f5f5799a932462ULL);
    EXPECT(virtiofs_name_hash(key, 0x0706050403020100ULL, tail, 7) == 0xa129ca6149be45e5ULL);
    EXPECT(virtiofs_name_hash(key, 1, "a", 1) != virtiofs_name_hash(key, 2, "a", 1));
}

int
main(void)
{
    test_errno();
    test_attr();
    test_names();
    test_xattr_list();
    test_dirents();
    test_name_hash();
    printf("virtiofs proto: %d failures\n", failures);
    return failures != 0;
}

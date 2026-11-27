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


/* The batch kido's internal/notify encodes in TestBatchGolden. */
static const uint8_t notify_golden[] = {
    0x38, 0x00, 0x01, 0x02, 0x00, 0x00, 0x08, 0x00, 0xa4, 0x81, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x86, 0xfd, 0x6a, 0x00, 0x00, 0x00, 0x00, 0xc1, 0x86, 0xfd, 0x6a, 0x00, 0x00, 0x00, 0x00, 0x15, 0xcd, 0x5b, 0x07, 0xb1, 0x68, 0xde, 0x3a, 0x64, 0x69, 0x72, 0x2f, 0x66, 0x69, 0x6c, 0x65, 0x34, 0x00, 0x01, 0x01, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x67, 0x6f, 0x6e, 0x65, 0x30, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void
put_notify(uint8_t *b, uint8_t kind, uint8_t state, const char *path)
{
    uint32_t n = (uint32_t)strlen(path), len = VIRTIOFS_NOTIFY_HEADER + n;
    memset(b, 0, VIRTIOFS_NOTIFY_HEADER);
    b[0] = (uint8_t)len;
    b[1] = (uint8_t)(len >> 8);
    b[2] = kind;
    b[3] = state;
    b[6] = (uint8_t)n;
    b[7] = (uint8_t)(n >> 8);
    memcpy(b + VIRTIOFS_NOTIFY_HEADER, path, n);
}

static void
test_notify(void)
{
    struct virtiofs_notify_rec r;
    uint32_t pos = 0, len = (uint32_t)sizeof(notify_golden);

    EXPECT(virtiofs_notify_next(notify_golden, len, &pos, &r) == 1);
    EXPECT(r.kind == VIRTIOFS_NOTIFY_PATH && r.state == VIRTIOFS_NOTIFY_PRESENT && r.mode == 0100644 && r.nlink == 1);
    EXPECT(r.size == 42 && r.mtime == 1795000000 && r.mtimensec == 123456789 && r.ctime == 1795000001 &&
           r.ctimensec == 987654321);
    EXPECT(r.pathlen == 8 && memcmp(r.path, "dir/file", 8) == 0);
    EXPECT(r.flags == 0);
    EXPECT(virtiofs_notify_next(notify_golden, len, &pos, &r) == 1);
    EXPECT(r.kind == VIRTIOFS_NOTIFY_PATH && r.state == VIRTIOFS_NOTIFY_ABSENT && r.flags == VIRTIOFS_NOTIFY_NAME &&
           r.pathlen == 4);
    EXPECT(virtiofs_notify_next(notify_golden, len, &pos, &r) == 1);
    EXPECT(r.kind == VIRTIOFS_NOTIFY_FLUSH && r.pathlen == 0);
    EXPECT(virtiofs_notify_next(notify_golden, len, &pos, &r) == 0 && pos == len);
    pos = 0;
    EXPECT(virtiofs_notify_next(notify_golden, len - 1, &pos, &r) == 1);
    EXPECT(virtiofs_notify_next(notify_golden, len - 1, &pos, &r) == 1);
    EXPECT(virtiofs_notify_next(notify_golden, len - 1, &pos, &r) == -1);

    static char deep[2 * VIRTIOFS_NOTIFY_MAX_DEPTH + 1], name[VIRTIOFS_NAME_MAX + 2];
    for (int i = 0; i < VIRTIOFS_NOTIFY_MAX_DEPTH; i++)
        memcpy(deep + 2 * i, "d/", 2);
    deep[2 * VIRTIOFS_NOTIFY_MAX_DEPTH - 1] = 0;
    memset(name, 'n', VIRTIOFS_NAME_MAX);
    EXPECT(virtiofs_notify_path_ok("a", 1) && virtiofs_notify_path_ok("a/b", 3) && virtiofs_notify_path_ok(".x", 2) &&
           virtiofs_notify_path_ok("x..", 3) && virtiofs_notify_path_ok("...", 3));
    EXPECT(virtiofs_notify_path_ok(deep, (uint32_t)strlen(deep)) && virtiofs_notify_path_ok(name, VIRTIOFS_NAME_MAX));
    deep[2 * VIRTIOFS_NOTIFY_MAX_DEPTH - 1] = '/';
    deep[2 * VIRTIOFS_NOTIFY_MAX_DEPTH] = 'd';
    EXPECT(!virtiofs_notify_path_ok(deep, 2 * VIRTIOFS_NOTIFY_MAX_DEPTH + 1));
    name[VIRTIOFS_NAME_MAX] = 'n';
    EXPECT(!virtiofs_notify_path_ok(name, VIRTIOFS_NAME_MAX + 1));
    const char *bad[] = {"", "/a", "a/", "a//b", ".", "..", "a/./b", "a/../b", "../a", "a/.."};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        EXPECT(!virtiofs_notify_path_ok(bad[i], (uint32_t)strlen(bad[i])));
    EXPECT(!virtiofs_notify_path_ok("a\0b", 3));

    uint8_t b[128];
    put_notify(b, VIRTIOFS_NOTIFY_PATH, VIRTIOFS_NOTIFY_ABSENT, "ok");
    pos = 0;
    EXPECT(virtiofs_notify_next(b, VIRTIOFS_NOTIFY_HEADER + 2, &pos, &r) == 1);
    b[4] = VIRTIOFS_NOTIFY_NAME | VIRTIOFS_NOTIFY_REPLACED;
    pos = 0;
    EXPECT(virtiofs_notify_next(b, VIRTIOFS_NOTIFY_HEADER + 2, &pos, &r) == 1 &&
           r.flags == (VIRTIOFS_NOTIFY_NAME | VIRTIOFS_NOTIFY_REPLACED));
    struct {
        int at;
        uint8_t value;
    } mutations[] = {{0, 49}, {2, 9}, {3, 3}, {4, 4}, {4, 0x80}, {5, 1}, {43, 0x3c}, {6, 3}, {48, '/'}, {49, 0}};
    for (size_t i = 0; i < sizeof(mutations) / sizeof(mutations[0]); i++) {
        put_notify(b, VIRTIOFS_NOTIFY_PATH, VIRTIOFS_NOTIFY_ABSENT, "ok");
        b[mutations[i].at] = mutations[i].value;
        pos = 0;
        EXPECT(virtiofs_notify_next(b, VIRTIOFS_NOTIFY_HEADER + 2, &pos, &r) == -1);
    }
    put_notify(b, VIRTIOFS_NOTIFY_FLUSH, 0, "");
    b[20] = 1;
    pos = 0;
    EXPECT(virtiofs_notify_next(b, VIRTIOFS_NOTIFY_HEADER, &pos, &r) == -1);
    put_notify(b, VIRTIOFS_NOTIFY_FLUSH, 0, "");
    b[4] = VIRTIOFS_NOTIFY_REPLACED;
    pos = 0;
    EXPECT(virtiofs_notify_next(b, VIRTIOFS_NOTIFY_HEADER, &pos, &r) == -1);
    put_notify(b, VIRTIOFS_NOTIFY_PATH, VIRTIOFS_NOTIFY_ABSENT, "..");
    pos = 0;
    EXPECT(virtiofs_notify_next(b, VIRTIOFS_NOTIFY_HEADER + 2, &pos, &r) == -1);
    pos = 0;
    EXPECT(virtiofs_notify_next(b, VIRTIOFS_NOTIFY_HEADER - 1, &pos, &r) == -1);
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
    test_notify();
    test_name_hash();
    printf("virtiofs proto: %d failures\n", failures);
    return failures != 0;
}

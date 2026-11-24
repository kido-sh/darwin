/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <sys/errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include "fuse_proto.h"

static const uint8_t from_linux[] = {
    [11] = EAGAIN, [35] = EDEADLK, [36] = ENAMETOOLONG, [37] = ENOLCK, [38] = ENOSYS,
    [39] = ENOTEMPTY, [40] = ELOOP, [42] = ENOMSG, [43] = EIDRM, [60] = ENOSTR, [61] = ENOATTR,
    [62] = ETIME, [63] = ENOSR, [66] = EREMOTE, [67] = ENOLINK, [71] = EPROTO, [72] = EMULTIHOP,
    [74] = EBADMSG, [75] = EOVERFLOW, [77] = EBADF, [84] = EILSEQ, [87] = EUSERS, [88] = ENOTSOCK,
    [89] = EDESTADDRREQ, [90] = EMSGSIZE, [91] = EPROTOTYPE, [92] = ENOPROTOOPT,
    [93] = EPROTONOSUPPORT, [94] = ESOCKTNOSUPPORT, [95] = ENOTSUP, [96] = EPFNOSUPPORT,
    [97] = EAFNOSUPPORT, [98] = EADDRINUSE, [99] = EADDRNOTAVAIL, [100] = ENETDOWN,
    [101] = ENETUNREACH, [102] = ENETRESET, [103] = ECONNABORTED, [104] = ECONNRESET,
    [105] = ENOBUFS, [106] = EISCONN, [107] = ENOTCONN, [108] = ESHUTDOWN, [109] = ETOOMANYREFS,
    [110] = ETIMEDOUT, [111] = ECONNREFUSED, [112] = EHOSTDOWN, [113] = EHOSTUNREACH,
    [114] = EALREADY, [115] = EINPROGRESS, [116] = ESTALE, [122] = EDQUOT, [125] = ECANCELED,
    [130] = EOWNERDEAD, [131] = ENOTRECOVERABLE,
};

int
virtiofs_errno(int32_t fuse_error)
{
    if (fuse_error == 0)
        return 0;
    if (fuse_error > 0 || fuse_error <= -(int32_t)sizeof(from_linux))
        return EIO;
    int linux_errno = -fuse_error;
    if (from_linux[linux_errno])
        return from_linux[linux_errno];
    return linux_errno <= ERANGE ? linux_errno : EIO;
}

int
virtiofs_attr_ok(const struct fuse_attr *attr)
{
    if (attr->size > INT64_MAX)
        return 0;
    switch (attr->mode & S_IFMT) {
    case S_IFREG: case S_IFDIR: case S_IFLNK: case S_IFIFO: case S_IFCHR: case S_IFBLK: case S_IFSOCK:
        return 1;
    default:
        return 0;
    }
}

int
virtiofs_entry_ok(const struct fuse_entry_out *entry)
{
    return entry->nodeid != 0 && virtiofs_attr_ok(&entry->attr);
}

int
virtiofs_name_ok(const char *name, uint32_t len)
{
    if (len == 0 || len > VIRTIOFS_NAME_MAX)
        return 0;
    for (uint32_t i = 0; i < len; i++) {
        if (name[i] == '/' || name[i] == '\0')
            return 0;
    }
    return 1;
}

int
virtiofs_dirent_next(const uint8_t *buf, uint32_t len, uint32_t *pos, int plus, struct virtiofs_dirent *d)
{
    uint32_t at = *pos;
    size_t head = plus ? FUSE_NAME_OFFSET_DIRENTPLUS : FUSE_NAME_OFFSET;

    if (at >= len)
        return 0;
    if (len - at < head)
        return -1;

    const struct fuse_direntplus *dp = (const struct fuse_direntplus *)(buf + at);
    const struct fuse_dirent *fd = plus ? &dp->dirent : (const struct fuse_dirent *)(buf + at);
    if (fd->namelen > VIRTIOFS_NAME_MAX)
        return -1;
    size_t record = plus ? FUSE_DIRENTPLUS_SIZE(dp) : FUSE_DIRENT_SIZE(fd);
    if (record > len - at || !virtiofs_name_ok(fd->name, fd->namelen))
        return -1;

    d->ino = fd->ino;
    d->off = fd->off;
    d->type = fd->type;
    d->namelen = fd->namelen;
    d->name = fd->name;
    d->entry = plus ? &dp->entry_out : NULL;
    *pos = at + (uint32_t)record;
    return 1;
}

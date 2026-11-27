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
    int mapped = from_linux[linux_errno];
    if (mapped)
        return mapped;
    return linux_errno <= ERANGE ? linux_errno : EIO;
}

int
virtiofs_attr_ok(const struct fuse_attr *attr)
{
    if (attr->size > VIRTIOFS_SIZE_MAX)
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
virtiofs_xattr_list_ok(const char *list, uint32_t len)
{
    uint32_t start = 0;

    for (uint32_t i = 0; i < len; i++) {
        if (list[i] != '\0')
            continue;
        if (i == start)
            return 0;
        start = i + 1;
    }
    return start == len;
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

/* A path below the mount's root: components a name may be, never "." or "..", at most MAX_DEPTH. */
int
virtiofs_notify_path_ok(const char *path, uint32_t len)
{
    uint32_t start = 0, depth = 0;

    if (len == 0 || len > VIRTIOFS_NOTIFY_MAX_PATH)
        return 0;
    for (uint32_t i = 0; i <= len; i++) {
        if (i < len && path[i] != '/')
            continue;
        uint32_t n = i - start;
        if (!virtiofs_name_ok(path + start, n) || ++depth > VIRTIOFS_NOTIFY_MAX_DEPTH)
            return 0;
        if (path[start] == '.' && (n == 1 || (n == 2 && path[start + 1] == '.')))
            return 0;
        start = i + 1;
    }
    return 1;
}

static uint16_t
le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

static uint32_t
le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t
le64(const uint8_t *p)
{
    return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32;
}

/* 1 with the record at *pos, 0 at the end, -1 if it is malformed, which refuses the whole batch. */
int
virtiofs_notify_next(const uint8_t *buf, uint32_t len, uint32_t *pos, struct virtiofs_notify_rec *rec)
{
    if (*pos == len)
        return 0;
    if (*pos > len || len - *pos < VIRTIOFS_NOTIFY_HEADER)
        return -1;
    const uint8_t *p = buf + *pos;
    uint32_t n = le16(p), plen = le16(p + 6);
    if (n != VIRTIOFS_NOTIFY_HEADER + plen || n > len - *pos || (p[4] & ~(VIRTIOFS_NOTIFY_NAME | VIRTIOFS_NOTIFY_REPLACED)) ||
        p[5])
        return -1;
    rec->kind = p[2];
    rec->state = p[3];
    rec->flags = p[4];
    rec->mode = le32(p + 8);
    rec->nlink = le32(p + 12);
    rec->size = le64(p + 16);
    rec->mtime = (int64_t)le64(p + 24);
    rec->ctime = (int64_t)le64(p + 32);
    rec->mtimensec = le32(p + 40);
    rec->ctimensec = le32(p + 44);
    rec->path = (const char *)p + VIRTIOFS_NOTIFY_HEADER;
    rec->pathlen = plen;
    if (rec->state > VIRTIOFS_NOTIFY_PRESENT || rec->mtimensec >= 1000000000 || rec->ctimensec >= 1000000000)
        return -1;
    if (rec->kind == VIRTIOFS_NOTIFY_FLUSH) {
        if (plen || rec->state || rec->flags || rec->mode || rec->nlink || rec->size || rec->mtime || rec->mtimensec ||
            rec->ctime || rec->ctimensec)
            return -1;
    } else if (rec->kind != VIRTIOFS_NOTIFY_PATH || !virtiofs_notify_path_ok(rec->path, plen)) {
        return -1;
    }
    *pos += n;
    return 1;
}

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

struct sip {
    uint64_t v0, v1, v2, v3, tail;
    uint32_t ntail, total;
};

static void
sip_word(struct sip *s, uint64_t m)
{
    s->v3 ^= m;
    for (int i = 0; i < 2; i++) {
        s->v0 += s->v1;
        s->v1 = ROTL(s->v1, 13);
        s->v1 ^= s->v0;
        s->v0 = ROTL(s->v0, 32);
        s->v2 += s->v3;
        s->v3 = ROTL(s->v3, 16);
        s->v3 ^= s->v2;
        s->v0 += s->v3;
        s->v3 = ROTL(s->v3, 21);
        s->v3 ^= s->v0;
        s->v2 += s->v1;
        s->v1 = ROTL(s->v1, 17);
        s->v1 ^= s->v2;
        s->v2 = ROTL(s->v2, 32);
    }
    s->v0 ^= m;
}

static void
sip_byte(struct sip *s, uint8_t b)
{
    s->tail |= (uint64_t)b << (8 * s->ntail);
    s->total++;
    if (++s->ntail == 8) {
        sip_word(s, s->tail);
        s->tail = 0;
        s->ntail = 0;
    }
}

/* SipHash-2-4 of the parent's node id (little-endian) followed by the name, under a per-mount key. */
uint64_t
virtiofs_name_hash(const uint64_t key[2], uint64_t parent, const char *name, uint32_t len)
{
    struct sip s = {
        .v0 = key[0] ^ 0x736f6d6570736575ULL,
        .v1 = key[1] ^ 0x646f72616e646f6dULL,
        .v2 = key[0] ^ 0x6c7967656e657261ULL,
        .v3 = key[1] ^ 0x7465646279746573ULL,
    };
    for (int i = 0; i < 8; i++)
        sip_byte(&s, (uint8_t)(parent >> (8 * i)));
    for (uint32_t i = 0; i < len; i++)
        sip_byte(&s, (uint8_t)name[i]);
    sip_word(&s, s.tail | (uint64_t)s.total << 56);
    s.v2 ^= 0xff;
    for (int i = 0; i < 2; i++)
        sip_word(&s, 0);
    return s.v0 ^ s.v1 ^ s.v2 ^ s.v3;
}


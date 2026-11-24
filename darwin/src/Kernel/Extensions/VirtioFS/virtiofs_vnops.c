/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/ubc.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode_if.h>
#include <kern/clock.h>
#include <IOKit/IOLib.h>
#include <libkern/libkern.h>
#include "fuse_proto.h"
#include "virtiofs.h"

#define DIRENT_LEN(namlen)   ((sizeof(struct dirent) + (namlen) + 1 - (__DARWIN_MAXNAMLEN + 1) + 3) & ~3)
#define DIRENT64_LEN(namlen) ((sizeof(struct direntry) + (namlen) - (MAXPATHLEN - 1) + 7) & ~7)

enum {
    VN_BUILDING = 0x01,
    VN_WANT = 0x02,
    VN_HASHED = 0x04,
    VN_OPEN = 0x08,
    VN_OPENING = 0x10,
    VN_LISTING = 0x20,
    VN_CLOSING = 0x40,
    VN_SIZING = 0x80,
};

struct virtiofs_node {
    LIST_ENTRY(virtiofs_node) link;
    struct virtiofs_mount *vmp;
    vnode_t vp;
    uint64_t nodeid;
    uint64_t nlookup;
    uint64_t fh;
    uint64_t expire;
    uint64_t attr_version;
    struct fuse_attr attr;
    uint32_t users;
    uint32_t flags;
};

#define VTON(vp) ((struct virtiofs_node *)vnode_fsnode(vp))

int (**virtiofs_vnodeop_p)(void *);

static struct virtiofs_bucket *
bucket(struct virtiofs_mount *vmp, uint64_t nodeid)
{
    return &vmp->hash[(nodeid * 0x9e3779b97f4a7c15ULL) >> (64 - VIRTIOFS_HASH_BITS)];
}

static uint64_t
deadline(uint64_t sec, uint32_t nsec)
{
    uint64_t interval;

    if (sec >= 86400) {
        sec = 86400;
        nsec = 0;
    }
    nanoseconds_to_absolutetime(sec * NSEC_PER_SEC + (nsec < NSEC_PER_SEC ? nsec : 0), &interval);
    return mach_absolute_time() + interval;
}

static void
wake(struct virtiofs_node *np, uint32_t done)
{
    np->flags &= ~done;
    if (np->flags & VN_WANT) {
        np->flags &= ~VN_WANT;
        IOLockWakeup(np->vmp->lock, np, false);
    }
}

static void
wait_clear(struct virtiofs_node *np, uint32_t busy)
{
    while (np->flags & busy) {
        np->flags |= VN_WANT;
        IOLockSleep(np->vmp->lock, np, THREAD_UNINT);
    }
}

static bool
attr_changed(const struct fuse_attr *a, const struct fuse_attr *b)
{
    return a->size != b->size || a->mtime != b->mtime || a->mtimensec != b->mtimensec || a->ctime != b->ctime ||
           a->ctimensec != b->ctimensec;
}

uint64_t
virtiofs_attr_version(struct virtiofs_mount *vmp)
{
    IOLockLock(vmp->lock);
    uint64_t version = vmp->attr_version;
    IOLockUnlock(vmp->lock);
    return version;
}

static void
node_apply(struct virtiofs_node *np, const struct fuse_attr *attr, uint64_t sec, uint32_t nsec, uint64_t version)
{
    struct virtiofs_mount *vmp = np->vmp;
    vnode_t vp = np->vp;

    IOLockLock(vmp->lock);
    if (np->attr_version > version) {
        IOLockUnlock(vmp->lock);
        return;
    }
    bool changed = attr_changed(&np->attr, attr);
    np->attr = *attr;
    np->expire = deadline(sec, nsec);
    np->attr_version = ++vmp->attr_version;
    if (!changed || !vnode_isreg(vp)) {
        IOLockUnlock(vmp->lock);
        return;
    }
    wait_clear(np, VN_SIZING);
    np->flags |= VN_SIZING;
    off_t size = (off_t)np->attr.size;
    IOLockUnlock(vmp->lock);

    off_t cached = ubc_getsize(vp);
    if (cached != size)
        ubc_setsize(vp, size);
    ubc_msync(vp, 0, cached > size ? cached : size, NULL, UBC_INVALIDATE);

    IOLockLock(vmp->lock);
    wake(np, VN_SIZING);
    IOLockUnlock(vmp->lock);
}

static int
node_attr(struct virtiofs_node *np, vfs_context_t ctx, struct fuse_attr *attr)
{
    IOLockLock(np->vmp->lock);
    bool cached = mach_absolute_time() < np->expire;
    if (cached)
        *attr = np->attr;
    IOLockUnlock(np->vmp->lock);
    if (cached)
        return 0;

    struct fuse_getattr_in in = {};
    struct fuse_attr_out out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_GETATTR, .nodeid = np->nodeid,
        .arg = { &in }, .arglen = { sizeof(in) }, .reply = &out, .replylen = sizeof(out),
    };
    uint64_t version = virtiofs_attr_version(np->vmp);
    int error = virtiofs_call(np->vmp, ctx, &c);
    if (!error && (c.replylen != sizeof(out) || !virtiofs_attr_ok(&out.attr) ||
                   IFTOVT(out.attr.mode) != vnode_vtype(np->vp)))
        error = EIO;
    if (error)
        return error;
    node_apply(np, &out.attr, out.attr_valid, out.attr_valid_nsec, version);
    IOLockLock(np->vmp->lock);
    *attr = np->attr;
    IOLockUnlock(np->vmp->lock);
    return 0;
}

static off_t
node_size(struct virtiofs_node *np)
{
    IOLockLock(np->vmp->lock);
    off_t size = (off_t)np->attr.size;
    IOLockUnlock(np->vmp->lock);
    return size;
}

static void
node_rename(vnode_t vp, vnode_t dvp, struct componentname *cnp)
{
    vnode_t parent = vnode_getparent(vp);
    const char *name = vnode_getname(vp);
    bool same = parent == dvp && name && strnlen(name, cnp->cn_namelen + 1) == (size_t)cnp->cn_namelen &&
                strncmp(name, cnp->cn_nameptr, cnp->cn_namelen) == 0;

    if (name)
        vnode_putname(name);
    if (parent)
        vnode_put(parent);
    if (!same)
        vnode_update_identity(vp, dvp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash,
                              VNODE_UPDATE_PARENT | VNODE_UPDATE_NAME);
}

static void
forget(struct virtiofs_mount *vmp, uint64_t nodeid, uint64_t nlookup)
{
    if (nodeid && nlookup)
        virtiofs_dev_forget(vmp->dev, nodeid, nlookup);
}

static int
node_found(struct virtiofs_node *np, vnode_t vp, const struct fuse_entry_out *entry, uint64_t version,
           uint64_t nlookup, vnode_t dvp, struct componentname *cnp)
{
    if (vnode_vtype(vp) != IFTOVT(entry->attr.mode)) {
        vnode_put(vp);
        forget(np->vmp, entry->nodeid, nlookup);
        return EIO;
    }
    IOLockLock(np->vmp->lock);
    np->nlookup += nlookup;
    IOLockUnlock(np->vmp->lock);
    node_apply(np, &entry->attr, entry->attr_valid, entry->attr_valid_nsec, version);
    if (dvp && cnp && vp != dvp)
        node_rename(vp, dvp, cnp);
    return 0;
}

int
virtiofs_node_get(struct virtiofs_mount *vmp, const struct fuse_entry_out *entry, uint64_t version,
                  uint64_t nlookup, vnode_t dvp, struct componentname *cnp, vnode_t *vpp)
{
    struct virtiofs_bucket *head = bucket(vmp, entry->nodeid);
    struct virtiofs_node *np, *fresh = NULL;
    vnode_t vp;
    int error;

    error = virtiofs_entry_ok(entry) ? ENOMEM : EIO;
    if (error == EIO || !(fresh = IOMallocZero(sizeof(*fresh)))) {
        forget(vmp, entry->nodeid, nlookup);
        return error;
    }

    IOLockLock(vmp->lock);
    for (;;) {
        LIST_FOREACH(np, head, link) {
            if (np->nodeid == entry->nodeid)
                break;
        }
        if (!np)
            break;
        if (np->flags & VN_BUILDING) {
            np->flags |= VN_WANT;
            IOLockSleep(vmp->lock, np, THREAD_UNINT);
            continue;
        }
        vp = np->vp;
        uint32_t vid = vnode_vid(vp);
        IOLockUnlock(vmp->lock);
        if (vnode_getwithvid(vp, vid) == 0) {
            IOFree(fresh, sizeof(*fresh));
            error = node_found(np, vp, entry, version, nlookup, dvp, cnp);
            if (!error)
                *vpp = vp;
            return error;
        }
        IOLockLock(vmp->lock);
    }

    np = fresh;
    np->vmp = vmp;
    np->nodeid = entry->nodeid;
    np->nlookup = nlookup;
    np->attr = entry->attr;
    np->expire = deadline(entry->attr_valid, entry->attr_valid_nsec);
    np->attr_version = ++vmp->attr_version;
    np->flags = VN_BUILDING | VN_HASHED;
    LIST_INSERT_HEAD(head, np, link);
    IOLockUnlock(vmp->lock);

    struct vnode_fsparam param = {
        .vnfs_mp = vmp->mp,
        .vnfs_vtype = IFTOVT(entry->attr.mode),
        .vnfs_str = "virtiofs",
        .vnfs_dvp = dvp,
        .vnfs_fsnode = np,
        .vnfs_vops = virtiofs_vnodeop_p,
        .vnfs_markroot = entry->nodeid == vmp->rootid,
        .vnfs_filesize = (off_t)entry->attr.size,
        .vnfs_cnp = cnp,
        .vnfs_flags = VNFS_NOCACHE | VNFS_CANTCACHE | VNFS_ADDFSREF,
    };
    error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &param, &vp);

    IOLockLock(vmp->lock);
    if (error)
        LIST_REMOVE(np, link);
    else
        np->vp = vp;
    wake(np, VN_BUILDING | (error ? VN_HASHED : 0));
    IOLockUnlock(vmp->lock);
    if (!error) {
        *vpp = vp;
        return 0;
    }
    IOFree(np, sizeof(*np));
    forget(vmp, entry->nodeid, nlookup);
    return error;
}

static int
handle_get(struct virtiofs_node *np, vfs_context_t ctx, uint64_t *fh)
{
    struct virtiofs_mount *vmp = np->vmp;

    IOLockLock(vmp->lock);
    wait_clear(np, VN_OPENING);
    if (np->flags & VN_OPEN) {
        np->users++;
        np->flags &= ~VN_CLOSING;
        *fh = np->fh;
        IOLockUnlock(vmp->lock);
        return 0;
    }
    np->flags |= VN_OPENING;
    IOLockUnlock(vmp->lock);

    struct fuse_open_in in = {};
    struct fuse_open_out out = {};
    struct virtiofs_call c = {
        .opcode = vnode_isdir(np->vp) ? FUSE_OPENDIR : FUSE_OPEN, .nodeid = np->nodeid,
        .arg = { &in }, .arglen = { sizeof(in) }, .reply = &out, .replylen = sizeof(out),
    };
    int error = virtiofs_call(vmp, ctx, &c);
    if (!error && c.replylen != sizeof(out))
        error = EIO;

    IOLockLock(vmp->lock);
    if (!error) {
        np->fh = out.fh;
        np->flags |= VN_OPEN;
        np->users++;
        *fh = out.fh;
    }
    wake(np, VN_OPENING);
    IOLockUnlock(vmp->lock);
    return error;
}

static void
handle_put(struct virtiofs_node *np, bool close)
{
    IOLockLock(np->vmp->lock);
    if (!close)
        np->users--;
    else if (np->flags & VN_OPEN)
        np->flags |= VN_CLOSING;
    bool release = np->users == 0 && (np->flags & VN_CLOSING);
    struct fuse_release_in in = { .fh = np->fh };
    if (release)
        np->flags &= ~(VN_OPEN | VN_CLOSING);
    IOLockUnlock(np->vmp->lock);
    if (!release)
        return;

    struct virtiofs_call c = {
        .opcode = vnode_isdir(np->vp) ? FUSE_RELEASEDIR : FUSE_RELEASE, .nodeid = np->nodeid,
        .arg = { &in }, .arglen = { sizeof(in) },
    };
    (void)virtiofs_call(np->vmp, NULL, &c);
}

static int
handle_check(struct virtiofs_node *np, vfs_context_t ctx)
{
    uint64_t fh;
    int error = handle_get(np, ctx, &fh);

    if (!error)
        handle_put(np, false);
    return error;
}

static int
lookup_name(struct virtiofs_node *dnp, vfs_context_t ctx, const char *name, uint32_t len,
            struct fuse_entry_out *entry, uint64_t *version)
{
    struct virtiofs_call c = {
        .opcode = FUSE_LOOKUP, .nodeid = dnp->nodeid,
        .arg = { name, "" }, .arglen = { len, 1 }, .reply = entry, .replylen = sizeof(*entry),
    };
    *version = virtiofs_attr_version(dnp->vmp);
    int error = virtiofs_call(dnp->vmp, ctx, &c);
    if (!error && c.replylen != sizeof(*entry))
        error = EIO;
    if (!error && entry->nodeid == 0)
        error = ENOENT;
    return error;
}

static int
virtiofs_vnop_lookup(struct vnop_lookup_args *ap)
{
    vnode_t dvp = ap->a_dvp;
    struct componentname *cnp = ap->a_cnp;
    struct virtiofs_node *dnp = VTON(dvp);
    struct fuse_entry_out entry;
    uint64_t version;

    *ap->a_vpp = NULLVP;
    if (!vnode_isdir(dvp))
        return ENOTDIR;
    if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
        int error = vnode_get(dvp);
        if (!error)
            *ap->a_vpp = dvp;
        return error;
    }
    if (cnp->cn_flags & ISDOTDOT) {
        vnode_t parent = vnode_getparent(dvp);
        if (parent) {
            *ap->a_vpp = parent;
            return 0;
        }
        int error = lookup_name(dnp, ap->a_context, "..", 2, &entry, &version);
        return error ? error : virtiofs_node_get(dnp->vmp, &entry, version, 1, NULLVP, NULL, ap->a_vpp);
    }
    if (!virtiofs_name_ok(cnp->cn_nameptr, (uint32_t)cnp->cn_namelen))
        return cnp->cn_namelen > VIRTIOFS_NAME_MAX ? ENAMETOOLONG : EINVAL;

    int error = lookup_name(dnp, ap->a_context, cnp->cn_nameptr, (uint32_t)cnp->cn_namelen, &entry, &version);
    if (error == ENOENT && (cnp->cn_flags & ISLASTCN) &&
        (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
        return EJUSTRETURN;
    if (error)
        return error;
    return virtiofs_node_get(dnp->vmp, &entry, version, 1, dvp, cnp, ap->a_vpp);
}

static int
virtiofs_vnop_open(struct vnop_open_args *ap)
{
    vnode_t vp = ap->a_vp;

    switch (vnode_vtype(vp)) {
    case VREG:
    case VDIR:
        if (ap->a_mode & (FWRITE | O_TRUNC))
            return EROFS;
        return handle_check(VTON(vp), ap->a_context);
    case VLNK:
        return 0;
    case VCHR:
    case VBLK:
        return ENXIO;
    default:
        return ENOTSUP;
    }
}

static int
virtiofs_vnop_close(__unused struct vnop_close_args *ap)
{
    return 0;
}

static int
virtiofs_vnop_getattr(struct vnop_getattr_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct vnode_attr *vap = ap->a_vap;
    struct virtiofs_node *np = VTON(vp);
    kauth_cred_t cred = vfs_context_ucred(ap->a_context);
    struct fuse_attr attr;

    int error = node_attr(np, ap->a_context, &attr);
    if (error)
        return error;
    VATTR_RETURN(vap, va_rdev, (dev_t)attr.rdev);
    VATTR_RETURN(vap, va_nlink, attr.nlink);
    VATTR_RETURN(vap, va_data_size, (off_t)attr.size);
    VATTR_RETURN(vap, va_total_size, (off_t)attr.size);
    VATTR_RETURN(vap, va_data_alloc, attr.blocks * 512);
    VATTR_RETURN(vap, va_total_alloc, attr.blocks * 512);
    VATTR_RETURN(vap, va_iosize, np->vmp->session.max_io);
    VATTR_RETURN(vap, va_uid, kauth_cred_getuid(cred));
    VATTR_RETURN(vap, va_gid, kauth_cred_getgid(cred));
    VATTR_RETURN(vap, va_mode, (mode_t)(attr.mode & ALLPERMS));
    VATTR_RETURN(vap, va_flags, 0);
    VATTR_RETURN(vap, va_fileid, attr.ino);
    VATTR_RETURN(vap, va_fsid, vfs_statfs(vnode_mount(vp))->f_fsid.val[0]);
    vap->va_access_time.tv_sec = (time_t)attr.atime;
    vap->va_access_time.tv_nsec = attr.atimensec;
    VATTR_SET_SUPPORTED(vap, va_access_time);
    vap->va_modify_time.tv_sec = (time_t)attr.mtime;
    vap->va_modify_time.tv_nsec = attr.mtimensec;
    VATTR_SET_SUPPORTED(vap, va_modify_time);
    vap->va_change_time.tv_sec = (time_t)attr.ctime;
    vap->va_change_time.tv_nsec = attr.ctimensec;
    VATTR_SET_SUPPORTED(vap, va_change_time);
    return 0;
}

static int
virtiofs_vnop_read(struct vnop_read_args *ap)
{
    vnode_t vp = ap->a_vp;
    uio_t uio = ap->a_uio;
    struct virtiofs_node *np = VTON(vp);
    struct fuse_attr attr;

    if (vnode_isdir(vp))
        return EISDIR;
    if (!vnode_isreg(vp))
        return EPERM;
    if (uio_offset(uio) < 0)
        return EINVAL;
    if (uio_resid(uio) == 0)
        return 0;
    int error = handle_check(np, ap->a_context);
    if (!error)
        error = node_attr(np, ap->a_context, &attr);
    if (error)
        return error;
    return cluster_read(vp, uio, (off_t)attr.size, ap->a_ioflag);
}

static int
virtiofs_vnop_pagein(struct vnop_pagein_args *ap)
{
    return cluster_pagein(ap->a_vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset, (int)ap->a_size,
                          node_size(VTON(ap->a_vp)), ap->a_flags);
}

static int
virtiofs_vnop_pageout(struct vnop_pageout_args *ap)
{
    if (ap->a_pl && !(ap->a_flags & UPL_NOCOMMIT))
        ubc_upl_commit_range(ap->a_pl, ap->a_pl_offset, (upl_size_t)ap->a_size,
                             UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY);
    return 0;
}

static int
virtiofs_vnop_blockmap(struct vnop_blockmap_args *ap)
{
    uint32_t bsize = vfs_devblocksize(vnode_mount(ap->a_vp));

    if (ap->a_foffset % bsize)
        return EINVAL;
    if (ap->a_bpn)
        *ap->a_bpn = ap->a_foffset / bsize;
    if (ap->a_run)
        *ap->a_run = ap->a_size;
    if (ap->a_poff)
        *(int *)ap->a_poff = 0;
    return 0;
}

static int
virtiofs_vnop_strategy(struct vnop_strategy_args *ap)
{
    buf_t bp = ap->a_bp;
    vnode_t vp = buf_vnode(bp);
    struct virtiofs_node *np = VTON(vp);
    uint32_t count = buf_count(bp);
    uint32_t max = np->vmp->session.max_io;
    off_t offset = buf_blkno(bp) * vfs_devblocksize(vnode_mount(vp));
    uint64_t fh = 0;
    int error = (buf_flags(bp) & B_READ) ? handle_get(np, NULL, &fh) : EROFS;
    bool held = !error;

    for (uint32_t done = 0; !error && done < count; done += max) {
        uint32_t chunk = count - done < max ? count - done : max;
        struct fuse_read_in in = { .fh = fh, .offset = (uint64_t)offset + done, .size = chunk };
        struct virtiofs_call c = {
            .opcode = FUSE_READ, .flags = VIRTIOFS_ZERO_FILL, .nodeid = np->nodeid,
            .arg = { &in }, .arglen = { sizeof(in) }, .datalen = chunk,
        };
        if (buf_upl(bp)) {
            c.upl = buf_upl(bp);
            c.upl_offset = buf_uploffset(bp) + done;
        } else {
            c.data = (void *)(buf_dataptr(bp) + done);
        }
        error = virtiofs_call(np->vmp, NULL, &c);
    }
    if (held)
        handle_put(np, false);
    buf_seterror(bp, error);
    buf_setresid(bp, error ? count : 0);
    buf_biodone(bp);
    return 0;
}

static int
virtiofs_vnop_mmap(__unused struct vnop_mmap_args *ap)
{
    return 0;
}

static int
virtiofs_vnop_fsync(__unused struct vnop_fsync_args *ap)
{
    return 0;
}

static uint8_t
dirent_type(uint32_t type)
{
    switch (type) {
    case DT_FIFO: case DT_CHR: case DT_DIR: case DT_BLK: case DT_REG: case DT_LNK: case DT_SOCK: case DT_WHT:
        return (uint8_t)type;
    default:
        return DT_UNKNOWN;
    }
}

static int
emit(uio_t uio, bool extended, const struct virtiofs_dirent *d, bool *full)
{
    union {
        struct dirent old;
        struct direntry ext;
    } rec;
    uint32_t len = extended ? DIRENT64_LEN(d->namelen) : DIRENT_LEN(d->namelen);

    if (len > uio_resid(uio)) {
        *full = true;
        return 0;
    }
    bzero(&rec, len);
    if (extended) {
        rec.ext.d_ino = d->ino;
        rec.ext.d_seekoff = d->off;
        rec.ext.d_reclen = (uint16_t)len;
        rec.ext.d_namlen = (uint16_t)d->namelen;
        rec.ext.d_type = dirent_type(d->type);
        memcpy(rec.ext.d_name, d->name, d->namelen);
    } else {
        rec.old.d_ino = (ino_t)d->ino;
        rec.old.d_reclen = (uint16_t)len;
        rec.old.d_namlen = (uint8_t)d->namelen;
        rec.old.d_type = dirent_type(d->type);
        memcpy(rec.old.d_name, d->name, d->namelen);
    }
    return uiomove((caddr_t)&rec, (int)len, uio);
}

static int
virtiofs_vnop_readdir(struct vnop_readdir_args *ap)
{
    vnode_t vp = ap->a_vp;
    uio_t uio = ap->a_uio;
    struct virtiofs_node *np = VTON(vp);
    struct virtiofs_mount *vmp = np->vmp;
    bool extended = (ap->a_flags & VNODE_READDIR_EXTENDED) != 0;
    user_ssize_t resid = uio_resid(uio);
    int count = 0, eof = 0;
    uint64_t fh;

    if (ap->a_flags & VNODE_READDIR_SEEKOFF32)
        return EINVAL;
    if (!vnode_isdir(vp))
        return ENOTDIR;
    if (resid <= 0)
        return EINVAL;
    uint32_t size = resid < vmp->session.max_io ? (uint32_t)resid : vmp->session.max_io;
    uint8_t *buf = IOMalloc(size);
    if (!buf)
        return ENOMEM;
    int error = handle_get(np, ap->a_context, &fh);
    if (error) {
        IOFree(buf, size);
        return error;
    }
    uint64_t cookie = (uint64_t)uio_offset(uio);
    struct fuse_read_in in = { .fh = fh, .offset = cookie, .size = size };
    struct virtiofs_call c = {
        .opcode = FUSE_READDIR, .nodeid = np->nodeid,
        .arg = { &in }, .arglen = { sizeof(in) }, .datalen = size, .data = buf,
    };
    IOLockLock(vmp->lock);
    wait_clear(np, VN_LISTING);
    np->flags |= VN_LISTING;
    IOLockUnlock(vmp->lock);
    error = virtiofs_call(vmp, ap->a_context, &c);
    IOLockLock(vmp->lock);
    wake(np, VN_LISTING);
    IOLockUnlock(vmp->lock);
    handle_put(np, false);

    if (!error) {
        struct virtiofs_dirent d;
        uint32_t pos = 0;
        bool full = false;
        int more = 0;
        while (!error && !full && (more = virtiofs_dirent_next(buf, c.datalen, &pos, 0, &d)) == 1) {
            error = emit(uio, extended, &d, &full);
            if (!error && !full) {
                count++;
                cookie = d.off;
            }
        }
        if (!error && !full && more < 0)
            error = EIO;
        if (!error && count == 0 && c.datalen > 0)
            error = EINVAL;
        eof = c.datalen == 0;
        uio_setoffset(uio, (off_t)cookie);
    }
    IOFree(buf, size);
    if (ap->a_eofflag)
        *ap->a_eofflag = eof;
    if (ap->a_numdirent)
        *ap->a_numdirent = count;
    return error;
}

static int
virtiofs_vnop_readlink(struct vnop_readlink_args *ap)
{
    struct virtiofs_node *np = VTON(ap->a_vp);

    if (!vnode_islnk(ap->a_vp))
        return EINVAL;
    char *buf = IOMalloc(MAXPATHLEN);
    if (!buf)
        return ENOMEM;
    struct virtiofs_call c = { .opcode = FUSE_READLINK, .nodeid = np->nodeid, .datalen = MAXPATHLEN, .data = buf };
    int error = virtiofs_call(np->vmp, ap->a_context, &c);
    if (!error && (c.datalen == 0 || c.datalen >= MAXPATHLEN || strnlen(buf, c.datalen) != c.datalen))
        error = EIO;
    if (!error)
        error = uiomove(buf, (int)c.datalen, ap->a_uio);
    IOFree(buf, MAXPATHLEN);
    return error;
}

static int
virtiofs_vnop_pathconf(struct vnop_pathconf_args *ap)
{
    switch (ap->a_name) {
    case _PC_LINK_MAX:
        *ap->a_retval = LINK_MAX;
        return 0;
    case _PC_NAME_MAX:
        *ap->a_retval = VIRTIOFS_NAME_MAX;
        return 0;
    case _PC_PATH_MAX:
        *ap->a_retval = MAXPATHLEN;
        return 0;
    case _PC_PIPE_BUF:
        *ap->a_retval = PIPE_BUF;
        return 0;
    case _PC_CHOWN_RESTRICTED:
    case _PC_NO_TRUNC:
        *ap->a_retval = 200112;
        return 0;
    case _PC_FILESIZEBITS:
        *ap->a_retval = 64;
        return 0;
    default:
        return EINVAL;
    }
}

static int
virtiofs_vnop_getxattr(__unused struct vnop_getxattr_args *ap)
{
    return ENOATTR;
}

static int
virtiofs_vnop_listxattr(struct vnop_listxattr_args *ap)
{
    if (!ap->a_uio)
        *ap->a_size = 0;
    return 0;
}

static int
virtiofs_vnop_inactive(struct vnop_inactive_args *ap)
{
    handle_put(VTON(ap->a_vp), true);
    return 0;
}

static int
virtiofs_vnop_reclaim(struct vnop_reclaim_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct virtiofs_node *np = VTON(vp);
    struct virtiofs_mount *vmp = np->vmp;

    handle_put(np, true);
    IOLockLock(vmp->lock);
    if (np->flags & VN_HASHED) {
        LIST_REMOVE(np, link);
        np->flags &= ~VN_HASHED;
    }
    uint64_t nlookup = np->nlookup;
    IOLockUnlock(vmp->lock);
    if (nlookup)
        virtiofs_dev_forget(vmp->dev, np->nodeid, nlookup);
    vnode_removefsref(vp);
    vnode_clearfsnode(vp);
    IOFree(np, sizeof(*np));
    return 0;
}

#define VOPFUNC int (*)(void *)

static const struct vnodeopv_entry_desc virtiofs_vnodeop_entries[] = {
    { &vnop_default_desc, (VOPFUNC)vn_default_error },
    { &vnop_lookup_desc, (VOPFUNC)virtiofs_vnop_lookup },
    { &vnop_open_desc, (VOPFUNC)virtiofs_vnop_open },
    { &vnop_close_desc, (VOPFUNC)virtiofs_vnop_close },
    { &vnop_getattr_desc, (VOPFUNC)virtiofs_vnop_getattr },
    { &vnop_read_desc, (VOPFUNC)virtiofs_vnop_read },
    { &vnop_pagein_desc, (VOPFUNC)virtiofs_vnop_pagein },
    { &vnop_pageout_desc, (VOPFUNC)virtiofs_vnop_pageout },
    { &vnop_blockmap_desc, (VOPFUNC)virtiofs_vnop_blockmap },
    { &vnop_strategy_desc, (VOPFUNC)virtiofs_vnop_strategy },
    { &vnop_mmap_desc, (VOPFUNC)virtiofs_vnop_mmap },
    { &vnop_fsync_desc, (VOPFUNC)virtiofs_vnop_fsync },
    { &vnop_readdir_desc, (VOPFUNC)virtiofs_vnop_readdir },
    { &vnop_readlink_desc, (VOPFUNC)virtiofs_vnop_readlink },
    { &vnop_pathconf_desc, (VOPFUNC)virtiofs_vnop_pathconf },
    { &vnop_getxattr_desc, (VOPFUNC)virtiofs_vnop_getxattr },
    { &vnop_listxattr_desc, (VOPFUNC)virtiofs_vnop_listxattr },
    { &vnop_inactive_desc, (VOPFUNC)virtiofs_vnop_inactive },
    { &vnop_reclaim_desc, (VOPFUNC)virtiofs_vnop_reclaim },
    { NULL, NULL }
};

struct vnodeopv_desc virtiofs_vnodeop_opv_desc = { &virtiofs_vnodeop_p, virtiofs_vnodeop_entries };

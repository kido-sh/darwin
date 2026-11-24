/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <IOKit/IOLib.h>
#include <libkern/libkern.h>
#include "fuse_proto.h"
#include "virtiofs.h"

#define VIRTIOFS_TAG_MAX 36

static vfstable_t virtiofs_vfsconf;

int
virtiofs_call(struct virtiofs_mount *vmp, vfs_context_t ctx, struct virtiofs_call *c)
{
    kauth_cred_t cred = ctx ? vfs_context_ucred(ctx) : NULL;

    return virtiofs_dev_call(vmp->dev, c, cred ? kauth_cred_getuid(cred) : 0, cred ? kauth_cred_getgid(cred) : 0,
                             ctx ? (uint32_t)vfs_context_pid(ctx) : 0);
}

static int
root_entry(struct virtiofs_mount *vmp, const char *share, vfs_context_t ctx, struct fuse_entry_out *entry,
           uint64_t *version)
{
    *version = virtiofs_attr_version(vmp);
    if (*share) {
        struct virtiofs_call c = {
            .opcode = FUSE_LOOKUP, .nodeid = FUSE_ROOT_ID,
            .arg = { share }, .arglen = { (uint32_t)strlen(share) + 1 }, .reply = entry, .replylen = sizeof(*entry),
        };
        int error = virtiofs_call(vmp, ctx, &c);
        if (!error && (c.replylen != sizeof(*entry) || entry->nodeid == 0))
            error = c.replylen == sizeof(*entry) ? ENOENT : EIO;
        if (!error && !S_ISDIR(entry->attr.mode)) {
            virtiofs_dev_forget(vmp->dev, entry->nodeid, 1);
            error = ENOTDIR;
        }
        return error;
    }

    struct fuse_getattr_in in = {};
    struct fuse_attr_out out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_GETATTR, .nodeid = FUSE_ROOT_ID,
        .arg = { &in }, .arglen = { sizeof(in) }, .reply = &out, .replylen = sizeof(out),
    };
    int error = virtiofs_call(vmp, ctx, &c);
    if (!error && c.replylen != sizeof(out))
        error = EIO;
    if (!error && !S_ISDIR(out.attr.mode))
        error = ENOTDIR;
    if (!error) {
        entry->nodeid = FUSE_ROOT_ID;
        entry->attr = out.attr;
        entry->attr_valid = out.attr_valid;
        entry->attr_valid_nsec = out.attr_valid_nsec;
    }
    return error;
}

static int
copyin_spec(user_addr_t data, vfs_context_t ctx, char *spec, size_t size)
{
    user_addr_t fspec;
    size_t len = 0;
    int error;

    if (vfs_context_is64bit(ctx)) {
        user64_addr_t p;
        error = copyin(data, &p, sizeof(p));
        fspec = (user_addr_t)p;
    } else {
        user32_addr_t p;
        error = copyin(data, &p, sizeof(p));
        fspec = (user_addr_t)p;
    }
    return error ? error : copyinstr(fspec, spec, size, &len);
}

static int
virtiofs_mount(mount_t mp, __unused vnode_t devvp, user_addr_t data, vfs_context_t ctx)
{
    char spec[VIRTIOFS_TAG_MAX + 1 + VIRTIOFS_NAME_MAX + 1];
    char tag[VIRTIOFS_TAG_MAX + 1];

    if (vfs_isupdate(mp))
        return ENOTSUP;
    if (!vfs_isrdonly(mp))
        return EROFS;
    int error = copyin_spec(data, ctx, spec, sizeof(spec));
    if (error)
        return error;

    const char *colon = strchr(spec, ':');
    size_t taglen = colon ? (size_t)(colon - spec) : strlen(spec);
    const char *share = colon ? colon + 1 : "";
    size_t sharelen = strlen(share);
    if (taglen == 0 || taglen > VIRTIOFS_TAG_MAX)
        return EINVAL;
    if (sharelen && (!virtiofs_name_ok(share, (uint32_t)sharelen) || !strcmp(share, ".") || !strcmp(share, "..")))
        return EINVAL;
    memcpy(tag, spec, taglen);
    tag[taglen] = '\0';

    struct virtiofs_dev *dev = virtiofs_dev_find(tag);
    if (!dev)
        return ENXIO;
    struct virtiofs_mount *vmp = IOMallocZero(sizeof(*vmp));
    IOLock *lock = IOLockAlloc();
    struct fuse_entry_out entry = {};
    uint64_t version = 0;
    vnode_t root = NULLVP;
    error = vmp && lock ? 0 : ENOMEM;
    if (!error) {
        vmp->mp = mp;
        vmp->dev = dev;
        vmp->lock = lock;
        error = virtiofs_dev_start(dev, &vmp->session);
    }
    if (!error)
        error = root_entry(vmp, share, ctx, &entry, &version);
    if (!error) {
        vmp->rootid = entry.nodeid;
        vfs_setfsprivate(mp, vmp);
        error = virtiofs_node_get(vmp, &entry, version, sharelen ? 1 : 0, NULLVP, NULL, &root);
    }
    if (error) {
        vfs_setfsprivate(mp, NULL);
        if (lock)
            IOLockFree(lock);
        if (vmp)
            IOFree(vmp, sizeof(*vmp));
        virtiofs_dev_release(dev);
        return error;
    }
    vnode_ref(root);
    vnode_put(root);
    vmp->root = root;

    struct vfsioattr io;
    vfs_ioattr(mp, &io);
    io.io_maxreadcnt = io.io_maxwritecnt = vmp->session.max_io;
    io.io_maxsegreadsize = io.io_maxsegwritesize = vmp->session.max_io;
    io.io_segreadcnt = io.io_segwritecnt = vmp->session.max_io / PAGE_SIZE + 1;
    vfs_setioattr(mp, &io);
    vfs_setflags(mp, MNT_NOSUID | MNT_NODEV);
    vfs_setlocklocal(mp);
    vfs_getnewfsid(mp);
    strlcpy(vfs_statfs(mp)->f_mntfromname, spec, sizeof(vfs_statfs(mp)->f_mntfromname));
    return 0;
}

static int
virtiofs_unmount(mount_t mp, int mntflags, __unused vfs_context_t ctx)
{
    struct virtiofs_mount *vmp = VFSTOVMP(mp);
    vnode_t root = vmp->root;
    bool force = (mntflags & MNT_FORCE) != 0;

    int error = vflush(mp, root, force ? FORCECLOSE : 0);
    if (!error && !force && vnode_isinuse(root, 1))
        error = EBUSY;
    if (error)
        return error;
    error = vnode_get(root);
    vnode_rele(root);
    if (!error)
        vnode_put(root);
    vflush(mp, NULLVP, FORCECLOSE);

    vfs_setfsprivate(mp, NULL);
    virtiofs_dev_release(vmp->dev);
    IOLockFree(vmp->lock);
    IOFree(vmp, sizeof(*vmp));
    return 0;
}

static int
virtiofs_root(mount_t mp, vnode_t *vpp, __unused vfs_context_t ctx)
{
    vnode_t root = VFSTOVMP(mp)->root;
    int error = vnode_get(root);

    if (!error)
        *vpp = root;
    return error;
}

static int
virtiofs_getattr(mount_t mp, struct vfs_attr *fsap, vfs_context_t ctx)
{
    struct virtiofs_mount *vmp = VFSTOVMP(mp);
    struct fuse_statfs_out out = {};
    struct virtiofs_call c = { .opcode = FUSE_STATFS, .nodeid = vmp->rootid, .reply = &out, .replylen = sizeof(out) };

    int error = virtiofs_call(vmp, ctx, &c);
    if (!error && c.replylen != sizeof(out))
        error = EIO;
    if (error)
        return error;

    const struct fuse_kstatfs *st = &out.st;
    VFSATTR_RETURN(fsap, f_bsize, st->frsize ? st->frsize : st->bsize);
    VFSATTR_RETURN(fsap, f_iosize, vmp->session.max_io);
    VFSATTR_RETURN(fsap, f_blocks, st->blocks);
    VFSATTR_RETURN(fsap, f_bfree, st->bfree);
    VFSATTR_RETURN(fsap, f_bavail, st->bavail);
    VFSATTR_RETURN(fsap, f_bused, st->blocks > st->bfree ? st->blocks - st->bfree : 0);
    VFSATTR_RETURN(fsap, f_files, st->files);
    VFSATTR_RETURN(fsap, f_ffree, st->ffree);
    return 0;
}

static int
virtiofs_start(__unused mount_t mp, __unused int flags, __unused vfs_context_t ctx)
{
    return 0;
}

static int
virtiofs_sync(__unused mount_t mp, __unused int waitfor, __unused vfs_context_t ctx)
{
    return 0;
}

static int
virtiofs_vget(__unused mount_t mp, __unused ino64_t ino, __unused vnode_t *vpp, __unused vfs_context_t ctx)
{
    return ENOTSUP;
}

static struct vfsops virtiofs_vfsops = {
    .vfs_mount = virtiofs_mount,
    .vfs_start = virtiofs_start,
    .vfs_unmount = virtiofs_unmount,
    .vfs_root = virtiofs_root,
    .vfs_getattr = virtiofs_getattr,
    .vfs_sync = virtiofs_sync,
    .vfs_vget = virtiofs_vget,
};

int
virtiofs_vfs_register(void)
{
    struct vnodeopv_desc *opv[] = { &virtiofs_vnodeop_opv_desc, &virtiofs_fifoop_opv_desc };
    struct vfs_fsentry vfe = {
        .vfe_vfsops = &virtiofs_vfsops,
        .vfe_vopcnt = 2,
        .vfe_opvdescs = opv,
        .vfe_flags = VFS_TBLTHREADSAFE | VFS_TBLFSNODELOCK | VFS_TBL64BITREADY | VFS_TBLNOTYPENUM |
                     VFS_TBLREADDIR_EXTENDED,
    };

    if (virtiofs_vfsconf)
        return 0;
    strlcpy(vfe.vfe_fsname, "virtiofs", sizeof(vfe.vfe_fsname));
    return vfs_fsadd(&vfe, &virtiofs_vfsconf);
}

int
virtiofs_vfs_unregister(void)
{
    if (virtiofs_vfsconf && vfs_fsremove(virtiofs_vfsconf) != 0)
        return EBUSY;
    virtiofs_vfsconf = NULL;
    return 0;
}

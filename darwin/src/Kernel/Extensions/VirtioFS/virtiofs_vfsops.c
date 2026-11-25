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
#include <sys/sysctl.h>
#include <IOKit/IOLib.h>
#include <libkern/libkern.h>
#include "fuse_proto.h"
#include "virtiofs.h"

#define VIRTIOFS_TAG_MAX 36

static vfstable_t virtiofs_vfsconf;
int virtiofs_spin_us = 50;
int virtiofs_entry_cache = 1;
int virtiofs_idle_files;
int virtiofs_revalidate_open = 1;

SYSCTL_DECL(_vfs_generic);
SYSCTL_NODE(_vfs_generic, OID_AUTO, virtiofs, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "virtio-fs");
SYSCTL_INT(_vfs_generic_virtiofs, OID_AUTO, spin_us, CTLFLAG_RW | CTLFLAG_LOCKED, &virtiofs_spin_us, 0,
           "how long a request waits for its reply before sleeping");
SYSCTL_INT(_vfs_generic_virtiofs, OID_AUTO, idle_files, CTLFLAG_RW | CTLFLAG_LOCKED, &virtiofs_idle_files, 0,
           "file handles a mount keeps open after their files close");
SYSCTL_INT(_vfs_generic_virtiofs, OID_AUTO, revalidate_open, CTLFLAG_RW | CTLFLAG_LOCKED, &virtiofs_revalidate_open, 0,
           "ask the host on every open of a file whose handle is kept");
SYSCTL_INT(_vfs_generic_virtiofs, OID_AUTO, entry_cache, CTLFLAG_RW | CTLFLAG_LOCKED, &virtiofs_entry_cache, 0,
           "reuse a looked-up name until the host's entry timeout");

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
    *version = virtiofs_version(vmp);
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

static void
mount_free(struct virtiofs_mount *vmp)
{
    size_t buckets = sizeof(*vmp->hash) << vmp->hash_bits;

    if (vmp->hash)
        IOFree(vmp->hash, buckets);
    if (vmp->names)
        IOFree(vmp->names, buckets);
    if (vmp->lock)
        IOLockFree(vmp->lock);
    if (vmp->move_lock)
        IOLockFree(vmp->move_lock);
    if (vmp->xattr_lock)
        IOLockFree(vmp->xattr_lock);
    virtiofs_dev_release(vmp->dev);
    IOFree(vmp, sizeof(*vmp));
}

static int
virtiofs_mount(mount_t mp, __unused vnode_t devvp, user_addr_t data, vfs_context_t ctx)
{
    char spec[VIRTIOFS_TAG_MAX + 1 + NAME_MAX + 1];
    char tag[VIRTIOFS_TAG_MAX + 1];

    if (vfs_context_suser(ctx))
        return EPERM;
    if (vfs_isupdate(mp))
        return ENOTSUP;
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
    if (!vmp) {
        virtiofs_dev_release(dev);
        return ENOMEM;
    }
    vmp->mp = mp;
    vmp->dev = dev;
    vmp->case_sensitive = -1;
    vmp->lock = IOLockAlloc();
    vmp->move_lock = IOLockAlloc();
    vmp->xattr_lock = IOLockAlloc();
    TAILQ_INIT(&vmp->idle);
    vmp->hash_bits = 10;
    while (vmp->hash_bits < 20 && (1 << vmp->hash_bits) < desiredvnodes / 8)
        vmp->hash_bits++;
    vmp->hash = IOMallocZero(sizeof(*vmp->hash) << vmp->hash_bits);
    vmp->names = IOMallocZero(sizeof(*vmp->names) << vmp->hash_bits);
    struct fuse_entry_out entry = {};
    uint64_t version = 0;
    vnode_t root = NULLVP;
    error = vmp->lock && vmp->move_lock && vmp->xattr_lock && vmp->hash && vmp->names ? 0 : ENOMEM;
    if (!error)
        error = virtiofs_dev_start(dev, &vmp->session);
    if (!error)
        error = root_entry(vmp, share, ctx, &entry, &version);
    if (!error) {
        vmp->rootid = entry.nodeid;
        vfs_setfsprivate(mp, vmp);
        error = virtiofs_node_get(vmp, &entry, version, sharelen ? 1 : 0, NULLVP, NULL, &root);
    }
    if (error) {
        vfs_setfsprivate(mp, NULL);
        mount_free(vmp);
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
    mount_free(vmp);
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
    if (VFSATTR_IS_ACTIVE(fsap, f_capabilities)) {
        vol_capabilities_attr_t *cap = &fsap->f_capabilities;
        uint32_t format = VOL_CAP_FMT_SYMBOLICLINKS | VOL_CAP_FMT_HARDLINKS | VOL_CAP_FMT_CASE_PRESERVING |
                          VOL_CAP_FMT_2TB_FILESIZE;
        uint32_t interfaces = VOL_CAP_INT_ADVLOCK | VOL_CAP_INT_FLOCK | VOL_CAP_INT_EXTENDED_ATTR;
        *cap = (vol_capabilities_attr_t){};
        int sensitive = __atomic_load_n(&vmp->case_sensitive, __ATOMIC_RELAXED);
        if (sensitive > 0)
            format |= VOL_CAP_FMT_CASE_SENSITIVE;
        cap->capabilities[VOL_CAPABILITIES_FORMAT] = format;
        cap->valid[VOL_CAPABILITIES_FORMAT] = format | (sensitive >= 0 ? VOL_CAP_FMT_CASE_SENSITIVE : 0) |
                                              VOL_CAP_FMT_JOURNAL | VOL_CAP_FMT_JOURNAL_ACTIVE |
                                              VOL_CAP_FMT_NO_ROOT_TIMES | VOL_CAP_FMT_ZERO_RUNS |
                                              VOL_CAP_FMT_FAST_STATFS | VOL_CAP_FMT_OPENDENYMODES |
                                              VOL_CAP_FMT_HIDDEN_FILES | VOL_CAP_FMT_PATH_FROM_ID;
        cap->capabilities[VOL_CAPABILITIES_INTERFACES] = interfaces;
        cap->valid[VOL_CAPABILITIES_INTERFACES] = interfaces | VOL_CAP_INT_SEARCHFS | VOL_CAP_INT_ATTRLIST |
                                                  VOL_CAP_INT_NFSEXPORT | VOL_CAP_INT_READDIRATTR |
                                                  VOL_CAP_INT_EXCHANGEDATA | VOL_CAP_INT_COPYFILE |
                                                  VOL_CAP_INT_ALLOCATE | VOL_CAP_INT_VOL_RENAME |
                                                  VOL_CAP_INT_EXTENDED_SECURITY | VOL_CAP_INT_USERACCESS |
                                                  VOL_CAP_INT_MANLOCK | VOL_CAP_INT_NAMEDSTREAMS |
                                                  VOL_CAP_INT_CLONE | VOL_CAP_INT_RENAME_SWAP |
                                                  VOL_CAP_INT_RENAME_EXCL;
        VFSATTR_SET_SUPPORTED(fsap, f_capabilities);
    }
    return 0;
}

static int
virtiofs_start(__unused mount_t mp, __unused int flags, __unused vfs_context_t ctx)
{
    return 0;
}

static int
sync_vnode(vnode_t vp, __unused void *arg)
{
    virtiofs_vnode_push(vp);
    return VNODE_RETURNED;
}

static int
virtiofs_sync(mount_t mp, __unused int waitfor, __unused vfs_context_t ctx)
{
    if (!vfs_isrdonly(mp))
        vnode_iterate(mp, VNODE_ITERATE_ACTIVE, sync_vnode, NULL);
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
    int error = vfs_fsadd(&vfe, &virtiofs_vfsconf);
    if (!error) {
        virtiofs_idle_files = desiredvnodes / 4;
        sysctl_register_oid(&sysctl__vfs_generic_virtiofs);
        sysctl_register_oid(&sysctl__vfs_generic_virtiofs_spin_us);
        sysctl_register_oid(&sysctl__vfs_generic_virtiofs_entry_cache);
        sysctl_register_oid(&sysctl__vfs_generic_virtiofs_idle_files);
        sysctl_register_oid(&sysctl__vfs_generic_virtiofs_revalidate_open);
    }
    return error;
}

int
virtiofs_vfs_unregister(void)
{
    if (virtiofs_vfsconf && vfs_fsremove(virtiofs_vfsconf) != 0)
        return EBUSY;
    if (virtiofs_vfsconf) {
        sysctl_unregister_oid(&sysctl__vfs_generic_virtiofs_revalidate_open);
        sysctl_unregister_oid(&sysctl__vfs_generic_virtiofs_idle_files);
        sysctl_unregister_oid(&sysctl__vfs_generic_virtiofs_entry_cache);
        sysctl_unregister_oid(&sysctl__vfs_generic_virtiofs_spin_us);
        sysctl_unregister_oid(&sysctl__vfs_generic_virtiofs);
    }
    virtiofs_vfsconf = NULL;
    return 0;
}

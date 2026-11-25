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
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/ubc.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode_if.h>
#include <sys/xattr.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <miscfs/fifofs/fifo.h>
#include <vfs/vfs_support.h>
#include <IOKit/IOLib.h>
#include <libkern/libkern.h>
#include "fuse_proto.h"
#include "virtiofs.h"

#define DIRENT_LEN(namlen)   ((sizeof(struct dirent) + (namlen) + 1 - (__DARWIN_MAXNAMLEN + 1) + 3) & ~3)
#define DIRENT64_LEN(namlen) ((sizeof(struct direntry) + (namlen) - (MAXPATHLEN - 1) + 7) & ~7)
#define READDIR_MIN          4096
#define WRITE_DEPTH          8

enum {
    VN_BUILDING = 0x01,
    VN_WANT = 0x02,
    VN_HASHED = 0x04,
    VN_OPEN = 0x08,
    VN_OPENING = 0x10,
    VN_LISTING = 0x20,
    VN_IDLE = 0x40,
    VN_SIZING = 0x80,
    VN_NAMED = 0x100,
    VN_WOPEN = 0x200,
    VN_WOPENING = 0x400,
    VN_MAPPED = 0x800,
    VN_MAPPED_WRITE = 0x1000,
    VN_TRUNCATING = 0x2000,
};

#define SETTABLE_MODE (ACCESSPERMS | S_ISVTX)

struct virtiofs_node {
    LIST_ENTRY(virtiofs_node) link;
    LIST_ENTRY(virtiofs_node) name_link;
    TAILQ_ENTRY(virtiofs_node) idle;
    struct virtiofs_mount *vmp;
    vnode_t vp;
    uint64_t nodeid;
    uint64_t nlookup;
    uint64_t fh;
    uint64_t wfh;
    uint64_t fh_ctime;
    uint64_t changed;
    uint64_t expire;
    uint64_t attr_version;
    uint64_t parent;
    uint64_t entry_expire;
    off_t push_limit;
    off_t skip_from;
    off_t skip_to;
    uint64_t name_version;
    char *name;
    uint32_t namelen;
    uint32_t fh_ctimensec;
    struct fuse_attr attr;
    uint32_t users;
    uint32_t readers;
    uint32_t wusers;
    uint32_t wopens;
    uint32_t ropens;
    uint32_t releasing;
    uint32_t paging;
    uint32_t flags;
    IOLock *wlock;
    thread_t creator;
};

#define VTON(vp) ((struct virtiofs_node *)vnode_fsnode(vp))

int (**virtiofs_vnodeop_p)(void *);
int (**virtiofs_fifoop_p)(void *);

static struct virtiofs_bucket *
bucket(struct virtiofs_mount *vmp, uint64_t nodeid)
{
    return &vmp->hash[(nodeid * 0x9e3779b97f4a7c15ULL) >> (64 - vmp->hash_bits)];
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
virtiofs_version(struct virtiofs_mount *vmp)
{
    IOLockLock(vmp->lock);
    uint64_t version = vmp->version;
    IOLockUnlock(vmp->lock);
    return version;
}

static off_t
reader_begin(struct virtiofs_node *np)
{
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_SIZING);
    np->readers++;
    off_t size = (off_t)np->attr.size;
    IOLockUnlock(np->vmp->lock);
    return size;
}

static void
reader_end(struct virtiofs_node *np)
{
    IOLockLock(np->vmp->lock);
    if (--np->readers == 0)
        wake(np, 0);
    IOLockUnlock(np->vmp->lock);
}

static void
readers_wait_locked(struct virtiofs_node *np)
{
    while (np->readers) {
        np->flags |= VN_WANT;
        IOLockSleep(np->vmp->lock, np, THREAD_UNINT);
    }
}

static void
page_drop(vnode_t vp, off_t page)
{
    for (int i = 0; i < 8 && ubc_page_op(vp, page, 0, NULL, NULL) == KERN_SUCCESS; i++)
        ubc_msync(vp, page, page + PAGE_SIZE, NULL, UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
    if (ubc_page_op(vp, page, 0, NULL, NULL) == KERN_SUCCESS)
        ubc_msync(vp, page, page + PAGE_SIZE, NULL, UBC_INVALIDATE);
}

static bool
node_apply(struct virtiofs_node *np, const struct fuse_attr *attr, uint64_t sec, uint32_t nsec, uint64_t version)
{
    struct virtiofs_mount *vmp = np->vmp;
    vnode_t vp = np->vp;

    IOLockLock(vmp->lock);
    wait_clear(np, VN_SIZING);
    bool changed = vnode_isreg(vp) && attr_changed(&np->attr, attr);
    bool sizing = changed && np->attr_version <= version && !(np->flags & VN_TRUNCATING);
    off_t size = (off_t)attr->size, old = (off_t)np->attr.size;
    if (sizing) {
        np->flags |= VN_SIZING;
        np->push_limit = old;
        if (size != old)
            readers_wait_locked(np);
    }
    if (np->attr_version > version || (np->flags & VN_TRUNCATING)) {
        if (sizing)
            wake(np, VN_SIZING);
        IOLockUnlock(vmp->lock);
        return false;
    }
    np->push_limit = MIN(old, size);
    np->attr = *attr;
    np->expire = deadline(sec, nsec);
    np->attr_version = ++vmp->version;
    IOLockUnlock(vmp->lock);
    if (!changed)
        return true;

    off_t cached = ubc_getsize(vp);
    if (size < cached)
        ubc_setsize(vp, size);
    if (ubc_pages_resident(vp)) {
        ubc_msync(vp, 0, MAX(cached, size), NULL, UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
        if (size > old && (old & PAGE_MASK))
            page_drop(vp, old & ~(off_t)PAGE_MASK);
    }
    IOLockLock(vmp->lock);
    np->push_limit = size;
    IOLockUnlock(vmp->lock);
    if (size > cached)
        ubc_setsize(vp, size);
    IOLockLock(vmp->lock);
    wake(np, VN_SIZING);
    IOLockUnlock(vmp->lock);
    return true;
}

static int
node_getattr(struct virtiofs_node *np, vfs_context_t ctx, struct fuse_attr *reply, bool *applied)
{
    struct fuse_getattr_in in = {};
    struct fuse_attr_out out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_GETATTR, .nodeid = np->nodeid,
        .arg = { &in }, .arglen = { sizeof(in) }, .reply = &out, .replylen = sizeof(out),
    };
    uint64_t version = virtiofs_version(np->vmp);
    int error = virtiofs_call(np->vmp, ctx, &c);
    if (!error && (c.replylen != sizeof(out) || !virtiofs_attr_ok(&out.attr) ||
                   IFTOVT(out.attr.mode) != vnode_vtype(np->vp)))
        error = EIO;
    if (error)
        return error;
    *reply = out.attr;
    *applied = node_apply(np, &out.attr, out.attr_valid, out.attr_valid_nsec, version);
    return 0;
}

static int
node_fetch(struct virtiofs_node *np, vfs_context_t ctx, struct fuse_attr *attr)
{
    bool applied;
    int error = node_getattr(np, ctx, attr, &applied);

    if (error)
        return error;
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_SIZING);
    *attr = np->attr;
    IOLockUnlock(np->vmp->lock);
    return 0;
}

static int
append_offset(struct virtiofs_node *np, vfs_context_t ctx, off_t *offset)
{
    struct fuse_attr reply;
    bool applied = false;
    int error = 0;

    for (int tries = 0; !error && !applied && tries < 3; tries++)
        error = node_getattr(np, ctx, &reply, &applied);
    if (error)
        return error;
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_SIZING);
    *offset = applied ? (off_t)np->attr.size : MAX((off_t)reply.size, (off_t)np->attr.size);
    IOLockUnlock(np->vmp->lock);
    return 0;
}

static int
node_attr(struct virtiofs_node *np, vfs_context_t ctx, struct fuse_attr *attr)
{
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_SIZING);
    bool cached = mach_absolute_time() < np->expire;
    if (cached)
        *attr = np->attr;
    IOLockUnlock(np->vmp->lock);
    return cached ? 0 : node_fetch(np, ctx, attr);
}

static void
node_changed(struct virtiofs_node *np, off_t at, off_t to, off_t size, bool exact)
{
    struct virtiofs_mount *vmp = np->vmp;
    vnode_t vp = np->vp;

    IOLockLock(vmp->lock);
    wait_clear(np, VN_SIZING);
    off_t old = (off_t)np->attr.size, from = MIN(at, old);
    bool resize = exact ? size != old : size > old;
    np->flags |= VN_SIZING;
    np->push_limit = old;
    np->skip_from = from;
    np->skip_to = to;
    if (resize) {
        readers_wait_locked(np);
        np->push_limit = MIN(old, size);
        np->attr.size = (uint64_t)size;
    }
    np->attr_version = ++vmp->version;
    np->expire = 0;
    IOLockUnlock(vmp->lock);
    bool resident = from < to && ubc_pages_resident(vp);
    off_t first = from & ~(off_t)PAGE_MASK, last = to & ~(off_t)PAGE_MASK;
    bool head = resident && (from & PAGE_MASK) && first < old;
    bool tail = resident && (to & PAGE_MASK) && to < old && !(head && last == first);
    if (head)
        page_drop(vp, first);
    if (resize && size > old) {
        IOLockLock(vmp->lock);
        np->push_limit = size;
        IOLockUnlock(vmp->lock);
    }
    if (tail)
        page_drop(vp, last);
    off_t inner = head ? first + PAGE_SIZE : from, end = MIN(tail ? last : to, round_page_64(old));
    if (resident && inner < end)
        ubc_msync(vp, inner, end, NULL, UBC_INVALIDATE);
    if (resize)
        ubc_setsize(vp, size);
    IOLockLock(vmp->lock);
    np->skip_from = np->skip_to = 0;
    wake(np, VN_SIZING | VN_TRUNCATING);
    IOLockUnlock(vmp->lock);
}

static void
node_stale(struct virtiofs_node *np)
{
    IOLockLock(np->vmp->lock);
    np->expire = 0;
    np->attr_version = ++np->vmp->version;
    IOLockUnlock(np->vmp->lock);
}

static bool
node_used_locked(struct virtiofs_node *np)
{
    return np->ropens || (np->flags & VN_MAPPED);
}

static off_t
node_size(struct virtiofs_node *np)
{
    IOLockLock(np->vmp->lock);
    off_t size = (off_t)np->attr.size;
    IOLockUnlock(np->vmp->lock);
    return size;
}

static off_t
node_push_limit(struct virtiofs_node *np, off_t offset)
{
    IOLockLock(np->vmp->lock);
    bool clip = (np->flags & VN_SIZING) && offset < round_page_64(np->push_limit);
    off_t size = clip ? np->push_limit : (off_t)np->attr.size;
    IOLockUnlock(np->vmp->lock);
    return size;
}

static void
node_skip(struct virtiofs_node *np, off_t from, off_t to)
{
    IOLockLock(np->vmp->lock);
    np->skip_from = from;
    np->skip_to = to;
    IOLockUnlock(np->vmp->lock);
}

static bool
is_ancestor(vnode_t vp, vnode_t dvp)
{
    vnode_t p = dvp;
    bool held = false, found = true;

    for (int depth = 0; depth < 4096; depth++) {
        if (p == vp || vnode_isvroot(p)) {
            found = p == vp;
            break;
        }
        vnode_t parent = vnode_getparent(p);
        if (held)
            vnode_put(p);
        p = parent;
        held = true;
        if (p == NULLVP) {
            found = false;
            break;
        }
    }
    if (held && p != NULLVP)
        vnode_put(p);
    return found;
}

static int
node_place(vnode_t vp, vnode_t dvp, struct componentname *cnp)
{
    struct virtiofs_mount *vmp = VTON(vp)->vmp;

    if (vp == dvp || vnode_isvroot(vp))
        return EIO;

    vnode_t parent = vnode_getparent(vp);
    const char *name = vnode_getname(vp);
    bool same = parent == dvp && name && strnlen(name, cnp->cn_namelen + 1) == (size_t)cnp->cn_namelen &&
                strncmp(name, cnp->cn_nameptr, cnp->cn_namelen) == 0;
    bool move = !same && vnode_isdir(vp);

    if (name)
        vnode_putname(name);
    if (parent)
        vnode_put(parent);
    if (same)
        return 0;
    if (move)
        IOLockLock(vmp->move_lock);
    int error = move && is_ancestor(vp, dvp) ? EIO : 0;
    if (!error)
        vnode_update_identity(vp, dvp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash,
                              VNODE_UPDATE_PARENT | VNODE_UPDATE_NAME);
    if (move)
        IOLockUnlock(vmp->move_lock);
    return error;
}

static void
node_free(struct virtiofs_node *np)
{
    if (np->wlock)
        IOLockFree(np->wlock);
    IOFree(np, sizeof(*np));
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
    int error = vnode_vtype(vp) == IFTOVT(entry->attr.mode) ? 0 : EIO;

    if (!error && dvp && cnp)
        error = node_place(vp, dvp, cnp);
    if (error) {
        vnode_put(vp);
        forget(np->vmp, entry->nodeid, nlookup);
        return error;
    }
    IOLockLock(np->vmp->lock);
    np->nlookup += nlookup;
    IOLockUnlock(np->vmp->lock);
    node_apply(np, &entry->attr, entry->attr_valid, entry->attr_valid_nsec, version);
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
    if (error == ENOMEM && (fresh = IOMallocZero(sizeof(*fresh))) && IFTOVT(entry->attr.mode) == VREG &&
        !(fresh->wlock = IOLockAlloc())) {
        IOFree(fresh, sizeof(*fresh));
        fresh = NULL;
    }
    if (!fresh) {
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
            node_free(fresh);
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
    np->attr_version = ++vmp->version;
    np->flags = VN_BUILDING | VN_HASHED;
    LIST_INSERT_HEAD(head, np, link);
    IOLockUnlock(vmp->lock);

    struct vnode_fsparam param = {
        .vnfs_mp = vmp->mp,
        .vnfs_vtype = IFTOVT(entry->attr.mode),
        .vnfs_str = "virtiofs",
        .vnfs_dvp = dvp,
        .vnfs_fsnode = np,
        .vnfs_vops = IFTOVT(entry->attr.mode) == VFIFO ? virtiofs_fifoop_p : virtiofs_vnodeop_p,
        .vnfs_markroot = entry->nodeid == vmp->rootid,
        .vnfs_rdev = NODEV,
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
    node_free(np);
    forget(vmp, entry->nodeid, nlookup);
    return error;
}

static int
handle_open(struct virtiofs_node *np, vfs_context_t ctx, uint32_t flags, uint64_t *fh)
{
    struct fuse_open_in in = { .flags = flags };
    struct fuse_open_out out = {};
    struct virtiofs_call c = {
        .opcode = vnode_isdir(np->vp) ? FUSE_OPENDIR : FUSE_OPEN, .nodeid = np->nodeid,
        .arg = { &in }, .arglen = { sizeof(in) }, .reply = &out, .replylen = sizeof(out),
    };
    int error = virtiofs_call(np->vmp, ctx, &c);

    if (!error && c.replylen != sizeof(out))
        error = EIO;
    *fh = out.fh;
    return error;
}

static void
handle_put(struct virtiofs_node *np)
{
    IOLockLock(np->vmp->lock);
    if (--np->users == 0)
        wake(np, 0);
    IOLockUnlock(np->vmp->lock);
}

static void
release_done(void *arg, __unused int error)
{
    struct virtiofs_node *np = arg;

    IOLockLock(np->vmp->lock);
    if (--np->releasing == 0)
        wake(np, 0);
    IOLockUnlock(np->vmp->lock);
}

static void
handle_release(struct virtiofs_node *np, uint64_t fh)
{
    struct fuse_release_in in = { .fh = fh };
    struct virtiofs_call c = {
        .opcode = vnode_isdir(np->vp) ? FUSE_RELEASEDIR : FUSE_RELEASE, .nodeid = np->nodeid,
        .arg = { &in }, .arglen = { sizeof(in) },
    };

    virtiofs_submit(np->vmp, NULL, &c, release_done, np);
}

static void
idle_remove_locked(struct virtiofs_node *np)
{
    if (np->flags & VN_IDLE) {
        TAILQ_REMOVE(&np->vmp->idle, np, idle);
        np->vmp->nidle--;
        np->flags &= ~VN_IDLE;
    }
}

static bool
handle_drop_locked(struct virtiofs_node *np, uint64_t *fh)
{
    idle_remove_locked(np);
    if (!(np->flags & VN_OPEN))
        return false;
    np->flags &= ~VN_OPEN;
    np->releasing++;
    *fh = np->fh;
    return true;
}

static bool
handle_current_locked(struct virtiofs_node *np, const struct fuse_attr *attr)
{
    return (np->flags & VN_OPEN) && np->fh_ctime == attr->ctime && np->fh_ctimensec == attr->ctimensec;
}

static void
opened_locked(struct virtiofs_node *np, bool count)
{
    if (count)
        np->ropens++;
}

static bool
handle_kept_locked(struct virtiofs_node *np)
{
    return !node_used_locked(np) && __atomic_load_n(&virtiofs_idle_files, __ATOMIC_RELAXED) <= 0;
}

static int
handle_validate(struct virtiofs_node *np, vfs_context_t ctx, bool *open, bool count)
{
    struct virtiofs_mount *vmp = np->vmp;
    bool ask = __atomic_load_n(&virtiofs_revalidate_open, __ATOMIC_RELAXED);
    struct fuse_attr attr;
    uint64_t fh, kept;

    IOLockLock(vmp->lock);
    wait_clear(np, VN_OPENING | VN_SIZING);
    *open = np->flags & VN_OPEN;
    bool current = !ask && mach_absolute_time() < np->expire && handle_current_locked(np, &np->attr) &&
                   !handle_kept_locked(np);
    if (current) {
        idle_remove_locked(np);
        opened_locked(np, count);
    }
    IOLockUnlock(vmp->lock);
    if (!*open || current)
        return 0;

    int error = ask ? node_fetch(np, ctx, &attr) : node_attr(np, ctx, &attr);
    if (error)
        return error;
    IOLockLock(vmp->lock);
    wait_clear(np, VN_OPENING);
    *open = np->flags & VN_OPEN;
    current = handle_current_locked(np, &attr) && !handle_kept_locked(np);
    if (current) {
        idle_remove_locked(np);
        opened_locked(np, count);
    }
    IOLockUnlock(vmp->lock);
    if (!*open || current)
        return 0;

    error = handle_open(np, ctx, 0, &fh);
    IOLockLock(vmp->lock);
    wait_clear(np, VN_OPENING);
    bool drop = error && !np->users && !node_used_locked(np) && handle_drop_locked(np, &kept);
    bool adopt = !error && !(np->flags & VN_OPEN);
    if (!error) {
        np->fh_ctime = attr.ctime;
        np->fh_ctimensec = attr.ctimensec;
        idle_remove_locked(np);
        opened_locked(np, count);
    }
    if (adopt) {
        np->fh = fh;
        np->flags |= VN_OPEN;
    } else if (!error) {
        np->releasing++;
    }
    IOLockUnlock(vmp->lock);
    if (!error && !adopt)
        handle_release(np, fh);
    if (drop)
        handle_release(np, kept);
    return error;
}

static int
handle_create_locked(struct virtiofs_node *np, vfs_context_t ctx, uint64_t *fh)
{
    struct virtiofs_mount *vmp = np->vmp;

    np->flags |= VN_OPENING;
    uint64_t ctime = np->attr.ctime;
    uint32_t ctimensec = np->attr.ctimensec;
    IOLockUnlock(vmp->lock);
    int error = handle_open(np, ctx, 0, fh);
    IOLockLock(vmp->lock);
    if (!error) {
        np->fh = *fh;
        np->fh_ctime = ctime;
        np->fh_ctimensec = ctimensec;
        np->flags |= VN_OPEN;
    }
    wake(np, VN_OPENING);
    return error;
}

static int
handle_get(struct virtiofs_node *np, vfs_context_t ctx, uint64_t *fh)
{
    struct virtiofs_mount *vmp = np->vmp;
    int error = 0;

    IOLockLock(vmp->lock);
    wait_clear(np, VN_OPENING);
    if (np->flags & VN_OPEN)
        *fh = np->fh;
    else
        error = handle_create_locked(np, ctx, fh);
    if (!error)
        np->users++;
    IOLockUnlock(vmp->lock);
    return error;
}

static int
handle_first(struct virtiofs_node *np, vfs_context_t ctx, bool count, bool *raced)
{
    struct virtiofs_mount *vmp = np->vmp;
    uint64_t fh;

    IOLockLock(vmp->lock);
    wait_clear(np, VN_OPENING);
    *raced = np->flags & VN_OPEN;
    int error = *raced ? 0 : handle_create_locked(np, ctx, &fh);
    if (!error && !*raced)
        opened_locked(np, count);
    IOLockUnlock(vmp->lock);
    return error;
}

static bool
handle_hold(struct virtiofs_node *np, uint64_t *fh)
{
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_OPENING);
    bool open = np->flags & VN_OPEN;
    if (open) {
        np->users++;
        *fh = np->fh;
    }
    IOLockUnlock(np->vmp->lock);
    return open;
}

static int
handle_check(struct virtiofs_node *np, vfs_context_t ctx)
{
    uint64_t fh;
    int error = handle_get(np, ctx, &fh);

    if (!error)
        handle_put(np);
    return error;
}

static int
file_open(struct virtiofs_node *np, vfs_context_t ctx, bool count)
{
    for (;;) {
        bool open, raced;
        int error = handle_validate(np, ctx, &open, count);
        if (error || open)
            return error;
        error = handle_first(np, ctx, count, &raced);
        if (error || !raced)
            return error;
    }
}

static int
whandle_get(struct virtiofs_node *np, vfs_context_t ctx, uint64_t *fh)
{
    struct virtiofs_mount *vmp = np->vmp;

    IOLockLock(vmp->lock);
    wait_clear(np, VN_WOPENING);
    if (np->flags & VN_WOPEN) {
        np->wusers++;
        *fh = np->wfh;
        IOLockUnlock(vmp->lock);
        return 0;
    }
    np->flags |= VN_WOPENING;
    IOLockUnlock(vmp->lock);

    int error = handle_open(np, ctx, VIRTIOFS_O_WRONLY, fh);

    IOLockLock(vmp->lock);
    if (!error) {
        np->wfh = *fh;
        np->flags |= VN_WOPEN;
        np->wusers++;
    }
    wake(np, VN_WOPENING);
    IOLockUnlock(vmp->lock);
    return error;
}

static void
whandle_put(struct virtiofs_node *np)
{
    IOLockLock(np->vmp->lock);
    if (--np->wusers == 0)
        wake(np, 0);
    IOLockUnlock(np->vmp->lock);
}

static void
whandle_close(struct virtiofs_node *np, bool force)
{
    struct virtiofs_mount *vmp = np->vmp;
    uint64_t fh = 0;

    IOLockLock(vmp->lock);
    while (force && (np->wusers || (np->flags & VN_WOPENING))) {
        np->flags |= VN_WANT;
        IOLockSleep(vmp->lock, np, THREAD_UNINT);
    }
    bool release = (np->flags & VN_WOPEN) &&
                   (force || !(np->wopens || np->wusers || (np->flags & (VN_WOPENING | VN_MAPPED_WRITE)) || np->creator));
    if (release) {
        np->flags &= ~VN_WOPEN;
        np->creator = NULL;
        np->releasing++;
        fh = np->wfh;
    }
    IOLockUnlock(vmp->lock);
    if (release)
        handle_release(np, fh);
}

static bool
whandle_hold(struct virtiofs_node *np, uint64_t *fh)
{
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_WOPENING);
    bool open = np->flags & VN_WOPEN;
    if (open) {
        np->wusers++;
        *fh = np->wfh;
    }
    IOLockUnlock(np->vmp->lock);
    return open;
}

static void
whandle_adopt(struct virtiofs_node *np, uint64_t fh, thread_t creator, bool open)
{
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_WOPENING);
    bool adopt = !(np->flags & VN_WOPEN);
    if (adopt) {
        np->wfh = fh;
        np->flags |= VN_WOPEN;
    } else {
        np->releasing++;
    }
    if (creator)
        np->creator = creator;
    if (open)
        np->wopens++;
    IOLockUnlock(np->vmp->lock);
    if (!adopt)
        handle_release(np, fh);
}

static int
wfile_check(struct virtiofs_node *np, vfs_context_t ctx, bool open)
{
    uint64_t fh;
    int error = handle_open(np, ctx, VIRTIOFS_O_WRONLY, &fh);

    if (!error)
        whandle_adopt(np, fh, NULL, open);
    return error;
}

static bool
wfile_created(struct virtiofs_node *np, int mode)
{
    IOLockLock(np->vmp->lock);
    bool mine = np->creator == current_thread();
    bool usable = mine && (mode & O_CREAT) && (np->flags & VN_WOPEN);
    if (mine)
        np->creator = NULL;
    if (usable && (mode & FWRITE))
        np->wopens++;
    IOLockUnlock(np->vmp->lock);
    return usable;
}

static void
file_closed(struct virtiofs_node *np, int mode)
{
    IOLockLock(np->vmp->lock);
    if ((mode & FREAD) && np->ropens)
        np->ropens--;
    if ((mode & FWRITE) && np->wopens)
        np->wopens--;
    IOLockUnlock(np->vmp->lock);
}

static int
push_range(vnode_t vp, off_t start, off_t end)
{
    return ubc_pages_resident(vp) ? ubc_msync(vp, start, end, NULL, UBC_PUSHDIRTY | UBC_SYNC) : 0;
}

static int
push_dirty(vnode_t vp)
{
    return push_range(vp, 0, ubc_getsize(vp));
}

void
virtiofs_vnode_push(vnode_t vp)
{
    if (vnode_isreg(vp)) {
        push_dirty(vp);
        whandle_close(VTON(vp), false);
    }
}

static void
file_idle(struct virtiofs_node *np)
{
    struct virtiofs_mount *vmp = np->vmp;
    uint32_t keep = (uint32_t)MAX(__atomic_load_n(&virtiofs_idle_files, __ATOMIC_RELAXED), 0);
    struct virtiofs_node *oldest, *next;
    uint64_t fh;

    IOLockLock(vmp->lock);
    if (!keep && !np->users && !node_used_locked(np) && handle_drop_locked(np, &fh)) {
        IOLockUnlock(vmp->lock);
        handle_release(np, fh);
        IOLockLock(vmp->lock);
    }
    if ((np->flags & (VN_OPEN | VN_IDLE)) == VN_OPEN && !node_used_locked(np)) {
        TAILQ_INSERT_TAIL(&vmp->idle, np, idle);
        vmp->nidle++;
        np->flags |= VN_IDLE;
    }
    for (oldest = TAILQ_FIRST(&vmp->idle); oldest && vmp->nidle > keep; oldest = next) {
        next = TAILQ_NEXT(oldest, idle);
        if (node_used_locked(oldest)) {
            idle_remove_locked(oldest);
        } else if (!oldest->users && handle_drop_locked(oldest, &fh)) {
            IOLockUnlock(vmp->lock);
            handle_release(oldest, fh);
            IOLockLock(vmp->lock);
            next = TAILQ_FIRST(&vmp->idle);
        }
    }
    IOLockUnlock(vmp->lock);
}

static void
dir_close(struct virtiofs_node *np)
{
    uint64_t fh;

    IOLockLock(np->vmp->lock);
    bool release = !np->users && handle_drop_locked(np, &fh);
    IOLockUnlock(np->vmp->lock);
    if (release)
        handle_release(np, fh);
}

static struct virtiofs_bucket *
name_bucket(struct virtiofs_mount *vmp, uint64_t parent, const char *name, uint32_t len)
{
    uint64_t h = parent ^ 0xcbf29ce484222325ULL;
    for (uint32_t i = 0; i < len; i++)
        h = (h ^ (uint8_t)name[i]) * 0x100000001b3ULL;
    return &vmp->names[(h * 0x9e3779b97f4a7c15ULL) >> (64 - vmp->hash_bits)];
}

static bool
name_matches(const struct virtiofs_node *np, uint64_t parent, const struct componentname *cnp)
{
    return np->parent == parent && np->namelen == (uint32_t)cnp->cn_namelen &&
           memcmp(np->name, cnp->cn_nameptr, np->namelen) == 0;
}

static void
name_drop_locked(struct virtiofs_node *np)
{
    if (!(np->flags & VN_NAMED))
        return;
    LIST_REMOVE(np, name_link);
    np->flags &= ~VN_NAMED;
    IOFree(np->name, np->namelen);
    np->name = NULL;
}

static vnode_t
name_lookup(struct virtiofs_node *dnp, struct componentname *cnp)
{
    struct virtiofs_mount *vmp = dnp->vmp;
    struct virtiofs_node *np;
    vnode_t vp = NULLVP;
    uint32_t vid = 0;

    if (!virtiofs_entry_cache)
        return NULLVP;
    IOLockLock(vmp->lock);
    LIST_FOREACH(np, name_bucket(vmp, dnp->nodeid, cnp->cn_nameptr, (uint32_t)cnp->cn_namelen), name_link) {
        if (name_matches(np, dnp->nodeid, cnp)) {
            if (mach_absolute_time() < np->entry_expire) {
                vp = np->vp;
                vid = vnode_vid(vp);
            }
            break;
        }
    }
    IOLockUnlock(vmp->lock);
    return vp && vnode_getwithvid(vp, vid) == 0 ? vp : NULLVP;
}

static void
name_remember(struct virtiofs_node *dnp, vnode_t vp, struct componentname *cnp, const struct fuse_entry_out *entry,
              uint64_t version)
{
    struct virtiofs_mount *vmp = dnp->vmp;
    struct virtiofs_node *np = VTON(vp), *other, *next;
    uint32_t len = (uint32_t)cnp->cn_namelen;
    bool keep = virtiofs_entry_cache && (entry->entry_valid || entry->entry_valid_nsec);
    char *name = keep ? IOMalloc(len) : NULL;
    uint64_t expire = deadline(entry->entry_valid, entry->entry_valid_nsec);
    struct virtiofs_bucket *head = name_bucket(vmp, dnp->nodeid, cnp->cn_nameptr, len);
    bool newer = false;

    if (name)
        memcpy(name, cnp->cn_nameptr, len);
    IOLockLock(vmp->lock);
    LIST_FOREACH_SAFE(other, head, name_link, next) {
        if (!name_matches(other, dnp->nodeid, cnp))
            continue;
        if (other->name_version > version)
            newer = true;
        else
            name_drop_locked(other);
    }
    if (version < dnp->changed || ((np->flags & VN_NAMED) && np->name_version > version))
        newer = true;
    else
        name_drop_locked(np);
    if (name && !newer) {
        np->name = name;
        np->namelen = len;
        np->parent = dnp->nodeid;
        np->entry_expire = expire;
        np->name_version = ++vmp->version;
        np->flags |= VN_NAMED;
        LIST_INSERT_HEAD(head, np, name_link);
        name = NULL;
    }
    IOLockUnlock(vmp->lock);
    if (name)
        IOFree(name, len);
}

static uint64_t
dir_changed(struct virtiofs_node *dnp, struct componentname *cnp, uint64_t before, bool *alone)
{
    struct virtiofs_mount *vmp = dnp->vmp;
    struct virtiofs_node *np, *next;

    IOLockLock(vmp->lock);
    LIST_FOREACH_SAFE(np, name_bucket(vmp, dnp->nodeid, cnp->cn_nameptr, (uint32_t)cnp->cn_namelen), name_link, next) {
        if (name_matches(np, dnp->nodeid, cnp))
            name_drop_locked(np);
    }
    if (alone)
        *alone = dnp->changed <= before;
    uint64_t version = dnp->changed = dnp->attr_version = ++vmp->version;
    dnp->expire = 0;
    IOLockUnlock(vmp->lock);
    return version;
}

static int
lookup_name(struct virtiofs_node *dnp, vfs_context_t ctx, const char *name, uint32_t len,
            struct fuse_entry_out *entry, uint64_t *version)
{
    struct virtiofs_call c = {
        .opcode = FUSE_LOOKUP, .nodeid = dnp->nodeid,
        .arg = { name, "" }, .arglen = { len, 1 }, .reply = entry, .replylen = sizeof(*entry),
    };
    *version = virtiofs_version(dnp->vmp);
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
    if ((*ap->a_vpp = name_lookup(dnp, cnp)))
        return 0;

    int error = lookup_name(dnp, ap->a_context, cnp->cn_nameptr, (uint32_t)cnp->cn_namelen, &entry, &version);
    if (error == ENOENT && (cnp->cn_flags & ISLASTCN) &&
        (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
        return EJUSTRETURN;
    if (!error)
        error = virtiofs_node_get(dnp->vmp, &entry, version, 1, dvp, cnp, ap->a_vpp);
    if (!error)
        name_remember(dnp, *ap->a_vpp, cnp, &entry, version);
    return error;
}

static int
virtiofs_vnop_open(struct vnop_open_args *ap)
{
    vnode_t vp = ap->a_vp;

    switch (vnode_vtype(vp)) {
    case VREG: {
        struct virtiofs_node *np = VTON(vp);
        bool write = ap->a_mode & FWRITE;
        if ((write || (ap->a_mode & O_TRUNC)) && vfs_isrdonly(vnode_mount(vp)))
            return EROFS;
        bool created = wfile_created(np, ap->a_mode);
        int error = (ap->a_mode & FREAD) ? file_open(np, ap->a_context, true) : 0;
        bool reading = !error && (ap->a_mode & FREAD);
        if (!error && write && !created)
            error = wfile_check(np, ap->a_context, true);
        if (created && (error || !write)) {
            if (write)
                file_closed(np, FWRITE);
            whandle_close(np, false);
        }
        if (error && reading) {
            file_closed(np, FREAD);
            file_idle(np);
        }
        return error;
    }
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
virtiofs_vnop_close(struct vnop_close_args *ap)
{
    if (!vnode_isreg(ap->a_vp))
        return 0;
    int error = (ap->a_fflag & FWRITE) ? push_dirty(ap->a_vp) : 0;
    file_closed(VTON(ap->a_vp), ap->a_fflag);
    return error;
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
    IOLockLock(np->vmp->lock);
    bool opened = node_used_locked(np);
    IOLockUnlock(np->vmp->lock);
    int error = opened ? handle_check(np, ap->a_context) : file_open(np, ap->a_context, false);
    if (!error)
        error = node_attr(np, ap->a_context, &attr);
    if (!error) {
        off_t size = reader_begin(np);
        error = cluster_read(vp, uio, size, ap->a_ioflag);
        reader_end(np);
    }
    if (!opened)
        file_idle(np);
    return error;
}

static int
write_sync(struct virtiofs_node *np, vfs_context_t ctx, uint64_t fh, uint32_t opcode)
{
    struct fuse_fsync_in in = { .fh = fh };
    struct virtiofs_call c = { .opcode = opcode, .nodeid = np->nodeid, .arg = { &in }, .arglen = { sizeof(in) } };
    int error = virtiofs_call(np->vmp, ctx, &c);

    return error == ENOSYS ? 0 : error;
}

struct write_batch;

struct write_part {
    struct write_batch *batch;
    struct fuse_write_in in;
    struct fuse_write_out out;
    int error;
};

struct write_batch {
    struct virtiofs_mount *vmp;
    uint32_t pending;
    struct write_part part[WRITE_DEPTH];
};

static void
write_part_done(void *arg, int error)
{
    struct write_part *part = arg;
    struct write_batch *batch = part->batch;
    IOLock *lock = batch->vmp->lock;

    part->error = error;
    IOLockLock(lock);
    if (__atomic_sub_fetch(&batch->pending, 1, __ATOMIC_RELEASE) == 0)
        IOLockWakeup(lock, batch, false);
    IOLockUnlock(lock);
}

static bool
write_batch_done(void *arg)
{
    return __atomic_load_n(&((struct write_batch *)arg)->pending, __ATOMIC_ACQUIRE) == 0;
}

static int
write_batch(struct virtiofs_node *np, vfs_context_t ctx, uint64_t fh, off_t at, const char *buf, uint32_t len,
            uint32_t *written)
{
    struct virtiofs_mount *vmp = np->vmp;
    uint32_t max = vmp->session.max_io, parts = (len + max - 1) / max;
    struct write_batch batch = { .vmp = vmp, .pending = parts };

    for (uint32_t i = 0; i < parts; i++) {
        struct write_part *part = &batch.part[i];
        uint32_t size = MIN(len - i * max, max);
        part->batch = &batch;
        part->in = (struct fuse_write_in){ .fh = fh, .offset = (uint64_t)at + i * max, .size = size };
        struct virtiofs_call c = {
            .opcode = FUSE_WRITE, .flags = VIRTIOFS_DATA_OUT, .nodeid = np->nodeid,
            .arg = { &part->in }, .arglen = { sizeof(part->in) }, .reply = &part->out, .replylen = sizeof(part->out),
            .data = (void *)(uintptr_t)(buf + i * max), .datalen = size,
        };
        virtiofs_submit(vmp, ctx, &c, write_part_done, part);
    }
    virtiofs_dev_poll(vmp->dev, write_batch_done, &batch, len);
    IOLockLock(vmp->lock);
    while (batch.pending)
        IOLockSleep(vmp->lock, &batch, THREAD_UNINT);
    IOLockUnlock(vmp->lock);

    int error = 0;
    *written = 0;
    for (uint32_t i = 0; i < parts; i++) {
        uint32_t size = MIN(len - i * max, max), done = batch.part[i].out.size;
        error = batch.part[i].error ? batch.part[i].error : done > size ? EIO : 0;
        if (error)
            break;
        *written += done;
        if (done < size)
            break;
    }
    return error;
}

static int
virtiofs_vnop_write(struct vnop_write_args *ap)
{
    vnode_t vp = ap->a_vp;
    uio_t uio = ap->a_uio;
    vfs_context_t ctx = ap->a_context;
    struct virtiofs_node *np = VTON(vp);
    uint64_t fh;

    if (vnode_isdir(vp))
        return EISDIR;
    if (!vnode_isreg(vp))
        return EPERM;
    if (uio_offset(uio) < 0)
        return EINVAL;
    if (vfs_isrdonly(vnode_mount(vp)))
        return EROFS;
    if (uio_resid(uio) == 0)
        return 0;

    IOLockLock(np->wlock);
    int error = whandle_get(np, ctx, &fh);
    if (error) {
        IOLockUnlock(np->wlock);
        return error;
    }
    if (ap->a_ioflag & IO_APPEND) {
        off_t end;
        error = append_offset(np, ctx, &end);
        if (!error)
            uio_setoffset(uio, end);
    }
    off_t start = uio_offset(uio), old = node_size(np), from = MIN(start, old);
    if (!error)
        error = push_range(vp, start, start + uio_resid(uio));
    uint32_t room = (uint32_t)MIN((user_ssize_t)np->vmp->session.max_io * WRITE_DEPTH, uio_resid(uio));
    char *buf = error ? NULL : IOMalloc(room);
    if (!error && !buf)
        error = ENOMEM;
    off_t reach = start;
    while (!error && uio_resid(uio) > 0) {
        uint32_t len = (uint32_t)MIN((user_ssize_t)room, uio_resid(uio)), written = 0;
        user_ssize_t resid = uio_resid(uio);
        off_t at = uio_offset(uio);
        error = uiomove(buf, (int)len, uio);
        if (!error) {
            reach = at + len;
            node_skip(np, from, reach);
            error = write_batch(np, ctx, fh, at, buf, len, &written);
        }
        if (error || written < len) {
            uio_setresid(uio, resid - written);
            uio_setoffset(uio, at + written);
            break;
        }
    }
    if (buf)
        IOFree(buf, room);
    off_t end = uio_offset(uio);
    if (end > start)
        error = (ap->a_ioflag & IO_SYNC) ? write_sync(np, ctx, fh, FUSE_FSYNC) : 0;
    whandle_put(np);
    if (reach > start)
        node_changed(np, start, reach, end > start ? end : 0, false);
    IOLockUnlock(np->wlock);
    return error;
}

static bool
pagein_done(void *np)
{
    return __atomic_load_n(&((struct virtiofs_node *)np)->paging, __ATOMIC_ACQUIRE) == 0;
}

static int
virtiofs_vnop_pagein(struct vnop_pagein_args *ap)
{
    struct virtiofs_node *np = VTON(ap->a_vp);
    int error = cluster_pagein(ap->a_vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset, (int)ap->a_size, node_size(np),
                               ap->a_flags);

    if (!error && ap->a_size <= np->vmp->session.max_io && !(ap->a_flags & UPL_IOSTREAMING) &&
        __atomic_load_n(&np->paging, __ATOMIC_RELAXED) <= 1)
        virtiofs_dev_poll(np->vmp->dev, pagein_done, np, (uint32_t)ap->a_size);
    return error;
}

static int
virtiofs_vnop_pageout(struct vnop_pageout_args *ap)
{
    vnode_t vp = ap->a_vp;

    if (vfs_isrdonly(vnode_mount(vp))) {
        if (ap->a_pl && !(ap->a_flags & UPL_NOCOMMIT))
            ubc_upl_commit_range(ap->a_pl, ap->a_pl_offset, (upl_size_t)ap->a_size,
                                 UPL_COMMIT_CLEAR_DIRTY | UPL_COMMIT_FREE_ON_EMPTY);
        return 0;
    }
    return cluster_pageout(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset, (int)ap->a_size,
                           node_push_limit(VTON(vp), ap->a_f_offset), ap->a_flags);
}

static int
virtiofs_vnop_mnomap(struct vnop_mnomap_args *ap)
{
    vnode_t vp = ap->a_vp;

    if (!vnode_isreg(vp))
        return 0;
    int error = push_dirty(vp);
    IOLockLock(VTON(vp)->vmp->lock);
    VTON(vp)->flags &= ~(VN_MAPPED | VN_MAPPED_WRITE);
    IOLockUnlock(VTON(vp)->vmp->lock);
    whandle_close(VTON(vp), false);
    return error;
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

struct virtiofs_part {
    struct fuse_write_out out;
    uint32_t want;
};

struct virtiofs_io {
    buf_t bp;
    struct virtiofs_node *np;
    uint32_t pending;
    uint32_t chunks;
    int error;
    bool write;
    struct virtiofs_part part[];
};

static size_t
io_size(bool write, uint32_t chunks)
{
    return sizeof(struct virtiofs_io) + (write ? chunks * sizeof(struct virtiofs_part) : 0);
}

static void
io_done(void *arg, int error)
{
    struct virtiofs_io *io = arg;

    if (error)
        __atomic_store_n(&io->error, error, __ATOMIC_RELAXED);
    if (__atomic_sub_fetch(&io->pending, 1, __ATOMIC_ACQ_REL) != 0)
        return;
    buf_t bp = io->bp;
    struct virtiofs_node *np = io->np;
    error = __atomic_load_n(&io->error, __ATOMIC_RELAXED);
    if (io->write) {
        for (uint32_t i = 0; !error && i < io->chunks; i++) {
            if (io->part[i].out.size != io->part[i].want)
                error = EIO;
        }
        node_stale(np);
        whandle_put(np);
    } else {
        if (buf_flags(bp) & B_PAGEIO)
            __atomic_sub_fetch(&np->paging, 1, __ATOMIC_RELEASE);
        handle_put(np);
    }
    IOFree(io, io_size(io->write, io->chunks));
    buf_seterror(bp, error);
    buf_setresid(bp, error ? buf_count(bp) : 0);
    buf_biodone(bp);
}

static uint32_t
write_spans(off_t offset, uint32_t count, off_t skip_from, off_t skip_to, uint32_t span[2][2])
{
    off_t end = offset + count;
    uint32_t n = 0;

    if (skip_from >= skip_to || skip_to <= offset || skip_from >= end) {
        span[0][0] = 0;
        span[0][1] = count;
        return count ? 1 : 0;
    }
    if (skip_from > offset) {
        span[n][0] = 0;
        span[n++][1] = (uint32_t)(skip_from - offset);
    }
    if (skip_to < end) {
        span[n][0] = (uint32_t)(skip_to - offset);
        span[n++][1] = (uint32_t)(end - skip_to);
    }
    return n;
}

static int
virtiofs_vnop_strategy(struct vnop_strategy_args *ap)
{
    buf_t bp = ap->a_bp;
    vnode_t vp = buf_vnode(bp);
    struct virtiofs_node *np = VTON(vp);
    struct virtiofs_io *io = NULL;
    bool write = !(buf_flags(bp) & B_READ);
    uint32_t count = buf_count(bp);
    uint32_t max = np->vmp->session.max_io;
    off_t offset = buf_blkno(bp) * vfs_devblocksize(vnode_mount(vp));
    uint32_t span[2][2] = { { 0, count } }, spans = count ? 1 : 0;
    uint64_t fh = 0;

    int error = write && vfs_isrdonly(vnode_mount(vp)) ? EROFS : 0;
    if (write) {
        off_t size = node_push_limit(np, offset);
        IOLockLock(np->vmp->lock);
        off_t skip_from = np->skip_from, skip_to = np->skip_to;
        IOLockUnlock(np->vmp->lock);
        count = offset >= size ? 0 : (uint32_t)MIN((off_t)count, size - offset);
        spans = write_spans(offset, count, skip_from, skip_to, span);
    }
    uint32_t chunks = 0;
    for (uint32_t p = 0; p < spans; p++)
        chunks += (span[p][1] + max - 1) / max;
    if (!error && chunks) {
        io = IOMallocZero(io_size(write, chunks));
        error = !io ? ENOMEM : write ? whandle_get(np, NULL, &fh) : handle_get(np, NULL, &fh);
    }
    if (error && io)
        IOFree(io, io_size(write, chunks));
    if (error || !chunks) {
        buf_seterror(bp, error);
        buf_setresid(bp, error ? buf_count(bp) : 0);
        buf_biodone(bp);
        return 0;
    }

    io->bp = bp;
    io->np = np;
    io->pending = chunks;
    io->chunks = chunks;
    io->write = write;
    if (!write && (buf_flags(bp) & B_PAGEIO))
        __atomic_add_fetch(&np->paging, 1, __ATOMIC_RELAXED);
    for (uint32_t p = 0, i = 0; p < spans; p++) {
        for (uint32_t done = 0; done < span[p][1]; done += max, i++) {
            uint32_t at = span[p][0] + done, len = MIN(span[p][1] - done, max);
            struct fuse_read_in rin = { .fh = fh, .offset = (uint64_t)offset + at, .size = len };
            struct fuse_write_in win = { .fh = fh, .offset = (uint64_t)offset + at, .size = len };
            struct virtiofs_call c = {
                .opcode = write ? FUSE_WRITE : FUSE_READ, .flags = write ? VIRTIOFS_DATA_OUT : VIRTIOFS_ZERO_FILL,
                .nodeid = np->nodeid, .datalen = len,
            };
            if (write) {
                io->part[i].want = len;
                c.arg[0] = &win;
                c.arglen[0] = sizeof(win);
                c.reply = &io->part[i].out;
                c.replylen = sizeof(io->part[i].out);
            } else {
                c.arg[0] = &rin;
                c.arglen[0] = sizeof(rin);
            }
            if (buf_upl(bp)) {
                c.upl = buf_upl(bp);
                c.upl_offset = buf_uploffset(bp) + at;
            } else {
                c.data = (void *)(buf_dataptr(bp) + at);
            }
            virtiofs_submit(np->vmp, NULL, &c, io_done, io);
        }
    }
    return 0;
}

static int
virtiofs_vnop_mmap(struct vnop_mmap_args *ap)
{
    vnode_t vp = ap->a_vp;

    if (vnode_isreg(vp)) {
        IOLockLock(VTON(vp)->vmp->lock);
        VTON(vp)->flags |= VN_MAPPED | ((ap->a_fflags & PROT_WRITE) ? VN_MAPPED_WRITE : 0);
        IOLockUnlock(VTON(vp)->vmp->lock);
    }
    return 0;
}

static int
file_sync(vnode_t vp, vfs_context_t ctx)
{
    struct virtiofs_node *np = VTON(vp);
    uint64_t fh;

    if (vfs_isrdonly(vnode_mount(vp)) || !(vnode_isreg(vp) || vnode_isdir(vp)))
        return 0;
    int error = vnode_isreg(vp) ? push_dirty(vp) : 0;
    if (vnode_isrecycled(vp))
        return error;
    if (error) {
        if (vnode_isreg(vp))
            whandle_close(np, false);
        return error;
    }
    if (vnode_isreg(vp) && whandle_hold(np, &fh)) {
        error = write_sync(np, ctx, fh, FUSE_FSYNC);
        whandle_put(np);
        whandle_close(np, false);
    } else if (handle_hold(np, &fh)) {
        error = write_sync(np, ctx, fh, vnode_isdir(vp) ? FUSE_FSYNCDIR : FUSE_FSYNC);
        handle_put(np);
    }
    return error;
}

static int
virtiofs_vnop_fsync(struct vnop_fsync_args *ap)
{
    return file_sync(ap->a_vp, ap->a_context);
}

static int
virtiofs_vnop_ioctl(struct vnop_ioctl_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct virtiofs_node *np = VTON(vp);
    struct radvisory *ra = (struct radvisory *)ap->a_data;
    struct fuse_attr attr;

    if (ap->a_command == F_FULLFSYNC || ap->a_command == F_BARRIERFSYNC)
        return file_sync(vp, ap->a_context);
    if (ap->a_command != F_RDADVISE)
        return ENOTSUP;
    if (!vnode_isreg(vp))
        return EINVAL;
    IOLockLock(np->vmp->lock);
    bool opened = node_used_locked(np);
    IOLockUnlock(np->vmp->lock);
    int error = opened ? 0 : file_open(np, ap->a_context, false);
    if (!error)
        error = node_attr(np, ap->a_context, &attr);
    if (!error) {
        off_t size = reader_begin(np);
        error = ra->ra_offset >= size ? EFBIG : advisory_read(vp, size, ra->ra_offset, ra->ra_count);
        reader_end(np);
    }
    if (!opened)
        file_idle(np);
    return error;
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
    uint64_t cookie = (uint64_t)uio_offset(uio);
    bool full = false, skipped;
    int count = 0, eof = 0;
    uint64_t fh;

    if (ap->a_flags & VNODE_READDIR_SEEKOFF32)
        return EINVAL;
    if (!vnode_isdir(vp))
        return ENOTDIR;
    if (resid <= 0)
        return EINVAL;
    uint32_t size = resid < READDIR_MIN ? READDIR_MIN : (resid < vmp->session.max_io ? (uint32_t)resid : vmp->session.max_io);
    uint8_t *buf = IOMallocZero(size);
    if (!buf)
        return ENOMEM;
    int error = handle_get(np, ap->a_context, &fh);
    if (error) {
        IOFree(buf, size);
        return error;
    }
    IOLockLock(vmp->lock);
    wait_clear(np, VN_LISTING);
    np->flags |= VN_LISTING;
    IOLockUnlock(vmp->lock);
    do {
        struct fuse_read_in in = { .fh = fh, .offset = cookie, .size = size };
        struct virtiofs_call c = {
            .opcode = FUSE_READDIR, .nodeid = np->nodeid,
            .arg = { &in }, .arglen = { sizeof(in) }, .datalen = size, .data = buf,
        };
        struct virtiofs_dirent d;
        uint64_t start = cookie;
        uint32_t pos = 0;
        int more = 0;

        skipped = false;
        error = virtiofs_call(vmp, ap->a_context, &c);
        while (!error && !full && (more = virtiofs_dirent_next(buf, c.datalen, &pos, 0, &d)) == 1) {
            if (!extended && d.namelen > MAXNAMLEN) {
                skipped = true;
                cookie = d.off;
                continue;
            }
            error = emit(uio, extended, &d, &full);
            if (!error && !full) {
                count++;
                cookie = d.off;
            }
        }
        if (!error && !full && count == 0 && (more < 0 || (skipped && cookie == start)))
            error = EIO;
        eof = !error && c.datalen == 0;
    } while (!error && !eof && !full && count == 0 && skipped);
    if (!error && full && count == 0)
        error = EINVAL;
    IOLockLock(vmp->lock);
    wake(np, VN_LISTING);
    IOLockUnlock(vmp->lock);
    handle_put(np);
    uio_setoffset(uio, (off_t)cookie);
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
    char *buf = IOMallocZero(MAXPATHLEN);
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
name_node(struct virtiofs_node *dnp, vfs_context_t ctx, const char *name, uint32_t len, struct fuse_entry_out *entry)
{
    uint64_t version;
    int error = lookup_name(dnp, ctx, name, len, entry, &version);

    if (!error)
        forget(dnp->vmp, entry->nodeid, 1);
    return error;
}

static int
case_sensitive(vnode_t vp, vfs_context_t ctx, int32_t *retval)
{
    struct virtiofs_node *np = VTON(vp);
    struct virtiofs_mount *vmp = np->vmp;
    int known = __atomic_load_n(&vmp->case_sensitive, __ATOMIC_RELAXED);

    if (known >= 0) {
        *retval = known;
        return 0;
    }
    vnode_t dvp = vnode_getparent(vp);
    const char *name = vnode_getname(vp);
    size_t len = name ? strnlen(name, VIRTIOFS_NAME_MAX + 1) : 0;
    char *flipped = len && len <= VIRTIOFS_NAME_MAX && dvp ? IOMalloc(len) : NULL;
    bool letters = false;

    for (size_t i = 0; flipped && i < len; i++) {
        char c = name[i];
        letters |= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        flipped[i] = c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
    }
    struct fuse_entry_out first, flip, again;
    if (letters && name_node(VTON(dvp), ctx, name, (uint32_t)len, &first) == 0 && first.nodeid == np->nodeid) {
        int error = name_node(VTON(dvp), ctx, flipped, (uint32_t)len, &flip);
        if (!error && flip.nodeid == np->nodeid)
            known = vnode_isdir(vp) || (first.attr.nlink == 1 && flip.attr.nlink == 1) ? 0 : -1;
        else if ((!error || error == ENOENT) && name_node(VTON(dvp), ctx, name, (uint32_t)len, &again) == 0 &&
                 again.nodeid == np->nodeid)
            known = 1;
    }
    if (flipped)
        IOFree(flipped, len);
    if (name)
        vnode_putname(name);
    if (dvp)
        vnode_put(dvp);
    if (known >= 0)
        __atomic_store_n(&vmp->case_sensitive, known, __ATOMIC_RELAXED);
    *retval = known > 0;
    return 0;
}

static int
virtiofs_vnop_pathconf(struct vnop_pathconf_args *ap)
{
    switch (ap->a_name) {
    case _PC_CASE_SENSITIVE:
        return case_sensitive(ap->a_vp, ap->a_context, ap->a_retval);
    case _PC_CASE_PRESERVING:
        *ap->a_retval = 1;
        return 0;
    case _PC_LINK_MAX:
        *ap->a_retval = LINK_MAX;
        return 0;
    case _PC_NAME_MAX:
    case _PC_NAME_CHARS_MAX:
        *ap->a_retval = NAME_MAX;
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

static void
node_resize(struct virtiofs_node *np, off_t size)
{
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_SIZING);
    np->flags |= VN_SIZING;
    np->push_limit = (off_t)np->attr.size;
    readers_wait_locked(np);
    np->push_limit = MIN((off_t)np->attr.size, size);
    np->attr.size = (uint64_t)size;
    np->attr_version = ++np->vmp->version;
    IOLockUnlock(np->vmp->lock);
    ubc_setsize(np->vp, size);
    IOLockLock(np->vmp->lock);
    wake(np, VN_SIZING);
    IOLockUnlock(np->vmp->lock);
}

static int
truncate_begin(struct virtiofs_node *np, vfs_context_t ctx, off_t size, uint64_t *fh, off_t *was)
{
    IOLockLock(np->vmp->lock);
    bool opened = np->wopens != 0;
    IOLockUnlock(np->vmp->lock);
    int error = opened ? 0 : wfile_check(np, ctx, false);
    if (!error)
        error = whandle_get(np, ctx, fh);
    if (error)
        return error;
    IOLockLock(np->vmp->lock);
    wait_clear(np, VN_SIZING);
    np->flags |= VN_TRUNCATING;
    *was = (off_t)np->attr.size;
    IOLockUnlock(np->vmp->lock);
    if (size < *was)
        error = push_dirty(np->vp);
    if (error) {
        IOLockLock(np->vmp->lock);
        wake(np, VN_TRUNCATING);
        IOLockUnlock(np->vmp->lock);
        whandle_put(np);
        return error;
    }
    if (size < *was)
        node_resize(np, size);
    return 0;
}

static int
virtiofs_vnop_setattr(struct vnop_setattr_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct vnode_attr *vap = ap->a_vap;
    struct virtiofs_node *np = VTON(vp);
    struct fuse_setattr_in in = {};
    struct fuse_attr_out out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_SETATTR, .nodeid = np->nodeid,
        .arg = { &in }, .arglen = { sizeof(in) }, .reply = &out, .replylen = sizeof(out),
    };
    bool truncate = VATTR_IS_ACTIVE(vap, va_data_size) && vnode_isreg(vp);
    uint64_t fh;

    if (vfs_isrdonly(vnode_mount(vp)))
        return EROFS;
    if (VATTR_IS_ACTIVE(vap, va_acl) || VATTR_IS_ACTIVE(vap, va_uuuid) || VATTR_IS_ACTIVE(vap, va_guuid) ||
        (VATTR_IS_ACTIVE(vap, va_flags) && vap->va_flags))
        return ENOTSUP;
    if (VATTR_IS_ACTIVE(vap, va_data_size) && vnode_isdir(vp))
        return EISDIR;
    VATTR_SET_SUPPORTED(vap, va_flags);
    VATTR_SET_SUPPORTED(vap, va_data_size);
    if (VATTR_IS_ACTIVE(vap, va_mode)) {
        in.valid |= FATTR_MODE;
        in.mode = vap->va_mode & SETTABLE_MODE;
        VATTR_SET_SUPPORTED(vap, va_mode);
    }
    if (VATTR_IS_ACTIVE(vap, va_uid)) {
        in.valid |= FATTR_UID;
        in.uid = vap->va_uid;
        VATTR_SET_SUPPORTED(vap, va_uid);
    }
    if (VATTR_IS_ACTIVE(vap, va_gid)) {
        in.valid |= FATTR_GID;
        in.gid = vap->va_gid;
        VATTR_SET_SUPPORTED(vap, va_gid);
    }
    if (VATTR_IS_ACTIVE(vap, va_access_time)) {
        in.valid |= FATTR_ATIME | ((vap->va_vaflags & VA_UTIMES_NULL) ? FATTR_ATIME_NOW : 0);
        in.atime = (uint64_t)vap->va_access_time.tv_sec;
        in.atimensec = (uint32_t)vap->va_access_time.tv_nsec;
        VATTR_SET_SUPPORTED(vap, va_access_time);
    }
    if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
        in.valid |= FATTR_MTIME | ((vap->va_vaflags & VA_UTIMES_NULL) ? FATTR_MTIME_NOW : 0);
        in.mtime = (uint64_t)vap->va_modify_time.tv_sec;
        in.mtimensec = (uint32_t)vap->va_modify_time.tv_nsec;
        VATTR_SET_SUPPORTED(vap, va_modify_time);
    }
    if (truncate) {
        in.valid |= FATTR_SIZE | FATTR_FH;
        in.size = (uint64_t)vap->va_data_size;
    }
    if (!in.valid)
        return 0;

    off_t was = 0;
    if (truncate) {
        IOLockLock(np->wlock);
        int error = truncate_begin(np, ap->a_context, vap->va_data_size, &fh, &was);
        if (error) {
            IOLockUnlock(np->wlock);
            return error;
        }
        in.fh = fh;
    }
    uint64_t version = virtiofs_version(np->vmp);
    int error = virtiofs_call(np->vmp, ap->a_context, &c);
    if (!error && (c.replylen != sizeof(out) || !virtiofs_attr_ok(&out.attr) ||
                   IFTOVT(out.attr.mode) != vnode_vtype(vp)))
        error = EIO;
    if (!error && truncate) {
        node_changed(np, vap->va_data_size, vap->va_data_size, vap->va_data_size, true);
    } else if (!error) {
        if (!node_apply(np, &out.attr, out.attr_valid, out.attr_valid_nsec, version))
            node_stale(np);
    } else if (truncate) {
        node_changed(np, was, was, was, true);
    } else {
        node_stale(np);
    }
    if (truncate) {
        whandle_put(np);
        whandle_close(np, false);
        IOLockUnlock(np->wlock);
    }
    return error;
}

static int
name_check(struct componentname *cnp)
{
    if (virtiofs_name_ok(cnp->cn_nameptr, (uint32_t)cnp->cn_namelen))
        return 0;
    return cnp->cn_namelen > VIRTIOFS_NAME_MAX ? ENAMETOOLONG : EINVAL;
}

static int
entry_reply(struct virtiofs_node *dnp, vfs_context_t ctx, struct virtiofs_call *c, struct fuse_entry_out *entry,
            uint32_t want, struct componentname *cnp, vnode_t *vpp, bool *answered)
{
    int error = name_check(cnp);

    if (error)
        return error;
    uint64_t before = virtiofs_version(dnp->vmp);
    error = virtiofs_call(dnp->vmp, ctx, c);
    bool alone;
    uint64_t version = dir_changed(dnp, cnp, before, &alone);
    bool ok = !error && c->replylen == want && virtiofs_entry_ok(entry) && (!answered || S_ISREG(entry->attr.mode));
    if (answered)
        *answered = ok;
    if (!error && !ok) {
        if (c->replylen == want && entry->nodeid)
            forget(dnp->vmp, entry->nodeid, 1);
        error = EIO;
    }
    if (!error)
        error = virtiofs_node_get(dnp->vmp, entry, before, answered ? 0 : 1, dnp->vp, cnp, vpp);
    if (!error && answered) {
        IOLockLock(dnp->vmp->lock);
        VTON(*vpp)->nlookup++;
        IOLockUnlock(dnp->vmp->lock);
    }
    if (!error && alone)
        name_remember(dnp, *vpp, cnp, entry, version);
    return error;
}

static int
make_node(struct virtiofs_node *dnp, vfs_context_t ctx, struct componentname *cnp, uint32_t mode, vnode_t *vpp)
{
    struct fuse_mknod_in in = { .mode = mode };
    struct fuse_entry_out out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_MKNOD, .nodeid = dnp->nodeid,
        .arg = { &in, cnp->cn_nameptr, "" }, .arglen = { sizeof(in), (uint32_t)cnp->cn_namelen, 1 },
        .reply = &out, .replylen = sizeof(out),
    };

    return entry_reply(dnp, ctx, &c, &out, sizeof(out), cnp, vpp, NULL);
}

static int
virtiofs_vnop_create(struct vnop_create_args *ap)
{
    struct virtiofs_node *dnp = VTON(ap->a_dvp);
    struct componentname *cnp = ap->a_cnp;
    uint32_t mode = (uint32_t)(S_IFREG | (ap->a_vap->va_mode & SETTABLE_MODE));
    struct fuse_create_in in = { .flags = VIRTIOFS_O_WRONLY | VIRTIOFS_O_CREAT | VIRTIOFS_O_EXCL, .mode = mode };
    struct {
        struct fuse_entry_out entry;
        struct fuse_open_out open;
    } out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_CREATE, .nodeid = dnp->nodeid,
        .arg = { &in, cnp->cn_nameptr, "" }, .arglen = { sizeof(in), (uint32_t)cnp->cn_namelen, 1 },
        .reply = &out, .replylen = sizeof(out),
    };

    if (vfs_isrdonly(vnode_mount(ap->a_dvp)))
        return EROFS;
    if (ap->a_vap->va_type != VREG)
        return EINVAL;
    bool answered = false;
    int error = entry_reply(dnp, ap->a_context, &c, &out.entry, sizeof(out), cnp, ap->a_vpp, &answered);
    if (error == ENOSYS)
        return make_node(dnp, ap->a_context, cnp, mode, ap->a_vpp);
    if (!error) {
        whandle_adopt(VTON(*ap->a_vpp), out.open.fh, current_thread(), false);
        if (vnode_ref(*ap->a_vpp) == 0)
            vnode_rele(*ap->a_vpp);
    } else if (answered) {
        struct fuse_release_in rin = { .fh = out.open.fh };
        struct virtiofs_call r = {
            .opcode = FUSE_RELEASE, .nodeid = out.entry.nodeid, .arg = { &rin }, .arglen = { sizeof(rin) },
        };
        virtiofs_call(dnp->vmp, NULL, &r);
        forget(dnp->vmp, out.entry.nodeid, 1);
    }
    return error;
}

static int
virtiofs_vnop_mknod(struct vnop_mknod_args *ap)
{
    enum vtype type = ap->a_vap->va_type;

    if (vfs_isrdonly(vnode_mount(ap->a_dvp)))
        return EROFS;
    if (type != VFIFO && type != VSOCK)
        return ENOTSUP;
    uint32_t mode = (uint32_t)((type == VFIFO ? S_IFIFO : S_IFSOCK) | (ap->a_vap->va_mode & SETTABLE_MODE));
    return make_node(VTON(ap->a_dvp), ap->a_context, ap->a_cnp, mode, ap->a_vpp);
}

static int
virtiofs_vnop_mkdir(struct vnop_mkdir_args *ap)
{
    struct virtiofs_node *dnp = VTON(ap->a_dvp);
    struct componentname *cnp = ap->a_cnp;
    struct fuse_mkdir_in in = { .mode = (uint32_t)(ap->a_vap->va_mode & SETTABLE_MODE) };
    struct fuse_entry_out out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_MKDIR, .nodeid = dnp->nodeid,
        .arg = { &in, cnp->cn_nameptr, "" }, .arglen = { sizeof(in), (uint32_t)cnp->cn_namelen, 1 },
        .reply = &out, .replylen = sizeof(out),
    };

    if (vfs_isrdonly(vnode_mount(ap->a_dvp)))
        return EROFS;
    return entry_reply(dnp, ap->a_context, &c, &out, sizeof(out), cnp, ap->a_vpp, NULL);
}

static int
virtiofs_vnop_symlink(struct vnop_symlink_args *ap)
{
    struct virtiofs_node *dnp = VTON(ap->a_dvp);
    struct componentname *cnp = ap->a_cnp;
    size_t len = strnlen(ap->a_target, MAXPATHLEN);
    struct fuse_entry_out out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_SYMLINK, .flags = VIRTIOFS_DATA_OUT, .nodeid = dnp->nodeid,
        .arg = { cnp->cn_nameptr, "" }, .arglen = { (uint32_t)cnp->cn_namelen, 1 },
        .reply = &out, .replylen = sizeof(out), .data = (void *)(uintptr_t)ap->a_target, .datalen = (uint32_t)len + 1,
    };

    if (vfs_isrdonly(vnode_mount(ap->a_dvp)))
        return EROFS;
    if (len == 0 || len >= MAXPATHLEN)
        return len ? ENAMETOOLONG : ENOENT;
    return entry_reply(dnp, ap->a_context, &c, &out, sizeof(out), cnp, ap->a_vpp, NULL);
}

static int
virtiofs_vnop_link(struct vnop_link_args *ap)
{
    struct virtiofs_node *tdnp = VTON(ap->a_tdvp);
    struct componentname *cnp = ap->a_cnp;
    struct fuse_link_in in = { .oldnodeid = VTON(ap->a_vp)->nodeid };
    struct fuse_entry_out out = {};
    struct virtiofs_call c = {
        .opcode = FUSE_LINK, .nodeid = tdnp->nodeid,
        .arg = { &in, cnp->cn_nameptr, "" }, .arglen = { sizeof(in), (uint32_t)cnp->cn_namelen, 1 },
        .reply = &out, .replylen = sizeof(out),
    };
    vnode_t vp;

    if (vfs_isrdonly(vnode_mount(ap->a_vp)))
        return EROFS;
    if (vnode_isdir(ap->a_vp))
        return EPERM;
    int error = entry_reply(tdnp, ap->a_context, &c, &out, sizeof(out), cnp, &vp, NULL);
    if (!error) {
        node_stale(VTON(vp));
        vnode_put(vp);
    }
    return error;
}

static void
node_gone(vnode_t vp)
{
    struct virtiofs_node *np = VTON(vp);

    IOLockLock(np->vmp->lock);
    if (np->attr.nlink)
        np->attr.nlink--;
    bool last = vnode_isdir(vp) || np->attr.nlink == 0;
    if (last)
        name_drop_locked(np);
    np->expire = 0;
    np->attr_version = ++np->vmp->version;
    IOLockUnlock(np->vmp->lock);
    if (last)
        vnode_recycle(vp);
}

static int
remove_name(vnode_t dvp, vnode_t vp, struct componentname *cnp, uint32_t opcode, vfs_context_t ctx)
{
    struct virtiofs_node *dnp = VTON(dvp);
    struct virtiofs_call c = {
        .opcode = opcode, .nodeid = dnp->nodeid,
        .arg = { cnp->cn_nameptr, "" }, .arglen = { (uint32_t)cnp->cn_namelen, 1 },
    };

    if (vfs_isrdonly(vnode_mount(dvp)))
        return EROFS;
    int error = name_check(cnp);
    if (error)
        return error;
    error = virtiofs_call(dnp->vmp, ctx, &c);
    dir_changed(dnp, cnp, 0, NULL);
    if (!error)
        node_gone(vp);
    return error;
}

static int
virtiofs_vnop_remove(struct vnop_remove_args *ap)
{
    if ((ap->a_flags & VNODE_REMOVE_NODELETEBUSY) && vnode_isinuse(ap->a_vp, 0))
        return EBUSY;
    return remove_name(ap->a_dvp, ap->a_vp, ap->a_cnp, FUSE_UNLINK, ap->a_context);
}

static int
virtiofs_vnop_rmdir(struct vnop_rmdir_args *ap)
{
    return remove_name(ap->a_dvp, ap->a_vp, ap->a_cnp, FUSE_RMDIR, ap->a_context);
}

static int
virtiofs_vnop_rename(struct vnop_rename_args *ap)
{
    struct virtiofs_node *fdnp = VTON(ap->a_fdvp), *tdnp = VTON(ap->a_tdvp);
    struct componentname *fcnp = ap->a_fcnp, *tcnp = ap->a_tcnp;
    struct fuse_rename_in in = { .newdir = tdnp->nodeid };
    uint32_t tlen = (uint32_t)tcnp->cn_namelen + 1;

    if (vfs_isrdonly(vnode_mount(ap->a_fdvp)))
        return EROFS;
    int error = name_check(fcnp);
    if (!error)
        error = name_check(tcnp);
    if (error)
        return error;
    char *target = IOMalloc(tlen);
    if (!target)
        return ENOMEM;
    memcpy(target, tcnp->cn_nameptr, tlen - 1);
    target[tlen - 1] = '\0';
    struct virtiofs_call c = {
        .opcode = FUSE_RENAME, .flags = VIRTIOFS_DATA_OUT, .nodeid = fdnp->nodeid,
        .arg = { &in, fcnp->cn_nameptr, "" }, .arglen = { sizeof(in), (uint32_t)fcnp->cn_namelen, 1 },
        .data = target, .datalen = tlen,
    };
    error = virtiofs_call(fdnp->vmp, ap->a_context, &c);
    IOFree(target, tlen);
    dir_changed(fdnp, fcnp, 0, NULL);
    dir_changed(tdnp, tcnp, 0, NULL);
    if (!error) {
        node_stale(VTON(ap->a_fvp));
        if (node_place(ap->a_fvp, ap->a_tdvp, tcnp)) {
            IOLockLock(fdnp->vmp->move_lock);
            const char *had = vnode_getname(ap->a_fvp);
            vnode_update_identity(ap->a_fvp, NULLVP, had ? NULL : tcnp->cn_nameptr, had ? 0 : tcnp->cn_namelen,
                                  had ? 0 : tcnp->cn_hash, VNODE_UPDATE_PARENT | VNODE_UPDATE_NAME);
            IOLockUnlock(fdnp->vmp->move_lock);
            if (had)
                vnode_putname(had);
        }
        if (ap->a_tvp && ap->a_tvp != ap->a_fvp)
            node_gone(ap->a_tvp);
    }
    return error;
}

static int
xattr_get(vnode_t vp, vfs_context_t ctx, uint32_t opcode, const char *name, uio_t uio, size_t *size)
{
    struct virtiofs_node *np = VTON(vp);
    uint32_t namelen = name ? (uint32_t)strlen(name) : 0;
    uint32_t room = uio ? (uint32_t)MIN(uio_resid(uio), (user_ssize_t)np->vmp->session.max_io) : 0;
    struct fuse_getxattr_in in = { .size = room };
    struct fuse_getxattr_out out = {};
    struct virtiofs_call c = {
        .opcode = opcode, .nodeid = np->nodeid,
        .arg = { &in, name, name ? "" : NULL }, .arglen = { sizeof(in), namelen, name ? 1 : 0 },
    };
    char *buf = NULL;

    if (uio && uio_offset(uio) != 0)
        return EINVAL;
    if (room) {
        if (!(buf = IOMalloc(room)))
            return ENOMEM;
        c.data = buf;
        c.datalen = room;
    } else {
        c.reply = &out;
        c.replylen = sizeof(out);
    }
    int error = virtiofs_call(np->vmp, ctx, &c);
    if (!error && !room && c.replylen != sizeof(out))
        error = EIO;
    if (!error && room && opcode == FUSE_LISTXATTR && !virtiofs_xattr_list_ok(buf, c.datalen))
        error = EIO;
    if (!error && room)
        error = uiomove(buf, (int)c.datalen, uio);
    if (!error && !room && uio && out.size)
        error = ERANGE;
    if (!error && !room && size)
        *size = out.size;
    if (buf)
        IOFree(buf, room);
    return error;
}

static int
virtiofs_vnop_getxattr(struct vnop_getxattr_args *ap)
{
    return xattr_get(ap->a_vp, ap->a_context, FUSE_GETXATTR, ap->a_name, ap->a_uio, ap->a_size);
}

static int
virtiofs_vnop_listxattr(struct vnop_listxattr_args *ap)
{
    return xattr_get(ap->a_vp, ap->a_context, FUSE_LISTXATTR, NULL, ap->a_uio, ap->a_size);
}

static int
virtiofs_vnop_setxattr(struct vnop_setxattr_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct virtiofs_node *np = VTON(vp);
    uio_t uio = ap->a_uio;
    uint32_t len = (uint32_t)uio_resid(uio);
    struct fuse_setxattr_in in = {
        .size = len,
        .flags = ((ap->a_options & XATTR_CREATE) ? VIRTIOFS_XATTR_CREATE : 0) |
                 ((ap->a_options & XATTR_REPLACE) ? VIRTIOFS_XATTR_REPLACE : 0),
    };
    uint32_t inlen = (np->vmp->session.flags & FUSE_SETXATTR_EXT) ? sizeof(in) : FUSE_COMPAT_SETXATTR_IN_SIZE;

    if (vfs_isrdonly(vnode_mount(vp)))
        return EROFS;
    if (uio_offset(uio) != 0)
        return EINVAL;
    if (uio_resid(uio) > (user_ssize_t)np->vmp->session.max_io)
        return E2BIG;
    char *buf = len ? IOMalloc(len) : NULL;
    if (len && !buf)
        return ENOMEM;
    int error = len ? uiomove(buf, (int)len, uio) : 0;
    struct virtiofs_call c = {
        .opcode = FUSE_SETXATTR, .flags = VIRTIOFS_DATA_OUT, .nodeid = np->nodeid,
        .arg = { &in, ap->a_name, "" }, .arglen = { inlen, (uint32_t)strlen(ap->a_name), 1 },
        .data = buf, .datalen = len,
    };
    IOLockLock(np->vmp->xattr_lock);
    if (!error && in.flags) {
        error = xattr_get(vp, ap->a_context, FUSE_GETXATTR, ap->a_name, NULL, NULL);
        if (error == 0 && (in.flags & VIRTIOFS_XATTR_CREATE))
            error = EEXIST;
        else if (error == ENOATTR && (in.flags & VIRTIOFS_XATTR_CREATE))
            error = 0;
    }
    if (!error)
        error = virtiofs_call(np->vmp, ap->a_context, &c);
    IOLockUnlock(np->vmp->xattr_lock);
    if (buf)
        IOFree(buf, len);
    node_stale(np);
    return error;
}

static int
virtiofs_vnop_removexattr(struct vnop_removexattr_args *ap)
{
    struct virtiofs_node *np = VTON(ap->a_vp);
    struct virtiofs_call c = {
        .opcode = FUSE_REMOVEXATTR, .nodeid = np->nodeid,
        .arg = { ap->a_name, "" }, .arglen = { (uint32_t)strlen(ap->a_name), 1 },
    };

    if (vfs_isrdonly(vnode_mount(ap->a_vp)))
        return EROFS;
    IOLockLock(np->vmp->xattr_lock);
    int error = virtiofs_call(np->vmp, ap->a_context, &c);
    IOLockUnlock(np->vmp->xattr_lock);
    node_stale(np);
    return error;
}

static int
virtiofs_vnop_inactive(struct vnop_inactive_args *ap)
{
    vnode_t vp = ap->a_vp;

    if (vnode_isreg(vp)) {
        push_dirty(vp);
        IOLockLock(VTON(vp)->vmp->lock);
        VTON(vp)->creator = NULL;
        IOLockUnlock(VTON(vp)->vmp->lock);
        whandle_close(VTON(vp), false);
        file_idle(VTON(vp));
    } else {
        dir_close(VTON(vp));
    }
    return 0;
}

static int
virtiofs_vnop_reclaim(struct vnop_reclaim_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct virtiofs_node *np = VTON(vp);
    struct virtiofs_mount *vmp = np->vmp;
    uint64_t fh;

    whandle_close(np, true);
    IOLockLock(vmp->lock);
    while (np->users || (np->flags & VN_OPENING)) {
        np->flags |= VN_WANT;
        IOLockSleep(vmp->lock, np, THREAD_UNINT);
    }
    bool release = handle_drop_locked(np, &fh);
    IOLockUnlock(vmp->lock);
    if (release)
        handle_release(np, fh);
    IOLockLock(vmp->lock);
    while (np->releasing) {
        np->flags |= VN_WANT;
        IOLockSleep(vmp->lock, np, THREAD_UNINT);
    }
    if (np->flags & VN_HASHED) {
        LIST_REMOVE(np, link);
        np->flags &= ~VN_HASHED;
    }
    name_drop_locked(np);
    uint64_t nlookup = np->nlookup;
    IOLockUnlock(vmp->lock);
    if (nlookup)
        virtiofs_dev_forget(vmp->dev, np->nodeid, nlookup);
    vnode_removefsref(vp);
    vnode_clearfsnode(vp);
    node_free(np);
    return 0;
}

#define VOPFUNC int (*)(void *)

static const struct vnodeopv_entry_desc virtiofs_vnodeop_entries[] = {
    { &vnop_default_desc, (VOPFUNC)vn_default_error },
    { &vnop_lookup_desc, (VOPFUNC)virtiofs_vnop_lookup },
    { &vnop_open_desc, (VOPFUNC)virtiofs_vnop_open },
    { &vnop_close_desc, (VOPFUNC)virtiofs_vnop_close },
    { &vnop_getattr_desc, (VOPFUNC)virtiofs_vnop_getattr },
    { &vnop_setattr_desc, (VOPFUNC)virtiofs_vnop_setattr },
    { &vnop_read_desc, (VOPFUNC)virtiofs_vnop_read },
    { &vnop_write_desc, (VOPFUNC)virtiofs_vnop_write },
    { &vnop_create_desc, (VOPFUNC)virtiofs_vnop_create },
    { &vnop_mknod_desc, (VOPFUNC)virtiofs_vnop_mknod },
    { &vnop_mkdir_desc, (VOPFUNC)virtiofs_vnop_mkdir },
    { &vnop_symlink_desc, (VOPFUNC)virtiofs_vnop_symlink },
    { &vnop_link_desc, (VOPFUNC)virtiofs_vnop_link },
    { &vnop_remove_desc, (VOPFUNC)virtiofs_vnop_remove },
    { &vnop_rmdir_desc, (VOPFUNC)virtiofs_vnop_rmdir },
    { &vnop_rename_desc, (VOPFUNC)virtiofs_vnop_rename },
    { &vnop_mnomap_desc, (VOPFUNC)virtiofs_vnop_mnomap },
    { &vnop_pagein_desc, (VOPFUNC)virtiofs_vnop_pagein },
    { &vnop_pageout_desc, (VOPFUNC)virtiofs_vnop_pageout },
    { &vnop_blockmap_desc, (VOPFUNC)virtiofs_vnop_blockmap },
    { &vnop_strategy_desc, (VOPFUNC)virtiofs_vnop_strategy },
    { &vnop_mmap_desc, (VOPFUNC)virtiofs_vnop_mmap },
    { &vnop_fsync_desc, (VOPFUNC)virtiofs_vnop_fsync },
    { &vnop_ioctl_desc, (VOPFUNC)virtiofs_vnop_ioctl },
    { &vnop_readdir_desc, (VOPFUNC)virtiofs_vnop_readdir },
    { &vnop_readlink_desc, (VOPFUNC)virtiofs_vnop_readlink },
    { &vnop_pathconf_desc, (VOPFUNC)virtiofs_vnop_pathconf },
    { &vnop_getxattr_desc, (VOPFUNC)virtiofs_vnop_getxattr },
    { &vnop_listxattr_desc, (VOPFUNC)virtiofs_vnop_listxattr },
    { &vnop_setxattr_desc, (VOPFUNC)virtiofs_vnop_setxattr },
    { &vnop_removexattr_desc, (VOPFUNC)virtiofs_vnop_removexattr },
    { &vnop_inactive_desc, (VOPFUNC)virtiofs_vnop_inactive },
    { &vnop_reclaim_desc, (VOPFUNC)virtiofs_vnop_reclaim },
    { NULL, NULL }
};

struct vnodeopv_desc virtiofs_vnodeop_opv_desc = { &virtiofs_vnodeop_p, virtiofs_vnodeop_entries };

static const struct vnodeopv_entry_desc virtiofs_fifoop_entries[] = {
    { &vnop_default_desc, (VOPFUNC)vn_default_error },
    { &vnop_lookup_desc, (VOPFUNC)fifo_lookup },
    { &vnop_open_desc, (VOPFUNC)fifo_open },
    { &vnop_close_desc, (VOPFUNC)fifo_close },
    { &vnop_getattr_desc, (VOPFUNC)virtiofs_vnop_getattr },
    { &vnop_setattr_desc, (VOPFUNC)virtiofs_vnop_setattr },
    { &vnop_read_desc, (VOPFUNC)fifo_read },
    { &vnop_write_desc, (VOPFUNC)fifo_write },
    { &vnop_ioctl_desc, (VOPFUNC)fifo_ioctl },
    { &vnop_select_desc, (VOPFUNC)fifo_select },
    { &vnop_revoke_desc, (VOPFUNC)fifo_revoke },
    { &vnop_mmap_desc, (VOPFUNC)fifo_mmap },
    { &vnop_fsync_desc, (VOPFUNC)virtiofs_vnop_fsync },
    { &vnop_pathconf_desc, (VOPFUNC)fifo_pathconf },
    { &vnop_getxattr_desc, (VOPFUNC)virtiofs_vnop_getxattr },
    { &vnop_listxattr_desc, (VOPFUNC)virtiofs_vnop_listxattr },
    { &vnop_setxattr_desc, (VOPFUNC)virtiofs_vnop_setxattr },
    { &vnop_removexattr_desc, (VOPFUNC)virtiofs_vnop_removexattr },
    { &vnop_inactive_desc, (VOPFUNC)virtiofs_vnop_inactive },
    { &vnop_reclaim_desc, (VOPFUNC)virtiofs_vnop_reclaim },
    { NULL, NULL }
};

struct vnodeopv_desc virtiofs_fifoop_opv_desc = { &virtiofs_fifoop_p, virtiofs_fifoop_entries };

/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/queue.h>
#include <IOKit/IOLocks.h>
#include "fuse.h"
#include "virtiofs_dev.h"

struct virtiofs_node;
LIST_HEAD(virtiofs_bucket, virtiofs_node);

struct virtiofs_mount {
    mount_t mp;
    struct virtiofs_dev *dev;
    struct virtiofs_session session;
    IOLock *lock;
    IOLock *move_lock;
    IOLock *xattr_lock;
    uint64_t rootid;
    uint64_t version;
    vnode_t root;
    uint32_t hash_bits;
    uint32_t nidle;
    int case_sensitive;
    struct virtiofs_bucket *hash;
    struct virtiofs_bucket *names;
    TAILQ_HEAD(virtiofs_idle, virtiofs_node) idle;
};

#define VFSTOVMP(mp) ((struct virtiofs_mount *)vfs_fsprivate(mp))

extern int virtiofs_entry_cache;
extern int virtiofs_idle_files;
extern int virtiofs_revalidate_open;
extern struct vnodeopv_desc virtiofs_vnodeop_opv_desc;
extern struct vnodeopv_desc virtiofs_fifoop_opv_desc;

int virtiofs_vfs_register(void);
int virtiofs_vfs_unregister(void);
int virtiofs_call(struct virtiofs_mount *vmp, vfs_context_t ctx, struct virtiofs_call *call);
uint64_t virtiofs_version(struct virtiofs_mount *vmp);
void virtiofs_vnode_push(vnode_t vp);
int virtiofs_node_get(struct virtiofs_mount *vmp, const struct fuse_entry_out *entry, uint64_t version,
                      uint64_t nlookup, vnode_t dvp, struct componentname *cnp, vnode_t *vpp);

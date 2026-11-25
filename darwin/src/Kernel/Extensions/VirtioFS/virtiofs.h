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

#define VIRTIOFS_HASH_BITS 10

struct virtiofs_node;
LIST_HEAD(virtiofs_bucket, virtiofs_node);

struct virtiofs_mount {
    mount_t mp;
    struct virtiofs_dev *dev;
    struct virtiofs_session session;
    IOLock *lock;
    IOLock *move_lock;
    uint64_t rootid;
    uint64_t attr_version;
    vnode_t root;
    struct virtiofs_bucket hash[1 << VIRTIOFS_HASH_BITS];
    struct virtiofs_bucket names[1 << VIRTIOFS_HASH_BITS];
};

#define VFSTOVMP(mp) ((struct virtiofs_mount *)vfs_fsprivate(mp))

extern int virtiofs_entry_cache;
extern struct vnodeopv_desc virtiofs_vnodeop_opv_desc;
extern struct vnodeopv_desc virtiofs_fifoop_opv_desc;

int virtiofs_vfs_register(void);
int virtiofs_vfs_unregister(void);
int virtiofs_call(struct virtiofs_mount *vmp, vfs_context_t ctx, struct virtiofs_call *call);
uint64_t virtiofs_attr_version(struct virtiofs_mount *vmp);
int virtiofs_node_get(struct virtiofs_mount *vmp, const struct fuse_entry_out *entry, uint64_t version,
                      uint64_t nlookup, vnode_t dvp, struct componentname *cnp, vnode_t *vpp);

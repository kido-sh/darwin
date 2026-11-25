/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

#define VIRTIOFS_DATA_OUT  0x1
#define VIRTIOFS_ZERO_FILL 0x2

struct virtiofs_dev;

struct virtiofs_call {
    uint32_t opcode;
    uint32_t flags;
    uint64_t nodeid;
    const void *arg[2];
    uint32_t arglen[2];
    void *reply;
    uint32_t replylen;
    uint32_t datalen;
    void *data;
    void *upl;
    uint32_t upl_offset;
};

typedef void (*virtiofs_done_t)(void *arg, int error);
typedef bool (*virtiofs_poll_t)(void *arg);

struct virtiofs_session {
    uint32_t max_io;
    uint32_t flags;
};

struct virtiofs_dev *virtiofs_dev_find(const char *tag);
void virtiofs_dev_release(struct virtiofs_dev *dev);
int virtiofs_dev_start(struct virtiofs_dev *dev, struct virtiofs_session *session);
int virtiofs_dev_call(struct virtiofs_dev *dev, struct virtiofs_call *call, uint32_t uid, uint32_t gid, uint32_t pid);
void virtiofs_dev_submit(struct virtiofs_dev *dev, struct virtiofs_call *call, virtiofs_done_t done, void *arg);
void virtiofs_dev_forget(struct virtiofs_dev *dev, uint64_t nodeid, uint64_t nlookup);
void virtiofs_dev_poll(struct virtiofs_dev *dev, virtiofs_poll_t done, void *arg, uint32_t datalen);

__END_DECLS

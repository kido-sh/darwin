/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include "VirtioFSDevice.h"
#include <IOKit/IOLib.h>
#include <libkern/OSByteOrder.h>
#include <sys/errno.h>
#include "fuse.h"

#define super IOService
OSDefineMetaClassAndStructors(VirtioFSDevice, IOService)

enum {
    kRequestBytes = 4096,
    kReplyBytes = 61440,
    kTimeoutMs = 5000,
    kHiprioQueue = 0,
    kFirstRequestQueue = 1,
    kUnknownOpcode = 999,
    kMaxForgets = 32,
};

bool
VirtioFSDevice::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;
    fPCIDevice->retain();
    if (!fPCIDevice->open(this) || !fTransport.attach(fPCIDevice)) {
        IOLog("VirtioFS: cannot attach to the PCI device\n");
        return false;
    }

    uint32_t low = fTransport.deviceFeatures(0);
    uint32_t high = fTransport.deviceFeatures(1);
    IOLog("VirtioFS: device features 0x%08x%08x, %u queues\n", high, low, fTransport.numQueues());
    if (!fTransport.negotiateFeatures(0)) {
        IOLog("VirtioFS: the device refused the features\n");
        return false;
    }

    volatile uint8_t *cfg = fTransport.deviceConfig();
    if (!cfg) {
        IOLog("VirtioFS: no device configuration\n");
        return false;
    }
    char tag[37];
    for (unsigned i = 0; i < 36; i++)
        tag[i] = (char)cfg[i];
    tag[36] = 0;
    IOLog("VirtioFS: tag \"%s\", %u request queues\n", tag, OSReadLittleInt32(cfg, 36));

    uint64_t shmPhys = 0, shmLength = 0;
    if (fTransport.sharedMemoryRegion(0, &shmPhys, &shmLength))
        IOLog("VirtioFS: DAX window at 0x%llx, %llu bytes\n", shmPhys, shmLength);
    else
        IOLog("VirtioFS: no DAX window\n");

    if (!fTransport.initQueue(&fHiprio, kHiprioQueue, 16) ||
        !fTransport.initQueue(&fRequests, kFirstRequestQueue, 64)) {
        IOLog("VirtioFS: cannot set up the queues\n");
        return false;
    }

    fBuffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
        kIODirectionInOut | kIOMemoryPhysicallyContiguous, kRequestBytes + kReplyBytes, 0xFFFFFFFFULL);
    if (!fBuffer)
        return false;
    if (fBuffer->prepare() != kIOReturnSuccess) {
        OSSafeReleaseNULL(fBuffer);
        return false;
    }
    fBytes = (uint8_t *)fBuffer->getBytesNoCopy();
    IOByteCount segment = 0;
    fPhys = fBuffer->getPhysicalSegment(0, &segment, 0);
    if (!fPhys || segment < kRequestBytes + kReplyBytes) {
        IOLog("VirtioFS: the request buffer is not physically contiguous\n");
        return false;
    }

    if (!setupInterrupt())
        IOLog("VirtioFS: no interrupt, polling only\n");
    fTransport.setDriverOk();

    probeInit();
    probeRoot();
    probeErrors();

    registerService();
    return true;
}

void
VirtioFSDevice::stop(IOService *provider)
{
    if (fInterruptSource) {
        fInterruptSource->disable();
        if (fWorkLoop)
            fWorkLoop->removeEventSource(fInterruptSource);
        OSSafeReleaseNULL(fInterruptSource);
    }
    fTransport.freeQueue(&fRequests);
    fTransport.freeQueue(&fHiprio);
    fTransport.detach();
    if (fPCIDevice)
        fPCIDevice->close(this);
    super::stop(provider);
}

void
VirtioFSDevice::free()
{
    OSSafeReleaseNULL(fInterruptSource);
    OSSafeReleaseNULL(fWorkLoop);
    if (fBuffer) {
        fBuffer->complete();
        OSSafeReleaseNULL(fBuffer);
    }
    OSSafeReleaseNULL(fPCIDevice);
    super::free();
}

bool
VirtioFSDevice::setupInterrupt()
{
    int type = 0;
    if (fPCIDevice->getInterruptType(0, &type) != kIOReturnSuccess)
        return false;
    fWorkLoop = IOWorkLoop::workLoop();
    if (!fWorkLoop)
        return false;
    fInterruptSource = IOInterruptEventSource::interruptEventSource(this,
        OSMemberFunctionCast(IOInterruptEventAction, this, &VirtioFSDevice::interruptOccurred),
        fPCIDevice, 0);
    if (!fInterruptSource || fWorkLoop->addEventSource(fInterruptSource) != kIOReturnSuccess) {
        OSSafeReleaseNULL(fInterruptSource);
        return false;
    }
    fInterruptSource->enable();
    return true;
}

void
VirtioFSDevice::interruptOccurred(IOInterruptEventSource *, int)
{
    fTransport.readIsr();
}

void *
VirtioFSDevice::request(uint32_t opcode, uint64_t nodeid, uint32_t argLen)
{
    bzero(fBytes, kRequestBytes + kReplyBytes);
    struct fuse_in_header *in = (struct fuse_in_header *)fBytes;
    fUnique += 2;
    in->len = (uint32_t)sizeof(*in) + argLen;
    in->opcode = opcode;
    in->unique = fUnique;
    in->nodeid = nodeid;
    return in + 1;
}

int
VirtioFSDevice::send(VirtQueue *vq, bool wantReply, uint32_t *replyLen)
{
    struct fuse_in_header *in = (struct fuse_in_header *)fBytes;
    VirtIOChainEntry chain[2] = {
        { fPhys, in->len, false, false },
        { fPhys + kRequestBytes, kReplyBytes, true, false },
    };
    uint32_t used = 0;

    *replyLen = 0;
    fTransport.addDescChain(vq, chain, wantReply ? 2 : 1);
    fTransport.notify(vq);
    if (!fTransport.pollForCompletion(vq, kTimeoutMs, &used)) {
        IOLog("VirtioFS: opcode %u got no answer\n", in->opcode);
        return -ETIMEDOUT;
    }
    if (!wantReply)
        return 0;

    struct fuse_out_header *out = (struct fuse_out_header *)(fBytes + kRequestBytes);
    if (used < sizeof(*out) || out->len < sizeof(*out) || out->len > used || out->unique != in->unique) {
        IOLog("VirtioFS: opcode %u: bad reply, %u bytes used, length %u, unique %llu for %llu\n",
              in->opcode, used, out->len, out->unique, in->unique);
        return -EPROTO;
    }
    *replyLen = out->len - (uint32_t)sizeof(*out);
    return out->error;
}

void *
VirtioFSDevice::reply()
{
    return fBytes + kRequestBytes + sizeof(struct fuse_out_header);
}

void
VirtioFSDevice::probeInit()
{
    static const uint32_t offers[] = {
        0xffffffffu,
        FUSE_ASYNC_READ | FUSE_BIG_WRITES | FUSE_DO_READDIRPLUS | FUSE_READDIRPLUS_AUTO | FUSE_MAX_PAGES,
    };

    for (unsigned i = 0; i < sizeof(offers) / sizeof(offers[0]); i++) {
        struct fuse_init_in *init = (struct fuse_init_in *)request(FUSE_INIT, 0, sizeof(struct fuse_init_in));
        init->major = FUSE_KERNEL_VERSION;
        init->minor = FUSE_KERNEL_MINOR_VERSION;
        init->max_readahead = 1u << 20;
        init->flags = offers[i];
        init->flags2 = (offers[i] & FUSE_INIT_EXT) ? 0xffffffffu : 0;

        uint32_t length = 0;
        int error = send(&fRequests, true, &length);
        IOLog("VirtioFS: INIT offering 0x%08x: error %d, %u bytes\n", offers[i], error, length);
        if (error)
            continue;

        struct fuse_init_out *out = (struct fuse_init_out *)reply();
        IOLog("VirtioFS: FUSE %u.%u flags 0x%08x flags2 0x%08x max_write %u max_readahead %u "
              "max_pages %u max_background %u congestion %u time_gran %u map_alignment %u "
              "max_stack_depth %u request_timeout %u\n",
              out->major, out->minor, out->flags, out->flags2, out->max_write, out->max_readahead,
              out->max_pages, out->max_background, out->congestion_threshold, out->time_gran,
              out->map_alignment, out->max_stack_depth, out->request_timeout);
        fInitFlags = out->flags;
        return;
    }
}

void
VirtioFSDevice::probeRoot()
{
    uint32_t length = 0;

    request(FUSE_STATFS, FUSE_ROOT_ID, 0);
    int error = send(&fRequests, true, &length);
    if (!error) {
        struct fuse_kstatfs *st = &((struct fuse_statfs_out *)reply())->st;
        IOLog("VirtioFS: STATFS blocks %llu bfree %llu bavail %llu files %llu ffree %llu bsize %u "
              "frsize %u namelen %u\n", st->blocks, st->bfree, st->bavail, st->files, st->ffree,
              st->bsize, st->frsize, st->namelen);
    } else {
        IOLog("VirtioFS: STATFS error %d\n", error);
    }

    request(FUSE_GETATTR, FUSE_ROOT_ID, sizeof(struct fuse_getattr_in));
    error = send(&fRequests, true, &length);
    if (!error) {
        struct fuse_attr_out *ao = (struct fuse_attr_out *)reply();
        IOLog("VirtioFS: root ino %llu mode 0%o nlink %u uid %u gid %u size %llu blksize %u "
              "attr_valid %llu.%09u\n", ao->attr.ino, ao->attr.mode, ao->attr.nlink, ao->attr.uid,
              ao->attr.gid, ao->attr.size, ao->attr.blksize, ao->attr_valid, ao->attr_valid_nsec);
    } else {
        IOLog("VirtioFS: GETATTR error %d\n", error);
    }

    request(FUSE_OPENDIR, FUSE_ROOT_ID, sizeof(struct fuse_open_in));
    error = send(&fRequests, true, &length);
    if (error) {
        IOLog("VirtioFS: OPENDIR error %d\n", error);
        return;
    }
    uint64_t fh = ((struct fuse_open_out *)reply())->fh;
    IOLog("VirtioFS: OPENDIR fh %llu open_flags 0x%x\n", fh, ((struct fuse_open_out *)reply())->open_flags);

    bool plus = (fInitFlags & FUSE_DO_READDIRPLUS) != 0;
    struct fuse_read_in *read = (struct fuse_read_in *)request(plus ? FUSE_READDIRPLUS : FUSE_READDIR,
        FUSE_ROOT_ID, sizeof(struct fuse_read_in));
    read->fh = fh;
    read->size = 8192;
    error = send(&fRequests, true, &length);
    IOLog("VirtioFS: %s error %d, %u bytes\n", plus ? "READDIRPLUS" : "READDIR", error, length);

    uint64_t nodeids[kMaxForgets];
    unsigned nodes = 0;
    uint8_t *p = (uint8_t *)reply();
    uint32_t off = 0;
    while (!error && off < length) {
        struct fuse_dirent *d;
        uint64_t nodeid = 0;
        size_t record;
        if (plus) {
            if (length - off < FUSE_NAME_OFFSET_DIRENTPLUS)
                break;
            struct fuse_direntplus *dp = (struct fuse_direntplus *)(p + off);
            d = &dp->dirent;
            nodeid = dp->entry_out.nodeid;
            record = FUSE_DIRENTPLUS_SIZE(dp);
        } else {
            if (length - off < FUSE_NAME_OFFSET)
                break;
            d = (struct fuse_dirent *)(p + off);
            record = FUSE_DIRENT_SIZE(d);
        }
        if (d->namelen > 1024 || record > length - off) {
            IOLog("VirtioFS: bad directory entry at %u\n", off);
            break;
        }
        IOLog("VirtioFS:   \"%.*s\" type %u ino %llu nodeid %llu off %llu\n", (int)d->namelen, d->name,
              d->type, d->ino, nodeid, d->off);
        bool dot = (d->namelen == 1 && d->name[0] == '.') ||
                   (d->namelen == 2 && d->name[0] == '.' && d->name[1] == '.');
        if (nodeid && !dot && nodes < kMaxForgets)
            nodeids[nodes++] = nodeid;
        off += (uint32_t)record;
    }
    for (unsigned i = 0; i < nodes; i++)
        forget(nodeids[i]);

    struct fuse_release_in *release = (struct fuse_release_in *)request(FUSE_RELEASEDIR, FUSE_ROOT_ID,
        sizeof(struct fuse_release_in));
    release->fh = fh;
    IOLog("VirtioFS: RELEASEDIR error %d\n", send(&fRequests, true, &length));
}

void
VirtioFSDevice::probeErrors()
{
    uint32_t length = 0;

    request(kUnknownOpcode, FUSE_ROOT_ID, 0);
    int error = send(&fRequests, true, &length);
    IOLog("VirtioFS: unknown opcode: error %d, ENOSYS is 38 on Linux and %d here\n", error, ENOSYS);

    char *name = (char *)request(FUSE_LOOKUP, FUSE_ROOT_ID, 300);
    memset(name, 'x', 299);
    error = send(&fRequests, true, &length);
    IOLog("VirtioFS: 299-byte name: error %d, ENAMETOOLONG is 36 on Linux and %d here\n", error, ENAMETOOLONG);
}

void
VirtioFSDevice::forget(uint64_t nodeid)
{
    struct fuse_forget_in *f = (struct fuse_forget_in *)request(FUSE_FORGET, nodeid, sizeof(struct fuse_forget_in));
    f->nlookup = 1;
    uint32_t length = 0;
    IOLog("VirtioFS: FORGET %llu: %d\n", nodeid, send(&fHiprio, false, &length));
}

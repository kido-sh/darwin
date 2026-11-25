/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include "VirtioFSDevice.h"
#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <sys/errno.h>
#include "fuse_proto.h"

#define super IOService
OSDefineMetaClassAndStructors(VirtioFSDevice, IOService)

enum {
    kHiprio = 0,
    kRequests = 1,
    kQueues = 2,
    kSlotBytes = 4096,
    kMaxSegments = 40,
    kTableBytes = kMaxSegments * sizeof(VRingDesc),
    kInBytes = 2048,
    kOutBytes = kSlotBytes - kTableBytes - kInBytes,
    kMaxSlots = 128,
    kMaxIO = 1 << 17,
    kMinIO = 1 << 12,
    kWaitSliceMs = 100,
};

extern "C" int virtiofs_spin_us;

static const char kTagKey[] = "VirtioFSTag";
static const uint64_t kMatchTimeoutNs = 1000 * 1000 * 1000;

struct VirtioFSSlot {
    VRingDesc *table;
    uint8_t *in;
    uint8_t *out;
    uint64_t tablePhys;
    uint64_t inPhys;
    uint64_t outPhys;
    VirtioFSSlot *next;
    struct virtiofs_call call;
    IOMemoryDescriptor *data;
    virtiofs_done_t done;
    void *arg;
    uint32_t used;
    bool replied;
    bool async;
};

struct VirtioFSRequest {
    VirtIOChainEntry chain[kMaxSegments];
    unsigned count;
    unsigned replyAt;
    IOMemoryDescriptor *data;
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
    fLock = IOLockAlloc();
    fSessionLock = IOLockAlloc();
    if (!fLock || !fSessionLock || !fPCIDevice->open(this))
        return false;
    if (!fTransport.attach(fPCIDevice)) {
        fPCIDevice->close(this);
        return false;
    }

    uint32_t features = fTransport.deviceFeatures(0) & VIRTIO_RING_F_INDIRECT_DESC;
    fIndirect = features != 0;
    volatile uint8_t *cfg = fTransport.deviceConfig();
    char tag[37] = {};
    for (unsigned i = 0; cfg && i < 36 && i < fTransport.deviceConfigLength(); i++)
        tag[i] = (char)cfg[i];

    if (!tag[0] || !fTransport.negotiateFeatures(features) || !setupQueues() || !setupSlots() ||
        !setupInterrupt()) {
        IOLog("VirtioFS: cannot set up the device\n");
        fTransport.detach();
        fPCIDevice->close(this);
        return false;
    }
    fTransport.setDriverOk();
    fInterruptSource->enable();

    setProperty(kTagKey, tag);
    registerService();
    IOLog("VirtioFS: tag \"%s\", %u slots, %u descriptors%s\n", tag, fSlotCount, fQueues[kRequests].queueSize,
          fIndirect ? ", indirect" : "");
    return true;
}

void
VirtioFSDevice::stop(IOService *provider)
{
    IOLockLock(fLock);
    failLocked(false);
    unlockAndReap();
    fWorkLoop->removeEventSource(fInterruptSource);
    OSSafeReleaseNULL(fInterruptSource);
    fTransport.detach();
    fPCIDevice->close(this);
    super::stop(provider);
}

void
VirtioFSDevice::free()
{
    OSSafeReleaseNULL(fInterruptSource);
    OSSafeReleaseNULL(fWorkLoop);
    if (fSlots)
        IOFree(fSlots, sizeof(VirtioFSSlot) * fSlotCount);
    if (fSlotMem) {
        fSlotMem->complete();
        fSlotMem->release();
    }
    for (unsigned q = 0; q < kQueues; q++) {
        if (fInFlight[q])
            IOFree(fInFlight[q], sizeof(VirtioFSSlot *) * fQueues[q].queueSize);
        fTransport.freeQueue(&fQueues[q]);
    }
    if (fLock)
        IOLockFree(fLock);
    if (fSessionLock)
        IOLockFree(fSessionLock);
    OSSafeReleaseNULL(fPCIDevice);
    super::free();
}

bool
VirtioFSDevice::setupQueues()
{
    static const uint16_t sizes[kQueues] = { 64, 256 };

    for (unsigned q = 0; q < kQueues; q++) {
        if (!fTransport.initQueue(&fQueues[q], (uint16_t)q, sizes[q]) ||
            (q == kRequests && fQueues[q].queueSize < kMaxSegments))
            return false;
        fTransport.enableFreeList(&fQueues[q]);
        fInFlight[q] = (VirtioFSSlot **)IOMallocZero(sizeof(VirtioFSSlot *) * fQueues[q].queueSize);
        if (!fInFlight[q])
            return false;
    }
    return true;
}

bool
VirtioFSDevice::setupSlots()
{
    unsigned count = fQueues[kRequests].queueSize < kMaxSlots ? fQueues[kRequests].queueSize : kMaxSlots;

    fSlotMem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionInOut, count * kSlotBytes,
                                                           PAGE_SIZE);
    if (!fSlotMem)
        return false;
    if (fSlotMem->prepare() != kIOReturnSuccess) {
        OSSafeReleaseNULL(fSlotMem);
        return false;
    }
    fSlots = (VirtioFSSlot *)IOMallocZero(sizeof(VirtioFSSlot) * count);
    if (!fSlots)
        return false;
    fSlotCount = count;

    uint8_t *base = (uint8_t *)fSlotMem->getBytesNoCopy();
    for (unsigned i = 0; i < count; i++) {
        VirtioFSSlot *slot = &fSlots[i];
        IOByteCount length = 0;
        uint64_t phys = fSlotMem->getPhysicalSegment(i * kSlotBytes, &length, kIOMemoryMapperNone);
        if (!phys || length < kSlotBytes)
            return false;
        slot->table = (VRingDesc *)(base + i * kSlotBytes);
        slot->in = (uint8_t *)slot->table + kTableBytes;
        slot->out = slot->in + kInBytes;
        slot->tablePhys = phys;
        slot->inPhys = phys + kTableBytes;
        slot->outPhys = slot->inPhys + kInBytes;
        slot->next = fFreeSlots;
        fFreeSlots = slot;
    }
    return true;
}

bool
VirtioFSDevice::setupInterrupt()
{
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
    return true;
}

void
VirtioFSDevice::interruptOccurred(IOInterruptEventSource *source, int)
{
    if (__atomic_load_n(&fDead, __ATOMIC_RELAXED)) {
        source->disable();
        return;
    }
    if (!(fTransport.readIsr() & 1))
        return;
    IOLockLock(fLock);
    drainLocked(kHiprio);
    drainLocked(kRequests);
    unlockAndReap();
}

void
VirtioFSDevice::drainLocked(unsigned q)
{
    VirtQueue *vq = &fQueues[q];
    uint32_t used = 0;
    uint16_t head = 0;
    bool freed = false;

    if (fDead)
        return;
    uint16_t pending = (uint16_t)(((volatile VRingUsedHdr *)vq->used)->idx - vq->lastUsedIdx);
    if (pending > vq->queueSize - vq->numFree) {
        failLocked(true);
        return;
    }
    while (pending-- && fTransport.pollForCompletion(vq, 0, &used, &head)) {
        VirtioFSSlot *slot = head < vq->queueSize ? fInFlight[q][head] : NULL;
        if (!slot) {
            failLocked(true);
            return;
        }
        fInFlight[q][head] = NULL;
        fTransport.freeDescChain(vq, head);
        freed = true;
        if (q == kHiprio) {
            putSlotLocked(slot);
        } else if (slot->async) {
            slot->used = used;
            __atomic_store_n(&slot->replied, true, __ATOMIC_RELAXED);
            slot->next = fReaped;
            fReaped = slot;
        } else {
            slot->used = used;
            __atomic_store_n(&slot->replied, true, __ATOMIC_RELAXED);
            IOLockWakeup(fLock, slot, true);
        }
    }
    if (freed)
        IOLockWakeup(fLock, vq, false);
}

void
VirtioFSDevice::failLocked(bool broken)
{
    if (fDead)
        return;
    if (broken)
        IOLog("VirtioFS: the device completed a request it was not given, failing it\n");
    __atomic_store_n(&fDead, true, __ATOMIC_RELAXED);
    fInterruptSource->disable();
    fQuiesced = fTransport.quiesce();
    for (unsigned i = 0; fQuiesced && i < fQueues[kRequests].queueSize; i++) {
        VirtioFSSlot *slot = fInFlight[kRequests][i];
        if (slot && slot->async) {
            fInFlight[kRequests][i] = NULL;
            slot->next = fReaped;
            fReaped = slot;
        }
    }
    for (unsigned i = 0; fQuiesced && i < fSlotCount; i++)
        IOLockWakeup(fLock, &fSlots[i], false);
    IOLockWakeup(fLock, &fFreeSlots, false);
    IOLockWakeup(fLock, &fQueues[kHiprio], false);
    IOLockWakeup(fLock, &fQueues[kRequests], false);
}

void
VirtioFSDevice::pauseLocked(void *event)
{
    if (fReaped) {
        unlockAndReap();
        IOLockLock(fLock);
        return;
    }
    AbsoluteTime deadline;
    clock_interval_to_deadline(kWaitSliceMs, kMillisecondScale, &deadline);
    if (IOLockSleepDeadline(fLock, event, deadline, THREAD_UNINT) != THREAD_TIMED_OUT)
        return;

    uint16_t seen = fQueues[kRequests].lastUsedIdx;
    drainLocked(kHiprio);
    drainLocked(kRequests);
    if (seen != fQueues[kRequests].lastUsedIdx && !fLostInterrupt) {
        fLostInterrupt = true;
        IOLog("VirtioFS: completions arrived without an interrupt\n");
    }
}

void
VirtioFSDevice::spinLocked(VirtioFSSlot *slot, uint32_t datalen)
{
    VirtQueue *vq = &fQueues[kRequests];
    volatile VRingUsedHdr *used = (volatile VRingUsedHdr *)vq->used;
    int spin = __atomic_load_n(&virtiofs_spin_us, __ATOMIC_RELAXED);
    uint64_t limit;

    if (fPolling || spin <= 0)
        return;
    fPolling = true;
    nanoseconds_to_absolutetime(((uint64_t)spin + datalen / 2048) * 1000, &limit);
    limit += mach_absolute_time();
    bool ready = true;
    while (ready && !slot->replied && !fDead) {
        unlockAndReap();
        while (!(ready = used->idx != __atomic_load_n(&vq->lastUsedIdx, __ATOMIC_RELAXED)) &&
               !__atomic_load_n(&slot->replied, __ATOMIC_RELAXED) && mach_absolute_time() < limit)
            __builtin_arm_yield();
        IOLockLock(fLock);
        if (ready)
            drainLocked(kRequests);
    }
    fPolling = false;
}

VirtioFSSlot *
VirtioFSDevice::takeSlotLocked()
{
    while (!fFreeSlots && !fDead) {
        fSlotWaiters++;
        pauseLocked(&fFreeSlots);
        fSlotWaiters--;
    }
    if (fDead)
        return NULL;
    VirtioFSSlot *slot = fFreeSlots;
    fFreeSlots = slot->next;
    slot->replied = false;
    slot->async = false;
    slot->used = 0;
    return slot;
}

void
VirtioFSDevice::putSlotLocked(VirtioFSSlot *slot)
{
    slot->next = fFreeSlots;
    fFreeSlots = slot;
    if (fSlotWaiters)
        IOLockWakeup(fLock, &fFreeSlots, true);
}

bool
VirtioFSDevice::submitLocked(unsigned q, VirtioFSSlot *slot, const VirtIOChainEntry *chain, unsigned count)
{
    VirtQueue *vq = &fQueues[q];
    VirtIOChainEntry table = { slot->tablePhys, (uint32_t)(count * sizeof(VRingDesc)), false, true };
    bool indirect = fIndirect && count > 1;

    if (indirect) {
        for (unsigned i = 0; i < count; i++) {
            slot->table[i].addr = chain[i].addr;
            slot->table[i].len = chain[i].len;
            slot->table[i].flags = (uint16_t)((chain[i].write ? VRING_DESC_F_WRITE : 0) |
                                              (i + 1 < count ? VRING_DESC_F_NEXT : 0));
            slot->table[i].next = (uint16_t)(i + 1);
        }
    }
    while (!fDead) {
        int head = indirect ? fTransport.addDescChainFree(vq, &table, 1) : fTransport.addDescChainFree(vq, chain, count);
        if (head >= 0) {
            fInFlight[q][head] = slot;
            fTransport.notify(vq);
            return true;
        }
        pauseLocked(vq);
    }
    return false;
}

VirtioFSSlot *
VirtioFSDevice::fillHeaderLocked(uint32_t opcode, uint64_t nodeid, uint32_t len)
{
    VirtioFSSlot *slot = takeSlotLocked();
    if (!slot)
        return NULL;
    struct fuse_in_header *in = (struct fuse_in_header *)slot->in;
    bzero(in, sizeof(*in));
    fUnique += 2;
    in->len = len;
    in->opcode = opcode;
    in->unique = fUnique;
    in->nodeid = nodeid;
    return slot;
}

static bool
addSegments(VirtIOChainEntry *chain, unsigned *count, IOMemoryDescriptor *md, uint32_t length, bool write)
{
    for (IOByteCount offset = 0; offset < length;) {
        IOByteCount segment = 0;
        addr64_t phys = md->getPhysicalSegment(offset, &segment, kIOMemoryMapperNone);
        if (!phys || !segment || *count >= kMaxSegments)
            return false;
        if (segment > length - offset)
            segment = length - offset;
        chain[(*count)++] = { phys, (uint32_t)segment, write, false };
        offset += segment;
    }
    return true;
}

static IOMemoryDescriptor *
dataDescriptor(const struct virtiofs_call *c)
{
    IODirection direction = (c->flags & VIRTIOFS_DATA_OUT) ? kIODirectionOut : kIODirectionIn;
    IOMemoryDescriptor *md;

    if (c->upl)
        md = IOMemoryDescriptor::withOptions(c->upl, c->datalen, c->upl_offset, NULL,
                                             kIOMemoryTypeUPL | kIOMemoryAsReference | direction);
    else
        md = IOMemoryDescriptor::withAddressRange((mach_vm_address_t)c->data, c->datalen, direction, kernel_task);
    if (md && md->prepare() != kIOReturnSuccess)
        OSSafeReleaseNULL(md);
    return md;
}

static void
finishData(IOMemoryDescriptor *data, const struct virtiofs_call *c, int error)
{
    if (!data)
        return;
    if (!error && !(c->flags & VIRTIOFS_DATA_OUT) && (c->flags & VIRTIOFS_ZERO_FILL) &&
        c->datalen < data->getLength())
        data->writeBytes(c->datalen, NULL, data->getLength() - c->datalen);
    data->complete();
    data->release();
}

static int
buildRequest(const struct virtiofs_call *c, VirtioFSRequest *r)
{
    bool out = (c->flags & VIRTIOFS_DATA_OUT) != 0;
    uint32_t args = c->arglen[0] + c->arglen[1];
    if (args < c->arglen[0] || args > kInBytes - sizeof(struct fuse_in_header) ||
        c->replylen > kOutBytes - sizeof(struct fuse_out_header) || c->datalen > kMaxIO)
        return EINVAL;

    r->data = NULL;
    if (c->datalen && !(r->data = dataDescriptor(c)))
        return ENOMEM;
    r->count = 1;
    bool fits = !r->data || !out || addSegments(r->chain, &r->count, r->data, c->datalen, false);
    fits = fits && r->count < kMaxSegments;
    r->replyAt = r->count++;
    fits = fits && (!r->data || out || addSegments(r->chain, &r->count, r->data, c->datalen, true));
    if (fits)
        return 0;
    finishData(r->data, c, EIO);
    return EIO;
}

static int
parseReply(VirtioFSSlot *slot, struct virtiofs_call *c)
{
    const struct fuse_in_header *in = (const struct fuse_in_header *)slot->in;
    bool out = (c->flags & VIRTIOFS_DATA_OUT) != 0;
    struct fuse_out_header reply;

    memcpy(&reply, slot->out, sizeof(reply));
    uint32_t room = c->replylen + (out ? 0 : c->datalen);
    if (slot->used < sizeof(reply) || reply.len < sizeof(reply) || reply.len > slot->used ||
        reply.len - sizeof(reply) > room || reply.unique != in->unique) {
        IOLog("VirtioFS: opcode %u: malformed reply\n", in->opcode);
        return EIO;
    }
    if (reply.error)
        return virtiofs_errno(reply.error);

    uint32_t payload = reply.len - (uint32_t)sizeof(reply);
    uint32_t fixed = payload < c->replylen ? payload : c->replylen;
    if (fixed)
        memcpy(c->reply, slot->out + sizeof(reply), fixed);
    c->replylen = fixed;
    if (!out)
        c->datalen = payload - fixed;
    return 0;
}

VirtioFSSlot *
VirtioFSDevice::startLocked(struct virtiofs_call *c, VirtioFSRequest *r, uint32_t uid, uint32_t gid, uint32_t pid,
                            bool async, virtiofs_done_t done, void *arg)
{
    uint32_t inLen = (uint32_t)sizeof(struct fuse_in_header) + c->arglen[0] + c->arglen[1];
    uint32_t dataOut = (c->flags & VIRTIOFS_DATA_OUT) ? c->datalen : 0;
    VirtioFSSlot *slot = fillHeaderLocked(c->opcode, c->nodeid, inLen + dataOut);
    if (!slot)
        return NULL;

    struct fuse_in_header *in = (struct fuse_in_header *)slot->in;
    in->uid = uid;
    in->gid = gid;
    in->pid = pid;
    if (c->arglen[0])
        memcpy(in + 1, c->arg[0], c->arglen[0]);
    if (c->arglen[1])
        memcpy((uint8_t *)(in + 1) + c->arglen[0], c->arg[1], c->arglen[1]);
    r->chain[0] = { slot->inPhys, inLen, false, false };
    r->chain[r->replyAt] = { slot->outPhys, (uint32_t)(sizeof(struct fuse_out_header) + c->replylen), true, false };
    if (async)
        slot->call = *c;
    slot->data = r->data;
    slot->done = done;
    slot->arg = arg;
    slot->async = async;
    if (submitLocked(kRequests, slot, r->chain, r->count))
        return slot;
    putSlotLocked(slot);
    return NULL;
}

int
VirtioFSDevice::call(struct virtiofs_call *c, uint32_t uid, uint32_t gid, uint32_t pid)
{
    VirtioFSRequest r;
    int error = buildRequest(c, &r);
    if (error)
        return error;

    error = EIO;
    IOLockLock(fLock);
    VirtioFSSlot *slot = startLocked(c, &r, uid, gid, pid, false, NULL, NULL);
    if (slot) {
        spinLocked(slot, c->datalen);
        while (!slot->replied && !fQuiesced)
            pauseLocked(slot);
        if (slot->replied) {
            error = parseReply(slot, c);
            putSlotLocked(slot);
        }
    }
    unlockAndReap();
    finishData(r.data, c, error);
    return error;
}

void
VirtioFSDevice::submit(struct virtiofs_call *c, virtiofs_done_t done, void *arg)
{
    VirtioFSRequest r;
    int error = buildRequest(c, &r);
    if (!error) {
        IOLockLock(fLock);
        if (!startLocked(c, &r, 0, 0, 0, true, done, arg))
            error = EIO;
        unlockAndReap();
        if (error)
            finishData(r.data, c, error);
    }
    if (error)
        done(arg, error);
}

void
VirtioFSDevice::unlockAndReap()
{
    VirtioFSSlot *list = fReaped;
    fReaped = NULL;
    IOLockUnlock(fLock);

    while (list) {
        VirtioFSSlot *slot = list;
        virtiofs_done_t done = slot->done;
        void *arg = slot->arg;
        list = slot->next;
        int error = slot->replied ? parseReply(slot, &slot->call) : EIO;
        finishData(slot->data, &slot->call, error);
        IOLockLock(fLock);
        putSlotLocked(slot);
        IOLockUnlock(fLock);
        done(arg, error);
    }
}

void
VirtioFSDevice::forget(uint64_t nodeid, uint64_t nlookup)
{
    uint32_t len = (uint32_t)(sizeof(struct fuse_in_header) + sizeof(struct fuse_forget_in));

    IOLockLock(fLock);
    VirtioFSSlot *slot = fillHeaderLocked(FUSE_FORGET, nodeid, len);
    if (slot) {
        ((struct fuse_forget_in *)(slot->in + sizeof(struct fuse_in_header)))->nlookup = nlookup;
        VirtIOChainEntry entry = { slot->inPhys, len, false, false };
        if (!submitLocked(kHiprio, slot, &entry, 1))
            putSlotLocked(slot);
    }
    unlockAndReap();
}

int
VirtioFSDevice::startSession(struct virtiofs_session *session)
{
    IOLockLock(fSessionLock);
    if (!fStarted && !fInitError) {
        struct fuse_init_in init = {};
        struct fuse_init_out reply = {};
        init.major = FUSE_KERNEL_VERSION;
        init.minor = FUSE_KERNEL_MINOR_VERSION;
        init.max_readahead = kMaxIO;
        init.flags = FUSE_ASYNC_READ | FUSE_PARALLEL_DIROPS;
        struct virtiofs_call c = {};
        c.opcode = FUSE_INIT;
        c.arg[0] = &init;
        c.arglen[0] = sizeof(init);
        c.reply = &reply;
        c.replylen = sizeof(reply);
        int error = call(&c, 0, 0, 0);
        if (!error && (reply.major != FUSE_KERNEL_VERSION || c.replylen < FUSE_COMPAT_22_INIT_OUT_SIZE))
            error = EPROTONOSUPPORT;
        if (error) {
            fInitError = error;
            IOLog("VirtioFS: FUSE_INIT failed with error %d\n", error);
        } else {
            uint32_t io = reply.max_write < reply.max_readahead ? reply.max_write : reply.max_readahead;
            fSession.max_io = io > kMaxIO ? kMaxIO : (io < kMinIO ? kMinIO : io & ~(kMinIO - 1));
            fSession.flags = reply.flags;
            fStarted = true;
            IOLog("VirtioFS: FUSE %u.%u, flags 0x%x, %u-byte I/O\n", reply.major, reply.minor, reply.flags,
                  fSession.max_io);
        }
    }
    int error = fInitError;
    if (!error)
        *session = fSession;
    IOLockUnlock(fSessionLock);
    return error;
}

VirtioFSDevice *
VirtioFSDevice::copyDevice(const char *tag)
{
    OSDictionary *match = serviceMatching("VirtioFSDevice");
    const OSSymbol *key = OSSymbol::withCString(kTagKey);
    OSString *value = OSString::withCString(tag);
    VirtioFSDevice *dev = NULL;

    if (match && key && value && propertyMatching(key, value, match)) {
        IOService *service = waitForMatchingService(match, kMatchTimeoutNs);
        dev = OSDynamicCast(VirtioFSDevice, service);
        if (!dev)
            OSSafeReleaseNULL(service);
    }
    OSSafeReleaseNULL(value);
    OSSafeReleaseNULL(key);
    OSSafeReleaseNULL(match);
    return dev;
}

extern "C" {

struct virtiofs_dev *
virtiofs_dev_find(const char *tag)
{
    return (struct virtiofs_dev *)VirtioFSDevice::copyDevice(tag);
}

void
virtiofs_dev_release(struct virtiofs_dev *dev)
{
    ((VirtioFSDevice *)dev)->release();
}

int
virtiofs_dev_start(struct virtiofs_dev *dev, struct virtiofs_session *session)
{
    return ((VirtioFSDevice *)dev)->startSession(session);
}

int
virtiofs_dev_call(struct virtiofs_dev *dev, struct virtiofs_call *call, uint32_t uid, uint32_t gid, uint32_t pid)
{
    return ((VirtioFSDevice *)dev)->call(call, uid, gid, pid);
}

void
virtiofs_dev_submit(struct virtiofs_dev *dev, struct virtiofs_call *call, virtiofs_done_t done, void *arg)
{
    ((VirtioFSDevice *)dev)->submit(call, done, arg);
}

void
virtiofs_dev_forget(struct virtiofs_dev *dev, uint64_t nodeid, uint64_t nlookup)
{
    ((VirtioFSDevice *)dev)->forget(nodeid, nlookup);
}

}

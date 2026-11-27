/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include "VirtioMetalDevice.h"
#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <libkern/OSByteOrder.h>

#define super IOService
OSDefineMetaClassAndStructors(VirtioMetalDevice, IOService)

enum {
    kCmdQueueIndex    = 0,
    kCmdQueueMaxSize  = 128,
    kMaxSlots         = 64,
    kSlotBytes        = 4096,
    kReqOff           = 0,
    kRespOff          = 64,
    kTableOff         = 128,
    kTableBytes       = kSlotBytes - kTableOff,
    kInlineTableDescs = kTableBytes / sizeof(VRingDesc),
    kRpcTimeoutMs     = 60000,
    kWaitSliceMs      = 50,
    kSpinUs           = 50,
    kMaxSegBytes      = 0x40000000u,
    kMaxWiredBytes    = 128u * 1024u * 1024u,
};

struct VirtioMetalSlot {
    struct kmtl_req_hdr *req;
    struct kmtl_resp_hdr *resp;
    VRingDesc *table;
    uint64_t reqPhys;
    uint64_t respPhys;
    uint64_t tablePhys;
    VirtioMetalSlot *next;
    uint32_t usedLen;
    bool replied;
};

static inline uint32_t
mmioRead32(volatile uint8_t *base, unsigned off)
{
    return OSReadLittleInt32((const void *)base, off);
}

static inline uint64_t
mmioRead64(volatile uint8_t *base, unsigned off)
{
    uint64_t lo = mmioRead32(base, off);
    uint64_t hi = mmioRead32(base, off + 4);
    return lo | (hi << 32);
}

bool
VirtioMetalDevice::readDeviceConfig()
{
    volatile uint8_t *cfg = fTransport.deviceConfig();
    if (!cfg || fTransport.deviceConfigLength() < sizeof(struct kmtl_device_config)) {
        IOLog("VirtioMetal: missing or truncated device config (%u bytes)\n",
              fTransport.deviceConfigLength());
        return false;
    }

    bzero(&fConfig, sizeof(fConfig));
    fConfig.magic                           = mmioRead32(cfg, offsetof(struct kmtl_device_config, magic));
    fConfig.version                         = mmioRead32(cfg, offsetof(struct kmtl_device_config, version));
    fConfig.host_page_size                  = mmioRead32(cfg, offsetof(struct kmtl_device_config, host_page_size));
    fConfig.flags                           = mmioRead32(cfg, offsetof(struct kmtl_device_config, flags));
    fConfig.max_buffer_length               = mmioRead64(cfg, offsetof(struct kmtl_device_config, max_buffer_length));
    fConfig.recommended_max_working_set_size = mmioRead64(cfg, offsetof(struct kmtl_device_config, recommended_max_working_set_size));
    fConfig.shm_aperture_bytes              = mmioRead64(cfg, offsetof(struct kmtl_device_config, shm_aperture_bytes));
    fConfig.max_threads_per_threadgroup[0]  = mmioRead32(cfg, offsetof(struct kmtl_device_config, max_threads_per_threadgroup) + 0);
    fConfig.max_threads_per_threadgroup[1]  = mmioRead32(cfg, offsetof(struct kmtl_device_config, max_threads_per_threadgroup) + 4);
    fConfig.max_threads_per_threadgroup[2]  = mmioRead32(cfg, offsetof(struct kmtl_device_config, max_threads_per_threadgroup) + 8);
    fConfig.max_threadgroup_memory_length   = mmioRead32(cfg, offsetof(struct kmtl_device_config, max_threadgroup_memory_length));
    fConfig.registry_id                     = mmioRead64(cfg, offsetof(struct kmtl_device_config, registry_id));

    if (fConfig.magic != KMTL_MAGIC || fConfig.version != KMTL_PROTO_VERSION) {
        IOLog("VirtioMetal: bad device config magic=0x%x version=%u (expected 0x%x v%u)\n",
              fConfig.magic, fConfig.version, KMTL_MAGIC, KMTL_PROTO_VERSION);
        return false;
    }
    return true;
}

bool
VirtioMetalDevice::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;
    fPCIDevice->retain();

    fLock = IOLockAlloc();
    if (!fLock || !fPCIDevice->open(this)) {
        if (fLock) {
            IOLockFree(fLock);
            fLock = NULL;
        }
        return false;
    }

    if (!fTransport.attach(fPCIDevice)) {
        IOLog("VirtioMetal: failed to attach VirtIO PCI transport\n");
        fPCIDevice->close(this);
        return false;
    }

    if (!readDeviceConfig()) {
        fTransport.detach();
        fPCIDevice->close(this);
        return false;
    }

    if (!fTransport.sharedMemoryRegion(KMTL_SHM_REGION_ID, &fShmPhys, &fShmLength) ||
        fShmPhys == 0 || fShmLength == 0) {
        IOLog("VirtioMetal: missing shared memory region %u\n", KMTL_SHM_REGION_ID);
        fTransport.detach();
        fPCIDevice->close(this);
        return false;
    }

    uint32_t features = fTransport.deviceFeatures(0) & VIRTIO_RING_F_INDIRECT_DESC;
    fIndirect = (features != 0);

    if (!fTransport.negotiateFeatures(features) || !setupQueue() || !setupSlots()) {
        IOLog("VirtioMetal: failed to negotiate features or initialize VQ0\n");
        fTransport.detach();
        fPCIDevice->close(this);
        return false;
    }

    if (!setupInterrupt())
        IOLog("VirtioMetal: no interrupt source, using timed drain\n");

    fTransport.setDriverOk();
    if (fInterruptSource)
        fInterruptSource->enable();

    setName("VirtioMetal");
    setProperty("IOUserClientClass", "VirtioMetalUserClient");
    registerService();

    IOLog("VirtioMetal: ready (shm phys=0x%llx len=%llu MiB, %u slots, %u descs%s%s)\n",
          fShmPhys, fShmLength >> 20, fSlotCount, fCmdQueue.queueSize,
          fIndirect ? ", indirect" : "", fUseInterrupts ? ", irq" : ", poll");
    return true;
}

void
VirtioMetalDevice::stop(IOService *provider)
{
    if (fLock) {
        IOLockLock(fLock);
        failLocked(false);
        IOLockUnlock(fLock);
    }
    if (fInterruptSource) {
        fInterruptSource->disable();
        if (fWorkLoop)
            fWorkLoop->removeEventSource(fInterruptSource);
        OSSafeReleaseNULL(fInterruptSource);
    }
    fTransport.detach();
    if (fPCIDevice)
        fPCIDevice->close(this);
    super::stop(provider);
}

void
VirtioMetalDevice::free()
{
    OSSafeReleaseNULL(fInterruptSource);
    OSSafeReleaseNULL(fWorkLoop);
    if (fSlots) {
        IOFree(fSlots, sizeof(VirtioMetalSlot) * fSlotCount);
        fSlots = NULL;
    }
    if (fSlotMem) {
        fSlotMem->complete();
        OSSafeReleaseNULL(fSlotMem);
    }
    if (fInFlight) {
        IOFree(fInFlight, sizeof(VirtioMetalSlot *) * fCmdQueue.queueSize);
        fInFlight = NULL;
    }
    fTransport.freeQueue(&fCmdQueue);
    if (fLock) {
        IOLockFree(fLock);
        fLock = NULL;
    }
    OSSafeReleaseNULL(fPCIDevice);
    super::free();
}

bool
VirtioMetalDevice::setupQueue()
{
    if (!fTransport.initQueue(&fCmdQueue, kCmdQueueIndex, kCmdQueueMaxSize))
        return false;
    if (fCmdQueue.queueSize < 4)
        return false;
    fTransport.enableFreeList(&fCmdQueue);
    fInFlight = (VirtioMetalSlot **)IOMallocZero(sizeof(VirtioMetalSlot *) * fCmdQueue.queueSize);
    return fInFlight != NULL;
}

bool
VirtioMetalDevice::setupSlots()
{
    unsigned count = fCmdQueue.queueSize < kMaxSlots ? fCmdQueue.queueSize : kMaxSlots;
    fSlotMem = IOBufferMemoryDescriptor::inTaskWithOptions(
        kernel_task, kIODirectionInOut, (vm_size_t)count * kSlotBytes, PAGE_SIZE);
    if (!fSlotMem)
        return false;
    if (fSlotMem->prepare() != kIOReturnSuccess) {
        OSSafeReleaseNULL(fSlotMem);
        return false;
    }

    fSlots = (VirtioMetalSlot *)IOMallocZero(sizeof(VirtioMetalSlot) * count);
    if (!fSlots)
        return false;
    fSlotCount = count;

    uint8_t *base = (uint8_t *)fSlotMem->getBytesNoCopy();
    bzero(base, (size_t)count * kSlotBytes);

    for (unsigned i = 0; i < count; i++) {
        VirtioMetalSlot *slot = &fSlots[i];
        IOByteCount segLen = 0;
        uint64_t phys = fSlotMem->getPhysicalSegment((IOByteCount)i * kSlotBytes, &segLen, kIOMemoryMapperNone);
        if (!phys || segLen < kSlotBytes)
            return false;
        uint8_t *va = base + (size_t)i * kSlotBytes;
        slot->req       = (struct kmtl_req_hdr *)(va + kReqOff);
        slot->resp      = (struct kmtl_resp_hdr *)(va + kRespOff);
        slot->table     = (VRingDesc *)(va + kTableOff);
        slot->reqPhys   = phys + kReqOff;
        slot->respPhys  = phys + kRespOff;
        slot->tablePhys = phys + kTableOff;
        slot->next      = fFreeSlots;
        fFreeSlots      = slot;
    }
    return true;
}

bool
VirtioMetalDevice::setupInterrupt()
{
    int type = 0;
    if (fPCIDevice->getInterruptType(0, &type) != kIOReturnSuccess)
        return false;

    fWorkLoop = IOWorkLoop::workLoop();
    if (!fWorkLoop)
        return false;
    fInterruptSource = IOInterruptEventSource::interruptEventSource(
        this,
        OSMemberFunctionCast(IOInterruptEventAction, this, &VirtioMetalDevice::interruptOccurred),
        fPCIDevice, 0);
    if (!fInterruptSource || fWorkLoop->addEventSource(fInterruptSource) != kIOReturnSuccess) {
        OSSafeReleaseNULL(fInterruptSource);
        OSSafeReleaseNULL(fWorkLoop);
        return false;
    }
    fUseInterrupts = true;
    return true;
}

void
VirtioMetalDevice::interruptOccurred(IOInterruptEventSource *source, int)
{
    if (__atomic_load_n(&fDead, __ATOMIC_RELAXED)) {
        source->disable();
        return;
    }
    if (!(fTransport.readIsr() & 1))
        return;
    IOLockLock(fLock);
    drainLocked();
    IOLockUnlock(fLock);
}

void
VirtioMetalDevice::drainLocked()
{
    if (fDead)
        return;

    VirtQueue *vq = &fCmdQueue;
    uint32_t usedLen = 0;
    uint16_t head = 0;
    bool freed = false;

    uint16_t pending = (uint16_t)(((volatile VRingUsedHdr *)vq->used)->idx - vq->lastUsedIdx);
    if (pending > vq->queueSize - vq->numFree) {
        failLocked(true);
        return;
    }
    while (pending-- && fTransport.pollForCompletion(vq, 0, &usedLen, &head)) {
        VirtioMetalSlot *slot = head < vq->queueSize ? fInFlight[head] : NULL;
        if (!slot) {
            failLocked(true);
            return;
        }
        fInFlight[head] = NULL;
        fTransport.freeDescChain(vq, head);
        freed = true;
        slot->usedLen = usedLen;
        __atomic_store_n(&slot->replied, true, __ATOMIC_RELEASE);
        IOLockWakeup(fLock, slot, true);
    }
    if (freed)
        IOLockWakeup(fLock, vq, false);
}

void
VirtioMetalDevice::failLocked(bool broken)
{
    if (fDead)
        return;
    if (broken)
        IOLog("VirtioMetal: device error or timeout, failing device\n");
    __atomic_store_n(&fDead, true, __ATOMIC_RELEASE);
    if (fInterruptSource)
        fInterruptSource->disable();
    fQuiesced = fTransport.quiesce();
    for (unsigned i = 0; i < fSlotCount; i++)
        IOLockWakeup(fLock, &fSlots[i], false);
    IOLockWakeup(fLock, &fFreeSlots, false);
    IOLockWakeup(fLock, &fCmdQueue, false);
}

bool
VirtioMetalDevice::tryReserveWiredBytes(uint64_t bytes)
{
    if (bytes == 0)
        return true;
    if (bytes > kMaxWiredBytes)
        return false;
    uint64_t cur = __atomic_load_n(&fWiredBytes, __ATOMIC_RELAXED);
    for (;;) {
        if (bytes > kMaxWiredBytes - cur)
            return false;
        if (__atomic_compare_exchange_n(&fWiredBytes, &cur, cur + bytes, false,
                                        __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
            return true;
    }
}

void
VirtioMetalDevice::releaseWiredBytes(uint64_t bytes)
{
    if (bytes == 0)
        return;
    __atomic_sub_fetch(&fWiredBytes, bytes, __ATOMIC_RELEASE);
}

bool
VirtioMetalDevice::claimOwnerUid(uid_t uid)
{
    if (!fLock)
        return false;
    IOLockLock(fLock);
    bool ok = false;
    if (!fUidBound) {
        fBoundUid = uid;
        fUidBound = true;
        ok = true;
    } else if (fBoundUid == uid) {
        ok = true;
    }
    IOLockUnlock(fLock);
    return ok;
}

VirtioMetalSlot *
VirtioMetalDevice::takeSlotLocked()
{
    while (!fFreeSlots && !fDead) {
        fSlotWaiters++;
        AbsoluteTime slice;
        clock_interval_to_deadline(kWaitSliceMs, kMillisecondScale, &slice);
        if (IOLockSleepDeadline(fLock, &fFreeSlots, slice, THREAD_UNINT) == THREAD_TIMED_OUT)
            drainLocked();
        fSlotWaiters--;
    }
    if (fDead || !fFreeSlots)
        return NULL;
    VirtioMetalSlot *slot = fFreeSlots;
    fFreeSlots = slot->next;
    slot->next = NULL;
    slot->usedLen = 0;
    __atomic_store_n(&slot->replied, false, __ATOMIC_RELAXED);
    return slot;
}

void
VirtioMetalDevice::putSlotLocked(VirtioMetalSlot *slot)
{
    slot->next = fFreeSlots;
    fFreeSlots = slot;
    if (fSlotWaiters)
        IOLockWakeup(fLock, &fFreeSlots, true);
}

uint32_t
VirtioMetalDevice::allocClientId()
{
    for (;;) {
        uint32_t id = __atomic_add_fetch(&fNextClientId, 1, __ATOMIC_RELAXED);
        if (id != 0)
            return id;
    }
}

static bool
countSegments(IOMemoryDescriptor *md, uint32_t length, unsigned *outCount)
{
    unsigned count = 0;
    for (IOByteCount off = 0; off < length;) {
        IOByteCount segLen = 0;
        addr64_t phys = md->getPhysicalSegment(off, &segLen, kIOMemoryMapperNone);
        if (!phys || segLen == 0)
            return false;
        if (segLen > length - off)
            segLen = length - off;
        if (segLen > kMaxSegBytes)
            segLen = kMaxSegBytes;
        count++;
        off += segLen;
    }
    *outCount = count;
    return true;
}

static bool
fillDirectSegments(VirtIOChainEntry *chain, unsigned *idx, unsigned maxCount,
                   IOMemoryDescriptor *md, uint32_t length, bool write)
{
    for (IOByteCount off = 0; off < length;) {
        IOByteCount segLen = 0;
        addr64_t phys = md->getPhysicalSegment(off, &segLen, kIOMemoryMapperNone);
        if (!phys || segLen == 0 || *idx >= maxCount)
            return false;
        if (segLen > length - off)
            segLen = length - off;
        if (segLen > kMaxSegBytes)
            segLen = kMaxSegBytes;
        chain[(*idx)++] = { phys, (uint32_t)segLen, write, false };
        off += segLen;
    }
    return true;
}

static bool
fillIndirectSegments(VRingDesc *table, unsigned *idx, unsigned totalCount,
                     IOMemoryDescriptor *md, uint32_t length, bool write)
{
    for (IOByteCount off = 0; off < length;) {
        IOByteCount segLen = 0;
        addr64_t phys = md->getPhysicalSegment(off, &segLen, kIOMemoryMapperNone);
        if (!phys || segLen == 0 || *idx >= totalCount)
            return false;
        if (segLen > length - off)
            segLen = length - off;
        if (segLen > kMaxSegBytes)
            segLen = kMaxSegBytes;
        unsigned i = (*idx)++;
        table[i].addr  = phys;
        table[i].len   = (uint32_t)segLen;
        table[i].flags = (uint16_t)((write ? VRING_DESC_F_WRITE : 0) |
                                    (i + 1 < totalCount ? VRING_DESC_F_NEXT : 0));
        table[i].next  = (uint16_t)(i + 1);
        off += segLen;
    }
    return true;
}

IOReturn
VirtioMetalDevice::submitRPC(const struct kmtl_req_hdr *req,
                             IOMemoryDescriptor *inDesc, uint32_t inLen,
                             struct kmtl_resp_hdr *resp,
                             IOMemoryDescriptor *outDesc, uint32_t outCap,
                             bool *outDmaActive)
{
    if (outDmaActive)
        *outDmaActive = false;
    if (!req || !resp)
        return kIOReturnBadArgument;
    if ((inLen > 0 && !inDesc) || (outCap > 0 && !outDesc))
        return kIOReturnBadArgument;
    if (__atomic_load_n(&fDead, __ATOMIC_ACQUIRE))
        return kIOReturnNotReady;

    unsigned inSegs = 0, outSegs = 0;
    if (inLen > 0 && !countSegments(inDesc, inLen, &inSegs))
        return kIOReturnVMError;
    if (outCap > 0 && !countSegments(outDesc, outCap, &outSegs))
        return kIOReturnVMError;

    unsigned totalDescs = 2 + inSegs + outSegs;
    bool useIndirect = fIndirect && totalDescs > 1;
    if (!useIndirect && totalDescs > fCmdQueue.queueSize)
        return kIOReturnMessageTooLarge;
    if (useIndirect && totalDescs > 0xFFFFu)
        return kIOReturnMessageTooLarge;

    IOBufferMemoryDescriptor *dynTableMem = NULL;
    VRingDesc *dynTable = NULL;
    uint64_t dynTablePhys = 0;
    if (useIndirect && totalDescs > kInlineTableDescs) {
        size_t tableBytes = (size_t)totalDescs * sizeof(VRingDesc);
        dynTableMem = IOBufferMemoryDescriptor::inTaskWithOptions(
            kernel_task, kIODirectionOut | kIOMemoryPhysicallyContiguous, tableBytes, PAGE_SIZE);
        if (!dynTableMem)
            return kIOReturnNoMemory;
        if (dynTableMem->prepare(kIODirectionOut) != kIOReturnSuccess) {
            dynTableMem->release();
            return kIOReturnNoMemory;
        }
        IOByteCount segLen = 0;
        dynTablePhys = dynTableMem->getPhysicalSegment(0, &segLen, kIOMemoryMapperNone);
        if (!dynTablePhys || segLen < tableBytes) {
            dynTableMem->complete(kIODirectionOut);
            dynTableMem->release();
            return kIOReturnNoMemory;
        }
        dynTable = (VRingDesc *)dynTableMem->getBytesNoCopy();
    }

    VirtIOChainEntry directChain[kCmdQueueMaxSize];

    IOLockLock(fLock);
    VirtioMetalSlot *slot = takeSlotLocked();
    if (!slot) {
        IOLockUnlock(fLock);
        if (dynTableMem) {
            dynTableMem->complete(kIODirectionOut);
            dynTableMem->release();
        }
        return kIOReturnNotReady;
    }

    *slot->req = *req;
    bzero(slot->resp, sizeof(*slot->resp));

    bool built = true;
    VirtIOChainEntry indirectEntry = {};
    if (useIndirect) {
        VRingDesc *tbl = dynTable ? dynTable : slot->table;
        uint64_t tblPhys = dynTable ? dynTablePhys : slot->tablePhys;
        unsigned idx = 0;

        tbl[idx].addr  = slot->reqPhys;
        tbl[idx].len   = sizeof(struct kmtl_req_hdr);
        tbl[idx].flags = VRING_DESC_F_NEXT;
        tbl[idx].next  = (uint16_t)(idx + 1);
        idx++;

        if (inLen > 0)
            built = fillIndirectSegments(tbl, &idx, totalDescs, inDesc, inLen, false);

        if (built && idx < totalDescs) {
            tbl[idx].addr  = slot->respPhys;
            tbl[idx].len   = sizeof(struct kmtl_resp_hdr);
            tbl[idx].flags = (uint16_t)(VRING_DESC_F_WRITE | (idx + 1 < totalDescs ? VRING_DESC_F_NEXT : 0));
            tbl[idx].next  = (uint16_t)(idx + 1);
            idx++;
        } else {
            built = false;
        }

        if (built && outCap > 0)
            built = fillIndirectSegments(tbl, &idx, totalDescs, outDesc, outCap, true);

        if (built && idx != totalDescs)
            built = false;

        indirectEntry = { tblPhys, (uint32_t)(totalDescs * sizeof(VRingDesc)), false, true };
    } else {
        unsigned idx = 0;
        directChain[idx++] = { slot->reqPhys, sizeof(struct kmtl_req_hdr), false, false };
        if (inLen > 0)
            built = fillDirectSegments(directChain, &idx, kCmdQueueMaxSize, inDesc, inLen, false);
        if (built && idx < kCmdQueueMaxSize)
            directChain[idx++] = { slot->respPhys, sizeof(struct kmtl_resp_hdr), true, false };
        else
            built = false;
        if (built && outCap > 0)
            built = fillDirectSegments(directChain, &idx, kCmdQueueMaxSize, outDesc, outCap, true);
        if (built && idx != totalDescs)
            built = false;
    }

    if (!built) {
        putSlotLocked(slot);
        IOLockUnlock(fLock);
        if (dynTableMem) {
            dynTableMem->complete(kIODirectionOut);
            dynTableMem->release();
        }
        return kIOReturnVMError;
    }

    int head = -1;
    while (!fDead) {
        head = useIndirect
            ? fTransport.addDescChainFree(&fCmdQueue, &indirectEntry, 1)
            : fTransport.addDescChainFree(&fCmdQueue, directChain, totalDescs);
        if (head >= 0) {
            fInFlight[head] = slot;
            fTransport.notify(&fCmdQueue);
            break;
        }
        AbsoluteTime slice;
        clock_interval_to_deadline(kWaitSliceMs, kMillisecondScale, &slice);
        if (IOLockSleepDeadline(fLock, &fCmdQueue, slice, THREAD_UNINT) == THREAD_TIMED_OUT)
            drainLocked();
    }

    if (head < 0) {
        putSlotLocked(slot);
        IOLockUnlock(fLock);
        if (dynTableMem) {
            dynTableMem->complete(kIODirectionOut);
            dynTableMem->release();
        }
        return kIOReturnNotReady;
    }

    /* Short busy-spin before blocking for low-latency RPCs. */
    uint64_t spinLimit = 0;
    nanoseconds_to_absolutetime((uint64_t)kSpinUs * 1000ULL, &spinLimit);
    spinLimit += mach_absolute_time();
    volatile VRingUsedHdr *usedHdr = (volatile VRingUsedHdr *)fCmdQueue.used;
    IOLockUnlock(fLock);
    while (!__atomic_load_n(&slot->replied, __ATOMIC_ACQUIRE) &&
           !__atomic_load_n(&fDead, __ATOMIC_RELAXED) &&
           mach_absolute_time() < spinLimit) {
        if (usedHdr->idx != __atomic_load_n(&fCmdQueue.lastUsedIdx, __ATOMIC_RELAXED))
            break;
        __builtin_arm_yield();
    }
    IOLockLock(fLock);
    drainLocked();

    /*
     * Long-running GPU waits/compiles are bounded by the host GPU watchdog and client teardown,
     * so they must not brick the whole VirtIO device on a short client-side timeout.
     */
    bool allowDeviceTimeout = (req->opcode != KMTL_OP_WAIT_QUEUE &&
                               req->opcode != KMTL_OP_COMMIT_COMMAND_BUFFER &&
                               req->opcode != KMTL_OP_CREATE_LIBRARY_SOURCE &&
                               req->opcode != KMTL_OP_CREATE_COMPUTE_PIPELINE &&
                               req->opcode != KMTL_OP_CREATE_FUNCTION);

    AbsoluteTime deadline = 0;
    if (allowDeviceTimeout)
        clock_interval_to_deadline(kRpcTimeoutMs, kMillisecondScale, &deadline);

    IOReturn ret = kIOReturnIOError;
    while (!__atomic_load_n(&slot->replied, __ATOMIC_ACQUIRE) && !fDead) {
        if (allowDeviceTimeout) {
            AbsoluteTime now;
            clock_get_uptime(&now);
            if (now >= deadline) {
                IOLog("VirtioMetal: opcode %u client %u timed out after %u ms\n",
                      req->opcode, req->client_id, kRpcTimeoutMs);
                failLocked(true);
                ret = kIOReturnTimeout;
                break;
            }
        }
        AbsoluteTime slice;
        clock_interval_to_deadline(kWaitSliceMs, kMillisecondScale, &slice);
        if (allowDeviceTimeout && slice > deadline)
            slice = deadline;
        uint16_t seen = fCmdQueue.lastUsedIdx;
        if (IOLockSleepDeadline(fLock, slot, slice, THREAD_UNINT) == THREAD_TIMED_OUT) {
            drainLocked();
            if (fUseInterrupts && seen != fCmdQueue.lastUsedIdx && !fLostInterrupt) {
                fLostInterrupt = true;
                IOLog("VirtioMetal: completion arrived without interrupt, draining on timeout\n");
            }
        }
    }

    bool dmaActive = false;
    if (__atomic_load_n(&slot->replied, __ATOMIC_ACQUIRE)) {
        if (slot->usedLen < sizeof(struct kmtl_resp_hdr)) {
            IOLog("VirtioMetal: short reply (%u bytes) for opcode %u\n", slot->usedLen, req->opcode);
            ret = kIOReturnIOError;
        } else {
            *resp = *slot->resp;
            ret = kIOReturnSuccess;
        }
        putSlotLocked(slot);
    } else if (fQuiesced) {
        putSlotLocked(slot);
    } else {
        /* Host did not ack reset; keep DMA buffers pinned to prevent DMA UAF. */
        dmaActive = true;
    }

    IOLockUnlock(fLock);

    if (outDmaActive)
        *outDmaActive = dmaActive;

    if (dynTableMem && !dmaActive) {
        dynTableMem->complete(kIODirectionOut);
        dynTableMem->release();
    }
    return ret;
}

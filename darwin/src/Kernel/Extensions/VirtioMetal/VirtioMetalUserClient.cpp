/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include "VirtioMetalUserClient.h"
#include <IOKit/IOLib.h>
#include <kern/task.h>
#include <sys/kauth.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(VirtioMetalUserClient, IOUserClient)

enum {
    kInitialBufferCap = 64,
    kMaxFunctionReqBytes = 1u << 20,
    kMaxErrorOutBytes = 65536u,
};

static bool
validateOpcodePayloadBounds(uint32_t opcode, uint32_t inLen, uint32_t outCap)
{
    switch (opcode) {
    case KMTL_OP_GET_DEVICE_INFO:
        return inLen == 0 && outCap <= 4096u;
    case KMTL_OP_CREATE_BUFFER:
    case KMTL_OP_DESTROY_BUFFER:
    case KMTL_OP_DESTROY_LIBRARY:
    case KMTL_OP_DESTROY_FUNCTION:
    case KMTL_OP_DESTROY_PIPELINE:
    case KMTL_OP_CREATE_QUEUE:
    case KMTL_OP_DESTROY_QUEUE:
        return inLen == 0 && outCap == 0;
    case KMTL_OP_CREATE_LIBRARY_SOURCE:
    case KMTL_OP_CREATE_LIBRARY_DATA:
        return inLen <= KMTL_MAX_IN_PAYLOAD && outCap <= KMTL_MAX_OUT_PAYLOAD;
    case KMTL_OP_CREATE_FUNCTION:
        return inLen <= kMaxFunctionReqBytes && outCap <= kMaxErrorOutBytes;
    case KMTL_OP_CREATE_COMPUTE_PIPELINE:
        return inLen == 0 && outCap <= kMaxErrorOutBytes;
    case KMTL_OP_COMMIT_COMMAND_BUFFER:
        return inLen <= KMTL_MAX_CMD_PAYLOAD && outCap <= kMaxErrorOutBytes;
    case KMTL_OP_WAIT_QUEUE:
        return inLen == 0 && outCap <= kMaxErrorOutBytes;
    default:
        return false;
    }
}

bool
VirtioMetalUserClient::initWithTask(task_t owningTask, void *securityToken, UInt32 type)
{
    if (!super::initWithTask(owningTask, securityToken, type))
        return false;

    fTask = owningTask;
    fBufLifecycleLock = IOLockAlloc();
    fLock = IOLockAlloc();
    if (!fBufLifecycleLock || !fLock) {
        if (fLock) {
            IOLockFree(fLock);
            fLock = NULL;
        }
        if (fBufLifecycleLock) {
            IOLockFree(fBufLifecycleLock);
            fBufLifecycleLock = NULL;
        }
        return false;
    }
    return true;
}

bool
VirtioMetalUserClient::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    VirtioMetalDevice *dev = OSDynamicCast(VirtioMetalDevice, provider);
    if (!dev)
        return false;
    if (!dev->claimOwnerUid(kauth_getuid())) {
        IOLog("VirtioMetalUserClient: rejecting cross-UID GPU open (uid=%u)\n", (unsigned)kauth_getuid());
        return false;
    }
    dev->retain();

    uint32_t clientId = dev->allocClientId();

    struct kmtl_req_hdr req = {};
    struct kmtl_resp_hdr resp = {};
    req.magic     = KMTL_MAGIC;
    req.version   = KMTL_PROTO_VERSION;
    req.opcode    = KMTL_OP_CLIENT_OPEN;
    req.client_id = clientId;

    IOReturn ret = dev->submitRPC(&req, NULL, 0, &resp, NULL, 0);
    if (ret != kIOReturnSuccess || resp.status != KMTL_OK) {
        IOLog("VirtioMetalUserClient: CLIENT_OPEN failed (io=0x%x status=%d)\n", ret, resp.status);
        dev->release();
        return false;
    }

    IOLockLock(fLock);
    fDevice = dev;
    fClientId = clientId;
    fClientOpened = true;
    IOLockUnlock(fLock);
    return true;
}

void
VirtioMetalUserClient::closeClient()
{
    if (!fLock || !fBufLifecycleLock)
        return;

    VirtioMetalBufferEntry *oldBufs = NULL;
    uint32_t oldCount = 0;
    uint32_t oldCap = 0;
    bool needCloseRpc = false;
    uint32_t clientId = 0;
    VirtioMetalDevice *dev = NULL;

    /* Hold fBufLifecycleLock across teardown so concurrent MAP/UNMAP/CREATE/DESTROY cannot race CLIENT_CLOSE. */
    IOLockLock(fBufLifecycleLock);
    IOLockLock(fLock);
    if (!fClosed) {
        fClosed = true;
        while (fActiveRpcCount > 0) {
            IOLockSleep(fLock, &fActiveRpcCount, THREAD_UNINT);
        }
        oldBufs = fBuffers;
        oldCount = fBufferCount;
        oldCap = fBufferCap;
        fBuffers = NULL;
        fBufferCount = 0;
        fBufferCap = 0;
        needCloseRpc = fClientOpened;
        fClientOpened = false;
        clientId = fClientId;
        dev = fDevice;
        if (dev)
            dev->retain();
    }
    IOLockUnlock(fLock);

    if (oldBufs) {
        for (uint32_t i = 0; i < oldCount; i++) {
            if (oldBufs[i].memMap) {
                oldBufs[i].memMap->unmap();
                oldBufs[i].memMap->release();
            }
            if (oldBufs[i].memDesc)
                oldBufs[i].memDesc->release();
        }
        IOFree(oldBufs, sizeof(VirtioMetalBufferEntry) * oldCap);
    }

    if (needCloseRpc && dev) {
        struct kmtl_req_hdr req = {};
        struct kmtl_resp_hdr resp = {};
        req.magic     = KMTL_MAGIC;
        req.version   = KMTL_PROTO_VERSION;
        req.opcode    = KMTL_OP_CLIENT_CLOSE;
        req.client_id = clientId;
        dev->submitRPC(&req, NULL, 0, &resp, NULL, 0);
    }
    IOLockUnlock(fBufLifecycleLock);

    if (dev)
        dev->release();
}

IOReturn
VirtioMetalUserClient::clientClose()
{
    closeClient();
    if (!isInactive())
        terminate();
    return kIOReturnSuccess;
}

IOReturn
VirtioMetalUserClient::clientDied()
{
    return super::clientDied();
}

void
VirtioMetalUserClient::stop(IOService *provider)
{
    closeClient();
    VirtioMetalDevice *dev = NULL;
    if (fLock) {
        IOLockLock(fLock);
        dev = fDevice;
        fDevice = NULL;
        IOLockUnlock(fLock);
    } else {
        dev = fDevice;
        fDevice = NULL;
    }
    OSSafeReleaseNULL(dev);
    super::stop(provider);
}

void
VirtioMetalUserClient::free()
{
    closeClient();
    VirtioMetalDevice *dev = fDevice;
    fDevice = NULL;
    OSSafeReleaseNULL(dev);
    if (fLock) {
        IOLockFree(fLock);
        fLock = NULL;
    }
    if (fBufLifecycleLock) {
        IOLockFree(fBufLifecycleLock);
        fBufLifecycleLock = NULL;
    }
    super::free();
}

VirtioMetalBufferEntry *
VirtioMetalUserClient::findBufferLocked(uint32_t handle)
{
    for (uint32_t i = 0; i < fBufferCount; i++) {
        if (fBuffers[i].handle == handle)
            return &fBuffers[i];
    }
    return NULL;
}

bool
VirtioMetalUserClient::recordBufferLocked(uint32_t handle, uint64_t shmOffset, uint64_t alignedLen)
{
    if (fClosed || handle == 0 || fBufferCount >= KMTL_MAX_BUFFERS_PER_CLIENT)
        return false;
    if (findBufferLocked(handle) != NULL)
        return false;

    if (fBufferCount == fBufferCap) {
        uint32_t newCap = fBufferCap ? (fBufferCap * 2) : kInitialBufferCap;
        if (newCap > KMTL_MAX_BUFFERS_PER_CLIENT)
            newCap = KMTL_MAX_BUFFERS_PER_CLIENT;
        VirtioMetalBufferEntry *nb = (VirtioMetalBufferEntry *)IOMallocZero(sizeof(VirtioMetalBufferEntry) * newCap);
        if (!nb)
            return false;
        if (fBuffers) {
            memcpy(nb, fBuffers, sizeof(VirtioMetalBufferEntry) * fBufferCount);
            IOFree(fBuffers, sizeof(VirtioMetalBufferEntry) * fBufferCap);
        }
        fBuffers = nb;
        fBufferCap = newCap;
    }

    VirtioMetalBufferEntry *e = &fBuffers[fBufferCount++];
    e->handle     = handle;
    e->shmOffset  = shmOffset;
    e->alignedLen = alignedLen;
    e->memDesc    = NULL;
    e->memMap     = NULL;
    return true;
}

bool
VirtioMetalUserClient::removeBufferLocked(uint32_t handle,
                                          IOMemoryDescriptor **outDesc,
                                          IOMemoryMap **outMap)
{
    if (outDesc)
        *outDesc = NULL;
    if (outMap)
        *outMap = NULL;
    for (uint32_t i = 0; i < fBufferCount; i++) {
        if (fBuffers[i].handle == handle) {
            if (outDesc)
                *outDesc = fBuffers[i].memDesc;
            if (outMap)
                *outMap = fBuffers[i].memMap;
            fBuffers[i] = fBuffers[fBufferCount - 1];
            bzero(&fBuffers[fBufferCount - 1], sizeof(VirtioMetalBufferEntry));
            fBufferCount--;
            return true;
        }
    }
    return false;
}

IOReturn
VirtioMetalUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                      IOExternalMethodDispatch *dispatch, OSObject *target,
                                      void *reference)
{
    (void)dispatch;
    (void)target;
    (void)reference;

    switch (selector) {
    case KMTL_UC_SELECTOR_RPC:
        return handleRPC(args);
    case KMTL_UC_SELECTOR_MAP_BUFFER:
        return handleMapBuffer(args);
    case KMTL_UC_SELECTOR_UNMAP_BUFFER:
        return handleUnmapBuffer(args);
    default:
        return kIOReturnUnsupported;
    }
}

IOReturn
VirtioMetalUserClient::handleMapBuffer(IOExternalMethodArguments *args)
{
    if (!fLock || !fBufLifecycleLock || current_task() != fTask)
        return kIOReturnNotPermitted;
    if (!args || !args->scalarInput || args->scalarInputCount < 1 ||
        !args->scalarOutput || args->scalarOutputCount < 2)
        return kIOReturnBadArgument;

    uint64_t handle64 = args->scalarInput[0];
    if (handle64 == 0 || handle64 > 0xFFFFFFFFULL)
        return kIOReturnBadArgument;
    uint32_t handle = (uint32_t)handle64;

    IOLockLock(fBufLifecycleLock);
    IOLockLock(fLock);
    if (fClosed || !fDevice) {
        IOLockUnlock(fLock);
        IOLockUnlock(fBufLifecycleLock);
        return kIOReturnNotOpen;
    }
    VirtioMetalDevice *dev = fDevice;
    dev->retain();

    VirtioMetalBufferEntry *entry = findBufferLocked(handle);
    if (!entry) {
        IOLockUnlock(fLock);
        IOLockUnlock(fBufLifecycleLock);
        dev->release();
        return kIOReturnNotFound;
    }

    if (entry->memMap) {
        args->scalarOutput[0] = (uint64_t)entry->memMap->getAddress();
        args->scalarOutput[1] = (uint64_t)entry->memMap->getSize();
        args->scalarOutputCount = 2;
        IOLockUnlock(fLock);
        IOLockUnlock(fBufLifecycleLock);
        dev->release();
        return kIOReturnSuccess;
    }

    if (!entry->memDesc) {
        entry->memDesc = IOMemoryDescriptor::withPhysicalAddress(
            (IOPhysicalAddress)(dev->shmPhys() + entry->shmOffset),
            (IOByteCount)entry->alignedLen,
            kIODirectionInOut);
        if (!entry->memDesc) {
            IOLockUnlock(fLock);
            IOLockUnlock(fBufLifecycleLock);
            dev->release();
            return kIOReturnNoMemory;
        }
    }

    IOMemoryDescriptor *desc = entry->memDesc;
    desc->retain();
    IOLockUnlock(fLock);

    IOMemoryMap *map = desc->createMappingInTask(fTask, 0, kIOMapAnywhere | kIOMapCopybackCache);
    desc->release();
    if (!map) {
        IOLockUnlock(fBufLifecycleLock);
        dev->release();
        return kIOReturnVMError;
    }

    IOLockLock(fLock);
    VirtioMetalBufferEntry *recheck = fClosed ? NULL : findBufferLocked(handle);
    if (!recheck || recheck->memMap) {
        uint64_t addr = 0;
        uint64_t size = 0;
        bool ok = false;
        if (recheck && recheck->memMap) {
            addr = (uint64_t)recheck->memMap->getAddress();
            size = (uint64_t)recheck->memMap->getSize();
            ok = true;
        }
        IOLockUnlock(fLock);
        map->unmap();
        map->release();
        IOLockUnlock(fBufLifecycleLock);
        dev->release();
        if (ok) {
            args->scalarOutput[0] = addr;
            args->scalarOutput[1] = size;
            args->scalarOutputCount = 2;
            return kIOReturnSuccess;
        }
        return kIOReturnNotFound;
    }

    recheck->memMap = map;
    args->scalarOutput[0] = (uint64_t)map->getAddress();
    args->scalarOutput[1] = (uint64_t)map->getSize();
    args->scalarOutputCount = 2;
    IOLockUnlock(fLock);
    IOLockUnlock(fBufLifecycleLock);
    dev->release();
    return kIOReturnSuccess;
}

IOReturn
VirtioMetalUserClient::handleUnmapBuffer(IOExternalMethodArguments *args)
{
    if (!fLock || !fBufLifecycleLock || current_task() != fTask)
        return kIOReturnNotPermitted;
    if (!args || !args->scalarInput || args->scalarInputCount < 1)
        return kIOReturnBadArgument;

    uint64_t handle64 = args->scalarInput[0];
    if (handle64 == 0 || handle64 > 0xFFFFFFFFULL)
        return kIOReturnBadArgument;

    IOMemoryMap *oldMap = NULL;
    IOLockLock(fBufLifecycleLock);
    IOLockLock(fLock);
    if (!fClosed) {
        VirtioMetalBufferEntry *entry = findBufferLocked((uint32_t)handle64);
        if (entry && entry->memMap) {
            oldMap = entry->memMap;
            entry->memMap = NULL;
        }
    }
    IOLockUnlock(fLock);
    if (oldMap) {
        oldMap->unmap();
        oldMap->release();
    }
    IOLockUnlock(fBufLifecycleLock);
    return kIOReturnSuccess;
}

IOReturn
VirtioMetalUserClient::handleRPC(IOExternalMethodArguments *args)
{
    if (!fLock || !fBufLifecycleLock)
        return kIOReturnNotReady;
    if (current_task() != fTask)
        return kIOReturnNotPermitted;
    if (!args || !args->structureInput || args->structureInputSize != sizeof(struct kmtl_uc_call))
        return kIOReturnBadArgument;
    if (!args->structureOutput || args->structureOutputSize < sizeof(struct kmtl_resp_hdr))
        return kIOReturnBadArgument;

    struct kmtl_uc_call call;
    memcpy(&call, args->structureInput, sizeof(call));

    if (!validateOpcodePayloadBounds(call.req.opcode, call.in_len, call.out_cap))
        return kIOReturnBadArgument;
    if ((call.in_len > 0 && call.in_ptr == 0) || (call.out_cap > 0 && call.out_ptr == 0))
        return kIOReturnBadArgument;

    bool isBufLifecycle = (call.req.opcode == KMTL_OP_CREATE_BUFFER ||
                           call.req.opcode == KMTL_OP_DESTROY_BUFFER);
    if (isBufLifecycle)
        IOLockLock(fBufLifecycleLock);

    VirtioMetalDevice *dev = NULL;
    uint32_t clientId = 0;
    IOLockLock(fLock);
    if (!fClosed && fDevice) {
        dev = fDevice;
        dev->retain();
        clientId = fClientId;
        fActiveRpcCount++;
    }
    IOLockUnlock(fLock);

    if (!dev) {
        if (isBufLifecycle)
            IOLockUnlock(fBufLifecycleLock);
        return kIOReturnNotOpen;
    }

    call.req.magic     = KMTL_MAGIC;
    call.req.version   = KMTL_PROTO_VERSION;
    call.req.client_id = clientId;

    uint64_t wiredQuota = (uint64_t)call.in_len + (uint64_t)call.out_cap;
    bool reservedWired = false;
    IOMemoryDescriptor *inDesc = NULL;
    IOMemoryDescriptor *outDesc = NULL;
    bool inPrepared = false;
    bool outPrepared = false;
    bool dmaActive = false;
    IOReturn ret = kIOReturnSuccess;
    struct kmtl_resp_hdr resp = {};

    /* For DESTROY_BUFFER, verify handle ownership and tear down mapping before host RPC. */
    if (call.req.opcode == KMTL_OP_DESTROY_BUFFER) {
        if (call.req.handle == 0 || call.req.handle > 0xFFFFFFFFULL) {
            ret = kIOReturnBadArgument;
            goto cleanup;
        }
        IOMemoryDescriptor *removedDesc = NULL;
        IOMemoryMap *removedMap = NULL;
        IOLockLock(fLock);
        bool found = removeBufferLocked((uint32_t)call.req.handle, &removedDesc, &removedMap);
        IOLockUnlock(fLock);
        if (!found) {
            ret = kIOReturnNotFound;
            goto cleanup;
        }
        if (removedMap) {
            removedMap->unmap();
            removedMap->release();
        }
        if (removedDesc)
            removedDesc->release();
    }

    if (wiredQuota > 0) {
        if (!dev->tryReserveWiredBytes(wiredQuota)) {
            ret = kIOReturnNoMemory;
            goto cleanup;
        }
        reservedWired = true;
    }

    if (call.in_len > 0) {
        inDesc = IOMemoryDescriptor::withAddressRange(call.in_ptr, call.in_len, kIODirectionOut, current_task());
        if (!inDesc) {
            ret = kIOReturnVMError;
            goto cleanup;
        }
        if (inDesc->prepare(kIODirectionOut) != kIOReturnSuccess) {
            ret = kIOReturnVMError;
            goto cleanup;
        }
        inPrepared = true;
    }

    if (call.out_cap > 0) {
        outDesc = IOMemoryDescriptor::withAddressRange(call.out_ptr, call.out_cap, kIODirectionIn, current_task());
        if (!outDesc) {
            ret = kIOReturnVMError;
            goto cleanup;
        }
        if (outDesc->prepare(kIODirectionIn) != kIOReturnSuccess) {
            ret = kIOReturnVMError;
            goto cleanup;
        }
        outPrepared = true;
    }

    ret = dev->submitRPC(&call.req, inDesc, call.in_len, &resp, outDesc, call.out_cap, &dmaActive);
    if (ret != kIOReturnSuccess)
        goto cleanup;

    if (call.req.opcode == KMTL_OP_CREATE_BUFFER && resp.status == KMTL_OK) {
        uint64_t shmPhys = dev->shmPhys();
        uint64_t shmLen = dev->shmLength();
        uint64_t shmOffset = resp.arg0;
        uint64_t alignedLen = resp.arg1;
        bool valid = (resp.handle != 0 && resp.handle <= 0xFFFFFFFFULL &&
                      alignedLen > 0 &&
                      ((shmPhys + shmOffset) & PAGE_MASK) == 0 &&
                      (alignedLen & PAGE_MASK) == 0 &&
                      shmOffset <= shmLen &&
                      alignedLen <= shmLen - shmOffset);
        bool recorded = false;
        if (valid) {
            IOLockLock(fLock);
            recorded = recordBufferLocked((uint32_t)resp.handle, shmOffset, alignedLen);
            IOLockUnlock(fLock);
        }
        if (!recorded) {
            IOLog("VirtioMetalUserClient: rejecting buffer handle=%llu off=0x%llx len=0x%llx (shmLen=0x%llx)\n",
                  resp.handle, shmOffset, alignedLen, shmLen);
            struct kmtl_req_hdr rollback = {};
            struct kmtl_resp_hdr rbResp = {};
            rollback.magic     = KMTL_MAGIC;
            rollback.version   = KMTL_PROTO_VERSION;
            rollback.opcode    = KMTL_OP_DESTROY_BUFFER;
            rollback.client_id = clientId;
            rollback.handle    = resp.handle;
            dev->submitRPC(&rollback, NULL, 0, &rbResp, NULL, 0);
            ret = valid ? kIOReturnNoMemory : kIOReturnBadArgument;
            goto cleanup;
        }
    }

    memcpy(args->structureOutput, &resp, sizeof(resp));
    args->structureOutputSize = sizeof(resp);
    ret = kIOReturnSuccess;

cleanup:
    if (!dmaActive) {
        if (outPrepared)
            outDesc->complete(kIODirectionIn);
        if (outDesc)
            outDesc->release();
        if (inPrepared)
            inDesc->complete(kIODirectionOut);
        if (inDesc)
            inDesc->release();
        if (reservedWired)
            dev->releaseWiredBytes(wiredQuota);
    }
    if (isBufLifecycle)
        IOLockUnlock(fBufLifecycleLock);
    IOLockLock(fLock);
    if (fActiveRpcCount > 0)
        fActiveRpcCount--;
    if (fActiveRpcCount == 0 && fClosed)
        IOLockWakeup(fLock, &fActiveRpcCount, /*oneThread=*/false);
    IOLockUnlock(fLock);
    dev->release();
    return ret;
}

IOReturn
VirtioMetalUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
                                           IOMemoryDescriptor **memory)
{
    (void)type;
    (void)options;
    (void)memory;
    /* Mapping is restricted to KMTL_UC_SELECTOR_MAP_BUFFER so foreign tasks cannot bypass tracking. */
    return kIOReturnUnsupported;
}

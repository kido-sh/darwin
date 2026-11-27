/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <sys/types.h>
#include "IOVirtIOTransport.h"
#include "kmtl_proto.h"

struct VirtioMetalSlot;

class VirtioMetalDevice : public IOService {
    OSDeclareDefaultStructors(VirtioMetalDevice)

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

    uint32_t allocClientId();
    uint64_t shmPhys() const { return fShmPhys; }
    uint64_t shmLength() const { return fShmLength; }
    const struct kmtl_device_config &deviceConfig() const { return fConfig; }

    bool tryReserveWiredBytes(uint64_t bytes);
    void releaseWiredBytes(uint64_t bytes);
    bool claimOwnerUid(uid_t uid);

    IOReturn submitRPC(const struct kmtl_req_hdr *req,
                       IOMemoryDescriptor *inDesc, uint32_t inLen,
                       struct kmtl_resp_hdr *resp,
                       IOMemoryDescriptor *outDesc, uint32_t outCap,
                       bool *outDmaActive = NULL);

private:
    bool readDeviceConfig();
    bool setupQueue();
    bool setupSlots();
    bool setupInterrupt();
    void interruptOccurred(IOInterruptEventSource *source, int count);
    void drainLocked();
    void failLocked(bool broken);
    VirtioMetalSlot *takeSlotLocked();
    void putSlotLocked(VirtioMetalSlot *slot);

    IOPCIDevice *fPCIDevice;
    IOVirtIOTransport fTransport;
    VirtQueue fCmdQueue;
    VirtioMetalSlot **fInFlight;
    IOBufferMemoryDescriptor *fSlotMem;
    VirtioMetalSlot *fSlots;
    VirtioMetalSlot *fFreeSlots;
    unsigned fSlotCount;
    unsigned fSlotWaiters;

    IOLock *fLock;
    IOWorkLoop *fWorkLoop;
    IOInterruptEventSource *fInterruptSource;

    struct kmtl_device_config fConfig;
    uint64_t fShmPhys;
    uint64_t fShmLength;
    uint64_t fWiredBytes;
    uint32_t fNextClientId;
    uid_t fBoundUid;
    bool fUidBound;

    bool fIndirect;
    bool fUseInterrupts;
    bool fDead;
    bool fQuiesced;
    bool fLostInterrupt;
};

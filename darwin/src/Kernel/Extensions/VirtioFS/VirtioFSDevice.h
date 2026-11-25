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
#include <IOKit/pci/IOPCIDevice.h>
#include "IOVirtIOTransport.h"
#include "virtiofs_dev.h"

struct VirtioFSSlot;
struct VirtioFSRequest;

class VirtioFSDevice : public IOService {
    OSDeclareDefaultStructors(VirtioFSDevice)

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

    static VirtioFSDevice *copyDevice(const char *tag);
    int startSession(struct virtiofs_session *session);
    int call(struct virtiofs_call *call, uint32_t uid, uint32_t gid, uint32_t pid);
    void submit(struct virtiofs_call *call, uint32_t uid, uint32_t gid, uint32_t pid, virtiofs_done_t done,
                void *arg);
    void forget(uint64_t nodeid, uint64_t nlookup);
    void poll(virtiofs_poll_t done, void *arg, uint32_t datalen);

private:
    bool setupQueues();
    bool setupSlots();
    bool setupInterrupt();
    void interruptOccurred(IOInterruptEventSource *source, int count);
    void drainLocked(unsigned q);
    void failLocked(bool broken);
    void pauseLocked(void *event);
    void spinLocked(virtiofs_poll_t done, void *arg, uint32_t datalen);
    void unlockAndReap();
    VirtioFSSlot *takeSlotLocked();
    void putSlotLocked(VirtioFSSlot *slot);
    bool submitLocked(unsigned q, VirtioFSSlot *slot, const VirtIOChainEntry *chain, unsigned count);
    VirtioFSSlot *fillHeaderLocked(uint32_t opcode, uint64_t nodeid, uint32_t len);
    VirtioFSSlot *startLocked(struct virtiofs_call *call, VirtioFSRequest *request, uint32_t uid, uint32_t gid,
                              uint32_t pid, bool async, virtiofs_done_t done, void *arg);

    IOPCIDevice *fPCIDevice;
    IOVirtIOTransport fTransport;
    VirtQueue fQueues[2];
    VirtioFSSlot **fInFlight[2];
    IOBufferMemoryDescriptor *fSlotMem;
    VirtioFSSlot *fSlots;
    VirtioFSSlot *fFreeSlots;
    VirtioFSSlot *fReaped;
    unsigned fSlotCount;
    unsigned fSlotWaiters;
    IOLock *fLock;
    IOLock *fSessionLock;
    IOWorkLoop *fWorkLoop;
    IOInterruptEventSource *fInterruptSource;
    bool fIndirect;
    bool fDead;
    bool fQuiesced;
    bool fStarted;
    bool fLostInterrupt;
    bool fPolling;
    int fInitError;
    struct virtiofs_session fSession;
    uint64_t fUnique;
};

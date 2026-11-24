/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "IOVirtIOTransport.h"

class VirtioFSDevice : public IOService {
    OSDeclareDefaultStructors(VirtioFSDevice)

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

private:
    bool setupInterrupt();
    void interruptOccurred(IOInterruptEventSource *source, int count);
    void *request(uint32_t opcode, uint64_t nodeid, uint32_t argLen);
    int send(VirtQueue *vq, bool wantReply, uint32_t *replyLen);
    void *reply();
    void probeInit();
    void probeRoot();
    void probeErrors();
    void forget(uint64_t nodeid);

    IOPCIDevice *fPCIDevice;
    IOVirtIOTransport fTransport;
    VirtQueue fHiprio;
    VirtQueue fRequests;
    IOBufferMemoryDescriptor *fBuffer;
    uint8_t *fBytes;
    uint64_t fPhys;
    IOWorkLoop *fWorkLoop;
    IOInterruptEventSource *fInterruptSource;
    uint64_t fUnique;
    uint32_t fInitFlags;
};

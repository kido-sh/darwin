/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <IOKit/IOUserClient.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOMemoryDescriptor.h>
#include "VirtioMetalDevice.h"

#define KMTL_UC_SELECTOR_MAP_BUFFER   1u
#define KMTL_UC_SELECTOR_UNMAP_BUFFER 2u

struct VirtioMetalBufferEntry {
    uint32_t handle;
    uint64_t shmOffset;
    uint64_t alignedLen;
    IOMemoryDescriptor *memDesc;
    IOMemoryMap *memMap;
};

class VirtioMetalUserClient : public IOUserClient {
    OSDeclareDefaultStructors(VirtioMetalUserClient)

public:
    bool initWithTask(task_t owningTask, void *securityToken, UInt32 type) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

    IOReturn clientClose() override;
    IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                            IOExternalMethodDispatch *dispatch, OSObject *target,
                            void *reference) override;
    IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
                                 IOMemoryDescriptor **memory) override;

private:
    IOReturn handleRPC(IOExternalMethodArguments *args);
    IOReturn handleMapBuffer(IOExternalMethodArguments *args);
    IOReturn handleUnmapBuffer(IOExternalMethodArguments *args);
    bool recordBufferLocked(uint32_t handle, uint64_t shmOffset, uint64_t alignedLen);
    bool removeBufferLocked(uint32_t handle, IOMemoryDescriptor **outDesc, IOMemoryMap **outMap);
    VirtioMetalBufferEntry *findBufferLocked(uint32_t handle);
    void closeClient();

    task_t fTask;
    VirtioMetalDevice *fDevice;
    IOLock *fBufLifecycleLock;
    IOLock *fLock;
    uint32_t fClientId;
    bool fClientOpened;
    bool fClosed;

    VirtioMetalBufferEntry *fBuffers;
    uint32_t fBufferCount;
    uint32_t fBufferCap;
};

/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#include <IOKit/IOService.h>

extern "C" {
int virtiofs_vfs_register(void);
int virtiofs_vfs_unregister(void);
}

class VirtioFSFilesystem : public IOService {
    OSDeclareDefaultStructors(VirtioFSFilesystem)

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
};

#define super IOService
OSDefineMetaClassAndStructors(VirtioFSFilesystem, IOService)

bool
VirtioFSFilesystem::start(IOService *provider)
{
    if (!super::start(provider) || virtiofs_vfs_register() != 0)
        return false;
    registerService();
    return true;
}

void
VirtioFSFilesystem::stop(IOService *provider)
{
    virtiofs_vfs_unregister();
    super::stop(provider);
}

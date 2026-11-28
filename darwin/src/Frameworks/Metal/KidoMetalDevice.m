/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#import "KidoMetalInternal.h"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <malloc/malloc.h>
#include <stdlib.h>
#include <string.h>

extern kern_return_t mach_vm_remap(vm_map_t target_task,
                                   mach_vm_address_t *target_address,
                                   mach_vm_size_t size,
                                   mach_vm_offset_t mask,
                                   int flags,
                                   vm_map_t src_task,
                                   mach_vm_address_t src_address,
                                   boolean_t copy,
                                   vm_prot_t *cur_protection,
                                   vm_prot_t *max_protection,
                                   vm_inherit_t inheritance);

enum {
    kErrorBufCap  = 65536,
    kLibOutBufCap = 262144,
};

static NSArray *
KidoDecodeFunctionNames(const char *buf, uint32_t len)
{
    if (!buf || len == 0)
        return [NSArray array];
    NSMutableArray *arr = [NSMutableArray array];
    uint32_t i = 0;
    while (i < len) {
        uint32_t start = i;
        while (i < len && buf[i] != '\0')
            i++;
        if (i > start) {
            NSString *s = [[NSString alloc] initWithBytes:buf + start
                                                   length:(NSUInteger)(i - start)
                                                 encoding:NSUTF8StringEncoding];
            if (s) {
                [arr addObject:s];
                [s release];
            }
        }
        i++;
    }
    return arr;
}

static KidoMTLDevice *
KidoGetDefaultDeviceSingleton(void)
{
    static KidoMTLDevice *sDevice = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        io_service_t service = IOServiceGetMatchingService(
            kIOMainPortDefault, IOServiceMatching("VirtioMetalDevice"));
        if (!service) {
            service = IOServiceGetMatchingService(
                kIOMainPortDefault, IOServiceNameMatching("VirtioMetal"));
        }
        if (!service)
            return;

        uint64_t regID = 0;
        IORegistryEntryGetRegistryEntryID(service, &regID);

        io_connect_t conn = MACH_PORT_NULL;
        kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
        IOObjectRelease(service);
        if (kr != KERN_SUCCESS || conn == MACH_PORT_NULL)
            return;

        struct kmtl_uc_call call;
        memset(&call, 0, sizeof(call));
        struct kmtl_device_info info;
        memset(&info, 0, sizeof(info));
        struct kmtl_resp_hdr resp;
        memset(&resp, 0, sizeof(resp));

        call.req.magic   = KMTL_MAGIC;
        call.req.version = KMTL_PROTO_VERSION;
        call.req.opcode  = KMTL_OP_GET_DEVICE_INFO;
        call.out_ptr     = (uint64_t)(uintptr_t)&info;
        call.out_cap     = (uint32_t)sizeof(info);

        size_t respSize = sizeof(resp);
        kr = IOConnectCallStructMethod(conn, KMTL_UC_SELECTOR_RPC,
                                       &call, sizeof(call),
                                       &resp, &respSize);
        if (kr != KERN_SUCCESS || respSize < sizeof(resp) ||
            resp.status != KMTL_OK || resp.extra_len < sizeof(info)) {
            IOServiceClose(conn);
            return;
        }
        if (regID != 0)
            info.registry_id = regID;

        sDevice = [[KidoMTLDevice alloc] initWithConnect:conn info:&info];
        if (!sDevice)
            IOServiceClose(conn);
    });
    return sDevice;
}

id
MTLCreateSystemDefaultDevice(void)
{
    KidoMTLDevice *dev = KidoGetDefaultDeviceSingleton();
    return dev ? [dev retain] : nil;
}

NSArray *
MTLCopyAllDevices(void)
{
    KidoMTLDevice *dev = KidoGetDefaultDeviceSingleton();
    if (dev) {
        id objs[1] = { dev };
        return [[NSArray alloc] initWithObjects:objs count:1];
    }
    return [[NSArray alloc] init];
}

NSArray *
MTLCopyAllDevicesWithObserver(id *observer, MTLDeviceNotificationHandler handler)
{
    (void)handler;
    if (observer)
        *observer = [[[NSObject alloc] init] autorelease];
    return MTLCopyAllDevices();
}

void
MTLRemoveDeviceObserver(id observer)
{
    (void)observer;
}

/* --- KidoMTLBuffer --- */

@implementation KidoMTLBuffer

- (instancetype)initWithDevice:(KidoMTLDevice *)device
                        handle:(uint64_t)handle
                     shmOffset:(uint64_t)shmOffset
                    alignedLen:(uint64_t)alignedLen
                        length:(NSUInteger)length
                    mappedAddr:(mach_vm_address_t)mappedAddr
                    mappedSize:(mach_vm_size_t)mappedSize
                       options:(MTLResourceOptions)options
{
    self = [super init];
    if (self) {
        _device = [device retain];
        _handle = handle;
        _shmOffset = shmOffset;
        _alignedLen = alignedLen;
        _length = length;
        _mappedAddr = mappedAddr;
        _mappedSize = mappedSize;
        _options = options;
        [_device adjustAllocatedSize:(int64_t)alignedLen];
    }
    return self;
}

- (void)setNoCopyPointer:(void *)ptr length:(NSUInteger)len inPlace:(BOOL)inPlace writable:(BOOL)writable deallocator:(void (^)(void *, NSUInteger))deallocator
{
    _noCopyPtr = ptr;
    _noCopyLen = len;
    _noCopyInPlace = inPlace;
    _noCopyWritable = writable;
    if (_deallocator) {
        Block_release(_deallocator);
        _deallocator = NULL;
    }
    if (deallocator)
        _deallocator = Block_copy(deallocator);
}

- (BOOL)hasNoCopyPointer
{
    return !_noCopyInPlace && _noCopyWritable && _noCopyPtr != NULL && _noCopyLen > 0;
}

- (void)syncFromNoCopy
{
    if (_noCopyInPlace || !_noCopyWritable || !_noCopyPtr || !_mappedAddr || _noCopyLen == 0)
        return;
    NSUInteger n = _noCopyLen < _length ? _noCopyLen : _length;
    if (n > 0)
        memcpy((void *)(uintptr_t)_mappedAddr, _noCopyPtr, n);
}

- (void)syncToNoCopyOffset:(NSUInteger)offset length:(NSUInteger)len
{
    if (_noCopyInPlace || !_noCopyWritable || !_noCopyPtr || !_mappedAddr || len == 0 || offset >= _noCopyLen)
        return;
    NSUInteger n = len;
    if (n > _noCopyLen - offset)
        n = _noCopyLen - offset;
    if (offset + n > _length)
        n = (offset < _length) ? (_length - offset) : 0;
    if (n > 0)
        memcpy((uint8_t *)_noCopyPtr + offset, (const uint8_t *)(uintptr_t)_mappedAddr + offset, n);
}

- (void)dealloc
{
    if (_device && _handle != 0) {
        if (_noCopyInPlace && _noCopyPtr != NULL && _noCopyLen > 0) {
            /*
             * Atomically replace the in-place SHM mapping at _noCopyPtr with fresh anonymous RAM
             * before tearing down the host SHM backing, preserving full contents if caller owns pointer.
             */
            mach_vm_address_t restoreTmp = 0;
            if (mach_vm_allocate(mach_task_self(), &restoreTmp, (mach_vm_size_t)_noCopyLen, VM_FLAGS_ANYWHERE) == KERN_SUCCESS) {
                if (_deallocator == NULL)
                    memcpy((void *)(uintptr_t)restoreTmp, _noCopyPtr, _noCopyLen);
                mach_vm_address_t targetAddr = (mach_vm_address_t)(uintptr_t)_noCopyPtr;
                vm_prot_t curProt = VM_PROT_NONE;
                vm_prot_t maxProt = VM_PROT_NONE;
                kern_return_t rkr = mach_vm_remap(mach_task_self(), &targetAddr, (mach_vm_size_t)_noCopyLen, 0,
                                                  VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                                                  mach_task_self(), restoreTmp, FALSE,
                                                  &curProt, &maxProt, VM_INHERIT_DEFAULT);
                if (rkr != KERN_SUCCESS)
                    mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)(uintptr_t)_noCopyPtr, (mach_vm_size_t)_noCopyLen);
                mach_vm_deallocate(mach_task_self(), restoreTmp, (mach_vm_size_t)_noCopyLen);
            } else {
                mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)(uintptr_t)_noCopyPtr, (mach_vm_size_t)_noCopyLen);
            }
            _noCopyInPlace = NO;
        }
        if (_mappedAddr != 0) {
            [_device unmapBufferHandle:_handle];
            _mappedAddr = 0;
        }
        struct kmtl_req_hdr req;
        struct kmtl_resp_hdr resp;
        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));
        req.opcode = KMTL_OP_DESTROY_BUFFER;
        req.handle = _handle;
        [_device rpc:&req inBuf:NULL inLen:0 resp:&resp outBuf:NULL outCap:0];
        [_device adjustAllocatedSize:-(int64_t)_alignedLen];
        _handle = 0;
    }
    if (_deallocator) {
        _deallocator(_noCopyPtr, _noCopyLen);
        Block_release(_deallocator);
        _deallocator = NULL;
    }
    [_label release];
    [_device release];
    [super dealloc];
}

- (uint64_t)kidoHandle { return _handle; }
- (void *)contents { return _noCopyPtr ? _noCopyPtr : (void *)(uintptr_t)_mappedAddr; }
- (NSUInteger)length { return _length; }
- (NSUInteger)allocatedSize { return (NSUInteger)_alignedLen; }
- (MTLGPUAddress)gpuAddress { return (MTLGPUAddress)(0x100000000ULL + _shmOffset); }
- (id)device { return _device; }
- (id)heap { return nil; }
- (NSUInteger)heapOffset { return 0; }
- (id)remoteStorageBuffer { return nil; }
- (NSInteger)sparseBufferTier { return 0; }
- (MTLResourceOptions)resourceOptions { return _options; }

- (MTLStorageMode)storageMode
{
    return (MTLStorageMode)((_options & MTLResourceStorageModeMask) >> MTLResourceStorageModeShift);
}

- (MTLCPUCacheMode)cpuCacheMode
{
    return (MTLCPUCacheMode)((_options & MTLResourceCPUCacheModeMask) >> MTLResourceCPUCacheModeShift);
}

- (MTLHazardTrackingMode)hazardTrackingMode
{
    MTLHazardTrackingMode m = (MTLHazardTrackingMode)((_options & MTLResourceHazardTrackingModeMask) >> MTLResourceHazardTrackingModeShift);
    return (m == MTLHazardTrackingModeDefault) ? MTLHazardTrackingModeTracked : m;
}

- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label
{
    if (_label != label) {
        [_label release];
        _label = [label copy];
    }
}

- (void)didModifyRange:(NSRange)range { (void)range; }
- (MTLPurgeableState)setPurgeableState:(MTLPurgeableState)state { (void)state; return MTLPurgeableStateNonVolatile; }
- (void)makeAliasable {}
- (BOOL)isAliasable { return NO; }
- (void)addDebugMarker:(NSString *)marker range:(NSRange)range { (void)marker; (void)range; }
- (void)removeAllDebugMarkers {}

@end

/* --- KidoMTLFunction --- */

@implementation KidoMTLFunction

- (instancetype)initWithDevice:(KidoMTLDevice *)device handle:(uint64_t)handle name:(NSString *)name
{
    self = [super init];
    if (self) {
        _device = [device retain];
        _handle = handle;
        _name = [name copy];
        _functionType = MTLFunctionTypeKernel;
        _options = MTLFunctionOptionNone;
    }
    return self;
}

- (void)dealloc
{
    if (_device && _handle != 0) {
        struct kmtl_req_hdr req;
        struct kmtl_resp_hdr resp;
        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));
        req.opcode = KMTL_OP_DESTROY_FUNCTION;
        req.handle = _handle;
        [_device rpc:&req inBuf:NULL inLen:0 resp:&resp outBuf:NULL outCap:0];
        _handle = 0;
    }
    [_name release];
    [_label release];
    [_device release];
    [super dealloc];
}

- (uint64_t)kidoHandle { return _handle; }
- (id)device { return _device; }
- (NSString *)name { return _name; }
- (MTLFunctionType)functionType { return _functionType; }
- (MTLFunctionOptions)options { return _options; }
- (NSUInteger)patchType { return 0; }
- (NSInteger)patchControlPointCount { return -1; }
- (NSArray *)vertexAttributes { return nil; }
- (NSArray *)stageInputAttributes { return nil; }
- (NSDictionary *)functionConstantsDictionary { return [NSDictionary dictionary]; }

- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label
{
    if (_label != label) {
        [_label release];
        _label = [label copy];
    }
}

@end

/* --- KidoMTLLibrary --- */

@implementation KidoMTLLibrary

- (instancetype)initWithDevice:(KidoMTLDevice *)device handle:(uint64_t)handle functionNames:(NSArray *)functionNames
{
    self = [super init];
    if (self) {
        _device = [device retain];
        _handle = handle;
        _type = MTLLibraryTypeExecutable;
        _functionNames = functionNames ? [functionNames copy] : [[NSArray alloc] init];
    }
    return self;
}

- (void)dealloc
{
    if (_device && _handle != 0) {
        struct kmtl_req_hdr req;
        struct kmtl_resp_hdr resp;
        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));
        req.opcode = KMTL_OP_DESTROY_LIBRARY;
        req.handle = _handle;
        [_device rpc:&req inBuf:NULL inLen:0 resp:&resp outBuf:NULL outCap:0];
        _handle = 0;
    }
    [_functionNames release];
    [_label release];
    [_installName release];
    [_device release];
    [super dealloc];
}

- (uint64_t)kidoHandle { return _handle; }
- (id)device { return _device; }
- (MTLLibraryType)type { return _type; }
- (NSString *)installName { return _installName; }
- (NSArray *)functionNames { return _functionNames ?: [NSArray array]; }

- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label
{
    if (_label != label) {
        [_label release];
        _label = [label copy];
    }
}

- (id)newFunctionWithName:(NSString *)name constantValues:(MTLFunctionConstantValues *)constantValues error:(NSError **)error
{
    if (!name || [name length] == 0) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorFunctionNotFound, NULL, 0, @"empty function name");
        return nil;
    }

    const char *nameUtf8 = [name UTF8String];
    if (!nameUtf8) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"invalid function name");
        return nil;
    }
    size_t nameLen = strlen(nameUtf8);
    if (nameLen == 0 || nameLen > 65536u) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"invalid function name length");
        return nil;
    }
    uint32_t numConsts = constantValues ? [constantValues kidoConstantCount] : 0;
    const KidoFuncConstEntry *consts = constantValues ? [constantValues kidoConstantEntries] : NULL;
    if (numConsts > 256u) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"too many function constants");
        return nil;
    }

    /* Wire layout: kmtl_function_req (16B) + name bytes + pad to 8B + (kmtl_func_const (16B) + val padded to 8B)* */
    size_t pos = sizeof(struct kmtl_function_req) + nameLen;
    pos = (pos + 7u) & ~(size_t)7u;
    size_t totalLen = pos;
    for (uint32_t i = 0; i < numConsts; i++) {
        size_t paddedVal = ((size_t)consts[i].valueLen + 7u) & ~(size_t)7u;
        size_t entryBytes = sizeof(struct kmtl_func_const) + paddedVal;
        if (entryBytes > KMTL_MAX_IN_PAYLOAD || totalLen > KMTL_MAX_IN_PAYLOAD - entryBytes) {
            if (error)
                *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"function request exceeds limit");
            return nil;
        }
        totalLen += entryBytes;
    }

    uint8_t *payload = (uint8_t *)calloc(1, totalLen);
    if (!payload) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"out of memory");
        return nil;
    }

    struct kmtl_function_req fr;
    memset(&fr, 0, sizeof(fr));
    fr.name_len = (uint32_t)nameLen;
    fr.num_constants = numConsts;
    memcpy(payload, &fr, sizeof(fr));
    memcpy(payload + sizeof(fr), nameUtf8, nameLen);

    size_t cur = pos;
    for (uint32_t i = 0; i < numConsts; i++) {
        struct kmtl_func_const fc;
        memset(&fc, 0, sizeof(fc));
        fc.index     = consts[i].index;
        fc.data_type = consts[i].dataType;
        fc.value_len = consts[i].valueLen;
        memcpy(payload + cur, &fc, sizeof(fc));
        cur += sizeof(fc);
        memcpy(payload + cur, consts[i].value, consts[i].valueLen);
        cur += ((size_t)consts[i].valueLen + 7u) & ~(size_t)7u;
    }

    char *errBuf = (char *)malloc(kErrorBufCap);
    struct kmtl_req_hdr req;
    struct kmtl_resp_hdr resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.opcode = KMTL_OP_CREATE_FUNCTION;
    req.handle = _handle;

    kern_return_t kr = [_device rpc:&req
                              inBuf:payload
                              inLen:(uint32_t)totalLen
                               resp:&resp
                             outBuf:errBuf
                             outCap:errBuf ? kErrorBufCap : 0];
    free(payload);

    if (kr != KERN_SUCCESS || resp.status != KMTL_OK || resp.handle == 0) {
        if (error) {
            NSInteger code = (resp.status == KMTL_ERR_NOENT) ? MTLLibraryErrorFunctionNotFound
                                                             : MTLLibraryErrorCompileFailure;
            *error = KidoMakeError(MTLLibraryErrorDomain, code, errBuf, resp.extra_len,
                                   @"failed to create Metal function");
        }
        free(errBuf);
        return nil;
    }

    if (error)
        *error = nil;
    free(errBuf);
    id fn = [[KidoMTLFunction alloc] initWithDevice:_device handle:resp.handle name:name];
    if (!fn) {
        struct kmtl_req_hdr dreq;
        struct kmtl_resp_hdr dresp;
        memset(&dreq, 0, sizeof(dreq));
        memset(&dresp, 0, sizeof(dresp));
        dreq.opcode = KMTL_OP_DESTROY_FUNCTION;
        dreq.handle = resp.handle;
        [_device rpc:&dreq inBuf:NULL inLen:0 resp:&dresp outBuf:NULL outCap:0];
    }
    return fn;
}

- (id)newFunctionWithName:(NSString *)functionName
{
    return [self newFunctionWithName:functionName constantValues:nil error:NULL];
}

- (void)newFunctionWithName:(NSString *)name
             constantValues:(MTLFunctionConstantValues *)constantValues
          completionHandler:(void (^)(id function, NSError *error))completionHandler
{
    if (!completionHandler)
        return;
    void (^copied)(id, NSError *) = Block_copy(completionHandler);
    [self retain];
    NSString *nameCopy = [name copy];
    MTLFunctionConstantValues *valsCopy = [constantValues copy];
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        @autoreleasepool {
            NSError *err = nil;
            id fn = [self newFunctionWithName:nameCopy constantValues:valsCopy error:&err];
            copied(fn, err);
            [fn release];
            [nameCopy release];
            [valsCopy release];
            [self release];
            Block_release(copied);
        }
    });
}

- (id)newFunctionWithDescriptor:(MTLFunctionDescriptor *)descriptor error:(NSError **)error
{
    if (!descriptor) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"nil descriptor");
        return nil;
    }
    return [self newFunctionWithName:descriptor.name constantValues:descriptor.constantValues error:error];
}

- (void)newFunctionWithDescriptor:(MTLFunctionDescriptor *)descriptor
                completionHandler:(void (^)(id function, NSError *error))completionHandler
{
    [self newFunctionWithName:descriptor.name constantValues:descriptor.constantValues completionHandler:completionHandler];
}

@end

/* --- KidoMTLComputePipelineState --- */

@implementation KidoMTLComputePipelineState

- (instancetype)initWithDevice:(KidoMTLDevice *)device
                        handle:(uint64_t)handle
                    maxThreads:(NSUInteger)maxThreads
                     execWidth:(NSUInteger)execWidth
                   staticTgMem:(NSUInteger)staticTgMem
                         label:(NSString *)label
{
    self = [super init];
    if (self) {
        _device = [device retain];
        _handle = handle;
        _maxTotalThreadsPerThreadgroup = maxThreads;
        _threadExecutionWidth = execWidth;
        _staticThreadgroupMemoryLength = staticTgMem;
        _label = [label copy];
    }
    return self;
}

- (void)dealloc
{
    if (_device && _handle != 0) {
        struct kmtl_req_hdr req;
        struct kmtl_resp_hdr resp;
        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));
        req.opcode = KMTL_OP_DESTROY_PIPELINE;
        req.handle = _handle;
        [_device rpc:&req inBuf:NULL inLen:0 resp:&resp outBuf:NULL outCap:0];
        _handle = 0;
    }
    [_label release];
    [_device release];
    [super dealloc];
}

- (uint64_t)kidoHandle { return _handle; }
- (id)device { return _device; }
- (NSString *)label { return _label; }
- (NSUInteger)maxTotalThreadsPerThreadgroup { return _maxTotalThreadsPerThreadgroup; }
- (NSUInteger)threadExecutionWidth { return _threadExecutionWidth; }
- (NSUInteger)staticThreadgroupMemoryLength { return _staticThreadgroupMemoryLength; }
- (NSUInteger)allocatedSize { return 0; }
- (BOOL)supportIndirectCommandBuffers { return NO; }
- (MTLResourceID)gpuResourceID { MTLResourceID r = { _handle }; return r; }
- (NSUInteger)imageblockMemoryLengthForDimensions:(MTLSize)dims { (void)dims; return 0; }
- (MTLComputePipelineReflection *)reflection { return nil; }
- (MTLSize)requiredThreadsPerThreadgroup { MTLSize s = { 0, 0, 0 }; return s; }

@end

/* --- KidoMTLDevice --- */

@implementation KidoMTLDevice

- (instancetype)initWithConnect:(io_connect_t)connect info:(const struct kmtl_device_info *)info
{
    self = [super init];
    if (self) {
        _connect = connect;
        _info = *info;
        _info.name[sizeof(_info.name) - 1] = '\0';
        _name = [[NSString alloc] initWithUTF8String:_info.name[0] ? _info.name : "VirtioMetal GPU"];
        NSString *archName = (_info.gpu_family_mask & KMTL_FAM_APPLE1) ? @"applegpu_g16p" : @"kido_vulkan_gpu";
        _architecture = [[MTLArchitecture alloc] initWithName:archName];
    }
    return self;
}

- (void)dealloc
{
    if (_connect != MACH_PORT_NULL) {
        IOServiceClose(_connect);
        _connect = MACH_PORT_NULL;
    }
    [_name release];
    [_architecture release];
    [super dealloc];
}

- (io_connect_t)ioConnect { return _connect; }
- (const struct kmtl_device_info *)deviceInfo { return &_info; }

- (void)adjustAllocatedSize:(int64_t)delta
{
    if (delta >= 0) {
        __atomic_add_fetch(&_currentAllocatedSize, (uint64_t)delta, __ATOMIC_RELAXED);
    } else {
        uint64_t sub = (uint64_t)(-delta);
        uint64_t cur = __atomic_load_n(&_currentAllocatedSize, __ATOMIC_RELAXED);
        while (cur > 0) {
            uint64_t next = cur > sub ? (cur - sub) : 0;
            if (__atomic_compare_exchange_n(&_currentAllocatedSize, &cur, next, 0,
                                            __ATOMIC_RELAXED, __ATOMIC_RELAXED))
                break;
        }
    }
}

- (kern_return_t)rpc:(struct kmtl_req_hdr *)req
               inBuf:(const void *)inBuf
               inLen:(uint32_t)inLen
                resp:(struct kmtl_resp_hdr *)resp
              outBuf:(void *)outBuf
              outCap:(uint32_t)outCap
{
    struct kmtl_uc_call call;
    memset(&call, 0, sizeof(call));
    call.req = *req;
    call.req.magic   = KMTL_MAGIC;
    call.req.version = KMTL_PROTO_VERSION;
    call.in_ptr      = (uint64_t)(uintptr_t)inBuf;
    call.in_len      = inLen;
    call.out_ptr     = (uint64_t)(uintptr_t)outBuf;
    call.out_cap     = outCap;

    size_t respSize = sizeof(*resp);
    memset(resp, 0, sizeof(*resp));
    kern_return_t kr = IOConnectCallStructMethod(_connect, KMTL_UC_SELECTOR_RPC,
                                                 &call, sizeof(call),
                                                 resp, &respSize);
    if (kr != KERN_SUCCESS || respSize < sizeof(*resp))
        return kr != KERN_SUCCESS ? kr : kIOReturnIOError;
    uint32_t maxOut = outBuf ? outCap : 0;
    if (resp->extra_len > maxOut)
        resp->extra_len = maxOut;
    return KERN_SUCCESS;
}

- (BOOL)mapBufferHandle:(uint64_t)handle outAddr:(mach_vm_address_t *)outAddr outSize:(mach_vm_size_t *)outSize
{
    if (!outAddr || !outSize || handle == 0 || handle > 0xFFFFFFFFULL)
        return NO;
    uint64_t inScalar = handle;
    uint64_t outScalars[2] = { 0, 0 };
    uint32_t outCount = 2;
    kern_return_t kr = IOConnectCallScalarMethod(_connect, KMTL_UC_SELECTOR_MAP_BUFFER,
                                                 &inScalar, 1,
                                                 outScalars, &outCount);
    if (kr != KERN_SUCCESS || outCount < 2 || outScalars[0] == 0 || outScalars[1] == 0)
        return NO;
    *outAddr = (mach_vm_address_t)outScalars[0];
    *outSize = (mach_vm_size_t)outScalars[1];
    return YES;
}

- (void)unmapBufferHandle:(uint64_t)handle
{
    if (handle == 0 || handle > 0xFFFFFFFFULL)
        return;
    uint64_t inScalar = handle;
    IOConnectCallScalarMethod(_connect, KMTL_UC_SELECTOR_UNMAP_BUFFER,
                              &inScalar, 1, NULL, NULL);
}

/* --- Device Info & Capabilities --- */

- (NSString *)name { return _name; }
- (uint64_t)registryID { return _info.registry_id; }
- (MTLArchitecture *)architecture { return _architecture; }
- (BOOL)hasUnifiedMemory { return (_info.flags & KMTL_FEAT_UNIFIED_MEMORY) != 0; }
- (BOOL)supportsBFloat16 { return (_info.flags & KMTL_FEAT_BFLOAT16) != 0; }
- (BOOL)isLowPower { return NO; }
- (BOOL)isHeadless { return YES; }
- (BOOL)isRemovable { return NO; }
- (MTLDeviceLocation)location { return MTLDeviceLocationBuiltIn; }
- (NSUInteger)locationNumber { return 0; }
- (uint64_t)maxTransferRate { return 0; }
- (NSUInteger)maxBufferLength { return (NSUInteger)_info.max_buffer_length; }
- (uint64_t)recommendedMaxWorkingSetSize { return _info.recommended_max_working_set_size; }
- (NSUInteger)currentAllocatedSize { return (NSUInteger)__atomic_load_n(&_currentAllocatedSize, __ATOMIC_RELAXED); }
- (NSUInteger)maxThreadgroupMemoryLength { return (NSUInteger)_info.max_threadgroup_memory_length; }

- (MTLSize)maxThreadsPerThreadgroup
{
    MTLSize s;
    s.width  = _info.max_threads_per_threadgroup[0] ?: 1024;
    s.height = _info.max_threads_per_threadgroup[1] ?: 1024;
    s.depth  = _info.max_threads_per_threadgroup[2] ?: 64;
    return s;
}

- (BOOL)supportsFamily:(MTLGPUFamily)gpuFamily
{
    uint64_t m = _info.gpu_family_mask;
    switch (gpuFamily) {
    case MTLGPUFamilyApple1:  return (m & KMTL_FAM_APPLE1) != 0;
    case MTLGPUFamilyApple2:  return (m & KMTL_FAM_APPLE2) != 0;
    case MTLGPUFamilyApple3:  return (m & KMTL_FAM_APPLE3) != 0;
    case MTLGPUFamilyApple4:  return (m & KMTL_FAM_APPLE4) != 0;
    case MTLGPUFamilyApple5:  return (m & KMTL_FAM_APPLE5) != 0;
    case MTLGPUFamilyApple6:  return (m & KMTL_FAM_APPLE6) != 0;
    case MTLGPUFamilyApple7:  return (m & KMTL_FAM_APPLE7) != 0;
    case MTLGPUFamilyApple8:  return (m & KMTL_FAM_APPLE8) != 0;
    case MTLGPUFamilyApple9:  return (m & KMTL_FAM_APPLE9) != 0;
    case MTLGPUFamilyApple10: return (m & KMTL_FAM_APPLE10) != 0;
    case MTLGPUFamilyCommon1: return (m & KMTL_FAM_COMMON1) != 0;
    case MTLGPUFamilyCommon2: return (m & KMTL_FAM_COMMON2) != 0;
    case MTLGPUFamilyCommon3: return (m & KMTL_FAM_COMMON3) != 0;
    case MTLGPUFamilyMac1:
    case MTLGPUFamilyMac2:
    case MTLGPUFamilyMacCatalyst1:
    case MTLGPUFamilyMacCatalyst2:
        return (m & KMTL_FAM_MAC2) != 0;
    case MTLGPUFamilyMetal3:  return (m & KMTL_FAM_METAL3) != 0;
    case MTLGPUFamilyMetal4:  return (m & KMTL_FAM_METAL4) != 0;
    default: return NO;
    }
}

- (BOOL)supportsFeatureSet:(MTLFeatureSet)featureSet
{
    if (featureSet >= MTLFeatureSet_macOS_GPUFamily1_v1 && featureSet <= MTLFeatureSet_macOS_GPUFamily2_v1)
        return (_info.gpu_family_mask & KMTL_FAM_MAC2) != 0;
    if (featureSet <= MTLFeatureSet_iOS_GPUFamily5_v1)
        return (_info.gpu_family_mask & KMTL_FAM_APPLE5) != 0;
    return NO;
}

- (BOOL)supports32BitFloatFiltering { return (_info.gpu_family_mask & KMTL_FAM_MAC2) != 0; }
- (BOOL)supports32BitMSAA { return (_info.gpu_family_mask & KMTL_FAM_APPLE7) != 0; }
- (BOOL)supportsBCTextureCompression { return (_info.gpu_family_mask & KMTL_FAM_MAC2) != 0; }
- (BOOL)supportsQueryTextureLOD { return (_info.gpu_family_mask & KMTL_FAM_APPLE6) != 0; }
- (BOOL)supportsFunctionPointers { return (_info.gpu_family_mask & KMTL_FAM_APPLE6) != 0; }
- (BOOL)supportsFunctionPointersFromRender { return (_info.gpu_family_mask & KMTL_FAM_APPLE7) != 0; }
- (BOOL)supportsDynamicLibraries { return (_info.gpu_family_mask & KMTL_FAM_APPLE6) != 0; }
- (BOOL)supportsRenderDynamicLibraries { return (_info.gpu_family_mask & KMTL_FAM_APPLE7) != 0; }
- (BOOL)supportsRaytracing { return (_info.gpu_family_mask & KMTL_FAM_APPLE7) != 0; }
- (BOOL)supportsRaytracingFromRender { return (_info.gpu_family_mask & KMTL_FAM_APPLE7) != 0; }
- (BOOL)supportsShaderBarycentricCoordinates { return (_info.gpu_family_mask & KMTL_FAM_APPLE6) != 0; }
- (BOOL)areBarycentricCoordsSupported { return [self supportsShaderBarycentricCoordinates]; }
- (BOOL)isDepth24Stencil8PixelFormatSupported { return NO; }
- (BOOL)areRasterOrderGroupsSupported { return (_info.gpu_family_mask & KMTL_FAM_APPLE4) != 0; }
- (BOOL)supportsPullModelInterpolation { return (_info.gpu_family_mask & KMTL_FAM_APPLE6) != 0; }
- (BOOL)supportsPlacementSparse { return NO; }
- (MTLArgumentBuffersTier)argumentBuffersSupport
{
    return (_info.gpu_family_mask & KMTL_FAM_APPLE6) ? MTLArgumentBuffersTier2 : MTLArgumentBuffersTier1;
}
- (MTLReadWriteTextureTier)readWriteTextureSupport
{
    return (_info.gpu_family_mask & KMTL_FAM_APPLE4) ? MTLReadWriteTextureTier2 : MTLReadWriteTextureTier1;
}
- (BOOL)shouldMaximizeConcurrentCompilation { return _shouldMaximizeConcurrentCompilation; }
- (void)setShouldMaximizeConcurrentCompilation:(BOOL)v { _shouldMaximizeConcurrentCompilation = v; }
- (NSUInteger)maximumConcurrentCompilationTaskCount { return 4; }
- (BOOL)supportsTextureSampleCount:(NSUInteger)sampleCount
{
    return sampleCount == 1 || sampleCount == 2 || sampleCount == 4;
}
- (NSUInteger)minimumLinearTextureAlignmentForPixelFormat:(NSUInteger)fmt { (void)fmt; return 16; }
- (NSUInteger)minimumTextureBufferAlignmentForPixelFormat:(NSUInteger)fmt { (void)fmt; return 16; }
- (MTLSizeAndAlign)heapBufferSizeAndAlignWithLength:(NSUInteger)length options:(MTLResourceOptions)options
{
    (void)options;
    NSUInteger align = _info.host_page_size ?: 16384;
    NSUInteger sz = (length > NSUIntegerMax - (align - 1)) ? NSUIntegerMax : ((length + align - 1) & ~(align - 1));
    MTLSizeAndAlign r = { sz, align };
    return r;
}
- (uint64_t)queryTimestampFrequency { return 1000000000ULL; }
- (NSUInteger)maxArgumentBufferSamplerCount { return 16; }
- (BOOL)areProgrammableSamplePositionsSupported { return NO; }
- (BOOL)supportsCounterSampling:(NSUInteger)point { (void)point; return NO; }
- (NSArray *)counterSets { return nil; }
- (uint64_t)peerGroupID { return 0; }
- (uint32_t)peerIndex { return 0; }
- (uint32_t)peerCount { return 0; }

/* --- Buffers (Zero-Copy Shared Memory) --- */

- (id)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options
{
    if (length == 0)
        return nil;

    struct kmtl_req_hdr req;
    struct kmtl_resp_hdr resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.opcode = KMTL_OP_CREATE_BUFFER;
    req.arg0   = (uint64_t)length;
    req.arg1   = (uint64_t)options;

    kern_return_t kr = [self rpc:&req inBuf:NULL inLen:0 resp:&resp outBuf:NULL outCap:0];
    if (kr != KERN_SUCCESS || resp.status != KMTL_OK || resp.handle == 0 || resp.handle > 0xFFFFFFFFULL)
        return nil;

    mach_vm_address_t addr = 0;
    mach_vm_size_t size = 0;
    if (![self mapBufferHandle:resp.handle outAddr:&addr outSize:&size] || size < length) {
        if (addr != 0)
            [self unmapBufferHandle:resp.handle];
        struct kmtl_req_hdr dreq;
        struct kmtl_resp_hdr dresp;
        memset(&dreq, 0, sizeof(dreq));
        memset(&dresp, 0, sizeof(dresp));
        dreq.opcode = KMTL_OP_DESTROY_BUFFER;
        dreq.handle = resp.handle;
        [self rpc:&dreq inBuf:NULL inLen:0 resp:&dresp outBuf:NULL outCap:0];
        return nil;
    }

    KidoMTLBuffer *buf = [[KidoMTLBuffer alloc] initWithDevice:self
                                                        handle:resp.handle
                                                     shmOffset:resp.arg0
                                                    alignedLen:resp.arg1
                                                        length:length
                                                    mappedAddr:addr
                                                    mappedSize:size
                                                       options:options];
    if (!buf) {
        [self unmapBufferHandle:resp.handle];
        struct kmtl_req_hdr dreq;
        struct kmtl_resp_hdr dresp;
        memset(&dreq, 0, sizeof(dreq));
        memset(&dresp, 0, sizeof(dresp));
        dreq.opcode = KMTL_OP_DESTROY_BUFFER;
        dreq.handle = resp.handle;
        [self rpc:&dreq inBuf:NULL inLen:0 resp:&dresp outBuf:NULL outCap:0];
    }
    return buf;
}

- (id)newBufferWithBytes:(const void *)pointer length:(NSUInteger)length options:(MTLResourceOptions)options
{
    if (!pointer || length == 0)
        return nil;
    KidoMTLBuffer *buf = [self newBufferWithLength:length options:options];
    if (!buf)
        return nil;
    memcpy([buf contents], pointer, length);
    return buf;
}

static void
KidoInspectNoCopyRegion(void *pointer, NSUInteger length, BOOL *outWritable, BOOL *outCanInPlace)
{
    *outWritable = NO;
    *outCanInPlace = NO;

    mach_vm_address_t start = (mach_vm_address_t)(uintptr_t)pointer;
    mach_vm_address_t end = start + (mach_vm_size_t)length;
    if (end <= start)
        return;

    /*
     * Walk all VM entries covering [start, end) because XNU splits anonymous vm_allocate
     * ranges > 128 MiB into multiple contiguous 128 MiB vm_map_entry chunks.
     */
    BOOL allWritable = YES;
    BOOL allPrivateRW = YES;
    mach_vm_address_t cur = start;
    while (cur < end) {
        mach_vm_address_t rAddr = cur;
        mach_vm_size_t rSize = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t objName = MACH_PORT_NULL;
        kern_return_t kr = mach_vm_region(mach_task_self(), &rAddr, &rSize,
                                          VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info,
                                          &count, &objName);
        if (objName != MACH_PORT_NULL)
            mach_port_deallocate(mach_task_self(), objName);
        if (kr != KERN_SUCCESS || rAddr > cur || rSize == 0) {
            allWritable = NO;
            allPrivateRW = NO;
            break;
        }
        if ((info.protection & VM_PROT_WRITE) == 0) {
            allWritable = NO;
            allPrivateRW = NO;
        }
        if ((info.protection & VM_PROT_READ) == 0 || info.shared) {
            allPrivateRW = NO;
        }
        mach_vm_address_t next = rAddr + rSize;
        if (next <= cur) {
            allWritable = NO;
            allPrivateRW = NO;
            break;
        }
        cur = next;
    }

    *outWritable = allWritable;
    if (allPrivateRW && (malloc_zone_from_ptr(pointer) == NULL || length >= (256u * 1024u)))
        *outCanInPlace = YES;
}

- (id)newBufferWithBytesNoCopy:(void *)pointer
                        length:(NSUInteger)length
                       options:(MTLResourceOptions)options
                   deallocator:(void (^)(void *pointer, NSUInteger length))deallocator
{
    if (!pointer || length == 0)
        return nil;

    NSUInteger hostPage = _info.host_page_size ?: 16384;
    NSUInteger guestPage = (NSUInteger)vm_page_size;
    NSUInteger reqAlign = hostPage > guestPage ? hostPage : guestPage;
    BOOL writable = NO;
    BOOL regionAllowsInPlace = NO;
    KidoInspectNoCopyRegion(pointer, length, &writable, &regionAllowsInPlace);
    BOOL canInPlace = regionAllowsInPlace &&
                      ((((uintptr_t)pointer) & (reqAlign - 1)) == 0) &&
                      ((length & (reqAlign - 1)) == 0);

    KidoMTLBuffer *buf = [self newBufferWithLength:length options:options];
    if (!buf)
        return nil;

    void *shmPtr = [buf contents];
    memcpy(shmPtr, pointer, length);

    BOOL inPlace = NO;
    if (canInPlace && [buf allocatedSize] == length) {
        /* Atomically overwrite private anonymous RW pages with zero VA-hole window. */
        mach_vm_address_t targetAddr = (mach_vm_address_t)(uintptr_t)pointer;
        vm_prot_t curProt = VM_PROT_NONE;
        vm_prot_t maxProt = VM_PROT_NONE;
        kern_return_t rkr = mach_vm_remap(mach_task_self(), &targetAddr, (mach_vm_size_t)length, 0,
                                          VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                                          mach_task_self(), (mach_vm_address_t)(uintptr_t)shmPtr, FALSE,
                                          &curProt, &maxProt, VM_INHERIT_NONE);
        if (rkr == KERN_SUCCESS && targetAddr == (mach_vm_address_t)(uintptr_t)pointer)
            inPlace = YES;
    }

    [buf setNoCopyPointer:pointer length:length inPlace:inPlace writable:writable deallocator:deallocator];
    return buf;
}

/* --- Libraries (MSL Source & Precompiled .metallib AIR) --- */

- (id)newLibraryWithSource:(NSString *)source options:(MTLCompileOptions *)options error:(NSError **)error
{
    if (!source) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorCompileFailure, NULL, 0, @"nil shader source");
        return nil;
    }

    const char *srcUtf8 = [source UTF8String];
    if (!srcUtf8) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorCompileFailure, NULL, 0, @"invalid UTF-8 source");
        return nil;
    }
    size_t srcLen = strlen(srcUtf8);
    if (srcLen > KMTL_MAX_IN_PAYLOAD - sizeof(struct kmtl_compile_opts)) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"shader source exceeds limit");
        return nil;
    }

    NSDictionary *macros = options ? options.preprocessorMacros : nil;
    NSArray *keys = macros ? [macros allKeys] : nil;
    NSUInteger keyCount = keys ? [keys count] : 0;
    if (keyCount > 1024u) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"too many preprocessor macros");
        return nil;
    }
    uint32_t numMacros = (uint32_t)keyCount;

    /* Compute total serialized size. */
    size_t totalLen = sizeof(struct kmtl_compile_opts) + srcLen;
    const char **kStrs = NULL;
    const char **vStrs = NULL;
    size_t *kLens = NULL;
    size_t *vLens = NULL;
    NSMutableArray *tempStrings = nil;

    if (numMacros > 0) {
        kStrs = (const char **)calloc(numMacros, sizeof(const char *));
        vStrs = (const char **)calloc(numMacros, sizeof(const char *));
        kLens = (size_t *)calloc(numMacros, sizeof(size_t));
        vLens = (size_t *)calloc(numMacros, sizeof(size_t));
        tempStrings = [[NSMutableArray alloc] initWithCapacity:numMacros];
        if (!kStrs || !vStrs || !kLens || !vLens || !tempStrings) {
            free(kStrs); free(vStrs); free(kLens); free(vLens); [tempStrings release];
            if (error)
                *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"out of memory");
            return nil;
        }
        for (uint32_t i = 0; i < numMacros; i++) {
            id kObj = [keys objectAtIndex:i];
            id vObj = [macros objectForKey:kObj];
            NSString *ks = [kObj isKindOfClass:[NSString class]] ? (NSString *)kObj : [kObj description];
            NSString *vs;
            if ([vObj isKindOfClass:[NSString class]])
                vs = (NSString *)vObj;
            else if ([vObj isKindOfClass:[NSNumber class]])
                vs = [(NSNumber *)vObj stringValue];
            else
                vs = vObj ? [vObj description] : @"";
            if (ks) [tempStrings addObject:ks];
            if (vs) [tempStrings addObject:vs];
            const char *ku = ks ? [ks UTF8String] : NULL;
            const char *vu = vs ? [vs UTF8String] : NULL;
            kStrs[i] = ku ? ku : "";
            vStrs[i] = vu ? vu : "";
            kLens[i] = strlen(kStrs[i]);
            vLens[i] = strlen(vStrs[i]);
            if (kLens[i] > KMTL_MAX_IN_PAYLOAD || vLens[i] > KMTL_MAX_IN_PAYLOAD ||
                sizeof(struct kmtl_macro_hdr) + kLens[i] + vLens[i] > KMTL_MAX_IN_PAYLOAD - totalLen) {
                free(kStrs); free(vStrs); free(kLens); free(vLens); [tempStrings release];
                if (error)
                    *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"shader source exceeds limit");
                return nil;
            }
            totalLen += sizeof(struct kmtl_macro_hdr) + kLens[i] + vLens[i];
        }
    }

    if (totalLen > KMTL_MAX_IN_PAYLOAD) {
        free(kStrs); free(vStrs); free(kLens); free(vLens); [tempStrings release];
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"shader source exceeds limit");
        return nil;
    }

    uint8_t *payload = (uint8_t *)malloc(totalLen);
    if (!payload) {
        free(kStrs); free(vStrs); free(kLens); free(vLens); [tempStrings release];
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"out of memory");
        return nil;
    }

    struct kmtl_compile_opts co;
    memset(&co, 0, sizeof(co));
    co.fast_math        = options ? (options.fastMathEnabled ? 1u : 0u) : 1u;
    co.language_version = options ? (uint32_t)options.languageVersion : 0u;
    co.source_len       = (uint32_t)srcLen;
    co.num_macros       = numMacros;

    size_t pos = 0;
    memcpy(payload + pos, &co, sizeof(co));
    pos += sizeof(co);
    if (srcLen > 0) {
        memcpy(payload + pos, srcUtf8, srcLen);
        pos += srcLen;
    }
    for (uint32_t i = 0; i < numMacros; i++) {
        struct kmtl_macro_hdr mh;
        mh.key_len = (uint32_t)kLens[i];
        mh.val_len = (uint32_t)vLens[i];
        memcpy(payload + pos, &mh, sizeof(mh));
        pos += sizeof(mh);
        if (kLens[i] > 0) {
            memcpy(payload + pos, kStrs[i], kLens[i]);
            pos += kLens[i];
        }
        if (vLens[i] > 0) {
            memcpy(payload + pos, vStrs[i], vLens[i]);
            pos += vLens[i];
        }
    }
    free(kStrs); free(vStrs); free(kLens); free(vLens); [tempStrings release];

    char *outBuf = (char *)malloc(kLibOutBufCap);
    struct kmtl_req_hdr req;
    struct kmtl_resp_hdr resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.opcode = KMTL_OP_CREATE_LIBRARY_SOURCE;

    kern_return_t kr = [self rpc:&req
                           inBuf:payload
                           inLen:(uint32_t)totalLen
                            resp:&resp
                          outBuf:outBuf
                          outCap:outBuf ? kLibOutBufCap : 0];
    free(payload);

    if (kr != KERN_SUCCESS || resp.status != KMTL_OK || resp.handle == 0) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorCompileFailure,
                                   outBuf, resp.extra_len, @"failed to compile Metal shader source");
        free(outBuf);
        return nil;
    }

    if (error)
        *error = nil;
    NSArray *fnNames = KidoDecodeFunctionNames(outBuf, resp.extra_len);
    free(outBuf);
    id lib = [[KidoMTLLibrary alloc] initWithDevice:self handle:resp.handle functionNames:fnNames];
    if (!lib) {
        struct kmtl_req_hdr dreq;
        struct kmtl_resp_hdr dresp;
        memset(&dreq, 0, sizeof(dreq));
        memset(&dresp, 0, sizeof(dresp));
        dreq.opcode = KMTL_OP_DESTROY_LIBRARY;
        dreq.handle = resp.handle;
        [self rpc:&dreq inBuf:NULL inLen:0 resp:&dresp outBuf:NULL outCap:0];
    }
    return lib;
}

- (void)newLibraryWithSource:(NSString *)source
                     options:(MTLCompileOptions *)options
           completionHandler:(MTLNewLibraryCompletionHandler)completionHandler
{
    if (!completionHandler)
        return;
    MTLNewLibraryCompletionHandler copied = Block_copy(completionHandler);
    NSString *srcCopy = [source copy];
    MTLCompileOptions *optsCopy = [options copy];
    [self retain];
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        @autoreleasepool {
            NSError *err = nil;
            id lib = [self newLibraryWithSource:srcCopy options:optsCopy error:&err];
            copied(lib, err);
            [lib release];
            [srcCopy release];
            [optsCopy release];
            [self release];
            Block_release(copied);
        }
    });
}

- (id)kidoNewLibraryWithRawBytes:(const void *)bytes length:(size_t)length error:(NSError **)error
{
    if (!bytes || length == 0 || length > KMTL_MAX_IN_PAYLOAD) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"invalid metallib data");
        return nil;
    }

    char *outBuf = (char *)malloc(kLibOutBufCap);
    struct kmtl_req_hdr req;
    struct kmtl_resp_hdr resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.opcode = KMTL_OP_CREATE_LIBRARY_DATA;

    kern_return_t kr = [self rpc:&req
                           inBuf:bytes
                           inLen:(uint32_t)length
                            resp:&resp
                          outBuf:outBuf
                          outCap:outBuf ? kLibOutBufCap : 0];

    if (kr != KERN_SUCCESS || resp.status != KMTL_OK || resp.handle == 0) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal,
                                   outBuf, resp.extra_len, @"failed to load metallib data");
        free(outBuf);
        return nil;
    }

    if (error)
        *error = nil;
    NSArray *fnNames = KidoDecodeFunctionNames(outBuf, resp.extra_len);
    free(outBuf);
    id lib = [[KidoMTLLibrary alloc] initWithDevice:self handle:resp.handle functionNames:fnNames];
    if (!lib) {
        struct kmtl_req_hdr dreq;
        struct kmtl_resp_hdr dresp;
        memset(&dreq, 0, sizeof(dreq));
        memset(&dresp, 0, sizeof(dresp));
        dreq.opcode = KMTL_OP_DESTROY_LIBRARY;
        dreq.handle = resp.handle;
        [self rpc:&dreq inBuf:NULL inLen:0 resp:&dresp outBuf:NULL outCap:0];
    }
    return lib;
}

- (id)newLibraryWithData:(dispatch_data_t)data error:(NSError **)error
{
    if (!data) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"nil dispatch_data_t");
        return nil;
    }
    const void *buf = NULL;
    size_t size = 0;
    dispatch_data_t mapped = dispatch_data_create_map(data, &buf, &size);
    if (!mapped) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"failed to map dispatch_data_t");
        return nil;
    }
    id lib = [self kidoNewLibraryWithRawBytes:buf length:size error:error];
    dispatch_release(mapped);
    return lib;
}

- (id)newLibraryWithFile:(NSString *)filepath error:(NSError **)error
{
    if (!filepath) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorFileNotFound, NULL, 0, @"nil library path");
        return nil;
    }
    NSData *data = [NSData dataWithContentsOfFile:filepath];
    if (!data || [data length] == 0) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorFileNotFound, NULL, 0, @"metallib file not found");
        return nil;
    }
    return [self kidoNewLibraryWithRawBytes:[data bytes] length:[data length] error:error];
}

- (id)newLibraryWithURL:(NSURL *)url error:(NSError **)error
{
    if (!url) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorFileNotFound, NULL, 0, @"nil library URL");
        return nil;
    }
    return [self newLibraryWithFile:[url path] error:error];
}

- (id)newDefaultLibraryWithBundle:(NSBundle *)bundle error:(NSError **)error
{
    NSString *path = nil;
    if (bundle)
        path = [bundle pathForResource:@"default" ofType:@"metallib"];
    if (!path && bundle) {
        NSString *cand = [[bundle bundlePath] stringByAppendingPathComponent:@"default.metallib"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:cand])
            path = cand;
    }
    if (!path) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorFileNotFound, NULL, 0, @"default.metallib not found");
        return nil;
    }
    return [self newLibraryWithFile:path error:error];
}

- (id)newDefaultLibrary
{
    return [self newDefaultLibraryWithBundle:[NSBundle mainBundle] error:NULL];
}

/* --- Compute Pipelines --- */

- (id)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor
                                    options:(MTLPipelineOption)options
                                 reflection:(MTLComputePipelineReflection **)reflection
                                      error:(NSError **)error
{
    (void)options;
    if (!descriptor || !descriptor.computeFunction) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"missing computeFunction");
        return nil;
    }
    if (![descriptor.computeFunction isKindOfClass:[KidoMTLFunction class]]) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorInternal, NULL, 0, @"invalid MTLFunction");
        return nil;
    }
    KidoMTLFunction *fn = (KidoMTLFunction *)descriptor.computeFunction;

    char *errBuf = (char *)malloc(kErrorBufCap);
    struct kmtl_req_hdr req;
    struct kmtl_resp_hdr resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.opcode = KMTL_OP_CREATE_COMPUTE_PIPELINE;
    req.handle = [fn kidoHandle];
    req.arg0   = (uint64_t)descriptor.maxTotalThreadsPerThreadgroup;
    req.arg1   = descriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth ? 1ULL : 0ULL;

    kern_return_t kr = [self rpc:&req
                           inBuf:NULL
                           inLen:0
                            resp:&resp
                          outBuf:errBuf
                          outCap:errBuf ? kErrorBufCap : 0];
    if (kr != KERN_SUCCESS || resp.status != KMTL_OK || resp.handle == 0) {
        if (error)
            *error = KidoMakeError(MTLLibraryErrorDomain, MTLLibraryErrorCompileFailure,
                                   errBuf, resp.extra_len, @"failed to create compute pipeline state");
        free(errBuf);
        return nil;
    }
    free(errBuf);
    if (error)
        *error = nil;

    if (reflection)
        *reflection = [[[MTLComputePipelineReflection alloc] init] autorelease];

    NSUInteger maxThreads = (NSUInteger)(resp.arg0 & 0xFFFFFFFFULL);
    NSUInteger execWidth  = (NSUInteger)(resp.arg0 >> 32);
    NSUInteger staticTg   = (NSUInteger)resp.arg1;

    id pso = [[KidoMTLComputePipelineState alloc] initWithDevice:self
                                                          handle:resp.handle
                                                      maxThreads:maxThreads ?: 1024
                                                       execWidth:execWidth ?: 32
                                                     staticTgMem:staticTg
                                                           label:descriptor.label];
    if (!pso) {
        struct kmtl_req_hdr dreq;
        struct kmtl_resp_hdr dresp;
        memset(&dreq, 0, sizeof(dreq));
        memset(&dresp, 0, sizeof(dresp));
        dreq.opcode = KMTL_OP_DESTROY_PIPELINE;
        dreq.handle = resp.handle;
        [self rpc:&dreq inBuf:NULL inLen:0 resp:&dresp outBuf:NULL outCap:0];
    }
    return pso;
}

- (id)newComputePipelineStateWithFunction:(id)computeFunction
                                  options:(MTLPipelineOption)options
                               reflection:(MTLComputePipelineReflection **)reflection
                                    error:(NSError **)error
{
    MTLComputePipelineDescriptor *desc = [[MTLComputePipelineDescriptor alloc] init];
    desc.computeFunction = computeFunction;
    id pso = [self newComputePipelineStateWithDescriptor:desc options:options reflection:reflection error:error];
    [desc release];
    return pso;
}

- (id)newComputePipelineStateWithFunction:(id)computeFunction error:(NSError **)error
{
    return [self newComputePipelineStateWithFunction:computeFunction
                                             options:MTLPipelineOptionNone
                                          reflection:NULL
                                               error:error];
}

- (void)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)descriptor
                                      options:(MTLPipelineOption)options
                            completionHandler:(MTLNewComputePipelineStateWithReflectionCompletionHandler)completionHandler
{
    if (!completionHandler)
        return;
    MTLNewComputePipelineStateWithReflectionCompletionHandler copied = Block_copy(completionHandler);
    MTLComputePipelineDescriptor *descCopy = [descriptor copy];
    [self retain];
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        @autoreleasepool {
            MTLComputePipelineReflection *refl = nil;
            NSError *err = nil;
            id pso = [self newComputePipelineStateWithDescriptor:descCopy options:options reflection:&refl error:&err];
            copied(pso, refl, err);
            [pso release];
            [descCopy release];
            [self release];
            Block_release(copied);
        }
    });
}

- (void)newComputePipelineStateWithFunction:(id)computeFunction
                          completionHandler:(MTLNewComputePipelineStateCompletionHandler)completionHandler
{
    if (!completionHandler)
        return;
    MTLNewComputePipelineStateCompletionHandler copied = Block_copy(completionHandler);
    [computeFunction retain];
    [self retain];
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        @autoreleasepool {
            NSError *err = nil;
            id pso = [self newComputePipelineStateWithFunction:computeFunction error:&err];
            copied(pso, err);
            [pso release];
            [computeFunction release];
            [self release];
            Block_release(copied);
        }
    });
}

- (void)newComputePipelineStateWithFunction:(id)computeFunction
                                    options:(MTLPipelineOption)options
                          completionHandler:(MTLNewComputePipelineStateWithReflectionCompletionHandler)completionHandler
{
    MTLComputePipelineDescriptor *desc = [[MTLComputePipelineDescriptor alloc] init];
    desc.computeFunction = computeFunction;
    [self newComputePipelineStateWithDescriptor:desc options:options completionHandler:completionHandler];
    [desc release];
}

/* --- Command Queues & Sync Objects --- */

- (id)newCommandQueueWithMaxCommandBufferCount:(NSUInteger)maxCommandBufferCount
{
    struct kmtl_req_hdr req;
    struct kmtl_resp_hdr resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.opcode = KMTL_OP_CREATE_QUEUE;
    req.arg0   = (uint64_t)maxCommandBufferCount;

    kern_return_t kr = [self rpc:&req inBuf:NULL inLen:0 resp:&resp outBuf:NULL outCap:0];
    if (kr != KERN_SUCCESS || resp.status != KMTL_OK || resp.handle == 0)
        return nil;

    id q = [[KidoMTLCommandQueue alloc] initWithDevice:self handle:resp.handle];
    if (!q) {
        struct kmtl_req_hdr dreq;
        struct kmtl_resp_hdr dresp;
        memset(&dreq, 0, sizeof(dreq));
        memset(&dresp, 0, sizeof(dresp));
        dreq.opcode = KMTL_OP_DESTROY_QUEUE;
        dreq.handle = resp.handle;
        [self rpc:&dreq inBuf:NULL inLen:0 resp:&dresp outBuf:NULL outCap:0];
    }
    return q;
}

- (id)newCommandQueue
{
    return [self newCommandQueueWithMaxCommandBufferCount:64];
}

- (id)newCommandQueueWithDescriptor:(MTLCommandQueueDescriptor *)descriptor
{
    NSUInteger maxCount = descriptor ? descriptor.maxCommandBufferCount : 64;
    return [self newCommandQueueWithMaxCommandBufferCount:maxCount];
}

- (id)newEvent { return [[KidoMTLEvent alloc] initWithDevice:self]; }
- (id)newSharedEvent { return [[KidoMTLSharedEvent alloc] initWithDevice:self]; }
- (id)newFence { return [[KidoMTLFence alloc] initWithDevice:self]; }
- (id)newResidencySetWithDescriptor:(MTLResidencySetDescriptor *)desc error:(NSError **)error
{
    if (error)
        *error = nil;
    return [[KidoMTLResidencySet alloc] initWithDevice:self descriptor:desc];
}

@end

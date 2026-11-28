/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#import <Foundation/Foundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMapTypes.h>
#include <dispatch/dispatch.h>
#include <Block.h>
#include <pthread.h>
#include <stdint.h>
#include "kmtl_proto.h"

#define KMTL_EXPORT __attribute__((visibility("default")))
#define KMTL_UC_SELECTOR_MAP_BUFFER   1u
#define KMTL_UC_SELECTOR_UNMAP_BUFFER 2u

/* --- Apple Metal ABI structs & enums (verified against macOS 26.5 SDK) --- */

typedef uint64_t MTLGPUAddress;

typedef struct {
    NSUInteger x, y, z;
} MTLOrigin;

typedef struct {
    NSUInteger width, height, depth;
} MTLSize;

typedef struct {
    MTLOrigin origin;
    MTLSize size;
} MTLRegion;

typedef struct {
    NSUInteger size;
    NSUInteger align;
} MTLSizeAndAlign;

typedef struct MTLResourceID {
    uint64_t _impl;
} MTLResourceID;

typedef NS_ENUM(NSUInteger, MTLPurgeableState) {
    MTLPurgeableStateKeepCurrent = 1,
    MTLPurgeableStateNonVolatile = 2,
    MTLPurgeableStateVolatile    = 3,
    MTLPurgeableStateEmpty       = 4,
};

typedef NS_ENUM(NSUInteger, MTLCPUCacheMode) {
    MTLCPUCacheModeDefaultCache  = 0,
    MTLCPUCacheModeWriteCombined = 1,
};

typedef NS_ENUM(NSUInteger, MTLStorageMode) {
    MTLStorageModeShared     = 0,
    MTLStorageModeManaged    = 1,
    MTLStorageModePrivate    = 2,
    MTLStorageModeMemoryless = 3,
};

typedef NS_ENUM(NSUInteger, MTLHazardTrackingMode) {
    MTLHazardTrackingModeDefault   = 0,
    MTLHazardTrackingModeUntracked = 1,
    MTLHazardTrackingModeTracked   = 2,
};

#define MTLResourceCPUCacheModeShift       0
#define MTLResourceCPUCacheModeMask        (0xfUL << MTLResourceCPUCacheModeShift)
#define MTLResourceStorageModeShift        4
#define MTLResourceStorageModeMask         (0xfUL << MTLResourceStorageModeShift)
#define MTLResourceHazardTrackingModeShift 8
#define MTLResourceHazardTrackingModeMask  (0x3UL << MTLResourceHazardTrackingModeShift)

typedef NS_OPTIONS(NSUInteger, MTLResourceOptions) {
    MTLResourceCPUCacheModeDefaultCache      = MTLCPUCacheModeDefaultCache << MTLResourceCPUCacheModeShift,
    MTLResourceCPUCacheModeWriteCombined     = MTLCPUCacheModeWriteCombined << MTLResourceCPUCacheModeShift,
    MTLResourceStorageModeShared             = MTLStorageModeShared << MTLResourceStorageModeShift,
    MTLResourceStorageModeManaged            = MTLStorageModeManaged << MTLResourceStorageModeShift,
    MTLResourceStorageModePrivate            = MTLStorageModePrivate << MTLResourceStorageModeShift,
    MTLResourceStorageModeMemoryless         = MTLStorageModeMemoryless << MTLResourceStorageModeShift,
    MTLResourceHazardTrackingModeDefault     = MTLHazardTrackingModeDefault << MTLResourceHazardTrackingModeShift,
    MTLResourceHazardTrackingModeUntracked   = MTLHazardTrackingModeUntracked << MTLResourceHazardTrackingModeShift,
    MTLResourceHazardTrackingModeTracked     = MTLHazardTrackingModeTracked << MTLResourceHazardTrackingModeShift,
};

typedef NS_ENUM(NSUInteger, MTLFeatureSet) {
    MTLFeatureSet_iOS_GPUFamily1_v1       = 0,
    MTLFeatureSet_iOS_GPUFamily5_v1       = 16,
    MTLFeatureSet_macOS_GPUFamily1_v1     = 10000,
    MTLFeatureSet_macOS_GPUFamily1_v2     = 10001,
    MTLFeatureSet_macOS_ReadWriteTextureTier2 = 10002,
    MTLFeatureSet_macOS_GPUFamily1_v3     = 10003,
    MTLFeatureSet_macOS_GPUFamily1_v4     = 10004,
    MTLFeatureSet_macOS_GPUFamily2_v1     = 10005,
};

typedef NS_ENUM(NSInteger, MTLGPUFamily) {
    MTLGPUFamilyApple1  = 1001,
    MTLGPUFamilyApple2  = 1002,
    MTLGPUFamilyApple3  = 1003,
    MTLGPUFamilyApple4  = 1004,
    MTLGPUFamilyApple5  = 1005,
    MTLGPUFamilyApple6  = 1006,
    MTLGPUFamilyApple7  = 1007,
    MTLGPUFamilyApple8  = 1008,
    MTLGPUFamilyApple9  = 1009,
    MTLGPUFamilyApple10 = 1010,

    MTLGPUFamilyMac1 = 2001,
    MTLGPUFamilyMac2 = 2002,

    MTLGPUFamilyCommon1 = 3001,
    MTLGPUFamilyCommon2 = 3002,
    MTLGPUFamilyCommon3 = 3003,

    MTLGPUFamilyMacCatalyst1 = 4001,
    MTLGPUFamilyMacCatalyst2 = 4002,

    MTLGPUFamilyMetal3 = 5001,
    MTLGPUFamilyMetal4 = 5002,
};

typedef NS_ENUM(NSUInteger, MTLDeviceLocation) {
    MTLDeviceLocationBuiltIn     = 0,
    MTLDeviceLocationSlot        = 1,
    MTLDeviceLocationExternal    = 2,
    MTLDeviceLocationUnspecified = NSUIntegerMax,
};

typedef NS_OPTIONS(NSUInteger, MTLPipelineOption) {
    MTLPipelineOptionNone                    = 0,
    MTLPipelineOptionBindingInfo             = 1 << 0,
    MTLPipelineOptionBufferTypeInfo          = 1 << 1,
    MTLPipelineOptionFailOnBinaryArchiveMiss = 1 << 2,
};

typedef NS_ENUM(NSUInteger, MTLReadWriteTextureTier) {
    MTLReadWriteTextureTierNone = 0,
    MTLReadWriteTextureTier1    = 1,
    MTLReadWriteTextureTier2    = 2,
};

typedef NS_ENUM(NSUInteger, MTLArgumentBuffersTier) {
    MTLArgumentBuffersTier1 = 0,
    MTLArgumentBuffersTier2 = 1,
};

typedef NS_ENUM(NSUInteger, MTLFunctionType) {
    MTLFunctionTypeVertex       = 1,
    MTLFunctionTypeFragment     = 2,
    MTLFunctionTypeKernel       = 3,
    MTLFunctionTypeVisible      = 5,
    MTLFunctionTypeIntersection = 6,
    MTLFunctionTypeMesh         = 7,
    MTLFunctionTypeObject       = 8,
};

typedef NS_OPTIONS(NSUInteger, MTLFunctionOptions) {
    MTLFunctionOptionNone = 0,
    MTLFunctionOptionCompileToBinary = 1 << 0,
};

typedef NS_ENUM(NSUInteger, MTLLanguageVersion) {
    MTLLanguageVersion1_0 = (1 << 16),
    MTLLanguageVersion1_1 = (1 << 16) + 1,
    MTLLanguageVersion1_2 = (1 << 16) + 2,
    MTLLanguageVersion2_0 = (2 << 16),
    MTLLanguageVersion2_1 = (2 << 16) + 1,
    MTLLanguageVersion2_2 = (2 << 16) + 2,
    MTLLanguageVersion2_3 = (2 << 16) + 3,
    MTLLanguageVersion2_4 = (2 << 16) + 4,
    MTLLanguageVersion3_0 = (3 << 16) + 0,
    MTLLanguageVersion3_1 = (3 << 16) + 1,
    MTLLanguageVersion3_2 = (3 << 16) + 2,
    MTLLanguageVersion4_0 = (4 << 16) + 0,
};

typedef NS_ENUM(NSInteger, MTLLibraryType) {
    MTLLibraryTypeExecutable = 0,
    MTLLibraryTypeDynamic    = 1,
};

typedef NS_ENUM(NSInteger, MTLLibraryOptimizationLevel) {
    MTLLibraryOptimizationLevelDefault = 0,
    MTLLibraryOptimizationLevelSize    = 1,
};

typedef NS_ENUM(NSInteger, MTLCompileSymbolVisibility) {
    MTLCompileSymbolVisibilityDefault = 0,
    MTLCompileSymbolVisibilityHidden  = 1,
};

typedef NS_ENUM(NSInteger, MTLMathMode) {
    MTLMathModeSafe    = 0,
    MTLMathModeRelaxed = 1,
    MTLMathModeFast    = 2,
};

typedef NS_ENUM(NSInteger, MTLMathFloatingPointFunctions) {
    MTLMathFloatingPointFunctionsFast    = 0,
    MTLMathFloatingPointFunctionsPrecise = 1,
};

typedef NS_ENUM(NSUInteger, MTLLibraryError) {
    MTLLibraryErrorUnsupported      = 1,
    MTLLibraryErrorInternal         = 2,
    MTLLibraryErrorCompileFailure   = 3,
    MTLLibraryErrorCompileWarning   = 4,
    MTLLibraryErrorFunctionNotFound = 5,
    MTLLibraryErrorFileNotFound     = 6,
};

typedef NS_ENUM(NSUInteger, MTLCommandBufferStatus) {
    MTLCommandBufferStatusNotEnqueued = 0,
    MTLCommandBufferStatusEnqueued    = 1,
    MTLCommandBufferStatusCommitted   = 2,
    MTLCommandBufferStatusScheduled   = 3,
    MTLCommandBufferStatusCompleted   = 4,
    MTLCommandBufferStatusError       = 5,
};

typedef NS_ENUM(NSUInteger, MTLCommandBufferError) {
    MTLCommandBufferErrorNone            = 0,
    MTLCommandBufferErrorInternal        = 1,
    MTLCommandBufferErrorTimeout         = 2,
    MTLCommandBufferErrorPageFault       = 3,
    MTLCommandBufferErrorAccessRevoked   = 4,
    MTLCommandBufferErrorNotPermitted    = 7,
    MTLCommandBufferErrorOutOfMemory     = 8,
    MTLCommandBufferErrorInvalidResource = 9,
    MTLCommandBufferErrorMemoryless      = 10,
    MTLCommandBufferErrorDeviceRemoved   = 11,
    MTLCommandBufferErrorStackOverflow   = 12,
};

typedef NS_OPTIONS(NSUInteger, MTLCommandBufferErrorOption) {
    MTLCommandBufferErrorOptionNone                   = 0,
    MTLCommandBufferErrorOptionEncoderExecutionStatus = 1 << 0,
};

typedef NS_ENUM(NSUInteger, MTLDispatchType) {
    MTLDispatchTypeSerial     = 0,
    MTLDispatchTypeConcurrent = 1,
};

typedef NS_OPTIONS(NSUInteger, MTLResourceUsage) {
    MTLResourceUsageRead   = 1 << 0,
    MTLResourceUsageWrite  = 1 << 1,
    MTLResourceUsageSample = 1 << 2,
};

typedef NS_OPTIONS(NSUInteger, MTLBarrierScope) {
    MTLBarrierScopeBuffers       = 1 << 0,
    MTLBarrierScopeTextures      = 1 << 1,
    MTLBarrierScopeRenderTargets = 1 << 2,
};

typedef NS_OPTIONS(NSUInteger, MTLStages) {
    MTLStageVertex = 1 << 0,
    MTLStageAll    = NSIntegerMax,
};

typedef NS_ENUM(NSUInteger, MTLDataType) {
    MTLDataTypeNone     = 0,
    MTLDataTypeStruct   = 1,
    MTLDataTypeArray    = 2,
    MTLDataTypeFloat    = 3,
    MTLDataTypeFloat2   = 4,
    MTLDataTypeFloat3   = 5,
    MTLDataTypeFloat4   = 6,
    MTLDataTypeFloat2x2 = 7,
    MTLDataTypeFloat2x3 = 8,
    MTLDataTypeFloat2x4 = 9,
    MTLDataTypeFloat3x2 = 10,
    MTLDataTypeFloat3x3 = 11,
    MTLDataTypeFloat3x4 = 12,
    MTLDataTypeFloat4x2 = 13,
    MTLDataTypeFloat4x3 = 14,
    MTLDataTypeFloat4x4 = 15,
    MTLDataTypeHalf     = 16,
    MTLDataTypeHalf2    = 17,
    MTLDataTypeHalf3    = 18,
    MTLDataTypeHalf4    = 19,
    MTLDataTypeHalf2x2  = 20,
    MTLDataTypeHalf2x3  = 21,
    MTLDataTypeHalf2x4  = 22,
    MTLDataTypeHalf3x2  = 23,
    MTLDataTypeHalf3x3  = 24,
    MTLDataTypeHalf3x4  = 25,
    MTLDataTypeHalf4x2  = 26,
    MTLDataTypeHalf4x3  = 27,
    MTLDataTypeHalf4x4  = 28,
    MTLDataTypeInt      = 29,
    MTLDataTypeInt2     = 30,
    MTLDataTypeInt3     = 31,
    MTLDataTypeInt4     = 32,
    MTLDataTypeUInt     = 33,
    MTLDataTypeUInt2    = 34,
    MTLDataTypeUInt3    = 35,
    MTLDataTypeUInt4    = 36,
    MTLDataTypeShort    = 37,
    MTLDataTypeShort2   = 38,
    MTLDataTypeShort3   = 39,
    MTLDataTypeShort4   = 40,
    MTLDataTypeUShort   = 41,
    MTLDataTypeUShort2  = 42,
    MTLDataTypeUShort3  = 43,
    MTLDataTypeUShort4  = 44,
    MTLDataTypeChar     = 45,
    MTLDataTypeChar2    = 46,
    MTLDataTypeChar3    = 47,
    MTLDataTypeChar4    = 48,
    MTLDataTypeUChar    = 49,
    MTLDataTypeUChar2   = 50,
    MTLDataTypeUChar3   = 51,
    MTLDataTypeUChar4   = 52,
    MTLDataTypeBool     = 53,
    MTLDataTypeBool2    = 54,
    MTLDataTypeBool3    = 55,
    MTLDataTypeBool4    = 56,
    MTLDataTypeLong     = 81,
    MTLDataTypeLong2    = 82,
    MTLDataTypeLong3    = 83,
    MTLDataTypeLong4    = 84,
    MTLDataTypeULong    = 85,
    MTLDataTypeULong2   = 86,
    MTLDataTypeULong3   = 87,
    MTLDataTypeULong4   = 88,
    MTLDataTypeBFloat   = 121,
    MTLDataTypeBFloat2  = 122,
    MTLDataTypeBFloat3  = 123,
    MTLDataTypeBFloat4  = 124,
};

typedef void (^MTLDeviceNotificationHandler)(id device, NSString *notifyName);
typedef void (^MTLCommandBufferHandler)(id commandBuffer);
typedef void (^MTLNewLibraryCompletionHandler)(id library, NSError *error);
typedef void (^MTLNewComputePipelineStateCompletionHandler)(id pso, NSError *error);
typedef void (^MTLNewComputePipelineStateWithReflectionCompletionHandler)(id pso, id reflection, NSError *error);
typedef void (^MTLSharedEventNotificationBlock)(id event, uint64_t value);

/* --- Exported C symbols & constants --- */

KMTL_EXPORT extern NSString *const MTLLibraryErrorDomain;
KMTL_EXPORT extern NSString *const MTLCommandBufferErrorDomain;
KMTL_EXPORT extern NSString *const MTLCommandBufferEncoderInfoErrorKey;
KMTL_EXPORT extern NSString *const MTLDeviceErrorDomain;
KMTL_EXPORT extern NSString *const MTLDeviceWasAddedNotification;
KMTL_EXPORT extern NSString *const MTLDeviceRemovalRequestedNotification;
KMTL_EXPORT extern NSString *const MTLDeviceWasRemovedNotification;

KMTL_EXPORT id MTLCreateSystemDefaultDevice(void);
KMTL_EXPORT NSArray *MTLCopyAllDevices(void);
KMTL_EXPORT NSArray *MTLCopyAllDevicesWithObserver(id *observer, MTLDeviceNotificationHandler handler);
KMTL_EXPORT void MTLRemoveDeviceObserver(id observer);

/* --- Public Exported Apple Metal Descriptor & Helper Classes --- */

KMTL_EXPORT
@interface MTLCompileOptions : NSObject <NSCopying> {
    NSDictionary *_preprocessorMacros;
    BOOL _fastMathEnabled;
    MTLMathMode _mathMode;
    MTLMathFloatingPointFunctions _mathFloatingPointFunctions;
    MTLLanguageVersion _languageVersion;
    MTLLibraryType _libraryType;
    NSString *_installName;
    NSArray *_libraries;
    BOOL _preserveInvariance;
    MTLLibraryOptimizationLevel _optimizationLevel;
    MTLCompileSymbolVisibility _compileSymbolVisibility;
    BOOL _allowReferencingUndefinedSymbols;
    NSUInteger _maxTotalThreadsPerThreadgroup;
    MTLSize _requiredThreadsPerThreadgroup;
    BOOL _enableLogging;
}
@property (nonatomic, copy) NSDictionary *preprocessorMacros;
@property (nonatomic) BOOL fastMathEnabled;
@property (nonatomic) MTLMathMode mathMode;
@property (nonatomic) MTLMathFloatingPointFunctions mathFloatingPointFunctions;
@property (nonatomic) MTLLanguageVersion languageVersion;
@property (nonatomic) MTLLibraryType libraryType;
@property (nonatomic, copy) NSString *installName;
@property (nonatomic, copy) NSArray *libraries;
@property (nonatomic) BOOL preserveInvariance;
@property (nonatomic) MTLLibraryOptimizationLevel optimizationLevel;
@property (nonatomic) MTLCompileSymbolVisibility compileSymbolVisibility;
@property (nonatomic) BOOL allowReferencingUndefinedSymbols;
@property (nonatomic) NSUInteger maxTotalThreadsPerThreadgroup;
@property (nonatomic) MTLSize requiredThreadsPerThreadgroup;
@property (nonatomic) BOOL enableLogging;
@end

typedef struct {
    uint32_t index;
    uint32_t dataType;
    uint32_t valueLen;
    uint8_t value[64];
} KidoFuncConstEntry;

KMTL_EXPORT
@interface MTLFunctionConstantValues : NSObject <NSCopying> {
    KidoFuncConstEntry *_entries;
    uint32_t _count;
    uint32_t _cap;
}
- (void)setConstantValue:(const void *)value type:(MTLDataType)type atIndex:(NSUInteger)index;
- (void)setConstantValues:(const void *)values type:(MTLDataType)type withRange:(NSRange)range;
- (void)setConstantValue:(const void *)value type:(MTLDataType)type withName:(NSString *)name;
- (void)reset;
- (uint32_t)kidoConstantCount;
- (const KidoFuncConstEntry *)kidoConstantEntries;
@end

KMTL_EXPORT
@interface MTLComputePipelineDescriptor : NSObject <NSCopying> {
    NSString *_label;
    id _computeFunction;
    BOOL _threadGroupSizeIsMultipleOfThreadExecutionWidth;
    NSUInteger _maxTotalThreadsPerThreadgroup;
    BOOL _supportIndirectCommandBuffers;
    NSUInteger _maxCallStackDepth;
    MTLSize _requiredThreadsPerThreadgroup;
}
@property (nonatomic, copy) NSString *label;
@property (nonatomic, retain) id computeFunction;
@property (nonatomic) BOOL threadGroupSizeIsMultipleOfThreadExecutionWidth;
@property (nonatomic) NSUInteger maxTotalThreadsPerThreadgroup;
@property (nonatomic) BOOL supportIndirectCommandBuffers;
@property (nonatomic) NSUInteger maxCallStackDepth;
@property (nonatomic) MTLSize requiredThreadsPerThreadgroup;
- (void)reset;
@end

KMTL_EXPORT
@interface MTLCommandBufferDescriptor : NSObject <NSCopying> {
    BOOL _retainedReferences;
    MTLCommandBufferErrorOption _errorOptions;
    id _logState;
}
@property (nonatomic) BOOL retainedReferences;
@property (nonatomic) MTLCommandBufferErrorOption errorOptions;
@property (nonatomic, retain) id logState;
@end

KMTL_EXPORT
@interface MTLCommandQueueDescriptor : NSObject <NSCopying> {
    NSUInteger _maxCommandBufferCount;
    id _logState;
}
@property (nonatomic) NSUInteger maxCommandBufferCount;
@property (nonatomic, retain) id logState;
@end

KMTL_EXPORT
@interface MTLFunctionDescriptor : NSObject <NSCopying> {
    NSString *_name;
    NSString *_specializedName;
    MTLFunctionConstantValues *_constantValues;
    MTLFunctionOptions _options;
    NSArray *_binaryArchives;
}
+ (MTLFunctionDescriptor *)functionDescriptor;
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *specializedName;
@property (nonatomic, copy) MTLFunctionConstantValues *constantValues;
@property (nonatomic) MTLFunctionOptions options;
@property (nonatomic, copy) NSArray *binaryArchives;
@end

KMTL_EXPORT
@interface MTLIntersectionFunctionDescriptor : MTLFunctionDescriptor
@end

KMTL_EXPORT
@interface MTLComputePassDescriptor : NSObject <NSCopying> {
    MTLDispatchType _dispatchType;
}
+ (MTLComputePassDescriptor *)computePassDescriptor;
@property (nonatomic) MTLDispatchType dispatchType;
@end

KMTL_EXPORT
@interface MTLBlitPassDescriptor : NSObject <NSCopying>
+ (MTLBlitPassDescriptor *)blitPassDescriptor;
@end

KMTL_EXPORT
@interface MTLArchitecture : NSObject <NSCopying> {
    NSString *_name;
}
- (instancetype)initWithName:(NSString *)name;
@property (readonly) NSString *name;
@end

KMTL_EXPORT
@interface MTLSharedEventListener : NSObject {
    dispatch_queue_t _dispatchQueue;
}
- (instancetype)init;
- (instancetype)initWithDispatchQueue:(dispatch_queue_t)dispatchQueue;
@property (readonly) dispatch_queue_t dispatchQueue;
+ (MTLSharedEventListener *)sharedListener;
@end

KMTL_EXPORT
@interface MTLSharedEventHandle : NSObject {
    NSString *_label;
}
@property (readonly) NSString *label;
@end

KMTL_EXPORT
@interface MTLComputePipelineReflection : NSObject {
    NSArray *_bindings;
}
@property (readonly) NSArray *bindings;
@property (readonly) NSArray *arguments;
@end

KMTL_EXPORT
@interface MTLArgumentDescriptor : NSObject <NSCopying> {
    MTLDataType _dataType;
    NSUInteger _index;
    NSUInteger _arrayLength;
    NSUInteger _access;
    NSUInteger _textureType;
    NSUInteger _constantBlockAlignment;
}
+ (MTLArgumentDescriptor *)argumentDescriptor;
@property (nonatomic) MTLDataType dataType;
@property (nonatomic) NSUInteger index;
@property (nonatomic) NSUInteger arrayLength;
@property (nonatomic) NSUInteger access;
@property (nonatomic) NSUInteger textureType;
@property (nonatomic) NSUInteger constantBlockAlignment;
@end

KMTL_EXPORT
@interface MTLHeapDescriptor : NSObject <NSCopying> {
    NSUInteger _size;
    MTLStorageMode _storageMode;
    MTLCPUCacheMode _cpuCacheMode;
    MTLHazardTrackingMode _hazardTrackingMode;
    MTLResourceOptions _resourceOptions;
    NSInteger _type;
}
@property (nonatomic) NSUInteger size;
@property (nonatomic) MTLStorageMode storageMode;
@property (nonatomic) MTLCPUCacheMode cpuCacheMode;
@property (nonatomic) MTLHazardTrackingMode hazardTrackingMode;
@property (nonatomic) MTLResourceOptions resourceOptions;
@property (nonatomic) NSInteger type;
@end

/* --- Apple Metal Protocol Declarations (for conformsToProtocol:) --- */

@protocol MTLAllocation <NSObject>
@end

@protocol MTLResource <MTLAllocation>
@end

@protocol MTLBuffer <MTLResource>
@end

@protocol MTLLibrary <NSObject>
@end

@protocol MTLFunction <NSObject>
@end

@protocol MTLComputePipelineState <MTLAllocation>
@end

@protocol MTLCommandQueue <NSObject>
@end

@protocol MTLCommandBuffer <NSObject>
@end

@protocol MTLCommandEncoder <NSObject>
@end

@protocol MTLComputeCommandEncoder <MTLCommandEncoder>
@end

@protocol MTLBlitCommandEncoder <MTLCommandEncoder>
@end

@protocol MTLEvent <NSObject>
@end

@protocol MTLSharedEvent <MTLEvent>
@end

@protocol MTLFence <NSObject>
@end

@protocol MTLDevice <NSObject>
@end

/* --- Internal Concrete Classes --- */

@class KidoMTLBuffer;
@class KidoMTLLibrary;
@class KidoMTLFunction;
@class KidoMTLComputePipelineState;
@class KidoMTLCommandQueue;
@class KidoMTLCommandBuffer;

@interface KidoMTLDevice : NSObject <MTLDevice> {
    io_connect_t _connect;
    struct kmtl_device_info _info;
    NSString *_name;
    MTLArchitecture *_architecture;
    uint64_t _currentAllocatedSize;
    BOOL _shouldMaximizeConcurrentCompilation;
}
- (instancetype)initWithConnect:(io_connect_t)connect info:(const struct kmtl_device_info *)info;
- (io_connect_t)ioConnect;
- (const struct kmtl_device_info *)deviceInfo;
- (kern_return_t)rpc:(struct kmtl_req_hdr *)req
               inBuf:(const void *)inBuf
               inLen:(uint32_t)inLen
                resp:(struct kmtl_resp_hdr *)resp
              outBuf:(void *)outBuf
              outCap:(uint32_t)outCap;
- (BOOL)mapBufferHandle:(uint64_t)handle outAddr:(mach_vm_address_t *)outAddr outSize:(mach_vm_size_t *)outSize;
- (void)unmapBufferHandle:(uint64_t)handle;
- (void)adjustAllocatedSize:(int64_t)delta;
- (BOOL)supportsBFloat16;
@end

KMTL_EXPORT
@interface MTLResidencySetDescriptor : NSObject <NSCopying> {
    NSString *_label;
    NSUInteger _initialCapacity;
}
@property (nonatomic, copy) NSString *label;
@property (nonatomic) NSUInteger initialCapacity;
@end

KMTL_EXPORT
@interface MTLCaptureDescriptor : NSObject <NSCopying> {
    id _captureObject;
    NSInteger _destination;
    NSURL *_outputURL;
}
@property (nonatomic, retain) id captureObject;
@property (nonatomic) NSInteger destination;
@property (nonatomic, copy) NSURL *outputURL;
@end

KMTL_EXPORT
@interface MTLCaptureManager : NSObject {
    BOOL _isCapturing;
}
+ (MTLCaptureManager *)sharedCaptureManager;
- (BOOL)supportsDestination:(NSInteger)destination;
- (BOOL)startCaptureWithDescriptor:(MTLCaptureDescriptor *)descriptor error:(NSError **)error;
- (void)startCaptureWithDevice:(id)device;
- (void)startCaptureWithCommandQueue:(id)commandQueue;
- (void)stopCapture;
@property (nonatomic, readonly) BOOL isCapturing;
@end

@interface KidoMTLResidencySet : NSObject {
    KidoMTLDevice *_device;
    NSString *_label;
    NSMutableArray *_allocations;
}
- (instancetype)initWithDevice:(KidoMTLDevice *)device descriptor:(MTLResidencySetDescriptor *)desc;
@end

@interface KidoMTLBuffer : NSObject <MTLBuffer> {
    KidoMTLDevice *_device;
    uint64_t _handle;
    uint64_t _shmOffset;
    uint64_t _alignedLen;
    NSUInteger _length;
    mach_vm_address_t _mappedAddr;
    mach_vm_size_t _mappedSize;
    MTLResourceOptions _options;
    NSString *_label;
    void *_noCopyPtr;
    NSUInteger _noCopyLen;
    BOOL _noCopyInPlace;
    BOOL _noCopyWritable;
    void (^_deallocator)(void *, NSUInteger);
}
- (instancetype)initWithDevice:(KidoMTLDevice *)device
                        handle:(uint64_t)handle
                     shmOffset:(uint64_t)shmOffset
                    alignedLen:(uint64_t)alignedLen
                        length:(NSUInteger)length
                    mappedAddr:(mach_vm_address_t)mappedAddr
                    mappedSize:(mach_vm_size_t)mappedSize
                       options:(MTLResourceOptions)options;
- (void)setNoCopyPointer:(void *)ptr length:(NSUInteger)len inPlace:(BOOL)inPlace writable:(BOOL)writable deallocator:(void (^)(void *, NSUInteger))deallocator;
- (BOOL)hasNoCopyPointer;
- (void)syncFromNoCopy;
- (void)syncToNoCopyOffset:(NSUInteger)offset length:(NSUInteger)len;
- (uint64_t)kidoHandle;
- (void *)contents;
- (NSUInteger)length;
@end

@interface KidoMTLLibrary : NSObject <MTLLibrary> {
    KidoMTLDevice *_device;
    uint64_t _handle;
    NSString *_label;
    MTLLibraryType _type;
    NSString *_installName;
    NSArray *_functionNames;
}
- (instancetype)initWithDevice:(KidoMTLDevice *)device handle:(uint64_t)handle functionNames:(NSArray *)functionNames;
- (uint64_t)kidoHandle;
@end

@interface KidoMTLFunction : NSObject <MTLFunction> {
    KidoMTLDevice *_device;
    uint64_t _handle;
    NSString *_name;
    NSString *_label;
    MTLFunctionType _functionType;
    MTLFunctionOptions _options;
}
- (instancetype)initWithDevice:(KidoMTLDevice *)device handle:(uint64_t)handle name:(NSString *)name;
- (uint64_t)kidoHandle;
@end

@interface KidoMTLComputePipelineState : NSObject <MTLComputePipelineState> {
    KidoMTLDevice *_device;
    uint64_t _handle;
    NSString *_label;
    NSUInteger _maxTotalThreadsPerThreadgroup;
    NSUInteger _threadExecutionWidth;
    NSUInteger _staticThreadgroupMemoryLength;
}
- (instancetype)initWithDevice:(KidoMTLDevice *)device
                        handle:(uint64_t)handle
                    maxThreads:(NSUInteger)maxThreads
                     execWidth:(NSUInteger)execWidth
                   staticTgMem:(NSUInteger)staticTgMem
                         label:(NSString *)label;
- (uint64_t)kidoHandle;
@end

typedef struct {
    KidoMTLCommandBuffer *cb;
    BOOL ready;
    BOOL waitingOnEvent;
} KidoPendingCommandBuffer;

@interface KidoMTLCommandQueue : NSObject <MTLCommandQueue> {
    KidoMTLDevice *_device;
    uint64_t _handle;
    NSString *_label;
    dispatch_queue_t _dispatchQueue;
    pthread_mutex_t _submitLock;
    pthread_cond_t _submitCond;
    KidoPendingCommandBuffer *_pending;
    size_t _pendingCount;
    size_t _pendingCap;
    uint64_t _completedSeq;
}
- (instancetype)initWithDevice:(KidoMTLDevice *)device handle:(uint64_t)handle;
- (uint64_t)kidoHandle;
- (dispatch_queue_t)kidoDispatchQueue;
- (uint64_t)kidoCompletedSeq;
- (void)kidoAdvanceCompletedSeq:(uint64_t)seq;
- (void)kidoEnqueueCommandBuffer:(KidoMTLCommandBuffer *)cb;
- (void)kidoRemoveUncommittedCommandBuffer:(KidoMTLCommandBuffer *)cb;
- (void)kidoCommitCommandBuffer:(KidoMTLCommandBuffer *)cb;
- (void)kidoWaitUntilSubmitted:(KidoMTLCommandBuffer *)cb;
- (void)kidoResumeAfterEventForCommandBuffer:(KidoMTLCommandBuffer *)cb;
@end

@interface KidoMTLCommandBuffer : NSObject <MTLCommandBuffer> {
    KidoMTLCommandQueue *_queue;
    KidoMTLDevice *_device;
    NSString *_label;
    BOOL _retainedReferences;
    MTLCommandBufferErrorOption _errorOptions;
    MTLCommandBufferStatus _status;
    NSError *_error;
    double _kernelStartTime;
    double _kernelEndTime;
    double _gpuStartTime;
    double _gpuEndTime;

    uint8_t *_stream;
    size_t _streamLen;
    size_t _streamCap;
    BOOL _streamOverflow;

    NSMutableArray *_retainedObjects;
    NSMutableArray *_syncInBuffers;
    NSMutableArray *_writeBackOps;
    NSMutableArray *_waitEvents;
    NSMutableArray *_signalEvents;
    MTLCommandBufferHandler *_scheduledHandlers;
    size_t _scheduledCount;
    size_t _scheduledCap;
    MTLCommandBufferHandler *_completedHandlers;
    size_t _completedCount;
    size_t _completedCap;

    pthread_mutex_t _lock;
    dispatch_group_t _doneGroup;
    BOOL _enqueued;
    BOOL _committed;
    BOOL _submittedToHost;
    BOOL _submitFailed;
    BOOL _asyncWaitStarted;
    uint64_t _commitSeq;
}
- (instancetype)initWithQueue:(KidoMTLCommandQueue *)queue
           retainedReferences:(BOOL)retainedReferences
                 errorOptions:(MTLCommandBufferErrorOption)errorOptions;
- (void)kidoAppendBytes:(const void *)bytes length:(size_t)len;
- (void)kidoAppendCommand:(const void *)cmd length:(size_t)len;
- (void)kidoAppendCommand:(const void *)cmd headerLen:(size_t)hdrLen extra:(const void *)extra extraLen:(size_t)extraLen;
- (void)kidoRetainObject:(id)obj;
- (void)kidoRecordSyncIn:(KidoMTLBuffer *)buf;
- (void)kidoRecordWriteBack:(KidoMTLBuffer *)buf offset:(NSUInteger)offset length:(NSUInteger)length;
- (BOOL)kidoMarkEnqueuedLocked;
- (BOOL)kidoMarkCommittedLocked;
- (BOOL)kidoPeekNextWaitEvent:(id *)outEvent value:(uint64_t *)outValue;
- (BOOL)kidoSubmitOnHostAndCheckNeedsAsyncWait:(uint64_t *)outSeq;
- (void)kidoStartAsyncWaitWithSeq:(uint64_t)seq;
- (BOOL)kidoIsSubmittedToHost;
@end

@interface KidoMTLComputeCommandEncoder : NSObject <MTLComputeCommandEncoder> {
    KidoMTLCommandBuffer *_cmdBuf;
    KidoMTLDevice *_device;
    NSString *_label;
    MTLDispatchType _dispatchType;
    BOOL _ended;
}
- (instancetype)initWithCommandBuffer:(KidoMTLCommandBuffer *)cmdBuf dispatchType:(MTLDispatchType)dispatchType;
@end

@interface KidoMTLBlitCommandEncoder : NSObject <MTLBlitCommandEncoder> {
    KidoMTLCommandBuffer *_cmdBuf;
    KidoMTLDevice *_device;
    NSString *_label;
    BOOL _ended;
}
- (instancetype)initWithCommandBuffer:(KidoMTLCommandBuffer *)cmdBuf;
@end

@interface KidoMTLEvent : NSObject <MTLEvent> {
    KidoMTLDevice *_device;
    NSString *_label;
}
- (instancetype)initWithDevice:(KidoMTLDevice *)device;
@end

typedef struct {
    uint64_t value;
    dispatch_queue_t queue;
    MTLSharedEventNotificationBlock block;
} KidoSharedEventWaiter;

@interface KidoMTLSharedEvent : KidoMTLEvent <MTLSharedEvent> {
    pthread_mutex_t _lock;
    pthread_cond_t _cond;
    uint64_t _signaledValue;
    KidoSharedEventWaiter *_waiters;
    size_t _waiterCount;
    size_t _waiterCap;
}
@property (readwrite) uint64_t signaledValue;
- (BOOL)waitUntilSignaledValue:(uint64_t)value timeoutMS:(uint64_t)milliseconds;
- (void)notifyListener:(MTLSharedEventListener *)listener atValue:(uint64_t)value block:(MTLSharedEventNotificationBlock)block;
- (MTLSharedEventHandle *)newSharedEventHandle;
@end

@interface KidoMTLFence : NSObject <MTLFence> {
    KidoMTLDevice *_device;
    NSString *_label;
}
- (instancetype)initWithDevice:(KidoMTLDevice *)device;
@end

__attribute__((visibility("hidden")))
NSError *KidoMakeError(NSString *domain, NSInteger code, const char *utf8, uint32_t len, NSString *fallback);
__attribute__((visibility("hidden")))
uint32_t KidoMTLDataTypeSize(MTLDataType type);

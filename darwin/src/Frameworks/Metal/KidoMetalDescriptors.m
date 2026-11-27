/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#import "KidoMetalInternal.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

NSString *const MTLLibraryErrorDomain = @"MTLLibraryErrorDomain";
NSString *const MTLCommandBufferErrorDomain = @"MTLCommandBufferErrorDomain";
NSString *const MTLCommandBufferEncoderInfoErrorKey = @"MTLCommandBufferEncoderInfoErrorKey";
NSString *const MTLDeviceErrorDomain = @"MTLDeviceErrorDomain";
NSString *const MTLDeviceWasAddedNotification = @"MTLDeviceWasAddedNotification";
NSString *const MTLDeviceRemovalRequestedNotification = @"MTLDeviceRemovalRequestedNotification";
NSString *const MTLDeviceWasRemovedNotification = @"MTLDeviceWasRemovedNotification";

NSError *
KidoMakeError(NSString *domain, NSInteger code, const char *utf8, uint32_t len, NSString *fallback)
{
    NSString *msg = nil;
    if (utf8 && len > 0) {
        msg = [[[NSString alloc] initWithBytes:utf8 length:len encoding:NSUTF8StringEncoding] autorelease];
    }
    if (!msg || [msg length] == 0)
        msg = fallback ?: @"Metal operation failed";
    NSDictionary *info = [NSDictionary dictionaryWithObject:msg forKey:NSLocalizedDescriptionKey];
    return [NSError errorWithDomain:domain code:code userInfo:info];
}

uint32_t
KidoMTLDataTypeSize(MTLDataType type)
{
    switch (type) {
    case MTLDataTypeBool:   case MTLDataTypeChar:   case MTLDataTypeUChar:   return 1;
    case MTLDataTypeBool2:  case MTLDataTypeChar2:  case MTLDataTypeUChar2:  return 2;
    case MTLDataTypeBool3:  case MTLDataTypeChar3:  case MTLDataTypeUChar3:  return 4;
    case MTLDataTypeBool4:  case MTLDataTypeChar4:  case MTLDataTypeUChar4:  return 4;

    case MTLDataTypeShort:  case MTLDataTypeUShort: case MTLDataTypeHalf:   case MTLDataTypeBFloat:  return 2;
    case MTLDataTypeShort2: case MTLDataTypeUShort2: case MTLDataTypeHalf2: case MTLDataTypeBFloat2: return 4;
    case MTLDataTypeShort3: case MTLDataTypeUShort3: case MTLDataTypeHalf3: case MTLDataTypeBFloat3: return 8;
    case MTLDataTypeShort4: case MTLDataTypeUShort4: case MTLDataTypeHalf4: case MTLDataTypeBFloat4: return 8;

    case MTLDataTypeInt:    case MTLDataTypeUInt:   case MTLDataTypeFloat:   return 4;
    case MTLDataTypeInt2:   case MTLDataTypeUInt2:  case MTLDataTypeFloat2:  return 8;
    case MTLDataTypeInt3:   case MTLDataTypeUInt3:  case MTLDataTypeFloat3:  return 16;
    case MTLDataTypeInt4:   case MTLDataTypeUInt4:  case MTLDataTypeFloat4:  return 16;

    case MTLDataTypeLong:   case MTLDataTypeULong:  return 8;
    case MTLDataTypeLong2:  case MTLDataTypeULong2: return 16;
    case MTLDataTypeLong3:  case MTLDataTypeULong3: return 32;
    case MTLDataTypeLong4:  case MTLDataTypeULong4: return 32;

    case MTLDataTypeHalf2x2:  return 8;
    case MTLDataTypeHalf2x3:  return 16;
    case MTLDataTypeHalf2x4:  return 16;
    case MTLDataTypeHalf3x2:  return 12;
    case MTLDataTypeHalf3x3:  return 24;
    case MTLDataTypeHalf3x4:  return 24;
    case MTLDataTypeHalf4x2:  return 16;
    case MTLDataTypeHalf4x3:  return 32;
    case MTLDataTypeHalf4x4:  return 32;

    case MTLDataTypeFloat2x2: return 16;
    case MTLDataTypeFloat2x3: return 32;
    case MTLDataTypeFloat2x4: return 32;
    case MTLDataTypeFloat3x2: return 24;
    case MTLDataTypeFloat3x3: return 48;
    case MTLDataTypeFloat3x4: return 48;
    case MTLDataTypeFloat4x2: return 32;
    case MTLDataTypeFloat4x3: return 64;
    case MTLDataTypeFloat4x4: return 64;
    default: return 0;
    }
}

/* --- MTLCompileOptions --- */

@implementation MTLCompileOptions

@synthesize preprocessorMacros = _preprocessorMacros;
@synthesize mathFloatingPointFunctions = _mathFloatingPointFunctions;
@synthesize languageVersion = _languageVersion;
@synthesize libraryType = _libraryType;
@synthesize installName = _installName;
@synthesize libraries = _libraries;
@synthesize preserveInvariance = _preserveInvariance;
@synthesize optimizationLevel = _optimizationLevel;
@synthesize compileSymbolVisibility = _compileSymbolVisibility;
@synthesize allowReferencingUndefinedSymbols = _allowReferencingUndefinedSymbols;
@synthesize maxTotalThreadsPerThreadgroup = _maxTotalThreadsPerThreadgroup;
@synthesize requiredThreadsPerThreadgroup = _requiredThreadsPerThreadgroup;
@synthesize enableLogging = _enableLogging;

- (instancetype)init
{
    self = [super init];
    if (self) {
        _fastMathEnabled = YES;
        _mathMode = MTLMathModeFast;
        _mathFloatingPointFunctions = MTLMathFloatingPointFunctionsFast;
        _languageVersion = 0;
        _libraryType = MTLLibraryTypeExecutable;
    }
    return self;
}

- (void)dealloc
{
    [_preprocessorMacros release];
    [_installName release];
    [_libraries release];
    [super dealloc];
}

- (BOOL)fastMathEnabled
{
    return _fastMathEnabled;
}

- (void)setFastMathEnabled:(BOOL)enabled
{
    _fastMathEnabled = enabled;
    _mathMode = enabled ? MTLMathModeFast : MTLMathModeSafe;
}

- (MTLMathMode)mathMode
{
    return _mathMode;
}

- (void)setMathMode:(MTLMathMode)mode
{
    _mathMode = mode;
    _fastMathEnabled = (mode != MTLMathModeSafe);
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLCompileOptions *c = [[MTLCompileOptions allocWithZone:zone] init];
    c.preprocessorMacros = _preprocessorMacros;
    c->_fastMathEnabled = _fastMathEnabled;
    c->_mathMode = _mathMode;
    c->_mathFloatingPointFunctions = _mathFloatingPointFunctions;
    c->_languageVersion = _languageVersion;
    c->_libraryType = _libraryType;
    c.installName = _installName;
    c.libraries = _libraries;
    c->_preserveInvariance = _preserveInvariance;
    c->_optimizationLevel = _optimizationLevel;
    c->_compileSymbolVisibility = _compileSymbolVisibility;
    c->_allowReferencingUndefinedSymbols = _allowReferencingUndefinedSymbols;
    c->_maxTotalThreadsPerThreadgroup = _maxTotalThreadsPerThreadgroup;
    c->_requiredThreadsPerThreadgroup = _requiredThreadsPerThreadgroup;
    c->_enableLogging = _enableLogging;
    return c;
}

@end

/* --- MTLFunctionConstantValues --- */

@implementation MTLFunctionConstantValues

- (void)dealloc
{
    free(_entries);
    _entries = NULL;
    [super dealloc];
}

- (void)setConstantValue:(const void *)value type:(MTLDataType)type atIndex:(NSUInteger)index
{
    if (!value || index > UINT32_MAX)
        return;
    uint32_t sz = KidoMTLDataTypeSize(type);
    if (sz == 0 || sz > sizeof(_entries[0].value))
        return;

    for (uint32_t i = 0; i < _count; i++) {
        if (_entries[i].index == (uint32_t)index) {
            _entries[i].dataType = (uint32_t)type;
            _entries[i].valueLen = sz;
            memset(_entries[i].value, 0, sizeof(_entries[i].value));
            memcpy(_entries[i].value, value, sz);
            return;
        }
    }

    if (_count >= 65536u)
        return;
    if (_count == _cap) {
        uint32_t newCap = _cap ? (_cap * 2u) : 16u;
        if (newCap < _cap || newCap > 65536u)
            newCap = 65536u;
        KidoFuncConstEntry *ne = (KidoFuncConstEntry *)realloc(_entries, sizeof(KidoFuncConstEntry) * (size_t)newCap);
        if (!ne)
            return;
        _entries = ne;
        _cap = newCap;
    }

    KidoFuncConstEntry *e = &_entries[_count++];
    memset(e, 0, sizeof(*e));
    e->index = (uint32_t)index;
    e->dataType = (uint32_t)type;
    e->valueLen = sz;
    memcpy(e->value, value, sz);
}

- (void)setConstantValues:(const void *)values type:(MTLDataType)type withRange:(NSRange)range
{
    if (!values || range.length == 0)
        return;
    uint32_t sz = KidoMTLDataTypeSize(type);
    if (sz == 0)
        return;
    const uint8_t *p = (const uint8_t *)values;
    for (NSUInteger i = 0; i < range.length; i++) {
        [self setConstantValue:(p + i * sz) type:type atIndex:(range.location + i)];
    }
}

- (void)setConstantValue:(const void *)value type:(MTLDataType)type withName:(NSString *)name
{
    /* Named constants are not yet wired in M0 protocol; safely ignore. */
    (void)value;
    (void)type;
    (void)name;
}

- (void)reset
{
    _count = 0;
}

- (uint32_t)kidoConstantCount
{
    return _count;
}

- (const KidoFuncConstEntry *)kidoConstantEntries
{
    return _entries;
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLFunctionConstantValues *c = [[MTLFunctionConstantValues allocWithZone:zone] init];
    if (_count > 0) {
        c->_entries = (KidoFuncConstEntry *)malloc(sizeof(KidoFuncConstEntry) * _count);
        if (c->_entries) {
            memcpy(c->_entries, _entries, sizeof(KidoFuncConstEntry) * _count);
            c->_count = _count;
            c->_cap = _count;
        }
    }
    return c;
}

@end

/* --- MTLComputePipelineDescriptor --- */

@implementation MTLComputePipelineDescriptor

@synthesize label = _label;
@synthesize computeFunction = _computeFunction;
@synthesize threadGroupSizeIsMultipleOfThreadExecutionWidth = _threadGroupSizeIsMultipleOfThreadExecutionWidth;
@synthesize maxTotalThreadsPerThreadgroup = _maxTotalThreadsPerThreadgroup;
@synthesize supportIndirectCommandBuffers = _supportIndirectCommandBuffers;
@synthesize maxCallStackDepth = _maxCallStackDepth;
@synthesize requiredThreadsPerThreadgroup = _requiredThreadsPerThreadgroup;

- (instancetype)init
{
    self = [super init];
    if (self) {
        _maxCallStackDepth = 1;
    }
    return self;
}

- (void)dealloc
{
    [_label release];
    [_computeFunction release];
    [super dealloc];
}

- (void)reset
{
    [_label release];
    _label = nil;
    [_computeFunction release];
    _computeFunction = nil;
    _threadGroupSizeIsMultipleOfThreadExecutionWidth = NO;
    _maxTotalThreadsPerThreadgroup = 0;
    _supportIndirectCommandBuffers = NO;
    _maxCallStackDepth = 1;
    _requiredThreadsPerThreadgroup.width = 0;
    _requiredThreadsPerThreadgroup.height = 0;
    _requiredThreadsPerThreadgroup.depth = 0;
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLComputePipelineDescriptor *c = [[MTLComputePipelineDescriptor allocWithZone:zone] init];
    c.label = _label;
    c.computeFunction = _computeFunction;
    c.threadGroupSizeIsMultipleOfThreadExecutionWidth = _threadGroupSizeIsMultipleOfThreadExecutionWidth;
    c.maxTotalThreadsPerThreadgroup = _maxTotalThreadsPerThreadgroup;
    c.supportIndirectCommandBuffers = _supportIndirectCommandBuffers;
    c.maxCallStackDepth = _maxCallStackDepth;
    c.requiredThreadsPerThreadgroup = _requiredThreadsPerThreadgroup;
    return c;
}

@end

/* --- MTLCommandBufferDescriptor --- */

@implementation MTLCommandBufferDescriptor

@synthesize retainedReferences = _retainedReferences;
@synthesize errorOptions = _errorOptions;
@synthesize logState = _logState;

- (instancetype)init
{
    self = [super init];
    if (self) {
        _retainedReferences = YES;
        _errorOptions = MTLCommandBufferErrorOptionNone;
    }
    return self;
}

- (void)dealloc
{
    [_logState release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLCommandBufferDescriptor *c = [[MTLCommandBufferDescriptor allocWithZone:zone] init];
    c.retainedReferences = _retainedReferences;
    c.errorOptions = _errorOptions;
    c.logState = _logState;
    return c;
}

@end

/* --- MTLCommandQueueDescriptor --- */

@implementation MTLCommandQueueDescriptor

@synthesize maxCommandBufferCount = _maxCommandBufferCount;
@synthesize logState = _logState;

- (instancetype)init
{
    self = [super init];
    if (self) {
        _maxCommandBufferCount = 64;
    }
    return self;
}

- (void)dealloc
{
    [_logState release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLCommandQueueDescriptor *c = [[MTLCommandQueueDescriptor allocWithZone:zone] init];
    c.maxCommandBufferCount = _maxCommandBufferCount;
    c.logState = _logState;
    return c;
}

@end

/* --- MTLFunctionDescriptor --- */

@implementation MTLFunctionDescriptor

@synthesize name = _name;
@synthesize specializedName = _specializedName;
@synthesize constantValues = _constantValues;
@synthesize options = _options;
@synthesize binaryArchives = _binaryArchives;

+ (MTLFunctionDescriptor *)functionDescriptor
{
    return [[[self alloc] init] autorelease];
}

- (void)dealloc
{
    [_name release];
    [_specializedName release];
    [_constantValues release];
    [_binaryArchives release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLFunctionDescriptor *c = [[[self class] allocWithZone:zone] init];
    c.name = _name;
    c.specializedName = _specializedName;
    c.constantValues = _constantValues;
    c.options = _options;
    c.binaryArchives = _binaryArchives;
    return c;
}

@end

@implementation MTLIntersectionFunctionDescriptor
@end

/* --- MTLComputePassDescriptor & MTLBlitPassDescriptor --- */

@implementation MTLComputePassDescriptor

@synthesize dispatchType = _dispatchType;

+ (MTLComputePassDescriptor *)computePassDescriptor
{
    return [[[self alloc] init] autorelease];
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLComputePassDescriptor *c = [[MTLComputePassDescriptor allocWithZone:zone] init];
    c.dispatchType = _dispatchType;
    return c;
}

@end

@implementation MTLBlitPassDescriptor

+ (MTLBlitPassDescriptor *)blitPassDescriptor
{
    return [[[self alloc] init] autorelease];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [[MTLBlitPassDescriptor allocWithZone:zone] init];
}

@end

/* --- MTLArchitecture --- */

@implementation MTLArchitecture

@synthesize name = _name;

- (instancetype)initWithName:(NSString *)name
{
    self = [super init];
    if (self) {
        _name = [name copy];
    }
    return self;
}

- (void)dealloc
{
    [_name release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [[MTLArchitecture allocWithZone:zone] initWithName:_name];
}

@end

/* --- MTLSharedEventListener & MTLSharedEventHandle --- */

@implementation MTLSharedEventListener

- (instancetype)init
{
    dispatch_queue_t q = dispatch_queue_create("sh.kido.metal.shared-event", DISPATCH_QUEUE_SERIAL);
    self = [self initWithDispatchQueue:q];
    if (q)
        dispatch_release(q);
    return self;
}

- (instancetype)initWithDispatchQueue:(dispatch_queue_t)dispatchQueue
{
    self = [super init];
    if (self) {
        if (dispatchQueue) {
            dispatch_retain(dispatchQueue);
            _dispatchQueue = dispatchQueue;
        } else {
            _dispatchQueue = dispatch_queue_create("sh.kido.metal.shared-event", DISPATCH_QUEUE_SERIAL);
        }
    }
    return self;
}

- (void)dealloc
{
    if (_dispatchQueue)
        dispatch_release(_dispatchQueue);
    [super dealloc];
}

- (dispatch_queue_t)dispatchQueue
{
    return _dispatchQueue;
}

+ (MTLSharedEventListener *)sharedListener
{
    static MTLSharedEventListener *sShared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sShared = [[MTLSharedEventListener alloc] init];
    });
    return sShared;
}

@end

@implementation MTLSharedEventHandle

@synthesize label = _label;

- (void)dealloc
{
    [_label release];
    [super dealloc];
}

@end

/* --- MTLComputePipelineReflection --- */

@implementation MTLComputePipelineReflection

- (instancetype)init
{
    self = [super init];
    if (self) {
        _bindings = [[NSArray alloc] init];
    }
    return self;
}

- (void)dealloc
{
    [_bindings release];
    [super dealloc];
}

- (NSArray *)bindings
{
    return _bindings;
}

- (NSArray *)arguments
{
    return _bindings;
}

@end

/* --- MTLArgumentDescriptor --- */

@implementation MTLArgumentDescriptor

@synthesize dataType = _dataType;
@synthesize index = _index;
@synthesize arrayLength = _arrayLength;
@synthesize access = _access;
@synthesize textureType = _textureType;
@synthesize constantBlockAlignment = _constantBlockAlignment;

+ (MTLArgumentDescriptor *)argumentDescriptor
{
    return [[[self alloc] init] autorelease];
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLArgumentDescriptor *c = [[MTLArgumentDescriptor allocWithZone:zone] init];
    c.dataType = _dataType;
    c.index = _index;
    c.arrayLength = _arrayLength;
    c.access = _access;
    c.textureType = _textureType;
    c.constantBlockAlignment = _constantBlockAlignment;
    return c;
}

@end

/* --- MTLHeapDescriptor --- */

@implementation MTLHeapDescriptor

@synthesize size = _size;
@synthesize storageMode = _storageMode;
@synthesize cpuCacheMode = _cpuCacheMode;
@synthesize hazardTrackingMode = _hazardTrackingMode;
@synthesize resourceOptions = _resourceOptions;
@synthesize type = _type;

- (id)copyWithZone:(NSZone *)zone
{
    MTLHeapDescriptor *c = [[MTLHeapDescriptor allocWithZone:zone] init];
    c.size = _size;
    c.storageMode = _storageMode;
    c.cpuCacheMode = _cpuCacheMode;
    c.hazardTrackingMode = _hazardTrackingMode;
    c.resourceOptions = _resourceOptions;
    c.type = _type;
    return c;
}

@end

/* --- Lightweight M0 Sync Objects --- */

@implementation KidoMTLEvent

- (instancetype)initWithDevice:(KidoMTLDevice *)device
{
    self = [super init];
    if (self) {
        _device = [device retain];
    }
    return self;
}

- (void)dealloc
{
    [_label release];
    [_device release];
    [super dealloc];
}

- (id)device { return _device; }
- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label
{
    if (_label != label) {
        [_label release];
        _label = [label copy];
    }
}

@end

@implementation KidoMTLSharedEvent

- (instancetype)initWithDevice:(KidoMTLDevice *)device
{
    self = [super initWithDevice:device];
    if (self) {
        pthread_mutex_init(&_lock, NULL);
        pthread_cond_init(&_cond, NULL);
        _signaledValue = 0;
    }
    return self;
}

- (void)dealloc
{
    for (size_t i = 0; i < _waiterCount; i++) {
        if (_waiters[i].queue)
            dispatch_release(_waiters[i].queue);
        if (_waiters[i].block)
            Block_release(_waiters[i].block);
    }
    free(_waiters);
    _waiters = NULL;
    pthread_cond_destroy(&_cond);
    pthread_mutex_destroy(&_lock);
    [super dealloc];
}

- (uint64_t)signaledValue
{
    pthread_mutex_lock(&_lock);
    uint64_t v = _signaledValue;
    pthread_mutex_unlock(&_lock);
    return v;
}

- (void)setSignaledValue:(uint64_t)value
{
    KidoSharedEventWaiter *fired = NULL;
    size_t firedCount = 0;

    pthread_mutex_lock(&_lock);
    _signaledValue = value;
    pthread_cond_broadcast(&_cond);

    if (_waiterCount > 0) {
        fired = (KidoSharedEventWaiter *)malloc(sizeof(KidoSharedEventWaiter) * _waiterCount);
        size_t keep = 0;
        for (size_t i = 0; i < _waiterCount; i++) {
            if (_waiters[i].value <= value) {
                if (fired) {
                    fired[firedCount++] = _waiters[i];
                } else {
                    if (_waiters[i].queue) dispatch_release(_waiters[i].queue);
                    if (_waiters[i].block) Block_release(_waiters[i].block);
                }
            } else {
                _waiters[keep++] = _waiters[i];
            }
        }
        _waiterCount = keep;
    }
    pthread_mutex_unlock(&_lock);

    for (size_t i = 0; i < firedCount; i++) {
        dispatch_queue_t q = fired[i].queue;
        MTLSharedEventNotificationBlock b = fired[i].block;
        [self retain];
        dispatch_async(q, ^{
            @autoreleasepool {
                b(self, value);
                Block_release(b);
                [self release];
            }
        });
        dispatch_release(q);
    }
    free(fired);
}

- (BOOL)waitUntilSignaledValue:(uint64_t)value timeoutMS:(uint64_t)milliseconds
{
    BOOL infinite = (milliseconds >= ((uint64_t)INT64_MAX / 2000ULL));
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));
    if (!infinite) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint64_t addSec = milliseconds / 1000ULL;
        uint64_t ns = (uint64_t)tv.tv_usec * 1000ULL + (milliseconds % 1000ULL) * 1000000ULL;
        addSec += ns / 1000000000ULL;
        if ((uint64_t)tv.tv_sec > (uint64_t)INT64_MAX - addSec) {
            infinite = YES;
        } else {
            ts.tv_sec = tv.tv_sec + (time_t)addSec;
            ts.tv_nsec = (long)(ns % 1000000000ULL);
        }
    }

    pthread_mutex_lock(&_lock);
    while (_signaledValue < value) {
        int rc = infinite ? pthread_cond_wait(&_cond, &_lock)
                          : pthread_cond_timedwait(&_cond, &_lock, &ts);
        if (rc == EINTR)
            continue;
        if (rc != 0)
            break;
    }
    BOOL ok = (_signaledValue >= value);
    pthread_mutex_unlock(&_lock);
    return ok;
}

- (void)notifyListener:(MTLSharedEventListener *)listener atValue:(uint64_t)value block:(MTLSharedEventNotificationBlock)block
{
    if (!block)
        return;
    dispatch_queue_t q = listener ? [listener dispatchQueue] : dispatch_get_global_queue(0, 0);
    if (!q)
        q = dispatch_get_global_queue(0, 0);
    MTLSharedEventNotificationBlock copied = Block_copy(block);
    BOOL fireNow = NO;
    uint64_t curVal = 0;

    pthread_mutex_lock(&_lock);
    if (_signaledValue >= value) {
        fireNow = YES;
        curVal = _signaledValue;
    } else {
        if (_waiterCount == _waiterCap) {
            size_t nc = _waiterCap ? (_waiterCap * 2) : 8;
            KidoSharedEventWaiter *nw = (KidoSharedEventWaiter *)realloc(
                _waiters, sizeof(KidoSharedEventWaiter) * nc);
            if (nw) {
                _waiters = nw;
                _waiterCap = nc;
            }
        }
        if (_waiterCount < _waiterCap) {
            dispatch_retain(q);
            _waiters[_waiterCount].value = value;
            _waiters[_waiterCount].queue = q;
            _waiters[_waiterCount].block = copied;
            _waiterCount++;
            copied = NULL;
        }
    }
    pthread_mutex_unlock(&_lock);

    if (fireNow && copied) {
        [self retain];
        dispatch_async(q, ^{
            @autoreleasepool {
                copied(self, curVal);
                Block_release(copied);
                [self release];
            }
        });
    } else if (copied) {
        Block_release(copied);
    }
}

- (MTLSharedEventHandle *)newSharedEventHandle
{
    return [[MTLSharedEventHandle alloc] init];
}

@end

@implementation KidoMTLFence

- (instancetype)initWithDevice:(KidoMTLDevice *)device
{
    self = [super init];
    if (self) {
        _device = [device retain];
    }
    return self;
}

- (void)dealloc
{
    [_label release];
    [_device release];
    [super dealloc];
}

- (id)device { return _device; }
- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label
{
    if (_label != label) {
        [_label release];
        _label = [label copy];
    }
}

@end

@implementation MTLResidencySetDescriptor
@synthesize label = _label;
@synthesize initialCapacity = _initialCapacity;

- (void)dealloc
{
    [_label release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLResidencySetDescriptor *c = [[MTLResidencySetDescriptor allocWithZone:zone] init];
    c.label = _label;
    c.initialCapacity = _initialCapacity;
    return c;
}
@end

@implementation MTLCaptureDescriptor
@synthesize captureObject = _captureObject;
@synthesize destination = _destination;
@synthesize outputURL = _outputURL;

- (void)dealloc
{
    [_captureObject release];
    [_outputURL release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    MTLCaptureDescriptor *c = [[MTLCaptureDescriptor allocWithZone:zone] init];
    c.captureObject = _captureObject;
    c.destination = _destination;
    c.outputURL = _outputURL;
    return c;
}
@end

@implementation MTLCaptureManager

+ (MTLCaptureManager *)sharedCaptureManager
{
    static MTLCaptureManager *sMgr = nil;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        sMgr = [[MTLCaptureManager alloc] init];
    });
    return sMgr;
}

- (BOOL)supportsDestination:(NSInteger)destination { (void)destination; return NO; }
- (BOOL)startCaptureWithDescriptor:(MTLCaptureDescriptor *)descriptor error:(NSError **)error
{
    (void)descriptor;
    if (error)
        *error = KidoMakeError(MTLDeviceErrorDomain, 1, NULL, 0, @"GPU capture is not supported");
    return NO;
}
- (void)startCaptureWithDevice:(id)device { (void)device; }
- (void)startCaptureWithCommandQueue:(id)commandQueue { (void)commandQueue; }
- (void)stopCapture { _isCapturing = NO; }
- (BOOL)isCapturing { return _isCapturing; }

@end

@implementation KidoMTLResidencySet

- (instancetype)initWithDevice:(KidoMTLDevice *)device descriptor:(MTLResidencySetDescriptor *)desc
{
    self = [super init];
    if (self) {
        _device = [device retain];
        _label = [desc.label copy];
        _allocations = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)dealloc
{
    [_allocations release];
    [_label release];
    [_device release];
    [super dealloc];
}

- (id)device { return _device; }
- (NSString *)label { return _label; }
- (uint64_t)allocatedSize { return 0; }
- (NSUInteger)allocationCount { return [_allocations count]; }
- (NSArray *)allAllocations { return [[_allocations copy] autorelease]; }
- (BOOL)containsAllocation:(id)allocation { return [_allocations containsObject:allocation]; }
- (void)addAllocation:(id)allocation { if (allocation) [_allocations addObject:allocation]; }
- (void)addAllocations:(const id *)allocations count:(NSUInteger)count
{
    if (!allocations)
        return;
    for (NSUInteger i = 0; i < count; i++) {
        if (allocations[i]) [_allocations addObject:allocations[i]];
    }
}
- (void)removeAllocation:(id)allocation { if (allocation) [_allocations removeObject:allocation]; }
- (void)removeAllocations:(const id *)allocations count:(NSUInteger)count
{
    if (!allocations)
        return;
    for (NSUInteger i = 0; i < count; i++) {
        if (allocations[i]) [_allocations removeObject:allocations[i]];
    }
}
- (void)removeAllAllocations { [_allocations removeAllObjects]; }
- (void)commit {}
- (void)requestResidency {}
- (void)endResidency {}

@end

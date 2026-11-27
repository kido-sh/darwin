/*
 * Copyright (c) 2026 The kido authors
 * SPDX-License-Identifier: Apache-2.0
 */
#import "KidoMetalInternal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    kCmdErrorBufCap = 8192,
    kFlushBatchCap  = 32,
};

typedef struct {
    KidoMTLCommandBuffer *cb;
    uint64_t seq;
    BOOL needAsync;
} KidoFlushedItem;

static double
KidoNowSeconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_UPTIME_RAW, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* --- KidoMTLCommandQueue --- */

@implementation KidoMTLCommandQueue

- (instancetype)initWithDevice:(KidoMTLDevice *)device handle:(uint64_t)handle
{
    self = [super init];
    if (self) {
        _device = [device retain];
        _handle = handle;
        _dispatchQueue = dispatch_queue_create("sh.kido.metal.cmdq", DISPATCH_QUEUE_SERIAL);
        pthread_mutex_init(&_submitLock, NULL);
        pthread_cond_init(&_submitCond, NULL);
    }
    return self;
}

- (void)dealloc
{
    /*
     * Do NOT dispatch_sync(_dispatchQueue, ^{}): the final async wait block on _dispatchQueue
     * can release the last reference to cb -> _queue, invoking dealloc on _dispatchQueue itself.
     */
    if (_dispatchQueue) {
        dispatch_release(_dispatchQueue);
        _dispatchQueue = NULL;
    }
    pthread_mutex_lock(&_submitLock);
    free(_pending);
    _pending = NULL;
    _pendingCount = 0;
    _pendingCap = 0;
    pthread_mutex_unlock(&_submitLock);
    pthread_cond_destroy(&_submitCond);
    pthread_mutex_destroy(&_submitLock);

    if (_device && _handle != 0) {
        struct kmtl_req_hdr req;
        struct kmtl_resp_hdr resp;
        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));
        req.opcode = KMTL_OP_DESTROY_QUEUE;
        req.handle = _handle;
        [_device rpc:&req inBuf:NULL inLen:0 resp:&resp outBuf:NULL outCap:0];
        _handle = 0;
    }
    [_label release];
    [_device release];
    [super dealloc];
}

- (BOOL)kidoAppendPendingLocked:(KidoMTLCommandBuffer *)cb ready:(BOOL)ready
{
    if (_pendingCount == _pendingCap) {
        size_t nc = _pendingCap ? (_pendingCap * 2) : 16;
        if (nc < _pendingCap)
            return NO;
        KidoPendingCommandBuffer *np = (KidoPendingCommandBuffer *)realloc(
            _pending, sizeof(KidoPendingCommandBuffer) * nc);
        if (!np)
            return NO;
        _pending = np;
        _pendingCap = nc;
    }
    _pending[_pendingCount].cb = cb;
    _pending[_pendingCount].ready = ready;
    _pending[_pendingCount].waitingOnEvent = NO;
    _pendingCount++;
    return YES;
}

/*
 * Pops and submits ready heads in fixed stack-allocated batches (zero heap allocation -> no OOM leak).
 * If the head has an unsatisfied encodeWaitForEvent:, defers asynchronously via notifyListener so we
 * never block while holding _submitLock or before dispatching earlier async waiters.
 * Must be called with _submitLock held; returns with _submitLock unlocked.
 */
- (void)kidoFlushAndUnlock
{
    for (;;) {
        KidoFlushedItem batch[kFlushBatchCap];
        size_t count = 0;
        KidoMTLSharedEvent *deferredEvent = nil;
        uint64_t deferredValue = 0;
        KidoMTLCommandBuffer *deferredCB = nil;

        while (count < kFlushBatchCap && _pendingCount > 0 && _pending[0].ready) {
            KidoMTLCommandBuffer *head = _pending[0].cb;
            id waitEvObj = nil;
            uint64_t waitVal = 0;
            if ([head kidoPeekNextWaitEvent:&waitEvObj value:&waitVal]) {
                if (!_pending[0].waitingOnEvent) {
                    _pending[0].waitingOnEvent = YES;
                    deferredEvent = (KidoMTLSharedEvent *)waitEvObj;
                    deferredValue = waitVal;
                    deferredCB = [head retain];
                } else {
                    [waitEvObj release];
                }
                break;
            }

            _pendingCount--;
            if (_pendingCount > 0)
                memmove(&_pending[0], &_pending[1], _pendingCount * sizeof(KidoPendingCommandBuffer));

            uint64_t seq = 0;
            BOOL asyncNeeded = [head kidoSubmitOnHostAndCheckNeedsAsyncWait:&seq];
            batch[count].cb = head;
            batch[count].seq = seq;
            batch[count].needAsync = asyncNeeded;
            count++;
        }

        BOOL moreReady = (!deferredEvent && _pendingCount > 0 && _pending[0].ready && !_pending[0].waitingOnEvent);
        if (count > 0)
            pthread_cond_broadcast(&_submitCond);
        pthread_mutex_unlock(&_submitLock);

        for (size_t i = 0; i < count; i++) {
            if (batch[i].needAsync)
                [batch[i].cb kidoStartAsyncWaitWithSeq:batch[i].seq];
            [batch[i].cb release];
        }

        if (deferredEvent) {
            [self retain];
            [deferredEvent notifyListener:nil atValue:deferredValue block:^(id ev, uint64_t v) {
                (void)ev;
                (void)v;
                [self kidoResumeAfterEventForCommandBuffer:deferredCB];
                [deferredCB release];
                [self release];
            }];
            [deferredEvent release];
            return;
        }

        if (!moreReady)
            return;
        pthread_mutex_lock(&_submitLock);
    }
}

- (void)kidoResumeAfterEventForCommandBuffer:(KidoMTLCommandBuffer *)cb
{
    pthread_mutex_lock(&_submitLock);
    if (_pendingCount > 0 && _pending[0].cb == cb)
        _pending[0].waitingOnEvent = NO;
    [self kidoFlushAndUnlock];
}

/* Strict lock order everywhere: _submitLock -> cb->_lock. */
- (void)kidoEnqueueCommandBuffer:(KidoMTLCommandBuffer *)cb
{
    [cb retain];
    pthread_mutex_lock(&_submitLock);
    if ([cb kidoMarkEnqueuedLocked]) {
        BOOL found = NO;
        for (size_t i = 0; i < _pendingCount; i++) {
            if (_pending[i].cb == cb) {
                found = YES;
                break;
            }
        }
        if (!found)
            [self kidoAppendPendingLocked:cb ready:NO];
    }
    pthread_mutex_unlock(&_submitLock);
    [cb release];
}

- (void)kidoRemoveUncommittedCommandBuffer:(KidoMTLCommandBuffer *)cb
{
    pthread_mutex_lock(&_submitLock);
    BOOL removed = NO;
    for (size_t i = 0; i < _pendingCount; i++) {
        if (_pending[i].cb == cb && !_pending[i].ready) {
            _pendingCount--;
            if (i < _pendingCount)
                memmove(&_pending[i], &_pending[i + 1], (_pendingCount - i) * sizeof(KidoPendingCommandBuffer));
            removed = YES;
            break;
        }
    }
    if (removed)
        pthread_cond_broadcast(&_submitCond);
    [self kidoFlushAndUnlock];
}

- (void)kidoCommitCommandBuffer:(KidoMTLCommandBuffer *)cb
{
    [cb retain];
    pthread_mutex_lock(&_submitLock);
    if (![cb kidoMarkCommittedLocked]) {
        pthread_mutex_unlock(&_submitLock);
        [cb release];
        return;
    }

    BOOL found = NO;
    for (size_t i = 0; i < _pendingCount; i++) {
        if (_pending[i].cb == cb) {
            if (!_pending[i].ready) {
                [cb retain];
                _pending[i].ready = YES;
            }
            found = YES;
            break;
        }
    }
    if (!found) {
        [cb retain];
        if (![self kidoAppendPendingLocked:cb ready:YES]) {
            /* Preserve FIFO ordering on OOM by waiting for earlier enqueued CBs to drain. */
            while (_pendingCount > 0)
                pthread_cond_wait(&_submitCond, &_submitLock);
            uint64_t seq = 0;
            BOOL asyncNeeded = [cb kidoSubmitOnHostAndCheckNeedsAsyncWait:&seq];
            pthread_cond_broadcast(&_submitCond);
            pthread_mutex_unlock(&_submitLock);
            if (asyncNeeded)
                [cb kidoStartAsyncWaitWithSeq:seq];
            [cb release];
            [cb release];
            return;
        }
    }

    [self kidoFlushAndUnlock];
    [cb release];
}

- (void)kidoWaitUntilSubmitted:(KidoMTLCommandBuffer *)cb
{
    pthread_mutex_lock(&_submitLock);
    while (![cb kidoIsSubmittedToHost])
        pthread_cond_wait(&_submitCond, &_submitLock);
    pthread_mutex_unlock(&_submitLock);
}

- (uint64_t)kidoHandle { return _handle; }
- (dispatch_queue_t)kidoDispatchQueue { return _dispatchQueue; }
- (uint64_t)kidoCompletedSeq { return __atomic_load_n(&_completedSeq, __ATOMIC_ACQUIRE); }
- (void)kidoAdvanceCompletedSeq:(uint64_t)seq
{
    uint64_t cur = __atomic_load_n(&_completedSeq, __ATOMIC_RELAXED);
    while (seq > cur) {
        if (__atomic_compare_exchange_n(&_completedSeq, &cur, seq, 0,
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED))
            break;
    }
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

- (id)commandBuffer
{
    return [[[KidoMTLCommandBuffer alloc] initWithQueue:self
                                     retainedReferences:YES
                                           errorOptions:MTLCommandBufferErrorOptionNone] autorelease];
}

- (id)commandBufferWithUnretainedReferences
{
    return [[[KidoMTLCommandBuffer alloc] initWithQueue:self
                                     retainedReferences:NO
                                           errorOptions:MTLCommandBufferErrorOptionNone] autorelease];
}

- (id)commandBufferWithDescriptor:(MTLCommandBufferDescriptor *)descriptor
{
    BOOL retainRefs = descriptor ? descriptor.retainedReferences : YES;
    MTLCommandBufferErrorOption errOpts = descriptor ? descriptor.errorOptions : MTLCommandBufferErrorOptionNone;
    return [[[KidoMTLCommandBuffer alloc] initWithQueue:self
                                     retainedReferences:retainRefs
                                           errorOptions:errOpts] autorelease];
}

- (void)insertDebugCaptureBoundary {}
- (void)addResidencySet:(id)residencySet { (void)residencySet; }
- (void)addResidencySets:(const id *)residencySets count:(NSUInteger)count { (void)residencySets; (void)count; }
- (void)removeResidencySet:(id)residencySet { (void)residencySet; }
- (void)removeResidencySets:(const id *)residencySets count:(NSUInteger)count { (void)residencySets; (void)count; }

@end

/* --- KidoMTLCommandBuffer Helpers --- */

@interface KidoWriteBackOp : NSObject {
@public
    KidoMTLBuffer *buffer;
    NSUInteger offset;
    NSUInteger length;
}
@end

@implementation KidoWriteBackOp
- (void)dealloc { [buffer release]; [super dealloc]; }
@end

@interface KidoEventOp : NSObject {
@public
    KidoMTLSharedEvent *event;
    uint64_t value;
}
@end

@implementation KidoEventOp
- (void)dealloc { [event release]; [super dealloc]; }
@end

/* --- KidoMTLCommandBuffer --- */

@implementation KidoMTLCommandBuffer

- (instancetype)initWithQueue:(KidoMTLCommandQueue *)queue
           retainedReferences:(BOOL)retainedReferences
                 errorOptions:(MTLCommandBufferErrorOption)errorOptions
{
    self = [super init];
    if (self) {
        _queue = [queue retain];
        _device = [(KidoMTLDevice *)[queue device] retain];
        _retainedReferences = retainedReferences;
        _errorOptions = errorOptions;
        _status = MTLCommandBufferStatusNotEnqueued;
        if (retainedReferences)
            _retainedObjects = [[NSMutableArray alloc] init];
        _syncInBuffers = [[NSMutableArray alloc] init];
        _writeBackOps = [[NSMutableArray alloc] init];
        _waitEvents = [[NSMutableArray alloc] init];
        _signalEvents = [[NSMutableArray alloc] init];
        pthread_mutex_init(&_lock, NULL);
        _doneGroup = dispatch_group_create();
    }
    return self;
}

- (void)dealloc
{
    if (_enqueued && !_committed && _queue)
        [_queue kidoRemoveUncommittedCommandBuffer:self];

    for (size_t i = 0; i < _scheduledCount; i++)
        Block_release(_scheduledHandlers[i]);
    free(_scheduledHandlers);
    for (size_t i = 0; i < _completedCount; i++)
        Block_release(_completedHandlers[i]);
    free(_completedHandlers);

    free(_stream);
    if (_doneGroup)
        dispatch_release(_doneGroup);
    pthread_mutex_destroy(&_lock);
    [_signalEvents release];
    [_waitEvents release];
    [_writeBackOps release];
    [_syncInBuffers release];
    [_retainedObjects release];
    [_error release];
    [_label release];
    [_queue release];
    [_device release];
    [super dealloc];
}

- (id)device { return _device; }
- (id)commandQueue { return _queue; }
- (BOOL)retainedReferences { return _retainedReferences; }
- (MTLCommandBufferErrorOption)errorOptions { return _errorOptions; }
- (double)kernelStartTime { return _kernelStartTime; }
- (double)kernelEndTime { return _kernelEndTime; }
- (double)GPUStartTime { return _gpuStartTime; }
- (double)GPUEndTime { return _gpuEndTime; }
- (id)logs { return nil; }

- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label
{
    if (_label != label) {
        [_label release];
        _label = [label copy];
    }
}

- (MTLCommandBufferStatus)status
{
    NSMutableArray *releaseRetained = nil;
    pthread_mutex_lock(&_lock);
    MTLCommandBufferStatus s = _status;
    if (s == MTLCommandBufferStatusScheduled && _submittedToHost && !_submitFailed &&
        !_asyncWaitStarted && _commitSeq > 0 &&
        _scheduledCount == 0 && _completedCount == 0 &&
        [_writeBackOps count] == 0 && [_signalEvents count] == 0 &&
        _commitSeq <= [_queue kidoCompletedSeq]) {
        s = MTLCommandBufferStatusCompleted;
        _status = s;
        releaseRetained = _retainedObjects;
        _retainedObjects = nil;
    }
    pthread_mutex_unlock(&_lock);
    [releaseRetained release];
    return s;
}

- (NSError *)error
{
    pthread_mutex_lock(&_lock);
    NSError *e = [[_error retain] autorelease];
    pthread_mutex_unlock(&_lock);
    return e;
}

- (void)kidoRetainObject:(id)obj
{
    if (!obj)
        return;
    pthread_mutex_lock(&_lock);
    if (_retainedReferences && !_committed && _retainedObjects)
        [_retainedObjects addObject:obj];
    pthread_mutex_unlock(&_lock);
}

- (void)kidoRecordSyncIn:(KidoMTLBuffer *)buf
{
    if (!buf || ![buf hasNoCopyPointer])
        return;
    pthread_mutex_lock(&_lock);
    if (!_committed && _syncInBuffers && ![_syncInBuffers containsObject:buf])
        [_syncInBuffers addObject:buf];
    pthread_mutex_unlock(&_lock);
}

- (void)kidoRecordWriteBack:(KidoMTLBuffer *)buf offset:(NSUInteger)offset length:(NSUInteger)length
{
    if (!buf || ![buf hasNoCopyPointer] || length == 0)
        return;
    pthread_mutex_lock(&_lock);
    if (!_committed && _writeBackOps) {
        BOOL dup = NO;
        NSUInteger n = [_writeBackOps count];
        for (NSUInteger i = 0; i < n; i++) {
            KidoWriteBackOp *ex = (KidoWriteBackOp *)[_writeBackOps objectAtIndex:i];
            if (ex->buffer == buf && ex->offset <= offset &&
                ex->length >= (offset - ex->offset) &&
                (ex->length - (offset - ex->offset)) >= length) {
                dup = YES;
                break;
            }
        }
        if (!dup) {
            KidoWriteBackOp *op = [[KidoWriteBackOp alloc] init];
            op->buffer = [buf retain];
            op->offset = offset;
            op->length = length;
            [_writeBackOps addObject:op];
            [op release];
        }
    }
    pthread_mutex_unlock(&_lock);
}

- (void)kidoAppendBytes:(const void *)bytes length:(size_t)len
{
    if (len == 0)
        return;
    pthread_mutex_lock(&_lock);
    if (_committed || _streamOverflow) {
        pthread_mutex_unlock(&_lock);
        return;
    }
    if (len > KMTL_MAX_CMD_PAYLOAD || _streamLen > KMTL_MAX_CMD_PAYLOAD - len) {
        _streamOverflow = YES;
        pthread_mutex_unlock(&_lock);
        return;
    }
    if (_streamLen + len > _streamCap) {
        size_t newCap = _streamCap ? (_streamCap * 2) : 4096;
        while (newCap < _streamLen + len)
            newCap *= 2;
        if (newCap > KMTL_MAX_CMD_PAYLOAD)
            newCap = KMTL_MAX_CMD_PAYLOAD;
        uint8_t *nb = (uint8_t *)realloc(_stream, newCap);
        if (!nb) {
            _streamOverflow = YES;
            pthread_mutex_unlock(&_lock);
            return;
        }
        _stream = nb;
        _streamCap = newCap;
    }
    memcpy(_stream + _streamLen, bytes, len);
    _streamLen += len;
    pthread_mutex_unlock(&_lock);
}

- (void)kidoAppendCommand:(const void *)cmd length:(size_t)len
{
    [self kidoAppendBytes:cmd length:len];
}

- (void)kidoAppendCommand:(const void *)cmd headerLen:(size_t)hdrLen extra:(const void *)extra extraLen:(size_t)extraLen
{
    size_t padded = (hdrLen + extraLen + 7u) & ~(size_t)7u;
    size_t pad = padded - (hdrLen + extraLen);
    [self kidoAppendBytes:cmd length:hdrLen];
    if (extra && extraLen > 0)
        [self kidoAppendBytes:extra length:extraLen];
    if (pad > 0) {
        static const uint8_t zeros[8] = { 0 };
        [self kidoAppendBytes:zeros length:pad];
    }
}

- (BOOL)kidoMarkEnqueuedLocked
{
    pthread_mutex_lock(&_lock);
    BOOL did = NO;
    if (!_enqueued && !_committed) {
        _enqueued = YES;
        if (_status == MTLCommandBufferStatusNotEnqueued)
            _status = MTLCommandBufferStatusEnqueued;
        did = YES;
    }
    pthread_mutex_unlock(&_lock);
    return did;
}

- (BOOL)kidoMarkCommittedLocked
{
    pthread_mutex_lock(&_lock);
    if (_committed) {
        pthread_mutex_unlock(&_lock);
        return NO;
    }
    _committed = YES;
    _enqueued = YES;
    _status = MTLCommandBufferStatusCommitted;
    _kernelStartTime = KidoNowSeconds();
    pthread_mutex_unlock(&_lock);
    return YES;
}

- (void)enqueue
{
    [_queue kidoEnqueueCommandBuffer:self];
}

- (void)addScheduledHandler:(MTLCommandBufferHandler)block
{
    if (!block)
        return;
    MTLCommandBufferHandler copied = Block_copy(block);
    BOOL invokeNow = NO;

    pthread_mutex_lock(&_lock);
    if (_status >= MTLCommandBufferStatusScheduled) {
        invokeNow = YES;
    } else {
        if (_scheduledCount == _scheduledCap) {
            size_t nc = _scheduledCap ? (_scheduledCap * 2) : 4;
            MTLCommandBufferHandler *na = (MTLCommandBufferHandler *)realloc(
                _scheduledHandlers, sizeof(MTLCommandBufferHandler) * nc);
            if (na) {
                _scheduledHandlers = na;
                _scheduledCap = nc;
            }
        }
        if (_scheduledCount < _scheduledCap) {
            _scheduledHandlers[_scheduledCount++] = copied;
            copied = NULL;
        }
    }
    pthread_mutex_unlock(&_lock);

    if (invokeNow && copied) {
        [self retain];
        dispatch_async(dispatch_get_global_queue(0, 0), ^{
            @autoreleasepool {
                copied(self);
                Block_release(copied);
                [self release];
            }
        });
    } else if (copied) {
        Block_release(copied);
    }
}

- (void)addCompletedHandler:(MTLCommandBufferHandler)block
{
    if (!block)
        return;
    MTLCommandBufferHandler copied = Block_copy(block);
    BOOL invokeNow = NO;
    BOOL startAsync = NO;
    uint64_t seq = 0;

    pthread_mutex_lock(&_lock);
    if (_status == MTLCommandBufferStatusCompleted || _status == MTLCommandBufferStatusError) {
        invokeNow = YES;
    } else {
        if (_completedCount == _completedCap) {
            size_t nc = _completedCap ? (_completedCap * 2) : 4;
            MTLCommandBufferHandler *na = (MTLCommandBufferHandler *)realloc(
                _completedHandlers, sizeof(MTLCommandBufferHandler) * nc);
            if (na) {
                _completedHandlers = na;
                _completedCap = nc;
            }
        }
        if (_completedCount < _completedCap) {
            _completedHandlers[_completedCount++] = copied;
            copied = NULL;
            if (_submittedToHost && !_submitFailed && !_asyncWaitStarted && _commitSeq > 0) {
                _asyncWaitStarted = YES;
                dispatch_group_enter(_doneGroup);
                seq = _commitSeq;
                startAsync = YES;
            }
        }
    }
    pthread_mutex_unlock(&_lock);

    if (startAsync)
        [self kidoStartAsyncWaitWithSeq:seq];

    if (invokeNow && copied) {
        [self retain];
        dispatch_async(dispatch_get_global_queue(0, 0), ^{
            @autoreleasepool {
                copied(self);
                Block_release(copied);
                [self release];
            }
        });
    } else if (copied) {
        Block_release(copied);
    }
}

- (BOOL)kidoIsSubmittedToHost
{
    pthread_mutex_lock(&_lock);
    BOOL s = _submittedToHost;
    pthread_mutex_unlock(&_lock);
    return s;
}

- (void)kidoFinalizeWithStatus:(MTLCommandBufferStatus)finalStatus
                         error:(NSError *)err
                      gpuStart:(double)gpuStart
                        gpuEnd:(double)gpuEnd
{
    double kernEnd = KidoNowSeconds();

    NSArray *wbList = nil;
    NSArray *sigList = nil;
    NSMutableArray *releaseRetained = nil;
    MTLCommandBufferHandler *schedList = NULL;
    size_t schedN = 0;
    MTLCommandBufferHandler *compList = NULL;
    size_t compN = 0;

    pthread_mutex_lock(&_lock);
    wbList = _writeBackOps;
    _writeBackOps = nil;
    sigList = _signalEvents;
    _signalEvents = nil;
    pthread_mutex_unlock(&_lock);

    if (finalStatus == MTLCommandBufferStatusCompleted) {
        NSUInteger wbCount = [wbList count];
        for (NSUInteger i = 0; i < wbCount; i++) {
            KidoWriteBackOp *op = (KidoWriteBackOp *)[wbList objectAtIndex:i];
            [op->buffer syncToNoCopyOffset:op->offset length:op->length];
        }
    }
    [wbList release];

    pthread_mutex_lock(&_lock);
    _kernelEndTime = kernEnd;
    _gpuStartTime = gpuStart;
    _gpuEndTime = gpuEnd;
    if (err) {
        [_error release];
        _error = err;
    }
    _status = finalStatus;
    schedList = _scheduledHandlers;
    schedN = _scheduledCount;
    _scheduledHandlers = NULL;
    _scheduledCount = 0;
    _scheduledCap = 0;
    compList = _completedHandlers;
    compN = _completedCount;
    _completedHandlers = NULL;
    _completedCount = 0;
    _completedCap = 0;
    releaseRetained = _retainedObjects;
    _retainedObjects = nil;
    pthread_mutex_unlock(&_lock);

    if (finalStatus == MTLCommandBufferStatusCompleted) {
        NSUInteger sigCount = [sigList count];
        for (NSUInteger i = 0; i < sigCount; i++) {
            KidoEventOp *op = (KidoEventOp *)[sigList objectAtIndex:i];
            op->event.signaledValue = op->value;
        }
    }
    [sigList release];

    for (size_t i = 0; i < schedN; i++) {
        schedList[i](self);
        Block_release(schedList[i]);
    }
    free(schedList);

    for (size_t i = 0; i < compN; i++) {
        compList[i](self);
        Block_release(compList[i]);
    }
    free(compList);

    [releaseRetained release];
}

- (void)kidoWaitAndFinalizeSeq:(uint64_t)seq
{
    NSError *err = nil;
    MTLCommandBufferStatus finalStatus = MTLCommandBufferStatusCompleted;
    double gpuStart = 0.0;
    double gpuEnd = 0.0;

    pthread_mutex_lock(&_lock);
    BOOL failed = _submitFailed;
    MTLCommandBufferStatus curStatus = _status;
    pthread_mutex_unlock(&_lock);

    if (failed) {
        [self kidoFinalizeWithStatus:curStatus error:nil gpuStart:0.0 gpuEnd:0.0];
        dispatch_group_leave(_doneGroup);
        return;
    }

    char *errBuf = (char *)malloc(kCmdErrorBufCap);
    struct kmtl_req_hdr req;
    struct kmtl_resp_hdr resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.opcode = KMTL_OP_WAIT_QUEUE;
    req.handle = [_queue kidoHandle];
    req.arg0   = seq;

    kern_return_t kr = [_device rpc:&req
                              inBuf:NULL
                              inLen:0
                               resp:&resp
                             outBuf:errBuf
                             outCap:errBuf ? kCmdErrorBufCap : 0];
    if (kr != KERN_SUCCESS || resp.status != KMTL_OK) {
        finalStatus = MTLCommandBufferStatusError;
        NSInteger code = (kr == kIOReturnTimeout) ? MTLCommandBufferErrorTimeout
                                                  : MTLCommandBufferErrorInternal;
        err = [KidoMakeError(MTLCommandBufferErrorDomain, code, errBuf, resp.extra_len,
                             @"GPU command buffer execution failed") retain];
    } else {
        gpuStart = (double)resp.arg0 * 1e-9;
        gpuEnd   = (double)resp.arg1 * 1e-9;
        uint64_t cleanUpTo = (resp.handle > 0 && resp.handle <= seq) ? (resp.handle - 1) : seq;
        if (cleanUpTo > 0)
            [_queue kidoAdvanceCompletedSeq:cleanUpTo];
    }
    free(errBuf);

    [self kidoFinalizeWithStatus:finalStatus error:err gpuStart:gpuStart gpuEnd:gpuEnd];
    dispatch_group_leave(_doneGroup);
}

- (void)kidoStartAsyncWaitWithSeq:(uint64_t)seq
{
    [self retain];
    dispatch_async([_queue kidoDispatchQueue], ^{
        @autoreleasepool {
            [self kidoWaitAndFinalizeSeq:seq];
            [self release];
        }
    });
}

- (BOOL)kidoPeekNextWaitEvent:(id *)outEvent value:(uint64_t *)outValue
{
    pthread_mutex_lock(&_lock);
    while ([_waitEvents count] > 0) {
        KidoEventOp *op = (KidoEventOp *)[_waitEvents objectAtIndex:0];
        if ([op->event signaledValue] >= op->value) {
            [_waitEvents removeObjectAtIndex:0];
            continue;
        }
        if (outEvent)
            *outEvent = [op->event retain];
        if (outValue)
            *outValue = op->value;
        pthread_mutex_unlock(&_lock);
        return YES;
    }
    pthread_mutex_unlock(&_lock);
    return NO;
}

- (BOOL)kidoSubmitOnHostAndCheckNeedsAsyncWait:(uint64_t *)outSeq
{
    *outSeq = 0;

    uint8_t *streamPtr = NULL;
    size_t streamLen = 0;
    BOOL overflow = NO;
    NSArray *waitList = nil;
    NSArray *syncInList = nil;

    pthread_mutex_lock(&_lock);
    streamPtr = _stream;
    streamLen = _streamLen;
    overflow = _streamOverflow;
    _stream = NULL;
    _streamLen = 0;
    _streamCap = 0;
    waitList = _waitEvents;
    _waitEvents = nil;
    syncInList = _syncInBuffers;
    _syncInBuffers = nil;
    pthread_mutex_unlock(&_lock);

    [waitList release];

    /* Upload any fallback (non-in-place) no-copy buffers before GPU execution. */
    NSUInteger syncCount = [syncInList count];
    for (NSUInteger i = 0; i < syncCount; i++) {
        KidoMTLBuffer *b = (KidoMTLBuffer *)[syncInList objectAtIndex:i];
        [b syncFromNoCopy];
    }
    [syncInList release];

    NSError *err = nil;
    uint64_t seq = 0;
    BOOL ok = YES;

    if (overflow) {
        ok = NO;
        err = [KidoMakeError(MTLCommandBufferErrorDomain, MTLCommandBufferErrorOutOfMemory,
                             NULL, 0, @"command stream exceeded maximum payload size") retain];
    } else {
        char *errBuf = (char *)malloc(kCmdErrorBufCap);
        struct kmtl_req_hdr req;
        struct kmtl_resp_hdr resp;
        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));
        req.opcode = KMTL_OP_COMMIT_COMMAND_BUFFER;
        req.handle = [_queue kidoHandle];
        req.arg0   = 0; /* Pipelined async commit */

        kern_return_t kr = [_device rpc:&req
                                  inBuf:streamPtr
                                  inLen:(uint32_t)streamLen
                                   resp:&resp
                                 outBuf:errBuf
                                 outCap:errBuf ? kCmdErrorBufCap : 0];
        if (kr != KERN_SUCCESS || resp.status != KMTL_OK) {
            ok = NO;
            NSInteger code = (kr == kIOReturnTimeout) ? MTLCommandBufferErrorTimeout
                                                      : MTLCommandBufferErrorInternal;
            err = [KidoMakeError(MTLCommandBufferErrorDomain, code, errBuf, resp.extra_len,
                                 @"GPU command buffer submission failed") retain];
        } else {
            seq = resp.arg0;
        }
        free(errBuf);
    }

    free(streamPtr);

    BOOL needAsync = NO;
    NSMutableArray *dropWB = nil;
    NSMutableArray *dropSig = nil;
    NSMutableArray *dropRet = nil;

    pthread_mutex_lock(&_lock);
    _commitSeq = seq;
    _submittedToHost = YES;
    if (!ok) {
        _submitFailed = YES;
        [_error release];
        _error = err;
        _status = MTLCommandBufferStatusError;
        _kernelEndTime = KidoNowSeconds();
        if (_scheduledCount > 0 || _completedCount > 0) {
            _asyncWaitStarted = YES;
            dispatch_group_enter(_doneGroup);
            needAsync = YES;
        } else {
            dropWB = _writeBackOps;
            _writeBackOps = nil;
            dropSig = _signalEvents;
            _signalEvents = nil;
            dropRet = _retainedObjects;
            _retainedObjects = nil;
        }
    } else {
        _status = MTLCommandBufferStatusScheduled;
        if (_scheduledCount > 0 || _completedCount > 0 ||
            [_writeBackOps count] > 0 || [_signalEvents count] > 0) {
            _asyncWaitStarted = YES;
            dispatch_group_enter(_doneGroup);
            needAsync = YES;
        }
    }
    pthread_mutex_unlock(&_lock);

    [dropWB release];
    [dropSig release];
    [dropRet release];

    *outSeq = seq;
    return needAsync;
}

- (void)commit
{
    [_queue kidoCommitCommandBuffer:self];
}

- (void)waitUntilScheduled
{
    pthread_mutex_lock(&_lock);
    if (!_committed) {
        pthread_mutex_unlock(&_lock);
        return;
    }
    BOOL submitted = _submittedToHost;
    pthread_mutex_unlock(&_lock);

    if (!submitted)
        [_queue kidoWaitUntilSubmitted:self];
}

- (void)waitUntilCompleted
{
    pthread_mutex_lock(&_lock);
    if (!_committed) {
        pthread_mutex_unlock(&_lock);
        return;
    }
    if (_status == MTLCommandBufferStatusCompleted || _status == MTLCommandBufferStatusError) {
        BOOL waitGroup = _asyncWaitStarted;
        pthread_mutex_unlock(&_lock);
        if (waitGroup)
            dispatch_group_wait(_doneGroup, DISPATCH_TIME_FOREVER);
        return;
    }
    BOOL submitted = _submittedToHost;
    pthread_mutex_unlock(&_lock);

    if (!submitted)
        [_queue kidoWaitUntilSubmitted:self];

    BOOL waitExisting = NO;
    uint64_t seq = 0;

    pthread_mutex_lock(&_lock);
    if (_status == MTLCommandBufferStatusCompleted || _status == MTLCommandBufferStatusError) {
        waitExisting = _asyncWaitStarted;
        pthread_mutex_unlock(&_lock);
        if (waitExisting)
            dispatch_group_wait(_doneGroup, DISPATCH_TIME_FOREVER);
        return;
    }
    if (_asyncWaitStarted) {
        waitExisting = YES;
    } else {
        _asyncWaitStarted = YES;
        dispatch_group_enter(_doneGroup);
        seq = _commitSeq;
    }
    pthread_mutex_unlock(&_lock);

    if (waitExisting) {
        dispatch_group_wait(_doneGroup, DISPATCH_TIME_FOREVER);
    } else {
        [self kidoWaitAndFinalizeSeq:seq];
    }
}

- (id)computeCommandEncoderWithDispatchType:(MTLDispatchType)dispatchType
{
    return [[[KidoMTLComputeCommandEncoder alloc] initWithCommandBuffer:self dispatchType:dispatchType] autorelease];
}

- (id)computeCommandEncoder
{
    return [self computeCommandEncoderWithDispatchType:MTLDispatchTypeSerial];
}

- (id)computeCommandEncoderWithDescriptor:(MTLComputePassDescriptor *)descriptor
{
    MTLDispatchType dt = descriptor ? descriptor.dispatchType : MTLDispatchTypeSerial;
    return [self computeCommandEncoderWithDispatchType:dt];
}

- (id)blitCommandEncoder
{
    return [[[KidoMTLBlitCommandEncoder alloc] initWithCommandBuffer:self] autorelease];
}

- (id)blitCommandEncoderWithDescriptor:(MTLBlitPassDescriptor *)descriptor
{
    (void)descriptor;
    return [self blitCommandEncoder];
}

- (void)encodeSignalEvent:(id)event value:(uint64_t)value
{
    if ([event isKindOfClass:[KidoMTLSharedEvent class]]) {
        KidoEventOp *op = [[KidoEventOp alloc] init];
        op->event = (KidoMTLSharedEvent *)[event retain];
        op->value = value;
        pthread_mutex_lock(&_lock);
        if (!_committed && _signalEvents)
            [_signalEvents addObject:op];
        pthread_mutex_unlock(&_lock);
        [op release];
    }
}

- (void)encodeWaitForEvent:(id)event value:(uint64_t)value
{
    if ([event isKindOfClass:[KidoMTLSharedEvent class]]) {
        KidoEventOp *op = [[KidoEventOp alloc] init];
        op->event = (KidoMTLSharedEvent *)[event retain];
        op->value = value;
        pthread_mutex_lock(&_lock);
        if (!_committed && _waitEvents)
            [_waitEvents addObject:op];
        pthread_mutex_unlock(&_lock);
        [op release];
    }
}

- (void)pushDebugGroup:(NSString *)string { (void)string; }
- (void)popDebugGroup {}
- (void)useResidencySet:(id)residencySet { (void)residencySet; }
- (void)useResidencySets:(const id *)residencySets count:(NSUInteger)count { (void)residencySets; (void)count; }

@end

/* --- KidoMTLComputeCommandEncoder --- */

@implementation KidoMTLComputeCommandEncoder

- (instancetype)initWithCommandBuffer:(KidoMTLCommandBuffer *)cmdBuf dispatchType:(MTLDispatchType)dispatchType
{
    self = [super init];
    if (self) {
        _cmdBuf = [cmdBuf retain];
        _device = [(KidoMTLDevice *)[cmdBuf device] retain];
        _dispatchType = dispatchType;
        struct kmtl_cmd_hdr hdr;
        hdr.op    = KMTL_CMD_BEGIN_COMPUTE;
        hdr.flags = (uint16_t)dispatchType;
        hdr.size  = (uint32_t)sizeof(hdr);
        [_cmdBuf kidoAppendCommand:&hdr length:sizeof(hdr)];
    }
    return self;
}

- (void)dealloc
{
    if (!_ended)
        [self endEncoding];
    [_label release];
    [_cmdBuf release];
    [_device release];
    [super dealloc];
}

- (id)device { return _device; }
- (MTLDispatchType)dispatchType { return _dispatchType; }

- (NSString *)label { return _label; }
- (void)setLabel:(NSString *)label
{
    if (_label != label) {
        [_label release];
        _label = [label copy];
    }
}

- (void)setComputePipelineState:(id)state
{
    if (_ended || ![state isKindOfClass:[KidoMTLComputePipelineState class]])
        return;
    KidoMTLComputePipelineState *pso = (KidoMTLComputePipelineState *)state;
    [_cmdBuf kidoRetainObject:pso];
    struct kmtl_cmd_set_pipeline cmd;
    cmd.hdr.op     = KMTL_CMD_SET_PIPELINE;
    cmd.hdr.flags  = 0;
    cmd.hdr.size   = (uint32_t)sizeof(cmd);
    cmd.pso_handle = [pso kidoHandle];
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)setBuffer:(id)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    if (_ended || index >= KMTL_MAX_BUFFER_SLOTS)
        return;
    uint64_t handle = 0;
    if (buffer) {
        if (![buffer isKindOfClass:[KidoMTLBuffer class]])
            return;
        KidoMTLBuffer *kbuf = (KidoMTLBuffer *)buffer;
        [_cmdBuf kidoRetainObject:kbuf];
        if ([kbuf hasNoCopyPointer]) {
            [_cmdBuf kidoRecordSyncIn:kbuf];
            [_cmdBuf kidoRecordWriteBack:kbuf offset:0 length:[kbuf length]];
        }
        handle = [kbuf kidoHandle];
    }
    struct kmtl_cmd_set_buffer cmd;
    cmd.hdr.op     = KMTL_CMD_SET_BUFFER;
    cmd.hdr.flags  = 0;
    cmd.hdr.size   = (uint32_t)sizeof(cmd);
    cmd.index      = (uint32_t)index;
    cmd.reserved   = 0;
    cmd.buf_handle = handle;
    cmd.offset     = (uint64_t)offset;
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)setBuffer:(id)buffer offset:(NSUInteger)offset attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index
{
    (void)stride;
    [self setBuffer:buffer offset:offset atIndex:index];
}

- (void)setBuffers:(const id *)buffers offsets:(const NSUInteger *)offsets withRange:(NSRange)range
{
    if (_ended || !buffers || !offsets)
        return;
    for (NSUInteger i = 0; i < range.length; i++) {
        [self setBuffer:buffers[i] offset:offsets[i] atIndex:(range.location + i)];
    }
}

- (void)setBuffers:(const id *)buffers offsets:(const NSUInteger *)offsets attributeStrides:(const NSUInteger *)strides withRange:(NSRange)range
{
    (void)strides;
    [self setBuffers:buffers offsets:offsets withRange:range];
}

- (void)setBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
    if (_ended || index >= KMTL_MAX_BUFFER_SLOTS)
        return;
    struct kmtl_cmd_set_buffer_offset cmd;
    cmd.hdr.op    = KMTL_CMD_SET_BUFFER_OFFSET;
    cmd.hdr.flags = 0;
    cmd.hdr.size  = (uint32_t)sizeof(cmd);
    cmd.index     = (uint32_t)index;
    cmd.reserved  = 0;
    cmd.offset    = (uint64_t)offset;
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)setBufferOffset:(NSUInteger)offset attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index
{
    (void)stride;
    [self setBufferOffset:offset atIndex:index];
}

- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
    if (_ended || !bytes || index >= KMTL_MAX_BUFFER_SLOTS || length == 0 || length > KMTL_MAX_SET_BYTES)
        return;
    struct kmtl_cmd_set_bytes cmd;
    size_t padded = (sizeof(cmd) + (size_t)length + 7u) & ~(size_t)7u;
    cmd.hdr.op    = KMTL_CMD_SET_BYTES;
    cmd.hdr.flags = 0;
    cmd.hdr.size  = (uint32_t)padded;
    cmd.index     = (uint32_t)index;
    cmd.length    = (uint32_t)length;
    [_cmdBuf kidoAppendCommand:&cmd headerLen:sizeof(cmd) extra:bytes extraLen:(size_t)length];
}

- (void)setBytes:(const void *)bytes length:(NSUInteger)length attributeStride:(NSUInteger)stride atIndex:(NSUInteger)index
{
    (void)stride;
    [self setBytes:bytes length:length atIndex:index];
}

- (void)setThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index
{
    if (_ended || index >= KMTL_MAX_BUFFER_SLOTS || length > UINT32_MAX)
        return;
    struct kmtl_cmd_set_tg_mem cmd;
    cmd.hdr.op    = KMTL_CMD_SET_TG_MEM;
    cmd.hdr.flags = 0;
    cmd.hdr.size  = (uint32_t)sizeof(cmd);
    cmd.index     = (uint32_t)index;
    cmd.length    = (uint32_t)length;
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)dispatchThreadgroups:(MTLSize)threadgroupsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
{
    if (_ended)
        return;
    struct kmtl_cmd_dispatch cmd;
    cmd.hdr.op    = KMTL_CMD_DISPATCH_THREADGROUPS;
    cmd.hdr.flags = 0;
    cmd.hdr.size  = (uint32_t)sizeof(cmd);
    cmd.gx = threadgroupsPerGrid.width;
    cmd.gy = threadgroupsPerGrid.height;
    cmd.gz = threadgroupsPerGrid.depth;
    cmd.tx = threadsPerThreadgroup.width;
    cmd.ty = threadsPerThreadgroup.height;
    cmd.tz = threadsPerThreadgroup.depth;
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)dispatchThreads:(MTLSize)threadsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
{
    if (_ended)
        return;
    struct kmtl_cmd_dispatch cmd;
    cmd.hdr.op    = KMTL_CMD_DISPATCH_THREADS;
    cmd.hdr.flags = 0;
    cmd.hdr.size  = (uint32_t)sizeof(cmd);
    cmd.gx = threadsPerGrid.width;
    cmd.gy = threadsPerGrid.height;
    cmd.gz = threadsPerGrid.depth;
    cmd.tx = threadsPerThreadgroup.width;
    cmd.ty = threadsPerThreadgroup.height;
    cmd.tz = threadsPerThreadgroup.depth;
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)dispatchThreadgroupsWithIndirectBuffer:(id)indirectBuffer
                          indirectBufferOffset:(NSUInteger)indirectBufferOffset
                         threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
{
    if (_ended || ![indirectBuffer isKindOfClass:[KidoMTLBuffer class]])
        return;
    KidoMTLBuffer *ibuf = (KidoMTLBuffer *)indirectBuffer;
    [_cmdBuf kidoRetainObject:ibuf];
    if ([ibuf hasNoCopyPointer])
        [_cmdBuf kidoRecordSyncIn:ibuf];
    struct kmtl_cmd_dispatch_indirect cmd;
    cmd.hdr.op     = KMTL_CMD_DISPATCH_INDIRECT;
    cmd.hdr.flags  = 0;
    cmd.hdr.size   = (uint32_t)sizeof(cmd);
    cmd.buf_handle = [ibuf kidoHandle];
    cmd.offset     = (uint64_t)indirectBufferOffset;
    cmd.tx = threadsPerThreadgroup.width;
    cmd.ty = threadsPerThreadgroup.height;
    cmd.tz = threadsPerThreadgroup.depth;
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)memoryBarrierWithScope:(MTLBarrierScope)scope
{
    if (_ended)
        return;
    struct kmtl_cmd_barrier cmd;
    cmd.hdr.op    = KMTL_CMD_MEMORY_BARRIER;
    cmd.hdr.flags = 0;
    cmd.hdr.size  = (uint32_t)sizeof(cmd);
    cmd.scope     = (uint32_t)scope;
    cmd.reserved  = 0;
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)memoryBarrierWithResources:(const id *)resources count:(NSUInteger)count
{
    (void)resources;
    (void)count;
    [self memoryBarrierWithScope:MTLBarrierScopeBuffers];
}

- (void)useResource:(id)resource usage:(MTLResourceUsage)usage
{
    (void)usage;
    if (resource)
        [_cmdBuf kidoRetainObject:resource];
}

- (void)useResources:(const id *)resources count:(NSUInteger)count usage:(MTLResourceUsage)usage
{
    (void)usage;
    if (!resources)
        return;
    for (NSUInteger i = 0; i < count; i++) {
        if (resources[i])
            [_cmdBuf kidoRetainObject:resources[i]];
    }
}

- (void)useHeap:(id)heap { (void)heap; }
- (void)useHeaps:(const id *)heaps count:(NSUInteger)count { (void)heaps; (void)count; }
- (void)updateFence:(id)fence { (void)fence; }
- (void)waitForFence:(id)fence { (void)fence; }
- (void)barrierAfterQueueStages:(MTLStages)afterStages beforeStages:(MTLStages)beforeStages { (void)afterStages; (void)beforeStages; }
- (void)insertDebugSignpost:(NSString *)string { (void)string; }
- (void)pushDebugGroup:(NSString *)string { (void)string; }
- (void)popDebugGroup {}

- (void)endEncoding
{
    if (_ended)
        return;
    _ended = YES;
    struct kmtl_cmd_hdr hdr;
    hdr.op    = KMTL_CMD_END_ENCODER;
    hdr.flags = 0;
    hdr.size  = (uint32_t)sizeof(hdr);
    [_cmdBuf kidoAppendCommand:&hdr length:sizeof(hdr)];
}

@end

/* --- KidoMTLBlitCommandEncoder --- */

@implementation KidoMTLBlitCommandEncoder

- (instancetype)initWithCommandBuffer:(KidoMTLCommandBuffer *)cmdBuf
{
    self = [super init];
    if (self) {
        _cmdBuf = [cmdBuf retain];
        _device = [(KidoMTLDevice *)[cmdBuf device] retain];
        struct kmtl_cmd_hdr hdr;
        hdr.op    = KMTL_CMD_BEGIN_BLIT;
        hdr.flags = 0;
        hdr.size  = (uint32_t)sizeof(hdr);
        [_cmdBuf kidoAppendCommand:&hdr length:sizeof(hdr)];
    }
    return self;
}

- (void)dealloc
{
    if (!_ended)
        [self endEncoding];
    [_label release];
    [_cmdBuf release];
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

- (void)copyFromBuffer:(id)sourceBuffer
          sourceOffset:(NSUInteger)sourceOffset
              toBuffer:(id)destinationBuffer
     destinationOffset:(NSUInteger)destinationOffset
                  size:(NSUInteger)size
{
    if (_ended)
        return;
    if (![sourceBuffer isKindOfClass:[KidoMTLBuffer class]] ||
        ![destinationBuffer isKindOfClass:[KidoMTLBuffer class]])
        return;
    KidoMTLBuffer *src = (KidoMTLBuffer *)sourceBuffer;
    KidoMTLBuffer *dst = (KidoMTLBuffer *)destinationBuffer;
    [_cmdBuf kidoRetainObject:src];
    [_cmdBuf kidoRetainObject:dst];
    if ([src hasNoCopyPointer])
        [_cmdBuf kidoRecordSyncIn:src];
    [_cmdBuf kidoRecordWriteBack:dst offset:destinationOffset length:size];

    struct kmtl_cmd_blit_copy cmd;
    cmd.hdr.op     = KMTL_CMD_BLIT_COPY_BUFFER;
    cmd.hdr.flags  = 0;
    cmd.hdr.size   = (uint32_t)sizeof(cmd);
    cmd.src_handle = [src kidoHandle];
    cmd.src_offset = (uint64_t)sourceOffset;
    cmd.dst_handle = [dst kidoHandle];
    cmd.dst_offset = (uint64_t)destinationOffset;
    cmd.size       = (uint64_t)size;
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)fillBuffer:(id)buffer range:(NSRange)range value:(uint8_t)value
{
    if (_ended || ![buffer isKindOfClass:[KidoMTLBuffer class]])
        return;
    KidoMTLBuffer *dst = (KidoMTLBuffer *)buffer;
    [_cmdBuf kidoRetainObject:dst];
    [_cmdBuf kidoRecordWriteBack:dst offset:range.location length:range.length];

    struct kmtl_cmd_blit_fill cmd;
    cmd.hdr.op     = KMTL_CMD_BLIT_FILL_BUFFER;
    cmd.hdr.flags  = 0;
    cmd.hdr.size   = (uint32_t)sizeof(cmd);
    cmd.dst_handle = [dst kidoHandle];
    cmd.offset     = (uint64_t)range.location;
    cmd.size       = (uint64_t)range.length;
    cmd.value      = value;
    memset(cmd.pad, 0, sizeof(cmd.pad));
    [_cmdBuf kidoAppendCommand:&cmd length:sizeof(cmd)];
}

- (void)synchronizeResource:(id)resource { (void)resource; }
- (void)updateFence:(id)fence { (void)fence; }
- (void)waitForFence:(id)fence { (void)fence; }
- (void)insertDebugSignpost:(NSString *)string { (void)string; }
- (void)pushDebugGroup:(NSString *)string { (void)string; }
- (void)popDebugGroup {}

- (void)endEncoding
{
    if (_ended)
        return;
    _ended = YES;
    struct kmtl_cmd_hdr hdr;
    hdr.op    = KMTL_CMD_END_ENCODER;
    hdr.flags = 0;
    hdr.size  = (uint32_t)sizeof(hdr);
    [_cmdBuf kidoAppendCommand:&hdr length:sizeof(hdr)];
}

@end

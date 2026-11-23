/*
 * IOVirtIONet.cpp - virtio-net Ethernet over IOVirtIOTransport (VIRTIO 1.1,
 * section 5.1).
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include "IOVirtIONet.h"
#include <IOKit/assert.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOLib.h>
#include <IOKit/network/IONetworkMedium.h>
#include <IOKit/network/IOOutputQueue.h>
#include <kern/thread.h>
#include <libkern/OSByteOrder.h>

// IONetworkStack.h: kIONetworkStackUserCommandKey, kIONetworkStackRegisterInterfaceAll
static const char *const kStackCommandKey = "IONetworkStackUserCommand";
static const unsigned long long kStackRegisterAll = 2;

// A guest without configd has no InterfaceNamer, and IONetworkStack leaves the interface
// unnamed (no en0) until someone asks: ask the way IORegisterNetworkInterface does, for up to 60 s
static void
nameInterfaceThread(void *arg, wait_result_t)
{
    IOService *netif = (IOService *)arg;

    for (int i = 0; i < 120; i++) {
        OSObject *name = netif->copyProperty(kIOBSDNameKey);
        bool named = OSDynamicCast(OSString, name) != NULL;
        OSSafeReleaseNULL(name);
        if (named)
            break;

        OSDictionary *match = IOService::serviceMatching("IONetworkStack");
        IOService *stack = match ? IOService::copyMatchingService(match) : NULL;
        OSDictionary *cmd = OSDictionary::withCapacity(1);
        OSNumber *all = OSNumber::withNumber(kStackRegisterAll, 32);
        if (stack && cmd && all) {
            cmd->setObject(kStackCommandKey, all);
            stack->setProperties(cmd);
        }
        OSSafeReleaseNULL(all);
        OSSafeReleaseNULL(cmd);
        OSSafeReleaseNULL(stack);
        OSSafeReleaseNULL(match);
        IOSleep(500);
    }
    netif->release();
    thread_terminate(current_thread());
}

#define super IOEthernetController
OSDefineMetaClassAndStructors(IOVirtIONet, IOEthernetController);

enum {
    // VIRTIO_NET_F_MAC (bit 5): device config's mac[] field is valid and
    // should be used as the interface's persistent address.
    VIRTIO_NET_F_MAC = (1u << 5),
    // VIRTIO_NET_F_MTU (bit 3): config's mtu is the device's maximum. Virtualization.framework's
    // device clears FEATURES_OK unless the driver accepts it; 1500 fits the receive buffers.
    VIRTIO_NET_F_MTU = (1u << 3),
};

// Without an interrupt the rings are polled every kPollMs. With one, the timer only catches an
// interrupt that never comes: kLostTicks ticks in a row that find frames no interrupt reported
// put the driver back to polling.
static const unsigned kPollMs = 5;
static const unsigned kFallbackPollMs = 50;
static const unsigned kLostTicks = 3;
static const uint16_t VRING_AVAIL_F_NO_INTERRUPT = 1;

bool IOVirtIONet::init(OSDictionary *properties)
{
    if (!super::init(properties))
        return false;

    fPCIDevice = NULL;
    fWorkLoop = NULL;
    fPollTimer = NULL;
    fInterruptSource = NULL;
    fInterface = NULL;
    bzero(&fRxQ, sizeof(fRxQ));
    bzero(&fTxQ, sizeof(fTxQ));
    bzero(fRxBuf, sizeof(fRxBuf));
    bzero(fTxBuf, sizeof(fTxBuf));
    fRxCount = fTxCount = 0;
    fTxNextSlot = 0;
    fTxOutstanding = 0;
    fTxLock = NULL;
    fUseInterrupts = false;
    fInterrupts = fInterruptsAtTick = fLostTicks = 0;
    fEnabled = false;
    fRxPackets = fTxPackets = 0;
    bzero(&fMACAddress, sizeof(fMACAddress));
    return true;
}

IOService *IOVirtIONet::probe(IOService *provider, SInt32 *score)
{
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci)
        return NULL;

    UInt16 vendor = pci->configRead16(kIOPCIConfigVendorID);
    UInt16 device = pci->configRead16(kIOPCIConfigDeviceID);
    IOLog("IOVirtIONet: probe vendor=0x%04x device=0x%04x\n", vendor, device);

    // Red Hat/Virtio vendor. Modern virtio-net is device id 0x1041;
    // legacy/transitional is 0x1000 - match both.
    if (vendor != 0x1af4)
        return NULL;
    if (device != 0x1041 && device != 0x1000)
        return NULL;

    if (score)
        *score = 5000;
    return this;
}

bool IOVirtIONet::readMACAddress()
{
    volatile uint8_t *cfg = fTransport.deviceConfig();
    if (!cfg)
        return false;

    for (int i = 0; i < 6; i++)
        fMACAddress.bytes[i] = cfg[i];
    return true;
}

void IOVirtIONet::postRxBuffer(unsigned slot)
{
    VirtIOChainEntry entry = { fRxBuf[slot]->getPhysicalAddress(),
                              (uint32_t)(kVNetHdrSize + kVNetMaxPacket), true };
    fTransport.addDescChain(&fRxQ, &entry, 1);
}

bool IOVirtIONet::initRxRing()
{
    for (unsigned i = 0; i < fRxCount; i++) {
        fRxBuf[i] = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
            kVNetHdrSize + kVNetMaxPacket, 0xFFFFFFFFULL);
        if (!fRxBuf[i])
            return false;
        bzero(fRxBuf[i]->getBytesNoCopy(), kVNetHdrSize + kVNetMaxPacket);
        postRxBuffer(i); // addDescChain's cyclic head index == i here (fresh queue)
    }
    return true;
}

bool IOVirtIONet::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;

    fPCIDevice->retain();
    if (!fPCIDevice->open(this)) {
        IOLog("IOVirtIONet: failed to open PCI device\n");
        return false;
    }

    // intx stays masked until setupInterrupt() has a handler: an unacknowledged level line stays high
    fPCIDevice->configWrite16(kIOPCIConfigCommand,
        fPCIDevice->configRead16(kIOPCIConfigCommand) | kIOPCICommandInterruptDisable);

    if (!fTransport.attach(fPCIDevice)) {
        IOLog("IOVirtIONet: failed to find/map virtio-pci capabilities\n");
        return false;
    }
    if (!fTransport.negotiateFeatures(fTransport.deviceFeaturesLow() & (VIRTIO_NET_F_MAC | VIRTIO_NET_F_MTU))) {
        IOLog("IOVirtIONet: device rejected feature negotiation\n");
        return false;
    }

    if (!readMACAddress()) {
        IOLog("IOVirtIONet: failed to read MAC from device config\n");
        return false;
    }
    IOLog("IOVirtIONet: MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
        fMACAddress.bytes[0], fMACAddress.bytes[1], fMACAddress.bytes[2],
        fMACAddress.bytes[3], fMACAddress.bytes[4], fMACAddress.bytes[5]);

    if (!fTransport.initQueue(&fRxQ, 0, kVNetRxCount) ||
        !fTransport.initQueue(&fTxQ, 1, kVNetTxCount)) {
        IOLog("IOVirtIONet: failed to init virtqueues\n");
        return false;
    }
    fRxCount = fRxQ.queueSize;
    fTxCount = fTxQ.queueSize;
    // tx completions are collected when sending and on every rx pass, never worth an interrupt
    ((VRingAvailHdr *)fTxQ.avail)->flags = VRING_AVAIL_F_NO_INTERRUPT;
    fTxLock = IOLockAlloc();
    if (!fTxLock)
        return false;
    for (unsigned i = 0; i < fTxCount; i++) {
        fTxBuf[i] = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
            kVNetHdrSize + kVNetMaxPacket, 0xFFFFFFFFULL);
        if (!fTxBuf[i])
            return false;
    }
    if (!initRxRing()) {
        IOLog("IOVirtIONet: failed to allocate RX ring\n");
        return false;
    }

    fTransport.setDriverOk();
    fTransport.notify(&fRxQ); // kick the device to start filling posted RX buffers

    fWorkLoop = getWorkLoop();
    if (!fWorkLoop)
        return false;

    fPollTimer = IOTimerEventSource::timerEventSource(this, &IOVirtIONet::pollTimerAction);
    if (!fPollTimer || fWorkLoop->addEventSource(fPollTimer) != kIOReturnSuccess) {
        IOLog("IOVirtIONet: failed to create poll timer\n");
        return false;
    }
    if (!setupInterrupt())
        IOLog("IOVirtIONet: no interrupt, polling every %u ms\n", kPollMs);
    IOLog("IOVirtIONet: %u rx and %u tx buffers%s\n", fRxCount, fTxCount,
          fUseInterrupts ? ", interrupt driven" : "");

    if (!publishLinkMedium())
        return false;

    if (!attachInterface((IONetworkInterface **)&fInterface, true)) {
        IOLog("IOVirtIONet: attachInterface failed\n");
        return false;
    }
    setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive, getCurrentMedium());

    registerService();

    thread_t namer;
    fInterface->retain();
    if (kernel_thread_start(nameInterfaceThread, fInterface, &namer) == KERN_SUCCESS)
        thread_deallocate(namer);
    else
        fInterface->release();
    return true;
}

void IOVirtIONet::stop(IOService *provider)
{
    if (fEnabled)
        disable(fInterface);

    if (fInterface) {
        detachInterface(fInterface, true);
        fInterface->release();
        fInterface = NULL;
    }
    if (fWorkLoop && fPollTimer)
        fWorkLoop->removeEventSource(fPollTimer);
    if (fWorkLoop && fInterruptSource) {
        if (fUseInterrupts)
            stopInterrupt();
        fWorkLoop->removeEventSource(fInterruptSource);
    }

    super::stop(provider);
}

void IOVirtIONet::free()
{
    for (int i = 0; i < kVNetRxCount; i++)
        if (fRxBuf[i]) { fRxBuf[i]->release(); fRxBuf[i] = NULL; }
    for (int i = 0; i < kVNetTxCount; i++)
        if (fTxBuf[i]) { fTxBuf[i]->release(); fTxBuf[i] = NULL; }

    fTransport.freeQueue(&fRxQ);
    fTransport.freeQueue(&fTxQ);
    fTransport.detach();

    if (fPollTimer) { fPollTimer->release(); fPollTimer = NULL; }
    OSSafeReleaseNULL(fInterruptSource);
    if (fTxLock) { IOLockFree(fTxLock); fTxLock = NULL; }

    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
        fPCIDevice = NULL;
    }

    super::free();
}

bool IOVirtIONet::publishLinkMedium()
{
    OSDictionary *mediumDict = OSDictionary::withCapacity(2);
    IONetworkMedium *autoMedium = NULL;
    IONetworkMedium *gigMedium = NULL;
    bool ok = false;

    if (!mediumDict)
        return false;

    autoMedium = IONetworkMedium::medium(
        kIOMediumEthernetAuto | kIOMediumOptionFullDuplex, 1000000000ULL);
    gigMedium = IONetworkMedium::medium(
        kIOMediumEthernet1000BaseT | kIOMediumOptionFullDuplex, 1000000000ULL);

    if (autoMedium && gigMedium &&
        IONetworkMedium::addMedium(mediumDict, autoMedium) &&
        IONetworkMedium::addMedium(mediumDict, gigMedium) &&
        publishMediumDictionary(mediumDict) &&
        setCurrentMedium(autoMedium)) {
        ok = true;
    }

    if (gigMedium) gigMedium->release();
    if (autoMedium) autoMedium->release();
    mediumDict->release();

    if (!ok)
        IOLog("IOVirtIONet: failed to publish link medium\n");
    return ok;
}

IOReturn IOVirtIONet::enable(IONetworkInterface *interface)
{
    if (fEnabled)
        return kIOReturnSuccess;
    IOLog("IOVirtIONet: enable interface=%p\n", interface);
    if (fPollTimer)
        fPollTimer->setTimeoutMS(kPollMs);
    fEnabled = true;
    return kIOReturnSuccess;
}

IOReturn IOVirtIONet::disable(IONetworkInterface *interface)
{
    if (!fEnabled)
        return kIOReturnSuccess;
    IOLog("IOVirtIONet: disable interface=%p\n", interface);
    if (fPollTimer)
        fPollTimer->cancelTimeout();
    fEnabled = false;
    return kIOReturnSuccess;
}

void IOVirtIONet::pollTimerAction(OSObject *owner, IOTimerEventSource *sender)
{
    IOVirtIONet *self = OSDynamicCast(IOVirtIONet, owner);
    if (!self)
        return;
    unsigned received = self->pollReceive();
    if (self->fUseInterrupts)
        self->checkLostInterrupt(received);
    if (self->fEnabled)
        sender->setTimeoutMS(self->fUseInterrupts ? kFallbackPollMs : kPollMs);
}

// The pci nub's first interrupt is the legacy intx line, as in IOVirtIOBlock::setupInterrupt.
// start() masks intx until this handler exists, so a level line is never left asserted.
bool IOVirtIONet::setupInterrupt()
{
    int type = 0;

    if (fPCIDevice->getInterruptType(0, &type) != kIOReturnSuccess)
        return false;
    fInterruptSource = IOInterruptEventSource::interruptEventSource(this,
        OSMemberFunctionCast(IOInterruptEventAction, this, &IOVirtIONet::interruptOccurred),
        fPCIDevice, 0);
    if (!fInterruptSource || fWorkLoop->addEventSource(fInterruptSource) != kIOReturnSuccess) {
        OSSafeReleaseNULL(fInterruptSource);
        return false;
    }
    fUseInterrupts = true;
    fInterruptSource->enable();
    fPCIDevice->configWrite16(kIOPCIConfigCommand,
        fPCIDevice->configRead16(kIOPCIConfigCommand) & ~kIOPCICommandInterruptDisable);
    return true;
}

void IOVirtIONet::stopInterrupt()
{
    fUseInterrupts = false;
    fPCIDevice->configWrite16(kIOPCIConfigCommand,
        fPCIDevice->configRead16(kIOPCIConfigCommand) | kIOPCICommandInterruptDisable);
    fInterruptSource->disable();
}

// Runs on the work loop. Frames the timer finds although no interrupt arrived since its last tick
// mean the line is not delivered; a frame racing the interrupt resets the count on the next tick.
void IOVirtIONet::checkLostInterrupt(unsigned received)
{
    if (received == 0 || fInterrupts != fInterruptsAtTick) {
        fLostTicks = 0;
    } else if (++fLostTicks >= kLostTicks) {
        IOLog("IOVirtIONet: frames arrive without an interrupt, polling every %u ms\n", kPollMs);
        stopInterrupt();
    }
    fInterruptsAtTick = fInterrupts;
}

// Runs on the work loop with the level line masked; reading the isr acknowledges it and drops
// the line, so it is read before the used ring and a frame landing meanwhile raises it again.
void IOVirtIONet::interruptOccurred(IOInterruptEventSource *, int)
{
    if (!(fTransport.readIsr() & 1))
        return;
    fInterrupts++;
    if (fEnabled)
        pollReceive();
}

unsigned IOVirtIONet::pollReceive()
{
    unsigned received = 0;
    uint32_t len; uint16_t id;

    // one ring's worth per pass, so the input queue is flushed under sustained load; frames
    // left behind arrived during the pass and raised the interrupt again
    while (received < fRxCount && fTransport.pollForCompletion(&fRxQ, 0, &len, &id)) {
        if (id < fRxCount && len > kVNetHdrSize && len <= kVNetHdrSize + kVNetMaxPacket) {
            size_t pktLen = len - kVNetHdrSize;
            uint8_t *payload = (uint8_t *)fRxBuf[id]->getBytesNoCopy() + kVNetHdrSize;

            mbuf_t m = allocatePacket((UInt32)pktLen);
            if (m) {
                if (mbuf_copyback(m, 0, pktLen, payload, MBUF_WAITOK) == 0) {
                    mbuf_pkthdr_setlen(m, pktLen);
                    mbuf_setlen(m, pktLen);
                }
                fRxPackets++;
                if (fInterface)
                    fInterface->inputPacket(m, (UInt32)pktLen, IONetworkInterface::kInputOptionQueuePacket);
            }
        }
        if (id < fRxCount)
            postRxBuffer(id); // hand the same buffer back to the device
        received++;
    }

    // tx completions raise no interrupt, so every pass collects them too
    IOLockLock(fTxLock);
    reclaimTxLocked();
    IOLockUnlock(fTxLock);

    if (received)
        fTransport.notify(&fRxQ);
    if (fInterface)
        fInterface->flushInputQueue();
    return received;
}

// caller holds fTxLock
void IOVirtIONet::reclaimTxLocked()
{
    while (fTransport.pollForCompletion(&fTxQ, 0, 0, 0)) {
        if (fTxOutstanding) fTxOutstanding--;
    }
}

UInt32 IOVirtIONet::outputPacket(mbuf_t m, void *param)
{
    size_t pktLen = mbuf_pkthdr_len(m);
    if (pktLen == 0 || pktLen > kVNetMaxPacket) {
        freePacket(m);
        return kIOReturnOutputDropped;
    }

    IOLockLock(fTxLock);
    reclaimTxLocked();
    if (fTxOutstanding >= fTxCount) {
        // Ring full - give the device a brief window to drain before
        // dropping (no backpressure signaling to the output queue yet).
        uint32_t len; uint16_t id;
        if (fTransport.pollForCompletion(&fTxQ, 50, &len, &id) && fTxOutstanding)
            fTxOutstanding--;
        if (fTxOutstanding >= fTxCount) {
            IOLockUnlock(fTxLock);
            freePacket(m);
            return kIOReturnOutputDropped;
        }
    }

    unsigned slot = fTxNextSlot;
    uint8_t *buf = (uint8_t *)fTxBuf[slot]->getBytesNoCopy();
    bzero(buf, kVNetHdrSize); // zeroed virtio_net_hdr_v1: no offload/GSO
    mbuf_copydata(m, 0, pktLen, buf + kVNetHdrSize);

    VirtIOChainEntry entry = { fTxBuf[slot]->getPhysicalAddress(),
                              (uint32_t)(kVNetHdrSize + pktLen), false };
    fTransport.addDescChain(&fTxQ, &entry, 1);
    fTransport.notify(&fTxQ);

    fTxOutstanding++;
    fTxNextSlot = (fTxNextSlot + 1) % fTxCount;
    fTxPackets++;
    IOLockUnlock(fTxLock);

    freePacket(m);
    return kIOReturnOutputSuccess;
}

IOReturn IOVirtIONet::getHardwareAddress(IOEthernetAddress *addr)
{
    if (!addr)
        return kIOReturnBadArgument;
    *addr = fMACAddress;
    return kIOReturnSuccess;
}

IOReturn IOVirtIONet::setPromiscuousMode(bool active)
{
    // No VIRTIO_NET_F_CTRL_VQ/CTRL_RX negotiated - promiscuous/multicast
    // filtering isn't controllable; the device already delivers what it
    // sees on QEMU's default usermode/tap backends.
    return kIOReturnSuccess;
}

IOReturn IOVirtIONet::setMulticastMode(bool active)
{
    return kIOReturnSuccess;
}

IOReturn IOVirtIONet::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
    return kIOReturnSuccess;
}

const OSString *IOVirtIONet::newVendorString() const
{
    return OSString::withCString("Red Hat / Virtio");
}

const OSString *IOVirtIONet::newModelString() const
{
    return OSString::withCString("virtio-net (IOVirtIONet)");
}

IOOutputQueue *IOVirtIONet::createOutputQueue()
{
    // Matches PDE1000: this tree's IONetworkController doesn't start the
    // optional IOOutputQueue during doEnable(), so use the legacy direct
    // output handler until the generic queue lifecycle is wired.
    return NULL;
}

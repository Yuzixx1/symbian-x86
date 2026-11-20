// qemu_virtio_net.cpp -- minimal polled virtio-net (PCI 1AF4:1000/1041) driver
// for the full Kern image on QEMU q35. Variant-owned; an ALTERNATIVE NIC
// backend to qemu_nic.cpp (e1000). Neither qemu_nic.cpp nor qemu_netbridge.cpp
// is touched. Polled only: no MSI-X / IRQ wiring.
//
// Discovery scans bus 0 for vendor 1AF4 (modern 1040..107F or legacy 1000),
// falls back to class 02.00 + subsystem device 1, then reads the virtio-1.0
// PCI capability list in BAR0 to locate COMMON / ISR / NOTIFY / DEVICE-CFG.
// A legacy (no-cap) device uses fixed offsets 0/0x1000/0x2000/0x3000.
//
// Two split virtqueues (RX=0, TX=1) live in static 4K-aligned RAM. Identity
// mapped RAM => descriptor/avail/used DMA addresses are direct virtual
// addresses. A static constructor calls vnet_init() at boot from HwInit0's
// __init_array walk; all entry points are idempotent.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include "qemu_variant.h"

namespace
    {
    // ---- PCI identity -----------------------------------------------------
    const TUint32 kVendor = 0x1AF4u;
    const TUint32 kDevLegacy = 0x1000u;
    const TUint32 kDevModernLo = 0x1040u;
    const TUint32 kDevModernHi = 0x107Fu;
    const TUint32 kSubsysNet = 1u;

    // ---- virtio PCI capability cfg types ---------------------------------
    const TUint kCapCommon = 1, kCapNotify = 2, kCapIsr = 3, kCapDevice = 4;

    // ---- common-config register offsets (relative to gCommon) ------------
    const TUint32 kCommonDevFeatSel = 0x00;
    const TUint32 kCommonDevFeat    = 0x04;
    const TUint32 kCommonDrvFeatSel = 0x08;
    const TUint32 kCommonDrvFeat    = 0x0C;
    const TUint32 kCommonNumQueues  = 0x12;
    const TUint32 kCommonStatus     = 0x14;
    const TUint32 kCommonQueueSel   = 0x16;
    const TUint32 kCommonQueueSize  = 0x18;
    const TUint32 kCommonQueueEnable= 0x1C;
    const TUint32 kCommonQueueNotify= 0x1E;
    const TUint32 kCommonDescLo     = 0x20;
    const TUint32 kCommonDescHi     = 0x24;
    const TUint32 kCommonDriverLo   = 0x28;
    const TUint32 kCommonDriverHi   = 0x2C;
    const TUint32 kCommonDeviceLo   = 0x30;
    const TUint32 kCommonDeviceHi   = 0x34;

    // device status bits
    const TUint8 kStatusAck = 0x01, kStatusDriver = 0x02, kStatusOk = 0x04;

    // virtio-net feature bits (feature word 0)
    const TUint32 kFMac   = (1u << 5);    // VIRTIO_NET_F_MAC
    const TUint32 kFStatus= (1u << 16);   // VIRTIO_NET_F_STATUS

    // ---- queue geometry ---------------------------------------------------
    const TUint kQueueSize = 16;          // cap at 16 descriptors per queue
    const TUint kBufSize   = 2048;
    const TUint kPageSize  = 4096;
    const TUint kSpinTx    = 2000000;

    const TUint16 kDescFNext  = 0x1;
    const TUint16 kDescFWrite = 0x2;

    struct VqDesc
        {
        volatile TUint64 iAddr;
        volatile TUint32 iLen;
        volatile TUint16 iFlags;
        volatile TUint16 iNext;
        };
    struct AvailRing
        {
        volatile TUint16 iFlags;
        volatile TUint16 iIdx;
        volatile TUint16 iRing[kQueueSize];
        volatile TUint16 iUsedEvent;
        };
    struct UsedElem
        {
        volatile TUint32 iId;
        volatile TUint32 iLen;
        };
    struct UsedRing
        {
        volatile TUint16 iFlags;
        volatile TUint16 iIdx;
        volatile UsedElem iRing[kQueueSize];
        volatile TUint16 iAvailEvent;
        };
    struct VQueue
        {
        volatile VqDesc*   iDesc;
        volatile AvailRing* iAvail;
        volatile UsedRing*  iUsed;
        TUint16 iNum;        // active descriptor count
        TUint16 iNotifyOff;  // per-queue notify offset from common config
        TUint16 iAvailIdx;   // our monotonic available-ring cursor
        TUint16 iUsedSeen;   // last processed used-ring index
        };

    // Per-queue vring storage. Legacy devices derive the ring offsets from a
    // single page frame using the full reported queue size (often 1024), so
    // reserve 64 KiB per queue; modern devices use the three explicit pages.
    __attribute__((aligned(4096))) static TUint8 gVqMem[2][64 * 1024];
    __attribute__((aligned(16)))   static TUint8 gTxBuf[kQueueSize][kBufSize];
    __attribute__((aligned(16)))   static TUint8 gRxBuf[kQueueSize][kBufSize];

    static VQueue gRx;
    static VQueue gTx;

    static TUint32 gBar0 = 0;
    static TUint32 gCommon = 0, gIsr = 0, gNotify = 0, gDevCfg = 0;
    static TUint32 gNotifyMul = 0;
    static TBool   gUseIo = EFalse;
    static TBool   gHaveCaps = EFalse;
    static TBool   gLegacy = EFalse;

    // Effective register offsets: modern uses capability-relative common-cfg
    // offsets; legacy (virtio 0.9.5) uses the fixed PCI I/O BAR map.
    static TUint32 gRDevFeat = 0, gRDrvFeat = 0, gRStatus = 0;
    static TUint32 gRQueueSel = 0, gRQueueSize = 0, gRQueueNotify = 0;
    static TUint32 gRDescLo = 0;
    static TUint8  gMac[6];
    static TUint32 gFeatures = 0;
    static TBool   gInit = EFalse;

    // ---- register access (MMIO or legacy I/O BAR) -------------------------
    static inline void Out8(unsigned short aP, unsigned char aV)
        { __asm__ __volatile__("out %1, %0" : : "a"(aV), "Nd"(aP)); }
    static inline unsigned char In8(unsigned short aP)
        { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aP)); return v; }
    static inline void Out16(unsigned short aP, unsigned short aV)
        { __asm__ __volatile__("out %1, %0" : : "a"(aV), "Nd"(aP)); }
    static inline unsigned short In16(unsigned short aP)
        { unsigned short v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aP)); return v; }
    static inline void Out32(unsigned short aP, unsigned int aV)
        { __asm__ __volatile__("out %1, %0" : : "a"(aV), "Nd"(aP)); }
    static inline unsigned int In32(unsigned short aP)
        { unsigned int v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aP)); return v; }

    static inline TUint8 Rd8(TUint32 aOff)
        {
        if (gUseIo) return (TUint8)In8((unsigned short)(gBar0 + aOff));
        return *(volatile TUint8*)(gBar0 + aOff);
        }
    static inline TUint16 Rd16(TUint32 aOff)
        {
        if (gUseIo) return (TUint16)In16((unsigned short)(gBar0 + aOff));
        return *(volatile TUint16*)(gBar0 + aOff);
        }
    static inline TUint32 Rd32(TUint32 aOff)
        {
        if (gUseIo) return (TUint32)In32((unsigned short)(gBar0 + aOff));
        return *(volatile TUint32*)(gBar0 + aOff);
        }
    static inline void Wr8(TUint32 aOff, TUint8 aVal)
        {
        if (gUseIo) Out8((unsigned short)(gBar0 + aOff), (unsigned char)aVal);
        else *(volatile TUint8*)(gBar0 + aOff) = aVal;
        }
    static inline void Wr16(TUint32 aOff, TUint16 aVal)
        {
        if (gUseIo) Out16((unsigned short)(gBar0 + aOff), (unsigned short)aVal);
        else *(volatile TUint16*)(gBar0 + aOff) = aVal;
        }
    static inline void Wr32(TUint32 aOff, TUint32 aVal)
        {
        if (gUseIo) Out32((unsigned short)(gBar0 + aOff), (unsigned int)aVal);
        else *(volatile TUint32*)(gBar0 + aOff) = aVal;
        }
    static inline void Barrier()
        { __asm__ __volatile__("" : : : "memory"); }

    // ---- PCI discovery ----------------------------------------------------
    static TBool FindVirtio(TUint& aBus, TUint& aDev, TUint& aFn)
        {
        for (TUint d = 0; d < 32; ++d)
            {
            TUint32 id0 = PciRead32(0, d, 0, 0);
            if ((id0 & 0xFFFFu) == 0xFFFFu)
                continue;
            TUint fns = (((PciRead32(0, d, 0, 0x0C) >> 16) & 0x80u) != 0) ? 8u : 1u;
            for (TUint f = 0; f < fns; ++f)
                {
                TUint32 id = PciRead32(0, d, f, 0);
                TUint32 vend = id & 0xFFFFu;
                TUint32 did  = (id >> 16) & 0xFFFFu;
                TUint32 subs = (PciRead32(0, d, f, 0x2C) >> 16) & 0xFFFFu;
                TBool match = EFalse;
                if (vend == kVendor &&
                    (did == kDevLegacy || (did >= kDevModernLo && did <= kDevModernHi)))
                    match = ETrue;
                if (!match && subs == kSubsysNet &&
                    ((PciRead32(0, d, f, 8) >> 24) & 0xFFu) == 0x02u)
                    match = ETrue;          // class 02 = network
                if (match)
                    { aBus = 0; aDev = d; aFn = f; return ETrue; }
                }
            }
        return EFalse;
        }

    // Walk the vendor-specific capability list to find the BAR0 offsets.
    static void ParseCaps(TUint aBus, TUint aDev, TUint aFn)
        {
        gHaveCaps = EFalse;
        if (!(PciRead32(aBus, aDev, aFn, 4) & 0x10u))
            return;                          // no capability list
        TUint cap = PciRead32(aBus, aDev, aFn, 0x34) & 0xFFu;
        TInt guard = 0;
        while (cap >= 0x40 && cap < 0x100 && guard++ < 32)
            {
            TUint32 a = PciRead32(aBus, aDev, aFn, cap);
            if ((a & 0xFFu) == 0x09u)         // PCI_CAP_ID_VNDR
                {
                TUint type = (a >> 16) & 0xFFu;
                TUint bar  = (a >> 24) & 0xFFu;
                TUint32 off = PciRead32(aBus, aDev, aFn, cap + 4);
                if (bar == 0)
                    {
                    if (type == kCapCommon) gCommon = off;
                    else if (type == kCapIsr) gIsr = off;
                    else if (type == kCapNotify) gNotify = off;
                    else if (type == kCapDevice) gDevCfg = off;
                    gHaveCaps = ETrue;
                    }
                if (type == kCapNotify)
                    gNotifyMul = PciRead32(aBus, aDev, aFn, cap + 12);
                }
            cap = (a >> 8) & 0xFFu;
            }
        }

    // ---- queue helpers ----------------------------------------------------
    static void Zero(VQueue& aQ)
        {
        for (TUint i = 0; i < kQueueSize; ++i)
            {
            aQ.iDesc[i].iAddr = 0; aQ.iDesc[i].iLen = 0;
            aQ.iDesc[i].iFlags = 0; aQ.iDesc[i].iNext = 0;
            aQ.iAvail->iRing[i] = 0;
            aQ.iUsed->iRing[i].iId = 0; aQ.iUsed->iRing[i].iLen = 0;
            }
        aQ.iAvail->iFlags = 0; aQ.iAvail->iIdx = 0; aQ.iAvail->iUsedEvent = 0;
        aQ.iUsed->iFlags = 0; aQ.iUsed->iIdx = 0; aQ.iUsed->iAvailEvent = 0;
        }

    static TInt SetupQueue(TUint16 aIndex, VQueue& aQ, TUint8* aMem)
        {
        Wr16(gRQueueSel, aIndex);
        TUint qsz = Rd16(gRQueueSize);
        if (!qsz)
            return KErrNotSupported;
        TUint use = qsz < kQueueSize ? qsz : kQueueSize;   // we drive <=16 slots
        aQ.iNum = (TUint16)use;
        aQ.iNotifyOff = gLegacy ? 0 : Rd16(gRQueueNotify);
        aQ.iAvailIdx = 0;
        aQ.iUsedSeen = 0;

        if (gLegacy)
            {
            // Legacy vring: avail follows desc; used is 4K-aligned after
            // avail. QEMU derives all three from the single page-frame
            // number using the *full* reported queue size, so use qsz here.
            aQ.iDesc  = (volatile VqDesc*)(aMem);
            aQ.iAvail = (volatile AvailRing*)(aMem + qsz * sizeof(VqDesc));
            TUint usedOff = qsz * sizeof(VqDesc) + 2 + 2 + qsz * 2 + 2;
            usedOff = (usedOff + kPageSize - 1) & ~(kPageSize - 1);
            aQ.iUsed  = (volatile UsedRing*)(aMem + usedOff);
            }
        else
            {
            // Modern: device consumes the three pointers we program.
            aQ.iDesc  = (volatile VqDesc*)(aMem);
            aQ.iAvail = (volatile AvailRing*)(aMem + kPageSize);
            aQ.iUsed  = (volatile UsedRing*)(aMem + 2 * kPageSize);
            }
        Zero(aQ);

        if (gLegacy)
            {
            // Queue Address is a guest page-frame number (address >> 12).
            Wr32(gRDescLo, (TUint32)aQ.iDesc >> 12);
            }
        else
            {
            Wr32(gRDescLo,      (TUint32)aQ.iDesc);
            Wr32(gRDescLo + 4,  0);
            Wr32(kCommonDriverLo, (TUint32)aQ.iAvail); Wr32(kCommonDriverHi, 0);
            Wr32(kCommonDeviceLo, (TUint32)aQ.iUsed);  Wr32(kCommonDeviceHi, 0);
            Wr16(kCommonQueueEnable, 1);
            }
        Barrier();
        return KErrNone;
        }

    // Notify the device that new available-ring entries are ready.
    static void Kick(TUint16 aQueueIndex, const VQueue& aQ)
        {
        TUint32 addr = gNotify + (TUint32)aQ.iNotifyOff * gNotifyMul;
        Wr16(addr, aQueueIndex);
        }

    static void PostRx(TUint16 aId)
        {
        TUint idx = (TUint)(gRx.iAvailIdx % gRx.iNum);
        volatile VqDesc& d = gRx.iDesc[idx];
        d.iAddr = (TUint64)(TUint32)gRxBuf[aId];
        d.iLen = kBufSize;
        d.iFlags = kDescFWrite;
        d.iNext = 0;
        Barrier();
        gRx.iAvail->iRing[idx] = aId;
        Barrier();
        ++gRx.iAvail->iIdx;
        ++gRx.iAvailIdx;
        Barrier();
        }
    }  // namespace

// ------------------------------------------------------------------ public
TInt vnet_init()
    {
    if (gInit)
        return gBar0 ? KErrNone : KErrNotFound;
    gInit = ETrue;

    TUint bus = 0, dev = 0, fn = 0;
    if (!FindVirtio(bus, dev, fn))
        { KPrintf("virtio-net: no PCI 1AF4 NIC found\r\n"); return KErrNotFound; }

    TUint32 barRaw = PciRead32(bus, dev, fn, 0x10);
    gUseIo = TBool((barRaw & 1u) != 0);        // bit0 => I/O BAR (legacy)
    // I/O BAR bit0 is the space indicator, not part of the port base.
    gBar0 = barRaw & (gUseIo ? 0xFFFFFFFCu : 0xFFFFFFF0u);
    if (!gBar0)
        { KPrintf("virtio-net: BAR0 unassigned\r\n"); return KErrGeneral; }
    PciWrite32(bus, dev, fn, 0x04,
               PciRead32(bus, dev, fn, 0x04) | 0x06u);   // MEM|bus-master

    ParseCaps(bus, dev, fn);
    gLegacy = !gHaveCaps;                       // no vendor caps => virtio 0.9.5
    gCommon = 0;

    if (gLegacy)
        {
        // Fixed legacy PCI I/O-BAR map (Linux VIRTIO_PCI_* offsets).
        gIsr = 0x13; gNotify = 0x10; gDevCfg = 0x14; gNotifyMul = 0;
        gRDevFeat = 0x00; gRDrvFeat = 0x04; gRStatus = 0x12;
        gRQueueSel = 0x0E; gRQueueSize = 0x0C; gRQueueNotify = 0xFFFFu;
        gRDescLo = 0x08;
        }
    else
        {
        gRDevFeat    = gCommon + kCommonDevFeat;
        gRDrvFeat    = gCommon + kCommonDrvFeat;
        gRStatus     = gCommon + kCommonStatus;
        gRQueueSel   = gCommon + kCommonQueueSel;
        gRQueueSize  = gCommon + kCommonQueueSize;
        gRQueueNotify= gCommon + kCommonQueueNotify;
        gRDescLo     = gCommon + kCommonDescLo;
        }

    // Reset then bring the device through ACK -> DRIVER.
    Wr8(gRStatus, 0);
    for (TInt i = 0; i < 100000 && Rd8(gRStatus); ++i) { }
    Wr8(gRStatus, kStatusAck);
    Wr8(gRStatus, (TUint8)(kStatusAck | kStatusDriver));

    // Negotiate feature word 0: MAC (5) and STATUS (16) when offered.
    if (!gLegacy)
        Wr8(gCommon + kCommonDevFeatSel, 0);
    TUint32 offered = Rd32(gRDevFeat);
    gFeatures = offered & (kFMac | kFStatus);
    if (!gLegacy)
        Wr8(gCommon + kCommonDrvFeatSel, 0);
    Wr32(gRDrvFeat, gFeatures);

    TInt r = SetupQueue(0, gRx, gVqMem[0]);
    if (r != KErrNone)
        { KPrintf("virtio-net: RX queue setup failed %d\r\n", r); return r; }
    r = SetupQueue(1, gTx, gVqMem[1]);
    if (r != KErrNone)
        { KPrintf("virtio-net: TX queue setup failed %d\r\n", r); return r; }

    // Pre-post all RX descriptors to the device (WRITE = device fills them).
    for (TUint i = 0; i < gRx.iNum; ++i)
        PostRx((TUint16)i);
    Kick(0, gRx);

    // Read MAC from the device-specific config (virtio-net: mac at offset 0).
    for (TUint i = 0; i < 6; ++i)
        gMac[i] = Rd8(gDevCfg + i);

    Wr8(gRStatus, (TUint8)(kStatusAck | kStatusDriver | kStatusOk));

    KPrintf("virtio-net: BAR0=%x MAC=%02x:%02x:%02x:%02x:%02x:%02x queues ready\r\n",
        gBar0, gMac[0], gMac[1], gMac[2], gMac[3], gMac[4], gMac[5]);
    return KErrNone;
    }

TInt vnet_tx(const TUint8* aBuf, TUint aLen)
    {
    if (!gBar0 && vnet_init() != KErrNone)
        return KErrNotReady;
    if (!aBuf || !aLen || aLen > kBufSize)
        return KErrArgument;

    // Reclaim used TX slots so we never overwrite an in-flight buffer.
    while ((TUint16)(gTx.iAvailIdx - gTx.iUsedSeen) >= gTx.iNum)
        {
        if (gTx.iUsed->iIdx == gTx.iUsedSeen)
            return KErrTimedOut;             // ring full and device stalled
        ++gTx.iUsedSeen;
        }

    TUint16 id = (TUint16)(gTx.iAvailIdx % gTx.iNum);
    for (TUint i = 0; i < aLen; ++i)
        gTxBuf[id][i] = aBuf[i];

    TUint idx = (TUint)(gTx.iAvailIdx % gTx.iNum);
    volatile VqDesc& d = gTx.iDesc[idx];
    d.iAddr = (TUint64)(TUint32)gTxBuf[id];
    d.iLen = aLen;
    d.iFlags = 0;                             // device reads this buffer
    d.iNext = 0;
    Barrier();
    gTx.iAvail->iRing[idx] = id;
    Barrier();
    ++gTx.iAvail->iIdx;
    ++gTx.iAvailIdx;
    Barrier();
    Kick(1, gTx);

    // Polled completion: wait for this descriptor to appear in the used ring.
    TInt spin = 0;
    for (; spin < kSpinTx; ++spin)
        if (gTx.iUsed->iIdx != gTx.iUsedSeen)
            break;
    if (gTx.iUsed->iIdx != gTx.iUsedSeen)
        ++gTx.iUsedSeen;
    return KErrNone;
    }

TInt vnet_recv(TUint8* aBuf, TUint aMax)
    {
    if (!gBar0 && vnet_init() != KErrNone)
        return 0;
    if (!aBuf || !aMax)
        return 0;

    Barrier();
    if (gRx.iUsed->iIdx == gRx.iUsedSeen)
        return 0;                             // non-blocking: nothing ready

    volatile UsedElem& e = gRx.iUsed->iRing[gRx.iUsedSeen % gRx.iNum];
    TUint16 id = (TUint16)e.iId;
    TUint len = e.iLen;
    if (id >= kQueueSize)
        len = 0;
    if (len > kBufSize)
        len = kBufSize;
    if (len > aMax)
        len = aMax;
    for (TUint i = 0; i < len; ++i)
        aBuf[i] = gRxBuf[id][i];

    ++gRx.iUsedSeen;
    PostRx(id);                               // recycle buffer back to device
    Kick(0, gRx);
    return (TInt)len;
    }

void vnet_info()
    {
    KPrintf("vnet: BAR0=%x io=%d caps=%d legacy=%d features=%x\r\n",
        gBar0, (TInt)gUseIo, (TInt)gHaveCaps, (TInt)gLegacy, gFeatures);
    KPrintf("vnet: MAC=%02x:%02x:%02x:%02x:%02x:%02x\r\n",
        gMac[0], gMac[1], gMac[2], gMac[3], gMac[4], gMac[5]);
    KPrintf("vnet: RX n=%d avail=%d used=%d  TX n=%d avail=%d used=%d\r\n",
        (TInt)gRx.iNum, (TInt)gRx.iAvailIdx, (TInt)gRx.iUsedSeen,
        (TInt)gTx.iNum, (TInt)gTx.iAvailIdx, (TInt)gTx.iUsedSeen);
    KPrintf("vnet: cfg common=%x isr=%x notify=%x device=%x mul=%x\r\n",
        gCommon, gIsr, gNotify, gDevCfg, gNotifyMul);
    }

// Boot hook: HwInit0 walks the .init_array after paging + TheAsic are ready.
__attribute__((constructor)) static void VnetBootInit()
    { vnet_init(); }

// qemu_virtio_rng.cpp -- minimal polled virtio-RNG driver for the full Kern
// image on QEMU q35. Variant-owned; a genuinely NEW entropy source for the OS.
// Neither qemu_virtio_blk.cpp nor any other worker's file is touched.
//
// Discovery scans bus 0 for vendor 1AF4 (legacy device 1005, modern 1040..107F
// with subsystem device 4 = VIRTIO_ID_RNG). The virtio-1.0 PCI vendor capability
// list locates COMMON / ISR / NOTIFY / DEVICE-CFG inside whichever BAR QEMU
// placed them in. All regions are accessed with 8/16/32-bit MMIO or port I/O
// depending on their BAR type. A device without caps is driven through the
// fixed legacy 0.9.5 register map.
//
// One split virtqueue (index 0) lives in static 4K-aligned RAM. Identity-mapped
// RAM => descriptor/avail/used DMA addresses are direct virtual addresses.
// Every request is a single writable (device-fills) descriptor; we kick the
// notify BAR and poll the used ring -- no IRQ/MSI-X. A static constructor calls
// vrng_init() at boot from HwInit0's .init_array walk; all entry points are
// idempotent.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include "qemu_variant.h"

namespace
    {
    // ---- PCI identity -----------------------------------------------------
    const TUint32 kVendor = 0x1AF4u;
    const TUint32 kDevLegacy = 0x1005u;          // virtio-rng (virtio 0.9.5)
    const TUint32 kDevModernLo = 0x1040u;
    const TUint32 kDevModernHi = 0x107Fu;
    const TUint32 kSubsysRng = 4u;               // virtio device id 4 = RNG

    // ---- virtio PCI capability cfg types ----------------------------------
    const TUint kCapCommon = 1, kCapNotify = 2, kCapIsr = 3, kCapDevice = 4;
    const TUint8 kPciCapVendor = 0x09u;          // PCI_CAP_ID_VNDR
    const TUint32 kStatusCapList = 0x00100000u;  // dword@04: status bit 4

    // ---- common-config register offsets (relative to gCfgBase) ------------
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
    const TUint32 kCommonDriverLo   = 0x28;
    const TUint32 kCommonDeviceLo   = 0x30;

    // device status bits
    const TUint8 kStatusAck = 0x01, kStatusDriver = 0x02, kStatusOk = 0x04;

    // ---- queue geometry ---------------------------------------------------
    const TUint kQueueSize = 16;
    const TUint kMaxGet = 65536;                 // generous per-request cap
    const TUint kPageSize = 4096;
    const TUint kSpinCompletion = 2000000;
    const TUint kSpinReset = 100000;

    const TUint16 kDescFWrite = 0x2;              // VRING_DESC_F_WRITE

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
        TUint16 iNum;
        TUint16 iNotifyOff;
        TUint16 iAvailIdx;
        TUint16 iUsedSeen;
        };

    // Legacy derives ring offsets from a single page using the full reported
    // queue size (often 1024), so reserve 64 KiB just like qemu_virtio_blk.cpp.
    __attribute__((aligned(4096))) static TUint8 gVqMem[64 * 1024];
    static VQueue gQ;

    // Region bases (absolute linear addresses; identity-mapped RAM).
    static TUint32 gCfgBase = 0, gDevCfgBase = 0, gNotifyBase = 0, gIsrBase = 0;
    static TBool   gCfgIo = EFalse, gDevIo = EFalse, gNotifyIo = EFalse;
    static TUint32 gBar0 = 0;
    static TUint32 gNotifyMul = 1;
    static TBool   gUseIo = EFalse;
    static TBool   gHaveCaps = EFalse;
    static TBool   gLegacy = EFalse;
    static TUint32 gFeatures = 0;
    static TUint   gLastLen = 0;                 // bytes returned by last get
    static TBool   gInit = EFalse;
    static TUint   gQueueSizeReported = 0;

    // Register offsets (legacy fixed map or modern common-config offsets).
    static TUint32 gRDevFeat = 0, gRDrvFeat = 0, gRStatus = 0;
    static TUint32 gRQueueSel = 0, gRQueueSize = 0, gRDescLo = 0;

    // ---- raw port I/O -----------------------------------------------------
    static inline void Out8(unsigned short aP, unsigned char aV)
        { __asm__ __volatile__("out %1, %0" : : "a"(aV), "Nd"(aP)); }
    static inline unsigned char In8(unsigned short aP)
        { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aP)); return v; }
    static inline void Out16(unsigned short aP, unsigned short aV)
        { __asm__ __volatile__("out %1, %0" : : "a"(aV), "Nd"(aP)); }
    static inline void Out32(unsigned short aP, unsigned int aV)
        { __asm__ __volatile__("out %1, %0" : : "a"(aV), "Nd"(aP)); }
    static inline unsigned int In32(unsigned short aP)
        { unsigned int v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aP)); return v; }

    static inline void PciAddr(TUint aBus, TUint aDev, TUint aFn, TUint aReg)
        {
        Out32(0xCF8, 0x80000000u | (aBus << 16) | (aDev << 11) |
                     (aFn << 8) | (aReg & 0xFCu));
        }
    static TUint8 PciRead8(TUint aBus, TUint aDev, TUint aFn, TUint aReg)
        {
        PciAddr(aBus, aDev, aFn, aReg);
        return (TUint8)In8((unsigned short)(0xCFCu + (aReg & 3u)));
        }
    static TUint32 PciCfg32(TUint aBus, TUint aDev, TUint aFn, TUint aReg)
        {
        PciAddr(aBus, aDev, aFn, aReg);
        return In32(0xCFC);
        }

    // ---- region access (common-config / device-config / notify) -----------
    static inline TUint8 CfgRd8(TUint32 aOff)
        {
        TUint32 a = gCfgBase + aOff;
        return gCfgIo ? (TUint8)In8((unsigned short)a) : *(volatile TUint8*)a;
        }
    static inline TUint16 CfgRd16(TUint32 aOff)
        {
        TUint32 a = gCfgBase + aOff;
        if (gCfgIo)
            return (TUint16)((TUint16)In8((unsigned short)a) |
                             ((TUint16)In8((unsigned short)(a + 1)) << 8));
        return *(volatile TUint16*)a;
        }
    static inline TUint32 CfgRd32(TUint32 aOff)
        {
        TUint32 a = gCfgBase + aOff;
        return gCfgIo ? In32((unsigned short)a) : *(volatile TUint32*)a;
        }
    static inline void CfgWr8(TUint32 aOff, TUint8 aVal)
        {
        TUint32 a = gCfgBase + aOff;
        if (gCfgIo) Out8((unsigned short)a, (unsigned char)aVal);
        else *(volatile TUint8*)a = aVal;
        }
    static inline void CfgWr16(TUint32 aOff, TUint16 aVal)
        {
        TUint32 a = gCfgBase + aOff;
        if (gCfgIo)
            {
            Out8((unsigned short)a, (unsigned char)(aVal & 0xFFu));
            Out8((unsigned short)(a + 1), (unsigned char)(aVal >> 8));
            }
        else *(volatile TUint16*)a = aVal;
        }
    static inline void CfgWr32(TUint32 aOff, TUint32 aVal)
        {
        TUint32 a = gCfgBase + aOff;
        if (gCfgIo) Out32((unsigned short)a, (unsigned int)aVal);
        else *(volatile TUint32*)a = aVal;
        }
    static inline void Barrier()
        { __asm__ __volatile__("" : : : "memory"); }

    // ---- PCI discovery ----------------------------------------------------
    static TBool FindVirtioRng(TUint& aBus, TUint& aDev, TUint& aFn)
        {
        for (TUint d = 0; d < 32; ++d)
            {
            if ((PciRead32(0, d, 0, 0) & 0xFFFFu) == 0xFFFFu)
                continue;
            TUint fns = ((PciRead32(0, d, 0, 0x0C) >> 16) & 0x80u) ? 8u : 1u;
            for (TUint f = 0; f < fns; ++f)
                {
                TUint32 id = PciRead32(0, d, f, 0);
                if ((id & 0xFFFFu) == 0xFFFFu)
                    continue;
                TUint32 vend = id & 0xFFFFu;
                TUint32 did = (id >> 16) & 0xFFFFu;
                TUint32 subs = (PciRead32(0, d, f, 0x2C) >> 16) & 0xFFFFu;
                TBool match = EFalse;
                if (vend == kVendor && did == kDevLegacy)
                    match = ETrue;
                else if (vend == kVendor && did >= kDevModernLo && did <= kDevModernHi &&
                         subs == kSubsysRng)
                    match = ETrue;
                else if (vend == kVendor && subs == kSubsysRng)
                    match = ETrue;      // subsystem device 4 = RNG (any rev)
                if (match)
                    { aBus = 0; aDev = d; aFn = f; return ETrue; }
                }
            }
        return EFalse;
        }

    static TUint32 BarBase(TUint aBus, TUint aDev, TUint aFn, TUint aBar,
                           TBool& aIsIo)
        {
        if (aBar > 5)
            { aIsIo = EFalse; return 0; }
        TUint32 raw = PciRead32(aBus, aDev, aFn, 0x10 + aBar * 4);
        aIsIo = TBool((raw & 1u) != 0);
        return raw & (aIsIo ? 0xFFFFFFFCu : 0xFFFFFFF0u);
        }

    // Walk the vendor-specific capability list using byte-granular config
    // reads (dword PciCfg32 rounds the register down to a dword, so it cannot
    // address the cap_len/cfg_type/bar bytes individually). Capabilities may
    // live in ANY BAR -- on q35 QEMU puts them in BAR4, not BAR0.
    static void ParseCaps(TUint aBus, TUint aDev, TUint aFn)
        {
        gHaveCaps = EFalse;
        if (!(PciRead32(aBus, aDev, aFn, 4) & kStatusCapList))
            return;                              // no capability list
        TUint cap = PciRead8(aBus, aDev, aFn, 0x34);
        TInt guard = 0;
        while (cap >= 0x40 && cap < 0x100 && guard++ < 32)
            {
            TUint8 vndr = PciRead8(aBus, aDev, aFn, cap);
            TUint8 next = PciRead8(aBus, aDev, aFn, cap + 1);
            if (vndr != kPciCapVendor)
                { cap = next; continue; }
            TUint8 type = PciRead8(aBus, aDev, aFn, cap + 3);
            TUint8 bar  = PciRead8(aBus, aDev, aFn, cap + 4);
            TUint32 off = PciCfg32(aBus, aDev, aFn, cap + 8);
            TBool io = EFalse;
            TUint32 base = BarBase(aBus, aDev, aFn, bar, io);
            if (base)
                {
                if (type == kCapCommon)
                    { gCfgBase = base + off; gCfgIo = io; gHaveCaps = ETrue; }
                else if (type == kCapIsr)
                    gIsrBase = base + off;
                else if (type == kCapDevice)
                    { gDevCfgBase = base + off; gDevIo = io; }
                else if (type == kCapNotify)
                    {
                    gNotifyBase = base + off; gNotifyIo = io;
                    gNotifyMul = PciCfg32(aBus, aDev, aFn, cap + 16);
                    if (!gNotifyMul) gNotifyMul = 1;
                    }
                }
            cap = next;
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

    static TInt SetupQueue()
        {
        CfgWr16(gRQueueSel, 0);
        TUint qsz = CfgRd16(gRQueueSize);
        gQueueSizeReported = qsz;
        if (!qsz || qsz > 0x4000)
            return KErrNotSupported;
        TUint use = qsz < kQueueSize ? qsz : kQueueSize;
        gQ.iNum = (TUint16)use;
        gQ.iNotifyOff = gLegacy ? 0 : CfgRd16(kCommonQueueNotify);
        gQ.iAvailIdx = 0;
        gQ.iUsedSeen = 0;

        if (gLegacy)
            {
            // Legacy vring: QEMU derives all three from a single page frame
            // number using the *full* reported queue size.
            gQ.iDesc  = (volatile VqDesc*)(gVqMem);
            gQ.iAvail = (volatile AvailRing*)(gVqMem + qsz * sizeof(VqDesc));
            TUint usedOff = qsz * sizeof(VqDesc) + 2 + 2 + qsz * 2 + 2;
            usedOff = (usedOff + kPageSize - 1) & ~(kPageSize - 1);
            gQ.iUsed  = (volatile UsedRing*)(gVqMem + usedOff);
            }
        else
            {
            gQ.iDesc  = (volatile VqDesc*)(gVqMem);
            gQ.iAvail = (volatile AvailRing*)(gVqMem + kPageSize);
            gQ.iUsed  = (volatile UsedRing*)(gVqMem + 2 * kPageSize);
            }
        Zero(gQ);

        if (gLegacy)
            CfgWr32(gRDescLo, (TUint32)gQ.iDesc >> 12);  // guest page-frame number
        else
            {
            CfgWr32(kCommonDescLo, (TUint32)gQ.iDesc);
            CfgWr32(kCommonDescLo + 4, 0);
            CfgWr32(kCommonDriverLo, (TUint32)gQ.iAvail);
            CfgWr32(kCommonDriverLo + 4, 0);
            CfgWr32(kCommonDeviceLo, (TUint32)gQ.iUsed);
            CfgWr32(kCommonDeviceLo + 4, 0);
            CfgWr16(kCommonQueueEnable, 1);
            }
        Barrier();
        return KErrNone;
        }

    static void Kick(TUint16 aQueueIndex, const VQueue& aQ)
        {
        TUint32 addr = gNotifyBase + (TUint32)aQ.iNotifyOff * gNotifyMul;
        if (gNotifyIo) Out16((unsigned short)addr, aQueueIndex);
        else *(volatile TUint16*)addr = aQueueIndex;
        }

    // Post one writable descriptor (the device fills it with random bytes),
    // kick, then poll the used ring for completion.
    static TInt DoGet(TUint8* aBuf, TUint aLen)
        {
        if (!gCfgBase) return KErrNotReady;
        if (!aBuf || !aLen || aLen > kMaxGet) return KErrArgument;

        TUint idx = (TUint)(gQ.iAvailIdx % gQ.iNum);
        volatile VqDesc* d = gQ.iDesc;
        d[idx].iAddr = (TUint64)(TUint32)aBuf;
        d[idx].iLen = (TUint32)aLen;
        d[idx].iFlags = kDescFWrite;             // device writes random data
        d[idx].iNext = 0;
        Barrier();

        gQ.iAvail->iRing[idx] = (TUint16)idx;
        Barrier();
        ++gQ.iAvail->iIdx;
        ++gQ.iAvailIdx;
        Barrier();

        Kick(0, gQ);

        TInt spin = 0;
        for (; spin < kSpinCompletion && gQ.iUsed->iIdx == gQ.iUsedSeen; ++spin) { }
        if (gQ.iUsed->iIdx == gQ.iUsedSeen)
            return KErrTimedOut;

        volatile UsedElem& e = gQ.iUsed->iRing[gQ.iUsedSeen % gQ.iNum];
        TUint len = e.iLen;
        if (len > aLen) len = aLen;
        ++gQ.iUsedSeen;
        gLastLen = len;
        return (TInt)len;
        }
    }  // namespace

// ------------------------------------------------------------------ public
TInt vrng_init()
    {
    if (gInit)
        return gCfgBase ? KErrNone : KErrNotFound;
    gInit = ETrue;

    TUint bus = 0, dev = 0, fn = 0;
    if (!FindVirtioRng(bus, dev, fn))
        { KPrintf("virtio-rng: no PCI 1AF4 RNG device found\r\n"); return KErrNotFound; }

    TUint32 barRaw = PciRead32(bus, dev, fn, 0x10);
    gUseIo = TBool((barRaw & 1u) != 0);
    gBar0 = barRaw & (gUseIo ? 0xFFFFFFFCu : 0xFFFFFFF0u);
    if (!gBar0)
        { KPrintf("virtio-rng: BAR0 unassigned\r\n"); return KErrGeneral; }
    PciWrite32(bus, dev, fn, 0x04,
               PciRead32(bus, dev, fn, 0x04) | 0x06u);   // MEM|bus-master

    ParseCaps(bus, dev, fn);
    gLegacy = !gHaveCaps;

    if (gLegacy)
        {
        gCfgBase = gBar0; gCfgIo = gUseIo;
        gDevCfgBase = gBar0 + 0x14; gDevIo = gUseIo;     // RNG config is empty
        gNotifyBase = gBar0 + 0x10; gNotifyIo = gUseIo; gNotifyMul = 1;
        gRDevFeat = 0x00; gRDrvFeat = 0x04; gRStatus = 0x12;
        gRQueueSel = 0x0E; gRQueueSize = 0x0C; gRDescLo = 0x08;
        }
    else
        {
        gRDevFeat  = kCommonDevFeat;
        gRDrvFeat  = kCommonDrvFeat;
        gRStatus   = kCommonStatus;
        gRQueueSel = kCommonQueueSel;
        gRQueueSize= kCommonQueueSize;
        gRDescLo   = kCommonDescLo;
        }
    if (!gCfgBase || !gNotifyBase)
        { KPrintf("virtio-rng: config regions incomplete\r\n"); return KErrGeneral; }

    // Reset, then bring the device up through ACK -> DRIVER.
    CfgWr8(gRStatus, 0);
    for (TInt i = 0; i < kSpinReset && CfgRd8(gRStatus); ++i) { }
    CfgWr8(gRStatus, kStatusAck);
    CfgWr8(gRStatus, (TUint8)(kStatusAck | kStatusDriver));

    // virtio-RNG defines no required feature bits; latch what is offered for
    // info() and negotiate none.
    if (!gLegacy)
        CfgWr8(kCommonDevFeatSel, 0);
    gFeatures = CfgRd32(gRDevFeat);
    if (!gLegacy)
        CfgWr8(kCommonDrvFeatSel, 0);
    CfgWr32(gRDrvFeat, 0);

    TInt r = SetupQueue();
    if (r != KErrNone)
        { KPrintf("virtio-rng: queue setup failed %d\r\n", r); return r; }

    CfgWr8(gRStatus, (TUint8)(kStatusAck | kStatusDriver | kStatusOk));

    KPrintf("virtio-rng: BAR=%x features=%x queue ready (n=%d)\r\n",
        gBar0, gFeatures, (TInt)gQ.iNum);
    return KErrNone;
    }

TInt vrng_get(TUint8* aBuf, TUint aLen)
    {
    if (!gCfgBase && vrng_init() != KErrNone)
        return KErrNotReady;
    return DoGet(aBuf, aLen);
    }

void vrng_info()
    {
    if (!gCfgBase && vrng_init() != KErrNone)
        { KPrintf("vrng: not present\r\n"); return; }
    KPrintf("vrng: BAR=%x features=%x legacy=%d caps=%d io=%d lastlen=%d\r\n",
        gBar0, gFeatures, (TInt)gLegacy, (TInt)gHaveCaps, (TInt)gUseIo,
        (TInt)gLastLen);
    KPrintf("vrng: queue n=%d avail=%d used=%d qsz=%d cfg=%x dev=%x notify=%x mul=%d\r\n",
        (TInt)gQ.iNum, (TInt)gQ.iAvailIdx, (TInt)gQ.iUsedSeen,
        (TInt)gQueueSizeReported, gCfgBase, gDevCfgBase, gNotifyBase,
        (TInt)gNotifyMul);
    }

// Boot hook: HwInit0 walks the .init_array after paging + TheAsic are ready.
__attribute__((constructor)) static void VrngBootInit()
    { vrng_init(); }

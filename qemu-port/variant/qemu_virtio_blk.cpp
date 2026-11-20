// qemu_virtio_blk.cpp -- minimal polled virtio-blk storage driver for the
// full Kern image on QEMU q35. Variant-owned; genuine code untouched.
//
// This is a NEW storage backend: today storage is AHCI/IDE only. This driver
// neither includes nor modifies qemu_ata.cpp / qemu_ahci.cpp.
//
// Discovery scans bus 0 for vendor 1AF4 (legacy device 1001, modern 1040..107F
// with subsystem device 2, or class 01.80 + subsystem 2). The virtio-1.0 PCI
// vendor capability list locates COMMON / ISR / NOTIFY / DEVICE-CFG inside
// whichever BARs QEMU placed them in (on q35: BAR4). A device without caps is
// driven through the fixed legacy 0.9.5 I/O-BAR register map. All regions are
// accessed with 8/16/32-bit MMIO or port I/O depending on their BAR type.
//
// One split virtqueue (index 0) lives in static 4K-aligned RAM. Identity-mapped
// RAM => descriptor/avail/used DMA addresses are direct virtual addresses.
// Every request uses a 3-descriptor chain: header (device reads), data, status
// (device writes); we kick and poll the used ring -- no IRQ/MSI-X. A static
// constructor calls vblk_init() at boot; all entry points are idempotent.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include "qemu_variant.h"

namespace
    {
    // ---- PCI identity -----------------------------------------------------
    const TUint32 kVendor = 0x1AF4u;
    const TUint32 kDevLegacy = 0x1001u;          // virtio-blk (virtio 0.9.5)
    const TUint32 kDevModernLo = 0x1040u;
    const TUint32 kDevModernHi = 0x107Fu;
    const TUint32 kSubsysBlk = 2u;               // virtio device id 2 = block

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

    // virtio-blk feature bits (feature word 0)
    const TUint32 kFSizeMax = (1u << 1);
    const TUint32 kFSegMax  = (1u << 2);
    const TUint32 kFGeometry= (1u << 4);
    const TUint32 kFReadOnly= (1u << 5);
    const TUint32 kFBlkSize = (1u << 6);
    const TUint32 kFScsi    = (1u << 7);
    const TUint32 kFFlush   = (1u << 9);

    // block request types
    const TUint32 kReqIn = 0, kReqOut = 1;

    // ---- queue geometry ---------------------------------------------------
    const TUint kQueueSize = 16;
    const TUint kSectorSize = 512;
    const TUint kMaxXfer = 128;                  // sectors per request (64 KiB)
    const TUint kPageSize = 4096;
    const TUint kSpinCompletion = 2000000;
    const TUint kSpinReset = 100000;

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
        TUint16 iNum;
        TUint16 iNotifyOff;
        TUint16 iAvailIdx;
        TUint16 iUsedSeen;
        };

    // 16-byte virtio-blk request header (type, reserved, sector).
    struct BlkReqHdr
        {
        volatile TUint32 iType;
        volatile TUint32 iReserved;
        volatile TUint64 iSector;
        };

    // Legacy derives ring offsets from a single page using the full reported
    // queue size (often 1024), so reserve 64 KiB like qemu_virtio_net.cpp.
    __attribute__((aligned(4096))) static TUint8 gVqMem[64 * 1024];
    __attribute__((aligned(16)))   static BlkReqHdr gHdr;
    static volatile TUint8 gStatus = 0xFF;

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
    static TUint64 gCapacity = 0;                // sectors (512 bytes each)
    static TBool   gReadOnly = EFalse;
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
    static inline TUint32 DevRd32(TUint32 aOff)
        {
        TUint32 a = gDevCfgBase + aOff;
        return gDevIo ? In32((unsigned short)a) : *(volatile TUint32*)a;
        }
    static inline void Barrier()
        { __asm__ __volatile__("" : : : "memory"); }

    // ---- PCI discovery ----------------------------------------------------
    static TBool FindVirtioBlk(TUint& aBus, TUint& aDev, TUint& aFn)
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
                TUint32 cr = PciRead32(0, d, f, 8);
                TUint cls = (cr >> 24) & 0xFFu;
                TUint sub = (cr >> 16) & 0xFFu;
                TBool match = EFalse;
                if (vend == kVendor && did == kDevLegacy)
                    match = ETrue;
                else if (vend == kVendor && did >= kDevModernLo && did <= kDevModernHi &&
                         (subs == kSubsysBlk || (cls == 0x01u && sub == 0x80u)))
                    match = ETrue;
                else if (subs == kSubsysBlk && cls == 0x01u && sub == 0x80u)
                    match = ETrue;
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

    // Build one 3-descriptor request chain, kick, then poll for completion.
    static TInt DoRequest(TUint32 aType, TUint32 aLba, TUint8* aBuf, TUint aCount)
        {
        if (!gCfgBase) return KErrNotReady;
        if (!aBuf || !aCount || aCount > kMaxXfer) return KErrArgument;

        gHdr.iType = aType;
        gHdr.iReserved = 0;
        gHdr.iSector = (TUint64)aLba;
        gStatus = 0xFF;
        Barrier();

        volatile VqDesc* d = gQ.iDesc;
        // desc 0: 16-byte header, device reads (no WRITE), chained.
        d[0].iAddr = (TUint64)(TUint32)&gHdr;
        d[0].iLen = (TUint32)sizeof(BlkReqHdr);
        d[0].iFlags = kDescFNext;
        d[0].iNext = 1;
        // desc 1: data. READ => device writes (kDescFWrite); WRITE => device reads.
        d[1].iAddr = (TUint64)(TUint32)aBuf;
        d[1].iLen = aCount * kSectorSize;
        d[1].iFlags = (TUint16)(kDescFNext | (aType == kReqIn ? kDescFWrite : 0));
        d[1].iNext = 2;
        // desc 2: 1-byte status, device writes, end of chain.
        d[2].iAddr = (TUint64)(TUint32)&gStatus;
        d[2].iLen = 1;
        d[2].iFlags = kDescFWrite;
        d[2].iNext = 0;
        Barrier();

        TUint idx = (TUint)(gQ.iAvailIdx % gQ.iNum);
        gQ.iAvail->iRing[idx] = 0;               // head descriptor index
        Barrier();
        ++gQ.iAvail->iIdx;
        ++gQ.iAvailIdx;
        Barrier();

        Kick(0, gQ);

        TInt spin = 0;
        for (; spin < kSpinCompletion && gQ.iUsed->iIdx == gQ.iUsedSeen; ++spin) { }
        if (gQ.iUsed->iIdx == gQ.iUsedSeen)
            return KErrTimedOut;
        ++gQ.iUsedSeen;
        // status: 0=OK 1=IOERR 2=UNSUPP 3=WRITEERR
        if (gStatus != 0)
            return KErrGeneral;
        return KErrNone;
        }
    }  // namespace

// ------------------------------------------------------------------ public
TInt vblk_init()
    {
    if (gInit)
        return gCfgBase ? KErrNone : KErrNotFound;
    gInit = ETrue;

    TUint bus = 0, dev = 0, fn = 0;
    if (!FindVirtioBlk(bus, dev, fn))
        { KPrintf("virtio-blk: no PCI 1AF4 block device found\r\n"); return KErrNotFound; }

    TUint32 barRaw = PciRead32(bus, dev, fn, 0x10);
    gUseIo = TBool((barRaw & 1u) != 0);
    gBar0 = barRaw & (gUseIo ? 0xFFFFFFFCu : 0xFFFFFFF0u);
    if (!gBar0)
        { KPrintf("virtio-blk: BAR0 unassigned\r\n"); return KErrGeneral; }
    PciWrite32(bus, dev, fn, 0x04,
               PciRead32(bus, dev, fn, 0x04) | 0x06u);   // MEM|bus-master

    ParseCaps(bus, dev, fn);
    gLegacy = !gHaveCaps;

    if (gLegacy)
        {
        gCfgBase = gBar0; gCfgIo = gUseIo;
        gDevCfgBase = gBar0 + 0x14; gDevIo = gUseIo;
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
    if (!gCfgBase || !gDevCfgBase || !gNotifyBase)
        { KPrintf("virtio-blk: config regions incomplete\r\n"); return KErrGeneral; }

    // Reset, then bring the device up through ACK -> DRIVER.
    CfgWr8(gRStatus, 0);
    for (TInt i = 0; i < kSpinReset && CfgRd8(gRStatus); ++i) { }
    CfgWr8(gRStatus, kStatusAck);
    CfgWr8(gRStatus, (TUint8)(kStatusAck | kStatusDriver));

    // Negotiate feature word 0. We need no optional features for basic I/O,
    // but latch RO (bit 5) and the capacity-affecting bits for info().
    if (!gLegacy)
        CfgWr8(kCommonDevFeatSel, 0);
    TUint32 offered = CfgRd32(gRDevFeat);
    gFeatures = offered &
        (kFSizeMax | kFSegMax | kFGeometry | kFReadOnly | kFBlkSize | kFScsi | kFFlush);
    gReadOnly = TBool((offered & kFReadOnly) != 0);
    if (!gLegacy)
        CfgWr8(kCommonDrvFeatSel, 0);
    CfgWr32(gRDrvFeat, 0);                       // no optional features needed

    TInt r = SetupQueue();
    if (r != KErrNone)
        { KPrintf("virtio-blk: queue setup failed %d\r\n", r); return r; }

    // Device config: capacity is an le64 at offset 0 of the blk config.
    TUint32 lo = DevRd32(0);
    TUint32 hi = DevRd32(4);
    gCapacity = ((TUint64)hi << 32) | (TUint64)lo;

    CfgWr8(gRStatus, (TUint8)(kStatusAck | kStatusDriver | kStatusOk));

    KPrintf("virtio-blk: BAR0=%x capacity=%u sectors (%u MB)%s\r\n",
        gBar0, (TUint)(gCapacity & 0xFFFFFFFFu),
        (TUint)(gCapacity / 2048u), gReadOnly ? " RO" : "");
    return KErrNone;
    }

TInt vblk_read(TUint32 aLba, TUint8* aBuf, TUint aCount)
    {
    if (!gCfgBase && vblk_init() != KErrNone)
        return KErrNotReady;
    return DoRequest(kReqIn, aLba, aBuf, aCount);
    }

TInt vblk_write(TUint32 aLba, const TUint8* aBuf, TUint aCount)
    {
    if (!gCfgBase && vblk_init() != KErrNone)
        return KErrNotReady;
    if (gReadOnly)
        return KErrNotSupported;
    return DoRequest(kReqOut, aLba, (TUint8*)aBuf, aCount);
    }

void vblk_info()
    {
    if (!gCfgBase && vblk_init() != KErrNone)
        { KPrintf("vblk: not present\r\n"); return; }
    KPrintf("vblk: BAR0=%x features=%x ro=%d legacy=%d caps=%d io=%d\r\n",
        gBar0, gFeatures, (TInt)gReadOnly, (TInt)gLegacy, (TInt)gHaveCaps,
        (TInt)gUseIo);
    KPrintf("vblk: capacity=%u sectors (%u MB) queue n=%d avail=%d used=%d qsz=%d\r\n",
        (TUint)(gCapacity & 0xFFFFFFFFu), (TUint)(gCapacity / 2048u),
        (TInt)gQ.iNum, (TInt)gQ.iAvailIdx, (TInt)gQ.iUsedSeen,
        (TInt)gQueueSizeReported);
    KPrintf("vblk: cfg=%x dev=%x notify=%x isr=%x mul=%d\r\n",
        gCfgBase, gDevCfgBase, gNotifyBase, gIsrBase, (TInt)gNotifyMul);
    }

// Boot hook: HwInit0 walks the .init_array after paging + TheAsic are ready.
__attribute__((constructor)) static void VblkBootInit()
    { vblk_init(); }

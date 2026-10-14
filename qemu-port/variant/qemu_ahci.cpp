// qemu_ahci.cpp — AHCI (SATA) driver for the Kern image: QEMU q35 ICH9,
// VirtualBox AHCI, real HW in IDE/compatibility mode (no AHCI BIOS needed).
//
// Covers what the storage phase needs: controller init (AE, BIOS handoff,
// per-port start + device detect), IDENTIFY (ATA vs ATAPI), LBA48/LBA28
// DMA read/write + flush, all polled (no IRQ wiring — matches qemu_ata).
// Ports present as Fat devices 4..7 via the Ata* dispatch in qemu_ata.cpp.
//
// References: Serial ATA AHCI 1.3 register model (GHC/ports, command list,
// Register H2D FIS 0x27, PRDT). No genuine code involved; Variant-owned.
//
// Limits: ATAPI skipped (like legacy), max 4 ports (dev 4..7), 32-bit DMA
// (identity-mapped low RAM — DBAU always 0). Command slot 1 per port
// (slot 0 left clear of any BIOS-era residue).

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

namespace
    {
    static inline TUint32 Rd32(TUint32 aAddr)
        { return *(volatile TUint32*)aAddr; }
    static inline void Wr32(TUint32 aAddr, TUint32 aVal)
        { *(volatile TUint32*)aAddr = aVal; }

    enum
        {
        kGhcCap = 0x00, kGhcGhc = 0x04, kGhcPi = 0x0C, kGhcBohc = 0x28,
        kGhcCap2 = 0x24,
        kGhcAe = 0x80000000u, kGhcHr = 0x00000001u,
        kBohcBos = 0x01u, kBohcOos = 0x02u, kBohcOoc = 0x08u, kBohcBb = 0x10u,
        kPortBase = 0x100, kPortStride = 0x80,
        kPxClb = 0x00, kPxClbu = 0x04, kPxFb = 0x08, kPxFbu = 0x0C,
        kPxIs = 0x10, kPxCmd = 0x18, kPxTfd = 0x20, kPxSig = 0x24,
        kPxSsts = 0x28, kPxSctl = 0x2C, kPxSerr = 0x30, kPxSact = 0x34, kPxCi = 0x38,
        kPxCmdSt = 0x01u, kPxCmdFre = 0x10u, kPxCmdFr = 0x4000u,
        kPxCmdCr = 0x8000u,
        kSigAta = 0x00000101u, kSigAtapi = 0xEB140101u,
        kFisRegH2D = 0x27,
        kCmdIdentify = 0xEC, kCmdReadDma = 0xC8, kCmdWriteDma = 0xCA,
        kCmdReadDmaExt = 0x25, kCmdWriteDmaExt = 0x35, kCmdFlush = 0xE7,
        kCmdFlushExt = 0xEA,
        kMaxPorts = 4,
        };

    const TInt kSpinHba = 20000000;
    const TInt kSpinPort = 10000000;
    const TInt kSpinCmd = 20000000;

    struct Port
        {
        TBool present;
        TBool atapi;
        TBool lba48;
        TUint32 sectors;  // capped to 32 bits (LBA28 range discipline below)
        TUint32 sectorsHi;
        char model[41];
        TUint32 abar;     // controller base, cached per port op
        TUint port;       // 0..N port index
        };
    Port gPorts[kMaxPorts];
    TUint32 gAbar = 0;
    TBool gInit = EFalse;

    // 1KB command lists (32 slots x 32B), 256B FIS areas, 256B tables.
    __attribute__((aligned(1024))) static TUint8 gClb[kMaxPorts][1024];
    __attribute__((aligned(256))) static TUint8 gFb[kMaxPorts][256];
    __attribute__((aligned(128))) static TUint8 gCt[kMaxPorts][256];
    static TUint8 gData[512];

    TUint32 PReg(TUint aPort, TUint aOff)
        { return gAbar + kPortBase + aPort * kPortStride + aOff; }

    TInt WaitClear(TUint32 aAddr, TUint32 aMask)
        {
        for (TInt i = 0; i < kSpinPort; ++i)
            if (!(Rd32(aAddr) & aMask))
                return KErrNone;
        return KErrTimedOut;
        }

    // Run one ATA command on a started port. aWrite: data H2D.
    // aBuf/aLen: single PRD (aLen<=512 for IDENTIFY; up to 128K for I/O).
    TInt RunCmd(TUint aPort, TUint8 aCmd, TUint32 aLba, TUint32 aLbaHi,
                TUint aCount, TUint8* aBuf, TUint aLen, TBool aWrite)
        {
        // Slot 1: slot 0 may carry SeaBIOS-era wedged HBA state.
        const TUint kSlot = 1;
        const TUint32 kSlotBit = 1u << kSlot;
        TUint8* clb = gClb[aPort] + kSlot * 32;
        TUint8* ct = gCt[aPort];
        for (TUint i = 0; i < 32; ++i)
            clb[i] = 0;
        // Header slot 0 (32B): DW0=CFL|W|C|PRDTL, DW1=PRDBC, DW2=CTBA,
        // DW3=CTBAU. Bytes: [0]=CFL|A|W|P, [1]=R|B|C|PMP, [2..3]=PRDTL,
        // [4..7]=PRDBC, [8..11]=CTBA, [12..15]=CTBAU, [16..31]=reserved.
        clb[0] = 5;
        if (aWrite)
            clb[0] |= 0x40;   // W (DW0 bit 6)
        clb[1] = 0x04;        // C (DW0 bit 10)
        if (aLen)
            { clb[2] = 1; clb[3] = 0; }  // PRDTL=1 for data, else 0
        else
            { clb[2] = 0; clb[3] = 0; }
        for (TUint i = 4; i < 8; ++i)
            clb[i] = 0;       // PRDBC
        TUint32 cta = (TUint32)ct;
        clb[8] = (TUint8)cta; clb[9] = (TUint8)(cta >> 8);
        clb[10] = (TUint8)(cta >> 16); clb[11] = (TUint8)(cta >> 24);
        for (TUint i = 12; i < 32; ++i)
            clb[i] = 0;       // CTBAU + reserved
        // Command table: CFIS = Register H2D (20B) + pad to 64.
        for (TUint i = 0; i < 64; ++i)
            ct[i] = 0;
        ct[0] = kFisRegH2D;
        ct[1] = 0x80;  // C=1
        ct[2] = aCmd;
        ct[4] = (TUint8)aLba; ct[5] = (TUint8)(aLba >> 8);
        ct[6] = (TUint8)(aLba >> 16);
        ct[7] = (TUint8)(0xE0 | ((aLba >> 24) & 0x0F));
        ct[8] = (TUint8)aLbaHi; ct[9] = (TUint8)(aLbaHi >> 8);
        ct[10] = (TUint8)(aLbaHi >> 16);
        ct[12] = (TUint8)aCount; ct[13] = (TUint8)(aCount >> 8);
        // PRDT entry 0 at ct+128: DBA(64) + reserved + DBC|I.
        // QEMU AHCI_SG: addr[0..7], reserved[8..11], flags_size[12..15].
        TUint8* prd = ct + 128;
        TUint32 dba = (TUint32)aBuf;
        prd[0] = (TUint8)dba; prd[1] = (TUint8)(dba >> 8);
        prd[2] = (TUint8)(dba >> 16); prd[3] = (TUint8)(dba >> 24);
        prd[4] = prd[5] = prd[6] = prd[7] = 0;
        prd[8] = prd[9] = prd[10] = prd[11] = 0;
        TUint32 dbc = aLen ? aLen - 1 : 0;
        prd[12] = (TUint8)dbc; prd[13] = (TUint8)(dbc >> 8);
        prd[14] = (TUint8)(dbc >> 16); prd[15] = 0;
        // Clear port IS, issue slot, poll CI.
        Wr32(PReg(aPort, kPxIs), 0xFFFFFFFFu);
        Wr32(PReg(aPort, kPxCi), kSlotBit);
        for (TInt i = 0; i < kSpinCmd; ++i)
            {
            if (!(Rd32(PReg(aPort, kPxCi)) & kSlotBit))
                {
                TUint32 tfd = Rd32(PReg(aPort, kPxTfd));
                if (tfd & 0x01u)
                    return KErrGeneral;  // ERR bit
                if (Rd32(PReg(aPort, kPxIs)) & 0x40000000u)
                    return KErrGeneral;  // TFES
                return KErrNone;
                }
            }
        KPrintf("ahci: port %u cmd %02x timeout (ci=%x tfd=%x is=%x serr=%x ssts=%x cmd=%x)\r\n",
            aPort, aCmd, Rd32(PReg(aPort, kPxCi)), Rd32(PReg(aPort, kPxTfd)),
            Rd32(PReg(aPort, kPxIs)), Rd32(PReg(aPort, kPxSerr)),
            Rd32(PReg(aPort, kPxSsts)), Rd32(PReg(aPort, kPxCmd)));
        return KErrTimedOut;
        }

    static TUint IdW(const TUint8* aId, TUint aW)
        { return (TUint)aId[2 * aW] | ((TUint)aId[2 * aW + 1] << 8); }
    static TUint32 IdD(const TUint8* aId, TUint aW)
        { return IdW(aId, aW) | ((TUint32)IdW(aId, aW + 1) << 16); }

    void ParseIdentify(Port& aP, const TUint8* aId)
        {
        for (TUint i = 0; i < 20; ++i)
            {
            char hi = (char)aId[2 * (27 + i)], lo = (char)aId[2 * (27 + i) + 1];
            // Model is word-swapped ASCII; also trim trailing spaces.
            aP.model[2 * i] = lo; aP.model[2 * i + 1] = hi;
            }
        aP.model[40] = 0;
        for (int i = 39; i >= 0 && aP.model[i] == ' '; --i)
            aP.model[i] = 0;
        aP.lba48 = (IdW(aId, 83) & 0x0400u) ? ETrue : EFalse;
        if (aP.lba48)
            {
            aP.sectors = IdD(aId, 100); aP.sectorsHi = IdD(aId, 102);
            }
        else
            {
            aP.sectors = IdD(aId, 60); aP.sectorsHi = 0;
            }
        }

    TInt StartPort(TUint aPort)
        {
        // Stop, idle, clear errors.
        Wr32(PReg(aPort, kPxCmd),
             Rd32(PReg(aPort, kPxCmd)) & ~kPxCmdSt);
        if (WaitClear(PReg(aPort, kPxCmd), kPxCmdCr) != KErrNone)
            return KErrTimedOut;
        Wr32(PReg(aPort, kPxSerr), 0xFFFFFFFFu);
        // NOTE: no COMRESET here — a DET pulse sets ERR_STAT/DF in QEMU's
        // device model and QEMU will not complete commands (nor clear PxCI)
        // while ERR_STAT is set. The link is already up from SeaBIOS.
        // Program CLB/FB (low 32 bits; high 0).
        Wr32(PReg(aPort, kPxClb), (TUint32)gClb[aPort]);
        Wr32(PReg(aPort, kPxClbu), 0);
        Wr32(PReg(aPort, kPxFb), (TUint32)gFb[aPort]);
        Wr32(PReg(aPort, kPxFbu), 0);
        // FIS receive enable, then start (wait FR before ST per spec).
        Wr32(PReg(aPort, kPxCmd),
             Rd32(PReg(aPort, kPxCmd)) | kPxCmdFre);
        for (TInt i = 0; i < kSpinPort; ++i)
            if (Rd32(PReg(aPort, kPxCmd)) & kPxCmdFr)
                break;
        Wr32(PReg(aPort, kPxCmd),
             Rd32(PReg(aPort, kPxCmd)) | kPxCmdSt);
        // Device present?
        TUint32 ssts = Rd32(PReg(aPort, kPxSsts));
        if ((ssts & 0x0Fu) != 0x03u)
            {
            Wr32(PReg(aPort, kPxCmd),
                 Rd32(PReg(aPort, kPxCmd)) & ~kPxCmdSt);
            return KErrNotFound;
            }
        return KErrNone;
        }
    }  // namespace

TInt AhciInit()
    {
    if (gInit)
        return KErrNone;
    for (TUint i = 0; i < kMaxPorts; ++i)
        {
        gPorts[i].present = EFalse;
        gPorts[i].model[0] = 0;
        }
    unsigned bus, dev, fn;
    if (!PciFindClass(0x01, 0x06, &bus, &dev, &fn))
        {
        gInit = ETrue;  // probe once; no SATA controller here
        return KErrNotFound;
        }
    // BAR5 = ABAR (32-bit mem). Enable MEM + bus master.
    TUint32 bar5 = PciRead32(bus, dev, fn, 0x24) & 0xFFFFFFF0u;
    if (!bar5)
        return KErrGeneral;
    TUint32 cmd = PciRead32(bus, dev, fn, 0x04);
    PciWrite32(bus, dev, fn, 0x04, cmd | 0x06u);
    gAbar = bar5;
    // BIOS/OS handoff when supported.
    TUint32 cap2 = Rd32(gAbar + kGhcCap2);
    if (cap2 & 0x01u)
        {
        Wr32(gAbar + kGhcBohc, Rd32(gAbar + kGhcBohc) | kBohcOos);
        for (TInt i = 0; i < kSpinHba; ++i)
            if (!(Rd32(gAbar + kGhcBohc) & kBohcBos))
                break;
        }
    Wr32(gAbar + kGhcGhc, Rd32(gAbar + kGhcGhc) | kGhcAe);
    // Full HBA reset: SeaBIOS used these ports to boot (stale CI/CR/FIS
    // state makes new command edges vanish). Resets all ports + state.
    Wr32(gAbar + kGhcGhc, Rd32(gAbar + kGhcGhc) | kGhcHr);
    for (TInt i = 0; i < kSpinHba; ++i)
        if (!(Rd32(gAbar + kGhcGhc) & kGhcHr))
            break;
    Wr32(gAbar + kGhcGhc, Rd32(gAbar + kGhcGhc) | kGhcAe);
    TUint32 pi = Rd32(gAbar + kGhcPi);
    TUint32 cap = Rd32(gAbar + kGhcCap);
    TUint nPorts = ((cap >> 8) & 0x1Fu) + 1;
    if (nPorts > kMaxPorts)
        nPorts = kMaxPorts;
    KPrintf("ahci: ABAR %x PI %x ports %u\r\n", gAbar, pi, nPorts);
    for (TUint p = 0; p < nPorts; ++p)
        {
        if (!(pi & (1u << p)))
            continue;
        if (StartPort(p) != KErrNone)
            continue;
        TUint32 sig = Rd32(PReg(p, kPxSig));
        if (sig == kSigAtapi)
            {
            gPorts[p].present = EFalse;
            gPorts[p].atapi = ETrue;
            continue;  // ATAPI packet transport: future work
            }
        if (sig != kSigAta)
            continue;
        TInt r = RunCmd(p, kCmdIdentify, 0, 0, 0, gData, 512, EFalse);
        if (r != KErrNone)
            {
            KPrintf("ahci: port %u IDENTIFY err %d\r\n", p, r);
            Wr32(PReg(p, kPxCmd), Rd32(PReg(p, kPxCmd)) & ~kPxCmdSt);
            continue;
            }
        gPorts[p].present = ETrue;
        gPorts[p].atapi = EFalse;
        gPorts[p].abar = gAbar;
        gPorts[p].port = p;
        ParseIdentify(gPorts[p], gData);
        KPrintf("ahci: port %u %s sectors=%u%s\r\n", p, gPorts[p].model,
            gPorts[p].sectors, gPorts[p].lba48 ? " LBA48" : "");
        }
    gInit = ETrue;
    return KErrNone;
    }

TInt AhciCount()
    {
    if (!gInit)
        AhciInit();
    TInt n = 0;
    for (TUint i = 0; i < kMaxPorts; ++i)
        if (gPorts[i].present && !gPorts[i].atapi)
            ++n;
    return n;
    }

TBool AhciPresent(int aDev)
    {
    if (!gInit)
        AhciInit();
    if (aDev < 0 || aDev >= (int)kMaxPorts)
        return EFalse;
    return (TBool)(gPorts[aDev].present && !gPorts[aDev].atapi);
    }

TUint32 AhciSectors(int aDev)
    {
    if (!AhciPresent(aDev))
        return 0;
    return gPorts[aDev].sectors;
    }

const char* AhciModel(int aDev)
    {
    if (!AhciPresent(aDev))
        return "(absent)";
    return gPorts[aDev].model;
    }

static TInt AhciIo(int aDev, TUint32 aLba, TUint aCount, TUint8* aBuf,
                   TBool aWrite)
    {
    if (!AhciPresent(aDev) || !aBuf || !aCount)
        return KErrArgument;
    Port& pt = gPorts[aDev];
    if (aLba + aCount < aLba)
        return KErrArgument;
    // LBA28 range discipline matches legacy (Fat uses <2TB volumes).
    if (!pt.lba48 && aLba + aCount > pt.sectors)
        return KErrArgument;
    while (aCount)
        {
        TUint n = aCount > 256 ? 256 : aCount;
        TInt r;
        if (pt.lba48)
            {
            r = RunCmd(pt.port, aWrite ? kCmdWriteDmaExt : kCmdReadDmaExt,
                       aLba, 0, n, aBuf, n * 512u, aWrite);
            }
        else
            {
            if (aLba + n > pt.sectors)
                return KErrArgument;
            r = RunCmd(pt.port, aWrite ? kCmdWriteDma : kCmdReadDma,
                       aLba, 0, n, aBuf, n * 512u, aWrite);
            }
        if (r != KErrNone)
            return r;
        aLba += n;
        aBuf += n * 512u;
        aCount -= n;
        }
    if (aWrite)
        {
        TInt r = RunCmd(pt.port, pt.lba48 ? kCmdFlushExt : kCmdFlush,
                        0, 0, 0, gData, 0, EFalse);
        if (r != KErrNone)
            return r;
        }
    return KErrNone;
    }

TInt AhciRead(int aDev, TUint32 aLba, TUint aCount, TUint8* aBuf)
    {
    return AhciIo(aDev, aLba, aCount, aBuf, EFalse);
    }

TInt AhciWrite(int aDev, TUint32 aLba, TUint aCount, const TUint8* aBuf)
    {
    return AhciIo(aDev, aLba, aCount, (TUint8*)aBuf, ETrue);
    }

void AhciInfo()
    {
    if (!gInit)
        AhciInit();
    if (!gAbar)
        {
        KPrintf("ahci: no SATA controller (PCI 01.06)\r\n");
        return;
        }
    for (TUint i = 0; i < kMaxPorts; ++i)
        {
        if (!gPorts[i].present)
            continue;
        KPrintf("ahci: port %u %s sectors=%u%s\r\n", i, gPorts[i].model,
            gPorts[i].sectors, gPorts[i].lba48 ? " LBA48" : "");
        }
    KPrintf("ahci: %d disk(s)\r\n", AhciCount());
    }

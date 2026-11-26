// qemu_pci.cpp — PCI configuration-space scanner for the shell.
//
// Raw x86 config access with two transports:
//   * port I/O 0xCF8/0xCFC (works everywhere, all 256 buses), and
//   * PCIe ECAM / MMCONFIG memory-mapped config, auto-detected by matching
//     the bus-0 host-bridge vendor:device against the port-I/O read at the
//     standard PCI-hole candidate bases (0xB0000000..0xE0000000). Reads there
//     are safe on real x86 (the hole returns 0xFFFFFFFF when no ECAM lives
//     at that base), so detection never faults.
// Enumeration walks bus 0 and then recurses through every PCI-PCI/CardBus
// bridge (class 06h, sub 04h/07h) into its secondary bus, so devices behind
// bridges (bus > 0) — typical on real boards and on q35 root ports — are
// visible to PciFindClass/PciList. Read-only; no genuine dependencies beyond
// KPrintf. The scan is lazy, bounded (depth cap + visited-bus set) and
// idempotent, so boot-time cost is unchanged.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>

static inline void Out32(unsigned short aPort, unsigned int aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
static inline unsigned int In32(unsigned short aPort)
    { unsigned int v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

// ---- transport selection -------------------------------------------------

static unsigned int gEcam = 0;        // 0 = port I/O only
static TBool gEcamTried = EFalse;

static unsigned int PciReadPort(unsigned int aBus, unsigned int aDev,
                                unsigned int aFn, unsigned int aReg)
    {
    Out32(0xCF8, 0x80000000u | (aBus << 16) | (aDev << 11) | (aFn << 8) | (aReg & 0xFCu));
    return In32(0xCFC);
    }

static void PciWritePort(unsigned int aBus, unsigned int aDev,
                         unsigned int aFn, unsigned int aReg, unsigned int aVal)
    {
    Out32(0xCF8, 0x80000000u | (aBus << 16) | (aDev << 11) | (aFn << 8) | (aReg & 0xFCu));
    Out32(0xCFC, aVal);
    }

// Detect an ECAM/MMCONFIG base in the PCI hole. A candidate base is accepted
// only when its bus0/dev0/fn0 dword equals the port-I/O read, so a machine
// without ECAM (i440fx) simply keeps using 0xCF8/0xCFC.
static void EcamDetect()
    {
    if (gEcamTried)
        return;
    gEcamTried = ETrue;
    unsigned int ref = PciReadPort(0, 0, 0, 0);
    if ((ref & 0xFFFFu) == 0xFFFFu)
        return;  // no readable host bridge at all; port I/O will report FFFF too
    static const unsigned int kBases[] =
        { 0xB0000000u, 0xC0000000u, 0xD0000000u, 0xE0000000u };
    for (unsigned int i = 0; i < sizeof(kBases) / sizeof(kBases[0]); ++i)
        {
        volatile unsigned int* p = (volatile unsigned int*)kBases[i];
        if (*p == ref)
            {
            gEcam = kBases[i];
            return;
            }
        }
    }

static unsigned int PciRead(unsigned int aBus, unsigned int aDev,
                            unsigned int aFn, unsigned int aReg)
    {
    EcamDetect();
    if (gEcam)
        {
        volatile unsigned int* p = (volatile unsigned int*)
            (gEcam + (aBus << 20) + (aDev << 15) + (aFn << 12) + (aReg & 0xFFCu));
        return *p;
        }
    return PciReadPort(aBus, aDev, aFn, aReg);
    }

static void PciWrite(unsigned int aBus, unsigned int aDev,
                     unsigned int aFn, unsigned int aReg, unsigned int aVal)
    {
    EcamDetect();
    if (gEcam)
        {
        volatile unsigned int* p = (volatile unsigned int*)
            (gEcam + (aBus << 20) + (aDev << 15) + (aFn << 12) + (aReg & 0xFFCu));
        *p = aVal;
        return;
        }
    PciWritePort(aBus, aDev, aFn, aReg, aVal);
    }

unsigned int PciRead32(unsigned int aBus, unsigned int aDev,
                       unsigned int aFn, unsigned int aReg)
    {
    return PciRead(aBus, aDev, aFn, aReg);
    }

void PciWrite32(unsigned int aBus, unsigned int aDev,
                unsigned int aFn, unsigned int aReg, unsigned int aVal)
    {
    PciWrite(aBus, aDev, aFn, aReg, aVal);
    }

// ---- recursive bridge walk ----------------------------------------------

static unsigned char gBusSeen[256];
static TBool gScanned = EFalse;

// Scan one bus and recurse into every bridge's secondary bus. Depth-capped
// and guarded by a visited set, so loops/misprogrammed bridges cannot hang.
static void ScanBus(unsigned int aBus, int aDepth)
    {
    if (gBusSeen[aBus] || aDepth > 16)
        return;
    gBusSeen[aBus] = 1;
    for (unsigned int dev = 0; dev < 32; ++dev)
        {
        if ((PciRead(aBus, dev, 0, 0) & 0xFFFFu) == 0xFFFFu)
            continue;
        unsigned int hdr = (PciRead(aBus, dev, 0, 0x0C) >> 16) & 0xFFu;
        unsigned int fns = (hdr & 0x80u) ? 8u : 1u;
        for (unsigned int fn = 0; fn < fns; ++fn)
            {
            if ((PciRead(aBus, dev, fn, 0) & 0xFFFFu) == 0xFFFFu)
                continue;
            unsigned int cr = PciRead(aBus, dev, fn, 8);
            unsigned int cls = (cr >> 24) & 0xFFu;
            unsigned int sub = (cr >> 16) & 0xFFu;
            if (cls == 0x06u && (sub == 0x04u || sub == 0x07u))
                {
                // PCI-PCI / CardBus bridge: secondary bus at reg 0x18 bits 8-15.
                unsigned int sec = (PciRead(aBus, dev, fn, 0x18) >> 8) & 0xFFu;
                if (sec && sec != aBus)
                    ScanBus(sec, aDepth + 1);
                }
            }
        }
    }

static void EnsureScanned()
    {
    if (gScanned)
        return;
    for (int i = 0; i < 256; ++i)
        gBusSeen[i] = 0;
    ScanBus(0, 0);
    gScanned = ETrue;
    }

// First function matching (class, sub) across ALL scanned buses (bus 0 first,
// then bridge-descendant buses in discovery order); EFalse when absent.
TBool PciFindClass(unsigned int aClass, unsigned int aSub,
                   unsigned int* aBus, unsigned int* aDev, unsigned int* aFn)
    {
    EnsureScanned();
    for (unsigned int bus = 0; bus < 256; ++bus)
        {
        if (!gBusSeen[bus])
            continue;
        for (unsigned int dev = 0; dev < 32; ++dev)
            {
            if ((PciRead(bus, dev, 0, 0) & 0xFFFFu) == 0xFFFFu)
                continue;
            unsigned int hdr = (PciRead(bus, dev, 0, 0x0C) >> 16) & 0xFFu;
            unsigned int fns = (hdr & 0x80u) ? 8u : 1u;
            for (unsigned int fn = 0; fn < fns; ++fn)
                {
                if ((PciRead(bus, dev, fn, 0) & 0xFFFFu) == 0xFFFFu)
                    continue;
                unsigned int cr = PciRead(bus, dev, fn, 8);
                if ((((cr >> 24) & 0xFFu) == aClass) &&
                    (((cr >> 16) & 0xFFu) == aSub))
                    {
                    if (aBus) *aBus = bus;
                    if (aDev) *aDev = dev;
                    if (aFn) *aFn = fn;
                    return ETrue;
                    }
                }
            }
        }
    return EFalse;
    }

static const char* ClassName(unsigned int aClass, unsigned int aSub)
    {
    switch (aClass)
        {
    case 0x00: return (aSub == 1) ? "VGA-pal" : "unclassified";
    case 0x01: return (aSub == 1) ? "IDE" : (aSub == 6) ? "SATA" : "storage";
    case 0x02: return (aSub == 0) ? "ethernet" : "network";
    case 0x03: return (aSub == 0) ? "VGA" : "display";
    case 0x04: return "multimedia";
    case 0x05: return "memory";
    case 0x06: return (aSub == 0) ? "host" : (aSub == 1) ? "ISA" :
                      (aSub == 4) ? "PCI-PCI" : (aSub == 7) ? "CardBus" : "bridge";
    case 0x07: return "comm";
    case 0x08: return "peripheral";
    case 0x0C: return (aSub == 3) ? "USB" : (aSub == 5) ? "SMBus" : "serial";
    case 0x0D: return "wireless";
    default:   return "?";
        }
    }

void PciList()
    {
    EnsureScanned();
    int found = 0;
    for (unsigned int bus = 0; bus < 256; ++bus)
        {
        if (!gBusSeen[bus])
            continue;
        for (unsigned int dev = 0; dev < 32; ++dev)
            {
            unsigned int id0 = PciRead(bus, dev, 0, 0);
            if ((id0 & 0xFFFFu) == 0xFFFFu)
                continue;
            unsigned int hdr = (PciRead(bus, dev, 0, 0x0C) >> 16) & 0xFFu;
            unsigned int fns = (hdr & 0x80u) ? 8u : 1u;
            for (unsigned int fn = 0; fn < fns; ++fn)
                {
                unsigned int id = PciRead(bus, dev, fn, 0);
                if ((id & 0xFFFFu) == 0xFFFFu)
                    continue;
                unsigned int cr = PciRead(bus, dev, fn, 8);
                unsigned int cls = (cr >> 24) & 0xFFu;
                unsigned int sub = (cr >> 16) & 0xFFu;
                KPrintf("%02x:%02x.%x %04x:%04x class %02x.%02x %s\r\n",
                    bus, dev, fn, id & 0xFFFFu, (id >> 16) & 0xFFFFu,
                    cls, sub, ClassName(cls, sub));
                ++found;
                }
            }
        }
    KPrintf("%d PCI device(s), ecam=%s\r\n", found, gEcam ? "mmconfig" : "port-io");
    }

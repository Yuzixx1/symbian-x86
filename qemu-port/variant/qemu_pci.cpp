// qemu_pci.cpp — PCI configuration-space scanner (bus 0) for the shell.
//
// Raw x86 port I/O (0xCF8/0xCFC); no genuine dependencies beyond KPrintf.
// Read-only: enumerates what QEMU (or real hardware) presents.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>

static inline void Out32(unsigned short aPort, unsigned int aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
static inline unsigned int In32(unsigned short aPort)
    { unsigned int v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

static unsigned int PciRead(unsigned int aBus, unsigned int aDev,
                            unsigned int aFn, unsigned int aReg)
    {
    Out32(0xCF8, 0x80000000u | (aBus << 16) | (aDev << 11) | (aFn << 8) | (aReg & 0xFCu));
    return In32(0xCFC);
    }

unsigned int PciRead32(unsigned int aBus, unsigned int aDev,
                       unsigned int aFn, unsigned int aReg)
    {
    return PciRead(aBus, aDev, aFn, aReg);
    }

void PciWrite32(unsigned int aBus, unsigned int aDev,
                unsigned int aFn, unsigned int aReg, unsigned int aVal)
    {
    Out32(0xCF8, 0x80000000u | (aBus << 16) | (aDev << 11) | (aFn << 8) | (aReg & 0xFCu));
    Out32(0xCFC, aVal);
    }

// First bus-0 function matching (class, sub); EFalse when absent.
TBool PciFindClass(unsigned int aClass, unsigned int aSub,
                   unsigned int* aBus, unsigned int* aDev, unsigned int* aFn)
    {
    for (unsigned int dev = 0; dev < 32; ++dev)
        {
        if ((PciRead(0, dev, 0, 0) & 0xFFFFu) == 0xFFFFu)
            continue;
        unsigned int hdr = (PciRead(0, dev, 0, 0x0C) >> 16) & 0xFFu;
        unsigned int fns = (hdr & 0x80u) ? 8u : 1u;
        for (unsigned int fn = 0; fn < fns; ++fn)
            {
            if ((PciRead(0, dev, fn, 0) & 0xFFFFu) == 0xFFFFu)
                continue;
            unsigned int cr = PciRead(0, dev, fn, 8);
            if ((((cr >> 24) & 0xFFu) == aClass) &&
                (((cr >> 16) & 0xFFu) == aSub))
                {
                if (aBus) *aBus = 0;
                if (aDev) *aDev = dev;
                if (aFn) *aFn = fn;
                return ETrue;
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
                      (aSub == 4) ? "PCI-PCI" : "bridge";
    case 0x07: return "comm";
    case 0x08: return "peripheral";
    case 0x0C: return (aSub == 3) ? "USB" : (aSub == 5) ? "SMBus" : "serial";
    case 0x0D: return "wireless";
    default:   return "?";
        }
    }

void PciList()
    {
    int found = 0;
    for (unsigned int dev = 0; dev < 32; ++dev)
        {
        unsigned int id0 = PciRead(0, dev, 0, 0);
        if ((id0 & 0xFFFFu) == 0xFFFFu)
            continue;
        unsigned int hdr = (PciRead(0, dev, 0, 0x0C) >> 16) & 0xFFu;
        unsigned int fns = (hdr & 0x80u) ? 8u : 1u;
        for (unsigned int fn = 0; fn < fns; ++fn)
            {
            unsigned int id = PciRead(0, dev, fn, 0);
            if ((id & 0xFFFFu) == 0xFFFFu)
                continue;
            unsigned int cr = PciRead(0, dev, fn, 8);
            unsigned int cls = (cr >> 24) & 0xFFu;
            unsigned int sub = (cr >> 16) & 0xFFu;
            KPrintf("%02x:%02x.%x %04x:%04x class %02x.%02x %s\r\n",
                0, dev, fn, id & 0xFFFFu, (id >> 16) & 0xFFFFu,
                cls, sub, ClassName(cls, sub));
            ++found;
            }
        }
    KPrintf("%d PCI device(s) on bus 0\r\n", found);
    }

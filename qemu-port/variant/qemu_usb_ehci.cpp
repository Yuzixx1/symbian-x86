// qemu_usb_ehci.cpp — EHCI (USB 2.0) host controller backend (polled).
//
// Complements the OHCI 1.1 backend: QEMU q35 exposes the ICH9 USB2 EHCI
// controller (8086:293A, class 0c.03.20) by default at 00:1d.7. We claim
// all root ports (CONFIGFLAG.0), park an idle async queue head and an empty
// frame list in identity-mapped 4K-aligned static RAM (virtual == physical),
// then run the controller with the async schedule enabled. No interrupts:
// everything is polled through MMIO, like qemu_nic / qemu_usb_ohci.
//
// Register offsets below are RELATIVE to the operational register base,
// which is BAR0 + CAPLENGTH (QEMU reports CAPLENGTH = 0x20). Capability
// registers (CAPLENGTH/HCIVERSION/HCSPARAMS/HCCPARAMS) are read from BAR0.
// Verified against QEMU's hw/usb/hcd-ehci-pci.c + ehci-regs.h.
//
// Variant-owned code; no genuine sources involved.

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

namespace
    {
    enum
        {
        // Capability registers, from BAR0.
        kCapLength       = 0x00,       // byte
        kHciVersion      = 0x02,       // word (0x0100 = EHCI 1.0)
        kHcsParams       = 0x04,       // dword, bits 0..7 = port count
        kHccParams       = 0x08,       // dword

        // Operational registers, relative to BAR0 + CAPLENGTH.
        kUsbCmd          = 0x00,
        kUsbSts          = 0x04,
        kUsbIntr         = 0x08,
        kFrIndex         = 0x0C,
        kCtrlDsSegment   = 0x10,
        kPeriodicListBase= 0x14,
        kAsyncListAddr   = 0x18,
        kConfigFlag      = 0x40,
        kPortSC          = 0x44,       // one dword per root port

        kCmdRun          = 0x00000001u,
        kCmdReset        = 0x00000002u,
        kCmdPeriodic     = 0x00000010u, // PSE
        kCmdAsync        = 0x00000020u, // ASE

        kStsHalted       = 0x00001000u,

        kCfgRoute        = 0x00000001u, // route all ports to this controller

        kPsCCS           = 0x00000001u, // current connect status
        kPsCSC           = 0x00000002u, // connect status change (RWC)
        kPsPED           = 0x00000004u, // port enabled
        kPsPEC           = 0x00000008u, // port enable change (RWC)
        kPsReset         = 0x00000100u, // port reset
        kPsSpeedMask     = 0x0000C000u, // bits 14..15: 0=full 1=low 2=high
        kPsSpeedShift    = 14,
        };

    unsigned gBar = 0;                  // BAR0 MMIO base (identity-mapped)
    unsigned gOp = 0;                   // BAR0 + CAPLENGTH
    unsigned gCapLen = 0, gVersion = 0, gHcs = 0, gHcc = 0;
    unsigned gBus = 0, gDev = 0, gFn = 0;
    int gPorts = 0;
    TBool gOk = EFalse;

    // 4K-aligned DMA-safe storage for the schedules. Static BSS lives in the
    // identity-mapped low RAM region, so no DMA mapping is required.
    __attribute__((aligned(4096))) static TUint32 gFrameList[1024];
    __attribute__((aligned(4096))) static TUint32 gAsyncQh[32];

    static inline TUint8 CapRd8(TUint aOff)
        { return *(volatile TUint8*)(gBar + aOff); }
    static inline TUint16 CapRd16(TUint aOff)
        { return *(volatile TUint16*)(gBar + aOff); }
    static inline TUint32 CapRd32(TUint aOff)
        { return *(volatile TUint32*)(gBar + aOff); }
    static inline void Wr(TUint aOff, TUint32 aVal)
        { *(volatile TUint32*)(gOp + aOff) = aVal; }
    static inline TUint32 Rd(TUint aOff)
        { return *(volatile TUint32*)(gOp + aOff); }

    // Boot constructors run before the PIT tick is bound, so
    // NKern::TickCount() may not advance during ehci_init(). Use a bounded
    // spin instead; QEMU's EHCI reset/attach needs no real-time delay.
    void ShortWait(TUint aUnits)
        {
        for (TUint i = 0; i < aUnits; ++i)
            {
            volatile TUint sink = 0;
            for (TUint j = 0; j < 20000; ++j)
                sink += (j | 1u);
            (void)sink;
            }
        }
    } // namespace

TInt ehci_init()
    {
    if (gOk)
        return KErrNone;

    // Find the first USB 2.0 controller: class 0x0C, subclass 0x03, IF 0x20.
    // PciFindClass() ignores the programming interface, so scan explicitly
    // (OHCI/UHCI share class 0c.03 but use interfaces 0x10/0x00).
    unsigned bar = 0;
    for (unsigned dev = 0; dev < 32 && !bar; ++dev)
        {
        if ((PciRead32(0, dev, 0, 0) & 0xFFFFu) == 0xFFFFu)
            continue;
        unsigned hdr = (PciRead32(0, dev, 0, 0x0C) >> 16) & 0xFFu;
        unsigned fns = (hdr & 0x80u) ? 8u : 1u;
        for (unsigned fn = 0; fn < fns && !bar; ++fn)
            {
            if ((PciRead32(0, dev, fn, 0) & 0xFFFFu) == 0xFFFFu)
                continue;
            TUint32 cr = PciRead32(0, dev, fn, 8);
            if (((cr >> 24) & 0xFFu) != 0x0C || ((cr >> 16) & 0xFFu) != 0x03 ||
                ((cr >> 8) & 0xFFu) != 0x20)
                continue;
            TUint32 b = PciRead32(0, dev, fn, 0x10);
            if (b & 1u)
                continue;                 // EHCI BAR0 is memory, not I/O
            bar = b & ~0xFFFu;
            if (!bar)
                continue;                 // BIOS/QEMU has not assigned it
            PciWrite32(0, dev, fn, 0x04,
                       PciRead32(0, dev, fn, 0x04) | 0x06u); // MEM | master
            gBus = 0; gDev = dev; gFn = fn;
            }
        }
    if (!bar)
        {
        KPrintf("ehci: no EHCI controller (class 0c.03.20)\r\n");
        return KErrNotSupported;
        }

    gBar = bar;
    gCapLen = CapRd8(kCapLength);
    if (!gCapLen || gCapLen > 0x80)
        gCapLen = 0x20;                   // defensive fallback (QEMU uses 0x20)
    gOp = gBar + gCapLen;
    gVersion = CapRd16(kHciVersion);
    gHcs = CapRd32(kHcsParams);
    gHcc = CapRd32(kHccParams);
    // HCSPARAMS bits 0..7 = number of downstream root ports.
    gPorts = (int)(gHcs & 0xFFu);
    if (gPorts > 8)
        gPorts = 8;

    // Halt before touching the schedule registers, then soft-reset.
    Wr(kUsbCmd, Rd(kUsbCmd) & ~(kCmdRun | kCmdAsync | kCmdPeriodic));
    for (int i = 0; i < 1000; ++i)
        {
        if (Rd(kUsbSts) & kStsHalted)
            break;
        ShortWait(1);
        }
    Wr(kUsbCmd, Rd(kUsbCmd) | kCmdReset);  // HCRST is self-clearing
    for (int i = 0; i < 1000; ++i)
        {
        if (!(Rd(kUsbCmd) & kCmdReset))
            break;
        ShortWait(1);
        }
    if (Rd(kUsbCmd) & kCmdReset)
        return KErrTimedOut;

    // Polled discipline: no USB interrupts.
    Wr(kUsbIntr, 0);
    Wr(kFrIndex, 0);

    // Empty periodic frame list (all entries terminate).
    for (int i = 0; i < 1024; ++i)
        gFrameList[i] = 0;
    // One idle async QH: H bit marks the head; its horizontal link points
    // back to itself (QH type + valid) so QEMU's schedule walker terminates.
    for (int i = 0; i < 32; ++i)
        gAsyncQh[i] = 0;
    gAsyncQh[0] = ((TUint32)&gAsyncQh[0] & ~0x1Fu) | 0x2u;
    gAsyncQh[1] = 0x00008000u;            // endpoint characteristics: H bit
    gAsyncQh[4] = 1;                      // current qTD: terminate
    gAsyncQh[5] = 1;                      // next qTD: terminate
    gAsyncQh[6] = 1;                      // alt next qTD: terminate

    Wr(kPeriodicListBase, (TUint32)&gFrameList[0]);
    Wr(kAsyncListAddr, (TUint32)&gAsyncQh[0]);
    // CTRLDSSEGMENT only exists/works when HCCPARAMS advertises 64-bit
    // addressing. Identity mapping keeps the high dword at 0 either way.
    if (gHcc & 0x1u)
        Wr(kCtrlDsSegment, 0);

    // Take ownership of all root ports from the companion UHCI controllers
    // and start the host controller with only the async schedule enabled.
    Wr(kConfigFlag, kCfgRoute);
    ShortWait(20);
    Wr(kUsbCmd, kCmdRun | kCmdAsync);

    gOk = ETrue;
    KPrintf("ehci: ver=%x bar=%x pci=%02x:%02x.%x caplen=%x hccp=%x ports=%d\r\n",
        gVersion, gBar, gBus, gDev, gFn, gCapLen, gHcc, gPorts);
    return KErrNone;
    }

// Raw PORTSC for one zero-based root port. 0xFFFFFFFF = no controller/bad port.
// Bits: CCS=0x1 CSC=0x2 PED=0x4 PEDC=0x8 RESET=0x100 speed=bits14..15.
TUint32 ehci_port_status(int aPort)
    {
    if (!gOk || aPort < 0 || aPort >= gPorts)
        return 0xFFFFFFFFu;
    return Rd(kPortSC + (unsigned)aPort * 4);
    }

int ehci_ports()
    {
    return gOk ? gPorts : 0;
    }

// Reset one root port. QEMU completes the reset on the 1->0 edge of RESET,
// then sets PED for a high-speed device; poll until that bit appears.
TInt ehci_port_reset(int aPort)
    {
    TUint32 v = ehci_port_status(aPort);
    if (v == 0xFFFFFFFFu)
        return KErrArgument;
    if (!(v & kPsCCS))
        return KErrNotFound;
    TUint off = kPortSC + (unsigned)aPort * 4;
    Wr(off, (v & ~(kPsCSC | kPsPEC)) | kPsReset);
    ShortWait(60);
    Wr(off, v & ~(kPsCSC | kPsPEC | kPsReset));
    for (int i = 0; i < 200; ++i)
        {
        v = Rd(off);
        if (v & kPsPED)
            return KErrNone;
        ShortWait(1);
        }
    return (v & kPsCCS) ? KErrGeneral : KErrNotFound;
    }

void ehci_info()
    {
    if (!gOk && ehci_init() != KErrNone)
        {
        KPrintf("ehci: no EHCI controller (class 0c.03.20)\r\n");
        return;
        }
    KPrintf("ehci: ver=%x bar=%x pci=%02x:%02x.%x caplen=%x hccp=%x ports=%d\r\n",
        gVersion, gBar, gBus, gDev, gFn, gCapLen, gHcc, gPorts);
    for (int p = 0; p < gPorts; ++p)
        {
        TUint32 s = ehci_port_status(p);
        TUint speed = (s & kPsSpeedMask) >> kPsSpeedShift;
        const char* sn = (speed == 2) ? "high" :
                         (speed == 1) ? "low" :
                         (speed == 0) ? "full" : "?";
        KPrintf("ehci: port %d status=%x conn=%d en=%d reset=%d speed=%s\r\n",
            p, s, (TInt)(s & kPsCCS) ? 1 : 0, (TInt)(s & kPsPED) ? 1 : 0,
            (TInt)(s & kPsReset) ? 1 : 0, sn);
        }
    }

// HwInit0 walks .init_array after paging/TheAsic are ready. ehci_init() is
// idempotent, so a later `ehci` shell command simply reuses the boot state.
__attribute__((constructor)) static void EhciBootInit()
    { ehci_init(); }

// qemu_usb_ohci.cpp — OHCI USB 1.1 host backend (polled).
//
// Why OHCI: VirtualBox has no UHCI (OHCI/EHCI only); OHCI covers USB 1.1
// HID on VBox and QEMU's pci-ohci. Same polled discipline as the UHCI
// backend: no IRQs, CC-field polling, per-transfer ED rebuild (control) or
// persistent periodic ED (interrupt). Identity-mapped RAM: virtual == phys.
//
// Front-end (enumeration, HID, shell) lives in qemu_usb.cpp and calls
// OhciControl/OhciIntPoll through the hc dispatch. Buffers are 256B-aligned
// so no transfer crosses a 4KB page (OHCI CBP..BE rule).
//
// References: Compaq OHCI 1.0a (§4 ED/TD, §5 init, §7 registers), Linux
// ohci.h (bit defines). Variant code; no genuine sources.

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

static inline void Out32(unsigned aPort, unsigned aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"((unsigned short)aPort)); }
static inline unsigned In32(unsigned aPort)
    { unsigned v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"((unsigned short)aPort)); return v; }

static unsigned PciRead(unsigned aBus, unsigned aDev, unsigned aFn, unsigned aReg)
    {
    Out32(0xCF8, 0x80000000u | (aBus << 16) | (aDev << 11) | (aFn << 8) | (aReg & 0xFCu));
    return In32(0xCFC);
    }

static void PciWrite(unsigned aBus, unsigned aDev, unsigned aFn, unsigned aReg, unsigned aVal)
    {
    Out32(0xCF8, 0x80000000u | (aBus << 16) | (aDev << 11) | (aFn << 8) | (aReg & 0xFCu));
    Out32(0xCFC, aVal);
    }

enum
    {
    kHcControl = 0x04, kHcCmd = 0x08, kHcIntDis = 0x14, kHcHCCA = 0x18,
    kHcCtlHead = 0x20, kHcFmInterval = 0x34, kHcPeriodic = 0x40,
    kHcLsThresh = 0x44, kHcRhA = 0x48, kHcRhStatus = 0x50, kHcRhPort = 0x54,
    kCtlOp = 0x80, kCtlCLE = 0x10, kCtlIE = 0x08, kCtlPLE = 0x04,
    kCmdHCR = 0x01, kCmdCLF = 0x02,
    kCcOk = 0x0, kCcNA = 0xF,
    kDpSetup = 0x00000000u, kDpOut = 0x00080000u, kDpIn = 0x00100000u,
    kTData0 = 0x02000000u, kTData1 = 0x03000000u,
    kDiNone = 0x00E00000u, kRound = 0x00040000u,
    kPsCCS = 0x00000001u, kPsPES = 0x00000002u, kPsPRS = 0x00000010u,
    kPsPPS = 0x00000100u, kPsLSDA = 0x00000200u, kPsCSC = 0x00010000u,
    kPsPRSC = 0x00100000u,
    };

struct OhciEd
    {
    TUint32 info, tailp, headp, next;
    };

struct OhciTd
    {
    TUint32 info, cbp, next, be;
    };

namespace
    {
    unsigned gBase = 0;
    TBool gOk = EFalse;
    int gPorts = 0;

    __attribute__((aligned(256))) TUint8 gHcca[256];
    __attribute__((aligned(16))) OhciEd gEdCtl;
    __attribute__((aligned(16))) OhciEd gEdInt;
    __attribute__((aligned(16))) OhciTd gTds[40];
    __attribute__((aligned(16))) OhciTd gTail;
    __attribute__((aligned(256))) TUint8 gXfer[256];
    TUint8 gSetupPkt[8];

    inline void Wr(unsigned aOff, unsigned aVal)
        { *(volatile unsigned*)(gBase + aOff) = aVal; }
    inline unsigned Rd(unsigned aOff)
        { return *(volatile unsigned*)(gBase + aOff); }

    void MsWait(TUint aMs)
        {
        TUint32 t0 = NKern::TickCount();
        while ((TUint32)(NKern::TickCount() - t0) < aMs) { }
        }

    TUint EdInfo(TUint8 aAddr, TUint8 aEp, TUint8 aMaxPkt, TBool aLow)
        {
        return (TUint)(aAddr & 0x7F) | ((TUint)(aEp & 0xF) << 7) |
               ((TUint)(aMaxPkt) << 16) | (aLow ? (1u << 13) : 0);
        }

    // Queue n TDs on a fresh control ED and ring CLF. Polls CC fields.
    TInt RunCtl(OhciTd* aTds, int aN, TUint8 aAddr, TUint8 aEp,
                TUint8 aMaxPkt, TBool aLow)
        {
        for (int i = 0; i < aN - 1; ++i)
            aTds[i].next = (TUint32)&aTds[i + 1];
        aTds[aN - 1].next = (TUint32)&gTail;
        gEdCtl.info = EdInfo(aAddr, aEp, aMaxPkt, aLow);
        gEdCtl.tailp = (TUint32)&gTail;
        gEdCtl.headp = (TUint32)&aTds[0];
        gEdCtl.next = 0;
        Wr(kHcCtlHead, (unsigned)&gEdCtl);
        Wr(kHcCmd, Rd(kHcCmd) | kCmdCLF);
        TUint32 t0 = NKern::TickCount();
        for (;;)
            {
            TBool done = ETrue;
            for (int i = 0; i < aN; ++i)
                if ((aTds[i].info >> 28) == kCcNA)
                    { done = EFalse; break; }
            if (done)
                break;
            if ((TUint32)(NKern::TickCount() - t0) > 2000)
                return KErrTimedOut;
            }
        for (int i = 0; i < aN; ++i)
            if ((aTds[i].info >> 28) != kCcOk)
                return KErrGeneral;
        return KErrNone;
        }

    void MkTd(OhciTd* aTd, TUint aDp, TUint aToggle, const void* aBuf, TUint aLen)
        {
        aTd->info = (0xFu << 28) | aToggle | kDiNone | kRound | aDp;
        if (!aLen)
            {
            aTd->cbp = 0;
            aTd->be = 0;
            }
        else
            {
            aTd->cbp = (TUint32)aBuf;
            aTd->be = (TUint32)aBuf + aLen - 1;
            }
        aTd->next = 0;
        }
    }

TInt OhciPresent()
    {
    for (unsigned dev = 0; dev < 32; ++dev)
        {
        unsigned id = PciRead(0, dev, 0, 0);
        if ((id & 0xFFFFu) == 0xFFFFu)
            continue;
        unsigned hdr = (PciRead(0, dev, 0, 0x0C) >> 16) & 0xFFu;
        unsigned fns = (hdr & 0x80u) ? 8u : 1u;
        for (unsigned fn = 0; fn < fns; ++fn)
            {
            unsigned v = PciRead(0, dev, fn, 8);
            if (((v >> 24) & 0xFFu) != 0x0C || (((v >> 16) & 0xFFu) != 3))
                continue;
            if ((((v >> 8) & 0xFFu) != 0x10))
                continue;  // want OHCI (UHCI/xHCI live elsewhere)
            return 1;
            }
        }
    return 0;
    }

TInt OhciInit()
    {
    if (gOk)
        return KErrNone;
    unsigned base = 0;
    for (unsigned dev = 0; dev < 32 && !base; ++dev)
        {
        unsigned id = PciRead(0, dev, 0, 0);
        if ((id & 0xFFFFu) == 0xFFFFu)
            continue;
        unsigned hdr = (PciRead(0, dev, 0, 0x0C) >> 16) & 0xFFu;
        unsigned fns = (hdr & 0x80u) ? 8u : 1u;
        for (unsigned fn = 0; fn < fns && !base; ++fn)
            {
            unsigned v = PciRead(0, dev, fn, 8);
            if (((v >> 24) & 0xFFu) != 0x0C || (((v >> 16) & 0xFFu) != 3))
                continue;
            if ((((v >> 8) & 0xFFu) != 0x10))
                continue;
            unsigned bar = PciRead(0, dev, fn, 0x10);
            if (bar & 1u)
                continue;  // OHCI BAR is MMIO
            base = bar & ~0xFFFu;
            unsigned cmd = PciRead(0, dev, fn, 4);
            PciWrite(0, dev, fn, 4, cmd | 0x06u);  // mem space + bus master
            }
        }
    if (!base)
        return KErrNotSupported;
    gBase = base;
    Wr(kHcCmd, kCmdHCR);  // hardware reset
    for (int i = 0; i < 1000; ++i)
        {
        if (!(Rd(kHcCmd) & kCmdHCR))
            break;
        MsWait(1);
        }
    if (Rd(kHcCmd) & kCmdHCR)
        return KErrTimedOut;
    Wr(kHcIntDis, 0x80000000u);  // MIE off: polled, like UHCI
    Wr(kHcHCCA, (unsigned)&gHcca[0]);
    for (int i = 0; i < 256; ++i)
        gHcca[i] = 0;
    // Persistent interrupt ED in every periodic slot (1ms poll is fine).
    gEdInt.info = 0;
    gEdInt.tailp = (TUint32)&gTail;
    gEdInt.headp = (TUint32)&gTail;
    gEdInt.next = 0;
    for (int i = 0; i < 32; ++i)
        ((TUint32*)&gHcca[0])[i] = (unsigned)&gEdInt;
    gTail.info = (0xFu << 28) | kDiNone;
    gTail.cbp = 0; gTail.next = 0; gTail.be = 0;
    Wr(kHcFmInterval, 11999);
    Wr(kHcPeriodic, 0x2A2F);
    Wr(kHcLsThresh, 0x0628);
    Wr(kHcRhStatus, 1u << 16);  // SetGlobalPower (no-op if per-port)
    Wr(kHcControl, kCtlOp | kCtlCLE | kCtlIE | kCtlPLE);
    gPorts = (int)(Rd(kHcRhA) & 0xFFu);
    if (gPorts > 4)
        gPorts = 4;
    gOk = ETrue;
    return KErrNone;
    }

int OhciPorts()
    {
    return gOk ? gPorts : 0;
    }

// Power + reset one root-hub port. Speed via aLow.
TInt OhciPortReset(int aPort, TBool& aLow)
    {
    if (!gOk || aPort < 0 || aPort >= gPorts)
        return KErrArgument;
    unsigned p = gBase + kHcRhPort + (unsigned)aPort * 4;
    unsigned v = *(volatile unsigned*)p;
    if (!(v & kPsCCS))
        return KErrNotFound;
    *(volatile unsigned*)p = kPsPPS;  // set port power
    MsWait(110);
    *(volatile unsigned*)p = kPsCSC;  // clear connect change
    *(volatile unsigned*)p = kPsPRS;  // set port reset
    MsWait(60);
    v = *(volatile unsigned*)p;
    *(volatile unsigned*)p = kPsPRSC;  // clear reset change
    if (!(v & kPsCCS))
        return KErrNotFound;
    aLow = (v & kPsLSDA) ? (TBool)1 : (TBool)0;
    if (!(v & kPsPES))
        return KErrGeneral;  // reset did not enable
    return KErrNone;
    }

TInt OhciControl(TUint8 aAddr, TUint8 aMaxPkt, TBool aLow,
                 TUint8 aReqType, TUint8 aReq, TUint16 aValue, TUint16 aIndex,
                 TUint16 aLen, TUint8* aData, TBool aIn)
    {
    if (!gOk)
        return KErrNotReady;
    gSetupPkt[0] = aReqType; gSetupPkt[1] = aReq;
    gSetupPkt[2] = (TUint8)aValue; gSetupPkt[3] = (TUint8)(aValue >> 8);
    gSetupPkt[4] = (TUint8)aIndex; gSetupPkt[5] = (TUint8)(aIndex >> 8);
    gSetupPkt[6] = (TUint8)aLen; gSetupPkt[7] = (TUint8)(aLen >> 8);
    OhciTd* t = gTds;
    MkTd(&t[0], kDpSetup, kTData0, gSetupPkt, 8);
    int n = 1;
    TUint tog = 1, left = aLen;
    TUint8* dst = gXfer;
    if (!aIn)
        {
        for (TUint i = 0; i < aLen; ++i)
            dst[i] = aData[i];
        }
    while (left)
        {
        TUint chunk = left > aMaxPkt ? aMaxPkt : left;
        MkTd(&t[n], aIn ? kDpIn : kDpOut, tog ? kTData1 : kTData0,
             dst, chunk);
        tog ^= 1;
        dst += chunk;
        left -= chunk;
        ++n;
        }
    MkTd(&t[n], aIn ? kDpOut : kDpIn, kTData1, 0, 0);
    ++n;
    TInt r = RunCtl(t, n, aAddr, 0, aMaxPkt, aLow);
    if (r == KErrNone && aIn)
        for (TUint i = 0; i < aLen; ++i)
            aData[i] = gXfer[i];
    return r;
    }

// Single interrupt-IN, explicit toggle. CC==0xF (NotAccessed) = idle/NAK.
TInt OhciIntPoll(TUint8 aAddr, TUint8 aEp, TUint8& aToggle,
                 TUint8* aDst, TUint aLen, TBool aLow, TUint8 aMaxPkt)
    {
    if (!gOk)
        return KErrNotReady;
    for (TUint i = 0; i < aLen; ++i)
        gXfer[i] = 0;
    OhciTd* t = &gTds[36];
    MkTd(t, kDpIn, aToggle ? kTData1 : kTData0, gXfer, aLen);
    t->next = (TUint32)&gTail;
    gEdInt.info = ((TUint)(aAddr & 0x7F)) | ((TUint)(aEp & 0xF) << 7) |
                  ((TUint)aMaxPkt << 16) | (aLow ? (1u << 13) : 0);
    gEdInt.tailp = (TUint32)&gTail;
    gEdInt.headp = (TUint32)t;
    gEdInt.next = 0;
    TUint32 t0 = NKern::TickCount();
    TInt r = KErrTimedOut;
    for (;;)
        {
        TUint cc = t->info >> 28;
        if (cc != kCcNA)
            {
            r = (cc == kCcOk) ? KErrNone : KErrGeneral;
            break;
            }
        if ((TUint32)(NKern::TickCount() - t0) > 20)
            break;
        }
    if (r == KErrNone)
        {
        for (TUint i = 0; i < aLen; ++i)
            aDst[i] = gXfer[i];
        aToggle ^= 1;
        return 1;
        }
    return (r == KErrTimedOut) ? KErrNotFound : r;
    }

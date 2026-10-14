// qemu_usb.cpp — UHCI USB 1.1 host + HID boot keyboard/mouse (polled).
//
// Why UHCI first: QEMU's piix3 presents 8086:7020; UHCI is port-I/O with a
// simple TD/QH schedule (OHCI/EHCI/xHCI come later for VirtualBox). Polled
// like our ATA driver: no IRQ wiring, UsbPoll() runs in the shell loop.
// Identity-mapped RAM means virtual == physical for TD/QH/buffers.
//
// References: Intel UHCI 1.1d (290E), FreeBSD uhci.h (TD bit layout),
// USB 1.1 + HID 1.1 (boot protocol). Variant code; no genuine sources.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

extern "C" void QemuKbdPushUsb(char aC);

static inline void Out8(unsigned aPort, unsigned char aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "d"((unsigned short)aPort)); }
static inline void Out16(unsigned aPort, unsigned short aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "d"((unsigned short)aPort)); }
static inline void Out32(unsigned aPort, unsigned aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"((unsigned short)aPort)); }
static inline unsigned char In8(unsigned aPort)
    { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "d"((unsigned short)aPort)); return v; }
static inline unsigned short In16(unsigned aPort)
    { unsigned short v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "d"((unsigned short)aPort)); return v; }
static inline unsigned int In32(unsigned aPort)
    { unsigned int v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"((unsigned short)aPort)); return v; }

// ---- PCI config (bus 0 only is enough for piix3) ----
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

// ---- UHCI constants ----
enum
    {
    kUsbCmd = 0x00, kUsbSts = 0x02, kUsbIntr = 0x04, kFrNum = 0x06,
    kFlBase = 0x08, kSof = 0x0C, kPort1 = 0x10, kPort2 = 0x12,
    kCmdRS = 0x0001, kCmdHCR = 0x0002, kCmdCF = 0x0040, kCmdMaxP = 0x0080,
    kStHCHalted = 0x0020,
    kPortCCS = 0x0001, kPortCSC = 0x0002, kPortPED = 0x0004,
    kPortLSDA = 0x0100, kPortPRST = 0x0200,
    kPidSetup = 0x2D, kPidIn = 0x69, kPidOut = 0xE1,
    kTdActive = 0x00800000u, kTdErr3 = 0x18000000u, kTdLS = 0x04000000u,
    kTdStalled = 0x00400000u,
    kLinkT = 0x1, kLinkQ = 0x2, kLinkV = 0x4,
    };

struct Td
    {
    TUint32 link, status, token, buffer;
    };

struct Qh
    {
    TUint32 head, element;
    };

namespace
    {
    unsigned gIo = 0;          // UHCI I/O base
    TBool gInit = EFalse;    TBool gPollOn = EFalse;    // UsbPoll active (keyboard feeding shell)

    __attribute__((aligned(4096))) TUint32 gFrames[1024];
    __attribute__((aligned(16))) Qh gQhCtl;
    __attribute__((aligned(16))) Qh gQhInt;
    __attribute__((aligned(16))) Td gTds[40];
    TUint8 gSetup[8];
    TUint8 gBuf[256];

    struct UsbDev
        {
        TBool present;
        TBool lowspeed;
        int hc;            // 0 = UHCI, 1 = OHCI
        TUint8 addr;
        TUint8 maxpkt0;
        TUint8 ifaceClass, ifaceSub, ifaceProto;
        TUint8 kbdEp, kbdInterval, kbdMaxPkt;
        TUint8 kbdToggle;
        TUint8 mouseEp, mouseInterval, mouseMaxPkt;
        TUint8 mouseToggle;
        TUint8 prevKeys[6];
        };
    UsbDev gDevs[4];
    TInt gNDevs = 0;
    TUint32 gUsbKeys = 0;  // chars pushed from USB HID (bring-up counter)

    void MsWait(TUint aMs)
        {
        TUint32 t0 = NKern::TickCount();
        while ((TUint32)(NKern::TickCount() - t0) < aMs) { }
        }

    // Queue one TD on the control QH and wait for completion.
    TInt RunChain(Td* aTds, int aN, TUint aTimeoutMs)
        {
        for (int i = 0; i < aN - 1; ++i)
            aTds[i].link = (TUint32)&aTds[i + 1] | kLinkV;
        aTds[aN - 1].link = kLinkT;
        gQhCtl.element = (TUint32)&aTds[0];
        TUint32 t0 = NKern::TickCount();
        for (;;)
            {
            TBool done = ETrue;
            for (int i = 0; i < aN; ++i)
                if (aTds[i].status & kTdActive)
                    { done = EFalse; break; }
            if (done)
                break;
            if ((TUint32)(NKern::TickCount() - t0) > aTimeoutMs)
                {
                gQhCtl.element = kLinkT;
                return KErrTimedOut;
                }
            }
        gQhCtl.element = kLinkT;
        // Done-OK = bits 16..23 clear (no Active, no error flags).
        for (int i = 0; i < aN; ++i)
            if (aTds[i].status & 0x00FF0000u)
                return KErrGeneral;
        return KErrNone;
        }

    void MkTd(Td* aTd, TUint8 aPid, TUint8 aAddr, TUint8 aEp, TUint aToggle,
              const void* aBuf, TUint aLen, TBool aLow)
        {
        TUint ml = aLen ? (aLen - 1) & 0x7FFu : 0x7FFu;
        aTd->link = kLinkT;
        aTd->status = kTdActive | kTdErr3 | (aLow ? kTdLS : 0);
        aTd->token = (TUint32)aPid | ((TUint32)aAddr << 8) |
                     ((TUint32)(aEp & 0xF) << 15) |
                     ((TUint32)(aToggle & 1) << 19) | (ml << 21);
        aTd->buffer = (TUint32)aBuf;
        }

    // One USB control transfer (SETUP + optional DATA + STATUS).
    TInt Control(TUint8 aAddr, TUint8 aMaxPkt, TBool aLow,
                 TUint8 aReqType, TUint8 aReq, TUint16 aValue, TUint16 aIndex,
                 TUint16 aLen, TUint8* aData, TBool aIn)
        {
        gSetup[0] = aReqType; gSetup[1] = aReq;
        gSetup[2] = (TUint8)aValue; gSetup[3] = (TUint8)(aValue >> 8);
        gSetup[4] = (TUint8)aIndex; gSetup[5] = (TUint8)(aIndex >> 8);
        gSetup[6] = (TUint8)aLen; gSetup[7] = (TUint8)(aLen >> 8);
        Td* t = gTds;
        MkTd(&t[0], kPidSetup, aAddr, 0, 0, gSetup, 8, aLow);
        int n = 1;
        TUint toggle = 1, left = aLen;
        TUint8* p = aData;
        while (left)
            {
            TUint chunk = left > aMaxPkt ? aMaxPkt : left;
            MkTd(&t[n], aIn ? kPidIn : kPidOut, aAddr, 0, toggle, p, chunk, aLow);
            toggle ^= 1;
            p += chunk;
            left -= chunk;
            ++n;
            }
        MkTd(&t[n], aIn ? kPidOut : kPidIn, aAddr, 0, 1, 0, 0, aLow);
        ++n;
        return RunChain(t, n, 2000);
        }

    TInt UhciReset()
        {
        Out16(gIo + kUsbCmd, kCmdHCR);
        for (int i = 0; i < 1000; ++i)
            {
            if (!(In16(gIo + kUsbCmd) & kCmdHCR))
                break;
            MsWait(1);
            }
        if (In16(gIo + kUsbCmd) & kCmdHCR)
            return KErrTimedOut;
        // Skeleton schedule: every frame -> control QH -> interrupt QH -> end.
        gQhCtl.head = (TUint32)&gQhInt | kLinkQ;
        gQhCtl.element = kLinkT;
        gQhInt.head = kLinkT;
        gQhInt.element = kLinkT;
        for (int i = 0; i < 1024; ++i)
            gFrames[i] = (TUint32)&gQhCtl | kLinkQ;
        Out32(gIo + kFlBase, (unsigned)&gFrames[0]);
        Out8(gIo + kSof, 0x40);
        Out16(gIo + kUsbSts, 0xFFFF);  // clear status
        Out16(gIo + kUsbIntr, 0);      // polled: no interrupts
        Out16(gIo + kUsbCmd, kCmdRS | kCmdCF | kCmdMaxP);
        return KErrNone;
        }

    // Reset + enable one root port. Returns speed via aLow.
    TInt PortReset(int aPort, TBool& aLow)
        {
        unsigned p = (unsigned)(gIo + (aPort ? kPort2 : kPort1));
        unsigned short sc = In16(p);
        if (!(sc & kPortCCS))
            return KErrNotFound;
        Out16(p, kPortPRST);           // keep other bits 0 (clears CSC)
        MsWait(60);
        Out16(p, 0);
        MsWait(10);
        sc = In16(p);
        if (!(sc & kPortCCS))
            return KErrNotFound;
        aLow = (sc & kPortLSDA) ? (TBool)1 : (TBool)0;
        if (!(sc & kPortPED))
            {
            Out16(p, kPortPED);
            MsWait(10);
            }
        return KErrNone;
        }

    TInt GetDesc(int aHc, TUint8 aAddr, TUint8 aMaxPkt, TBool aLow,
                 TUint8 aType, TUint8 aIdx, TUint16 aLen)
        {
        if (aHc)
            return OhciControl(aAddr, aMaxPkt, aLow, 0x80, 6,
                               (TUint16)((aType << 8) | aIdx), 0, aLen,
                               gBuf, ETrue);
        return Control(aAddr, aMaxPkt, aLow, 0x80, 6,
                       (TUint16)((aType << 8) | aIdx), 0, aLen, gBuf, ETrue);
        }

    TInt XControl(int aHc, TUint8 aAddr, TUint8 aMaxPkt, TBool aLow,
                  TUint8 aReqType, TUint8 aReq, TUint16 aValue, TUint16 aIndex,
                  TUint16 aLen)
        {
        if (aHc)
            return OhciControl(aAddr, aMaxPkt, aLow, aReqType, aReq,
                               aValue, aIndex, aLen, gBuf, EFalse);
        return Control(aAddr, aMaxPkt, aLow, aReqType, aReq, aValue,
                       aIndex, aLen, gBuf, EFalse);
        }

    // Full enumeration of a reset port into gDevs[slot]. Addr = slot+1.
    TInt EnumerateDev(int aHc, TBool aLow, int aSlot)
        {
        UsbDev& d = gDevs[aSlot];
        d.hc = aHc;
        for (int i = 0; i < 6; ++i)
            d.prevKeys[i] = 0;
        d.kbdEp = 0; d.mouseEp = 0;
        d.kbdMaxPkt = 8; d.mouseMaxPkt = 4;
        // USB spec: SET_CONFIGURATION resets all endpoint toggles to DATA0.
        d.kbdToggle = 0; d.mouseToggle = 0;
        TBool low = aLow;
        d.lowspeed = low;
        TInt r;
        // 8-byte device descriptor at addr 0 (works with any maxpkt0).
        r = GetDesc(aHc, 0, 8, low, 1, 0, 8);
        if (r != KErrNone)
            return r;
        d.maxpkt0 = gBuf[7];
        if (!d.maxpkt0)
            d.maxpkt0 = 8;
        d.addr = (TUint8)(aSlot + 1);
        r = XControl(aHc, 0, d.maxpkt0, low, 0, 5, d.addr, 0, 0);
        if (r != KErrNone)
            return r;
        MsWait(10);
        r = GetDesc(aHc, d.addr, d.maxpkt0, low, 1, 0, 18);
        if (r != KErrNone)
            return r;
        // Full config descriptor: first 9 bytes give wTotalLength.
        r = GetDesc(aHc, d.addr, d.maxpkt0, low, 2, 0, 9);
        if (r != KErrNone)
            return r;
        TUint16 total = (TUint16)(gBuf[2] | (gBuf[3] << 8));
        if (total > sizeof(gBuf))
            total = sizeof(gBuf);
        r = GetDesc(aHc, d.addr, d.maxpkt0, low, 2, 0, total);
        if (r != KErrNone)
            return r;
        // Walk interfaces/endpoints for HID boot kbd + mouse.
        for (TUint o = 0; o + 9 <= total;)
            {
            TUint8 len = gBuf[o], type = gBuf[o + 1];
            if (!len || o + len > total)
                break;
            if (type == 4 && len >= 9 && gBuf[o + 5] == 3)
                {
                TUint8 sub = gBuf[o + 6], proto = gBuf[o + 7];
                TUint8 ifn = gBuf[o + 2];
                // Scan this interface's endpoint descriptors.
                for (TUint q = o + len; q + 7 <= total;)
                    {
                    TUint8 el = gBuf[q], et = gBuf[q + 1];
                    if (!el || q + el > total)
                        break;
                    if (et == 4)
                        break;  // next interface
                    if (et == 5 && el >= 7 && (gBuf[q + 3] & 3) == 3)
                        {
                        TUint8 ep = gBuf[q + 2] & 0x0F;
                        TUint8 iv = gBuf[q + 6];
                        TUint16 mp = (TUint16)(gBuf[q + 4] | (gBuf[q + 5] << 8));
                        if (mp > 64)
                            mp = 64;
                        if (!iv)
                            iv = 10;
                        if (sub == 1 && proto == 1 && !d.kbdEp)
                            {
                            d.kbdEp = ep;
                            d.kbdInterval = iv;
                            d.kbdMaxPkt = mp ? (TUint8)mp : 8;
                            d.ifaceClass = 3; d.ifaceSub = sub; d.ifaceProto = proto;
                            // Boot protocol for this interface.
                            XControl(aHc, d.addr, d.maxpkt0, low, 0x21, 0x0B, 0,
                                    ifn, 0);
                            }
                        else if (sub == 1 && proto == 2 && !d.mouseEp)
                            {
                            d.mouseEp = ep;
                            d.mouseInterval = iv;
                            d.mouseMaxPkt = mp ? (TUint8)mp : 4;
                            XControl(aHc, d.addr, d.maxpkt0, low, 0x21, 0x0B, 0,
                                    ifn, 0);
                            }
                        }
                    q += el;
                    }
                }
            o += len;
            }
        r = XControl(aHc, d.addr, d.maxpkt0, low, 0, 9, 1, 0, 0);
        if (r != KErrNone)
            return r;
        d.present = ETrue;
        return KErrNone;
        }

    // HID usage -> ASCII (pairs: plain, shifted).
    char HidChar(TUint8 aUse, TBool aShift)
        {
        if (aUse >= 0x04 && aUse <= 0x1D)
            return (char)('a' + (aUse - 0x04) + (aShift ? ('A' - 'a') : 0));
        switch (aUse)
            {
        case 0x1E: return aShift ? '!' : '1';
        case 0x1F: return aShift ? '@' : '2';
        case 0x20: return aShift ? '#' : '3';
        case 0x21: return aShift ? '$' : '4';
        case 0x22: return aShift ? '%' : '5';
        case 0x23: return aShift ? '^' : '6';
        case 0x24: return aShift ? '&' : '7';
        case 0x25: return aShift ? '*' : '8';
        case 0x26: return aShift ? '(' : '9';
        case 0x27: return aShift ? ')' : '0';
        case 0x28: return '\n';
        case 0x2A: return '\b';
        case 0x2B: return '\t';
        case 0x2C: return ' ';
        case 0x2D: return aShift ? '_' : '-';
        case 0x2E: return aShift ? '+' : '=';
        case 0x2F: return aShift ? '{' : '[';
        case 0x30: return aShift ? '}' : ']';
        case 0x31: return aShift ? '|' : '\\';
        case 0x33: return aShift ? ':' : ';';
        case 0x34: return aShift ? '"' : '\'';
        case 0x35: return aShift ? '~' : '`';
        case 0x36: return aShift ? '<' : ',';
        case 0x37: return aShift ? '>' : '.';
        case 0x38: return aShift ? '?' : '/';
        default: return 0;
            }
        }

    void KbdReport(int aSlot, const TUint8* aRep)
        {
        TBool shift = (aRep[0] & 0x22) ? (TBool)1 : (TBool)0;
        for (int i = 0; i < 6; ++i)
            {
            TUint8 k = aRep[2 + i];
            if (!k || k == 1)
                continue;  // empty / ErrorRollOver
            TBool seen = EFalse;
            for (int j = 0; j < 6; ++j)
                if (gDevs[aSlot].prevKeys[j] == k)
                    { seen = ETrue; break; }
            if (!seen)
                {
                char c = HidChar(k, shift);
                if (c)
                    {
                    QemuKbdPushUsb(c);
                    ++gUsbKeys;
                    }
                }
            }
        }

    void KbdRemember(int aSlot, const TUint8* aRep)
        {
        for (int i = 0; i < 6; ++i)
            gDevs[aSlot].prevKeys[i] = aRep[2 + i];
        }

    // One interrupt-IN poll on a device endpoint. Returns 1 on fresh data,
    // KErrNotFound on NAK (idle: return fast, the shell loop re-arms).
    // Dispatches on the device's host controller.
    TInt IntPoll(UsbDev& aDev, int aSlot, TUint8 aEp, TUint8& aToggle,
                 TUint8* aDst, TUint aLen)
        {
        (void)aSlot;
        if (aDev.hc)
            return OhciIntPoll(aDev.addr, aEp, aToggle, aDst, aLen,
                               aDev.lowspeed, (TUint8)aLen);
        Td* t = &gTds[32];
        MkTd(t, kPidIn, aDev.addr, aEp, aToggle, aDst, aLen, aDev.lowspeed);
        t->link = kLinkT;
        gQhInt.element = (TUint32)t;
        TUint32 t0 = NKern::TickCount();
        TInt r = KErrTimedOut;
        for (;;)
            {
            TUint32 st = t->status;
            if (!(st & kTdActive))
                { r = KErrNone; break; }  // completed: evaluate below
            if ((st & 0x00080000u) && !(st & 0x00760000u))
                { r = KErrNotFound; break; }  // NAK, no other error: idle
            if ((TUint32)(NKern::TickCount() - t0) > 20)
                break;
            }
        gQhInt.element = kLinkT;
        if (r == KErrNone)
            {
            TUint32 st = t->status;
            if (st & 0x007E0000u)
                return KErrGeneral;
            aToggle ^= 1;
            return 1;
            }
        return r;
        }
    }

TInt UsbInit()
    {
    if (gInit)
        return gNDevs ? KErrNone : KErrNotFound;
    // Find a UHCI controller (class 0c03, prog-if 0x00).
    unsigned io = 0;
    for (unsigned dev = 0; dev < 32 && !io; ++dev)
        {
        unsigned id = PciRead(0, dev, 0, 0);
        if ((id & 0xFFFFu) == 0xFFFFu)
            continue;
        unsigned hdr = (PciRead(0, dev, 0, 0x0C) >> 16) & 0xFFu;
        unsigned fns = (hdr & 0x80u) ? 8u : 1u;
        for (unsigned fn = 0; fn < fns && !io; ++fn)
            {
            unsigned v = PciRead(0, dev, fn, 8);
            if (((v >> 24) & 0xFFu) != 0x0C || (((v >> 16) & 0xFFu) != 3))
                continue;
            if (((v >> 8) & 0xFFu) != 0x00)
                continue;  // not UHCI (OHCI/EHCI/xHCI later)
            unsigned bar = PciRead(0, dev, fn, 0x20);
            if (!(bar & 1u))
                continue;  // UHCI BAR must be I/O space
            io = bar & ~0x1Fu;
            unsigned cmd = PciRead(0, dev, fn, 4);
            PciWrite(0, dev, fn, 4, cmd | 0x05u);  // I/O space + bus master
            }
        }
    gIo = io;
    for (int i = 0; i < 4; ++i)
        {
        gDevs[i].present = EFalse;
        gDevs[i].addr = 0;
        }
    gNDevs = 0;
    if (io && UhciReset() == KErrNone)
        {
        TBool low = (TBool)0;
        for (int p = 0; p < 2 && gNDevs < 4; ++p)
            {
            if (PortReset(p, low) != KErrNone)
                continue;
            if (EnumerateDev(0, low, gNDevs) == KErrNone)
                ++gNDevs;
            }
        }
    if (OhciPresent() && OhciInit() == KErrNone)
        {
        int ports = OhciPorts();
        for (int p = 0; p < ports && gNDevs < 4; ++p)
            {
            TBool low = (TBool)0;
            if (OhciPortReset(p, low) != KErrNone)
                continue;
            if (EnumerateDev(1, low, gNDevs) == KErrNone)
                ++gNDevs;
            }
        }
    if (!io && !OhciPresent())
        return KErrNotSupported;
    gInit = ETrue;
    gPollOn = (gNDevs > 0) ? (TBool)1 : (TBool)0;
    return gNDevs ? KErrNone : KErrNotFound;
    }

void UsbPoll()
    {
    if (!gPollOn)
        return;
    static TUint8 rep[8];
    for (int i = 0; i < gNDevs; ++i)
        {
        UsbDev& d = gDevs[i];
        if (!d.present || !d.kbdEp)
            continue;
        TInt r = IntPoll(d, i, d.kbdEp, d.kbdToggle, rep, 8);
        if (r == 1)
            {
            KbdReport(i, rep);
            KbdRemember(i, rep);
            }
        }
    }

void UsbList()
    {
    if (!gInit)
        {
        if (!XhciKeyboardReady())
            KPrintf("usb: not initialized (run `usb` first)\r\n");
        return;
        }
    if (!gNDevs)
        {
        if (!XhciKeyboardReady())
            KPrintf("usb: UHCI/OHCI ok, no devices\r\n");
        return;
        }
    for (int i = 0; i < gNDevs; ++i)
        {
        UsbDev& d = gDevs[i];
        KPrintf("usb dev %d: %s addr %d %s", i, d.hc ? "ohci" : "uhci",
            d.addr, d.lowspeed ? "low-speed" : "full-speed");
        if (d.kbdEp)
            KPrintf(" HID-keyboard ep %d", d.kbdEp);
        if (d.mouseEp)
            KPrintf(" HID-mouse ep %d", d.mouseEp);
        KPrintf("\r\n");
        }
    KPrintf("usb: %u key(s) from USB HID so far\r\n", gUsbKeys);
    }

void UsbDebugPoll()
    {
    if (!gNDevs || !gDevs[0].present || !gDevs[0].kbdEp)
        {
        KPrintf("usb: no keyboard to poll\r\n");
        return;
        }
    static TUint8 rep[8];
    for (int i = 0; i < 8; ++i)
        rep[i] = 0xAA;
    TInt r = IntPoll(gDevs[0], 0, gDevs[0].kbdEp, gDevs[0].kbdToggle, rep, 8);
    if (!gDevs[0].hc)
        KPrintf("poll: r=%d tog=%d st=0x%x rep=%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
            r, gDevs[0].kbdToggle, gTds[32].status,
            rep[0], rep[1], rep[2], rep[3], rep[4], rep[5], rep[6], rep[7]);
    else
        KPrintf("poll: r=%d tog=%d rep=%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
            r, gDevs[0].kbdToggle,
            rep[0], rep[1], rep[2], rep[3], rep[4], rep[5], rep[6], rep[7]);
    }

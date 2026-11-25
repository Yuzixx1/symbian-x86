// qemu_variant_int.cpp — QEMU Interrupt variant for the full Kern image.
//
// Genuine contract: kernel/eka/include/kernel/x86/assp.h class Interrupt
// (Bind/Unbind/Enable/Disable/Clear/SetPriority; AddTimingEntropy is kernel's).
// Interrupt routing on x86: genuine __X86VectorIrq (vectors.cia) funnels every
// hardware vector through the single X86_IrqHandler hook, which the variant
// installs (Asic::Init1). Our plain-C dispatcher identifies the active IRQ
// from the 8259 In-Service Registers, calls the bound TIsr, sends EOI, and
// returns into the genuine postamble (nest count + reschedule check stay
// genuine).

#include <x86.h>
#include <nk_priv.h>
#include <kernel/kernel.h>       // Kern::AddEvent, Kern::DfcQue0, TRawEvent (e32event.h)
#include <kernel/kern_priv.h>    // K::EventThread / K::EventHeadPtr / K::EventTailPtr
#include <e32keys.h>             // genuine TStdScanCode (EStdKey*) values
#include "qemu_variant.h"

static inline void Out8(unsigned short aPort, unsigned char aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
static inline unsigned char In8(unsigned short aPort)
    { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

// Direct PIC mask helpers (variant/qemu_assp.cpp).  These read the live IMR
// and clear/set one bit, unlike Interrupt::Enable's shadow mask (0xFF), which
// would re-mask the already-live tick and COM1 IRQs.
namespace QemuIrq
    {
    void Enable(int aIrq);
    void Disable(int aIrq);
    }

// Gating hook for the shell line editor.  Default ON (graphics apps need
// typematic suppression); a strong definition in kern_shell.cpp may return
// gTextMode ? 0 : 1 so holding Backspace in the shell still repeats.  Weak
// => link-safe until that hook lands.  Exact signature:
// extern "C" int QemuKbdTypematicSuppress(void).
extern "C" __attribute__((weak)) int QemuKbdTypematicSuppress() { return 1; }

namespace
    {
    struct QBinding
        {
        TIsr iIsr;
        TAny* iPtr;
        };
    QBinding gBindings[16];
    unsigned char gMasterMask = 0xFF;
    unsigned char gSlaveMask = 0xFF;

    // PS/2 delivery diagnostics (qemu_variant_int.cpp IRQ1 ISR).  Counted
    // separately from the shell's shared input ring so `kbd` can prove that
    // window keypresses really arrive through IRQ1 rather than COM1.
    volatile unsigned int gKbdIsrCount = 0;
    volatile unsigned int gKbdScancodes = 0;
    volatile unsigned int gKbdKeys = 0;

    // Per-key typematic suppression.  QEMU's 8042 emits its first hardware
    // typematic make ~500 ms after a physical press and then every ~100 ms.
    // A time-gap filter cannot work: kTypematicGapMs=100 ms is SHORTER than
    // the 500 ms initial delay, so the first repeat was never suppressed and
    // every ~3rd following repeat leaked through.  A 700 ms hold pushed 2-3
    // extra makes into the ring; after the break those queued makes re-armed
    // DOOM's DOWN latch with no physical press behind it -> stuck key.
    //
    // Fix: track DOWN STATE per scancode.  The first make of a press sets
    // sKeyDown[sc] and is forwarded; any later make of the SAME scancode while
    // the bit is still set is a hardware repeat and is DROPPED (no push, no
    // counter).  The matching break clears the bit.  In graphics mode each
    // physical press therefore yields exactly ONE make + ONE break, zero
    // backlog, zero re-arm; independent bits make multi-key holds work.
    // Gated by QemuKbdTypematicSuppress() so the text-mode shell still gets
    // its hardware typematic (Backspace / arrow-key repeat).
    unsigned char sKeyDown[128] = {0};   // 1 = this make scancode is held down

    // Post-break guard (secondary filter, kept).  A physical release arms a
    // short suppression window for that key's MAKE code: a typematic make
    // still queued in the 8042 output buffer when the break was read (and
    // delivered just after it) must not be forwarded as a fake press.  The
    // bitmap above clears on break, so this guard closes the small window
    // between the break and such a stray make.  Per-scancode, gated by the
    // same hook; text-mode shell repeats are unchanged.
    const unsigned int kPostBreakGapMs = 100u;
    unsigned int sLastBreakAt[128] = {0};   // NTickCount() of last break per key

    // Sparse ISR-safe diagnostic: prove repeats are actually being filtered.
    // Direct COM1 port I/O only -- no KPrintf/formatter/VGA teletype in IRQ
    // context.  First few drops only, so the serial log stays quiet.
    const int kMaxRepeatDropMarks = 32;
    int sRepeatDropMarks = 0;

    static void KbdSerialChar(unsigned char aC)
        {
        while (!(In8(0x3FD) & 0x20)) { }    // COM1 LSR.THRE
        Out8(0x3F8, aC);
        }

    static void KbdRepeatDropMark(unsigned char aSc)
        {
        if (sRepeatDropMarks >= kMaxRepeatDropMarks) return;
        ++sRepeatDropMarks;
        static const char kMsg[] = "KBD: repeat dropped ";
        for (const char* p = kMsg; *p; ++p) KbdSerialChar((unsigned char)*p);
        KbdSerialChar((unsigned char)('0' + ((aSc >> 4) & 0x0F)));
        KbdSerialChar((unsigned char)('0' + (aSc & 0x0F)));
        KbdSerialChar('\r');
        KbdSerialChar('\n');
        }

    void WriteMasks()
        {
        Out8(0x21, gMasterMask);
        Out8(0xA1, gSlaveMask);
        }

    void Eoi(int aIrq)
        {
        if (aIrq >= 8)
            Out8(0xA0, 0x20);  // slave first, then the cascade
        Out8(0x20, 0x20);      // master always needs its EOI
        }
    }

TInt Interrupt::Bind(TInt anId, TIsr anIsr, TAny* aPtr)
    {
    if (anId < 0 || anId >= 16 || !anIsr)
        return KErrArgument;
    gBindings[anId].iIsr = anIsr;
    gBindings[anId].iPtr = aPtr;
    return KErrNone;
    }

TInt Interrupt::Unbind(TInt anId)
    {
    if (anId < 0 || anId >= 16)
        return KErrArgument;
    gBindings[anId].iIsr = 0;
    gBindings[anId].iPtr = 0;
    return KErrNone;
    }

TInt Interrupt::Enable(TInt anId)
    {
    if (anId < 0 || anId >= 16)
        return KErrArgument;
    if (anId < 8)
        gMasterMask = (unsigned char)(gMasterMask & ~(1u << anId));
    else
        {
        gSlaveMask = (unsigned char)(gSlaveMask & ~(1u << (anId - 8)));
        gMasterMask = (unsigned char)(gMasterMask & ~0x04u);  // cascade
        }
    WriteMasks();
    return KErrNone;
    }

TInt Interrupt::Disable(TInt anId)
    {
    if (anId < 0 || anId >= 16)
        return KErrArgument;
    if (anId < 8)
        gMasterMask = (unsigned char)(gMasterMask | (1u << anId));
    else
        gSlaveMask = (unsigned char)(gSlaveMask | (1u << (anId - 8)));
    WriteMasks();
    return KErrNone;
    }

TInt Interrupt::Clear(TInt anId)
    {
    if (anId < 0 || anId >= 16)
        return KErrArgument;
    Eoi(anId);  // drop any pending request on the 8259
    return KErrNone;
    }

TInt Interrupt::SetPriority(TInt anId, TInt /*aPriority*/)
    {
    if (anId < 0 || anId >= 16)
        return KErrArgument;
    return KErrNone;  // 8259 has fixed priority order
    }

// --- C dispatch (called by genuine __X86VectorIrq via X86_IrqHandler) ---
// The vector number is NOT read from the stack: whether GCC emits a prologue
// for the genuine entry asm is unknowable here, so the active IRQ is instead
// read from the 8259 In-Service Registers (OCW3/0x0B) — layout-independent.
static int ActiveIrq()
    {
    Out8(0x20, 0x0Bu);
    int m = In8(0x20);
    Out8(0xA0, 0x0Bu);
    int s = In8(0xA0);
    if (s)
        {
        for (int i = 7; i >= 0; --i)
            if (s & (1 << i))
                return 8 + i;
        }
    if (m)
        {
        for (int i = 7; i >= 0; --i)
            if (m & (1 << i))
                return i;
        }
    return -1;  // spurious: nothing in service
    }

extern "C" void X86IrqDispatcher()
    {
    // Genuine installs IRQ gates only at 0x30+ (vectors.cia DECLARE_X86_INT);
    // 0x20/0x21 are exec-call gates. The 8259 ISR identifies the IRQ.
    int irq = ActiveIrq();
    if (irq < 0 || irq >= 16)
        {
        Out8(0x20, 0x20);  // spurious: keep the master happy
        return;
        }
    TIsr isr = gBindings[irq].iIsr;
    if (isr)
        isr(gBindings[irq].iPtr);
    Eoi(irq);
    }

// --- PIT tick ISR: genuine NTimerQ intake (cf. Phase-9 Irq0Handler_C) ---
extern "C" void QemuTickIsr(TAny* /*aPtr*/)
    {
    NTimerQ* q = (NTimerQ*)NTimerQ::TimerAddress();
    q->Tick();
    }

// --- Genuine EWSRV raw-input bridge -----------------------------------------
// PS/2 ISRs -> genuine EKA2 kernel event queue -> genuine EWSRV CEvent::RunL.
// Design mirrors the genuine interrupt-driven keyboard driver
// (kernelhwsrv bsptemplate .../keyboard_interrupt.cpp): the ISR only queues a
// DFC, and the DFC (thread context, kernel unlocked, interrupts on) calls the
// public Kern::AddEvent API — never from ISR context, whose preconditions
// (eventq.cpp: "call in a thread context", "interrupts enabled") forbid it.
//
// Gate: \KBINJ.TXT on the boot FAT volume, resolved once from QemuKbdArm() at
// shell time (FAT is mounted only after Asic::Init3's FatMount).  Content
// selects the source mask ("ptr"=pointer only, "key"=keyboard only, else both);
// absent file => gate OFF => the ws-ring path is unchanged.
namespace
    {
    const TUint kInjRingSize = 16;
    struct InjEnt
        {
        TRawEvent iEv;   // full genuine-layout event copy
        TInt iAux;       // key: TStdScanCode; pointer: (x<<16)|(y&0xffff); button: bit<<8|down
        };
    volatile TUint gInjHead = 0, gInjTail = 0;   // SPSC ring: ISR=producer, DFC=consumer
    InjEnt gInjRing[kInjRingSize];
    volatile TUint gInjQueued = 0, gInjDelivered = 0, gInjDropped = 0;
    // Gate mask: 0=off (or unresolved), bit0=pointer ISR, bit1=keyboard ISR.
    // 0x80000000 flag = unresolved (no volume yet at Init3); resolved once a
    // FAT volume is mounted (shell-time QemuKbdArm).
    volatile TUint gInjGate = 0x80000000u;
    volatile TBool gInjDfcQueued = EFalse;

    void KbdInjDfcFn(TAny*);
    TDfc gInjDfc(KbdInjDfcFn, 0, 1);             // priority 1 < KNumDfcPriorities => real DFC

    // TRawEvent's payload fields are protected; a local subclass initialises
    // them through the genuine <e32event.h> layout (same trick as the pointer
    // bridge's TPointerRawEvent in qemu_display.cpp — no magic offsets, no
    // #define protected public, no kernel patch).
    struct TPortRawEvent : public TRawEvent
        {
        void SetKey(TType aType, TInt aStdScanCode)
            {
            iType = (TUint8)aType;
            iTip = 0;
            iPointerNumber = 0;
            iDeviceNumber = 1;        // DeviceNumber() == iDeviceNumber-1 == 0
            iTicks = Kern::TickCount();
            iU.key.scanCode = aStdScanCode;
            iU.key.repeats = 0;
            }
        void SetPointer(TType aType, TInt aX, TInt aY)
            {
            iType = (TUint8)aType;
            iTip = 0;
            iPointerNumber = 0;
            iDeviceNumber = 1;
            iTicks = Kern::TickCount();
            iU.pos.x = aX;             // button events carry Pos() in EWSRV's RunL
            iU.pos.y = aY;
            }
        };

    // ISR-safe push into the ring + queue the DFC (TDfc::Add is ISR-safe).
    // aKindBit is the gate bit for this event source (1=pointer, 2=keyboard).
    void InjPush(const TRawEvent& aEv, TInt aAux, TUint aKindBit)
        {
        if (!(gInjGate & aKindBit))
            return;
        TUint h = gInjHead;
        TUint n = (h + 1) % kInjRingSize;
        if (n == (TUint)gInjTail)
            { ++gInjDropped; return; }          // ring full: drop, never block in ISR
        gInjRing[h].iEv = aEv;
        gInjRing[h].iAux = aAux;
        gInjHead = n;                            // x86 TSO: entry store precedes head store
        ++gInjQueued;
        if (gInjDfcQueued)
            gInjDfc.Add();
        }

    // DFC: thread context on Kern::DfcQue0()'s thread.  Wrap Kern::AddEvent in
    // ThreadEnterCS exactly like the genuine ExecHandler::AddEvent wrapper
    // (eventq.cpp:67-69); the rest of the preconditions (thread context, kernel
    // unlocked, interrupts enabled, no fast mutex held) already hold for a DFC.
    void KbdInjDfcFn(TAny*)
        {
        for (;;)
            {
            TUint t = gInjTail;
            if (t == (TUint)gInjHead)
                break;
            InjEnt e = gInjRing[t];
            gInjTail = (t + 1) % kInjRingSize;
            TRawEvent* tailBefore = K::EventTailPtr;
            DThread* evtThread = K::EventThread;
            NKern::ThreadEnterCS();
            TInt add = Kern::AddEvent(e.iEv);
            NKern::ThreadLeaveCS();
            ++gInjDelivered;
            // consumed=1 <=> K::TryDeliverEvent handed this event to the pending
            // K::EventRequest, i.e. genuine EWSRV's UserSvr::RequestEvent -> RunL.
            TBool consumed = K::EventTailPtr != tailBefore;
            const char* nm = "?";
            switch (e.iEv.Type())
                {
                case TRawEvent::EKeyDown:      nm = "EKeyDown"; break;
                case TRawEvent::EKeyUp:        nm = "EKeyUp"; break;
                case TRawEvent::EPointerMove:  nm = "EPointerMove"; break;
                case TRawEvent::EButton1Down:  nm = "EButton1Down"; break;
                case TRawEvent::EButton1Up:    nm = "EButton1Up"; break;
                case TRawEvent::EButton2Down:  nm = "EButton2Down"; break;
                case TRawEvent::EButton2Up:    nm = "EButton2Up"; break;
                case TRawEvent::EButton3Down:  nm = "EButton3Down"; break;
                case TRawEvent::EButton3Up:    nm = "EButton3Up"; break;
                default: break;
                }
            if (e.iEv.Type() == TRawEvent::EKeyDown || e.iEv.Type() == TRawEvent::EKeyUp)
                KPrintf("KBINJ: %s std=0x%02x add=%d consumed=%d evtThread=%x head=%x tail=%x\r\n",
                        nm, (unsigned)(e.iAux & 0xFFu), add, (TInt)consumed,
                        (TUint)evtThread, (TUint)K::EventHeadPtr, (TUint)K::EventTailPtr);
            else
                KPrintf("KBINJ: %s aux=%d add=%d consumed=%d evtThread=%x head=%x tail=%x\r\n",
                        nm, e.iAux, add, (TInt)consumed,
                        (TUint)evtThread, (TUint)K::EventHeadPtr, (TUint)K::EventTailPtr);
            }
        }

    // PS/2 set-1 scancode -> genuine TStdScanCode (<e32keys.h>).  Letters and
    // digits map to their ASCII codes exactly like the genuine bsptemplate
    // convertCode tables do; everything else uses the EStdKey* enum, so there
    // are no magic numbers.  aSc is the full byte (break bit included); the
    // returned value is the same for make and break — the caller picks
    // EKeyDown/EKeyUp from the break bit, as a genuine driver does.
    TInt KbdStdScancode(TBool aExt, TUint aSc)
        {
        TUint mk = aSc & 0x7Fu;
        if (aExt)
            {
            switch (mk)
                {
                case 0x1C: return EStdKeyNkpEnter;
                case 0x1D: return EStdKeyRightCtrl;
                case 0x35: return EStdKeyNkpForwardSlash;
                case 0x37: return EStdKeyPrintScreen;
                case 0x38: return EStdKeyRightAlt;
                case 0x47: return EStdKeyHome;
                case 0x48: return EStdKeyUpArrow;
                case 0x49: return EStdKeyPageUp;
                case 0x4B: return EStdKeyLeftArrow;
                case 0x4D: return EStdKeyRightArrow;
                case 0x4F: return EStdKeyEnd;
                case 0x50: return EStdKeyDownArrow;
                case 0x51: return EStdKeyPageDown;
                case 0x52: return EStdKeyInsert;
                case 0x53: return EStdKeyDelete;
                case 0x5B: return EStdKeyLeftFunc;
                case 0x5C: return EStdKeyRightFunc;
                case 0x5D: return EStdKeyMenu;
                default: return 0;
                }
            }
        switch (mk)
            {
            case 0x01: return EStdKeyEscape;
            case 0x02: return '1';  case 0x03: return '2';  case 0x04: return '3';
            case 0x05: return '4';  case 0x06: return '5';  case 0x07: return '6';
            case 0x08: return '7';  case 0x09: return '8';  case 0x0A: return '9';
            case 0x0B: return '0';
            case 0x0C: return EStdKeyMinus;
            case 0x0D: return EStdKeyEquals;
            case 0x0E: return EStdKeyBackspace;
            case 0x0F: return EStdKeyTab;
            case 0x10: return 'Q';  case 0x11: return 'W';  case 0x12: return 'E';
            case 0x13: return 'R';  case 0x14: return 'T';  case 0x15: return 'Y';
            case 0x16: return 'U';  case 0x17: return 'I';  case 0x18: return 'O';
            case 0x19: return 'P';
            case 0x1A: return EStdKeySquareBracketLeft;
            case 0x1B: return EStdKeySquareBracketRight;
            case 0x1C: return EStdKeyEnter;
            case 0x1D: return EStdKeyLeftCtrl;
            case 0x1E: return 'A';  case 0x1F: return 'S';  case 0x20: return 'D';
            case 0x21: return 'F';  case 0x22: return 'G';  case 0x23: return 'H';
            case 0x24: return 'J';  case 0x25: return 'K';  case 0x26: return 'L';
            case 0x27: return EStdKeySemiColon;
            case 0x28: return EStdKeySingleQuote;
            case 0x29: return EStdKeyXXX;
            case 0x2A: return EStdKeyLeftShift;
            case 0x2B: return EStdKeyBackSlash;
            case 0x2C: return 'Z';  case 0x2D: return 'X';  case 0x2E: return 'C';
            case 0x2F: return 'V';  case 0x30: return 'B';  case 0x31: return 'N';
            case 0x32: return 'M';
            case 0x33: return EStdKeyComma;
            case 0x34: return EStdKeyFullStop;
            case 0x35: return EStdKeyForwardSlash;
            case 0x36: return EStdKeyRightShift;
            case 0x38: return EStdKeyLeftAlt;
            case 0x39: return EStdKeySpace;
            case 0x3A: return EStdKeyCapsLock;
            case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44:
                return EStdKeyF1 + (TInt)(mk - 0x3B);   // F1..F10 are contiguous
            case 0x45: return EStdKeyNumLock;
            case 0x46: return EStdKeyScrollLock;
            case 0x47: return EStdKeyNkp7;  case 0x48: return EStdKeyNkp8;
            case 0x49: return EStdKeyNkp9;
            case 0x4A: return EStdKeyNkpMinus;
            case 0x4B: return EStdKeyNkp4;  case 0x4C: return EStdKeyNkp5;
            case 0x4D: return EStdKeyNkp6;
            case 0x4E: return EStdKeyNkpPlus;
            case 0x4F: return EStdKeyNkp1;  case 0x50: return EStdKeyNkp2;
            case 0x51: return EStdKeyNkp3;
            case 0x52: return EStdKeyNkp0;
            case 0x53: return EStdKeyNkpFullStop;
            case 0x57: return EStdKeyF11;
            case 0x58: return EStdKeyF12;
            default: return 0;
            }
        }
    }  // namespace

void QemuEwsrvPushKey(TBool aDown, TUint aStdScanCode)
    {
    TPortRawEvent ev;
    ev.SetKey(aDown ? TRawEvent::EKeyDown : TRawEvent::EKeyUp, (TInt)aStdScanCode);
    InjPush(ev, (TInt)aStdScanCode, 2u);
    }

void QemuEwsrvPushPointerMove(TInt aX, TInt aY)
    {
    TPortRawEvent ev;
    ev.SetPointer(TRawEvent::EPointerMove, aX, aY);
    InjPush(ev, (aX << 16) | (aY & 0xFFFF), 1u);
    }

void QemuEwsrvPushButton(TUint aPs2ButtonBit, TBool aDown, TInt aX, TInt aY)
    {
    // PS/2 flag bits: 1=left -> EButton1, 2=right -> EButton2, 4=middle -> EButton3
    TRawEvent::TType t;
    if (aPs2ButtonBit == 1u)      t = aDown ? TRawEvent::EButton1Down : TRawEvent::EButton1Up;
    else if (aPs2ButtonBit == 2u) t = aDown ? TRawEvent::EButton2Down : TRawEvent::EButton2Up;
    else                          t = aDown ? TRawEvent::EButton3Down : TRawEvent::EButton3Up;
    TPortRawEvent ev;
    ev.SetPointer(t, aX, aY);
    InjPush(ev, (TInt)((aPs2ButtonBit << 8) | (aDown ? 1u : 0u)), 1u);
    }

TBool QemuInjGateActive()
    {
    return (gInjGate & 3u) != 0;
    }

TUint QemuInjGateMask()
    {
    return gInjGate & 3u;
    }

TUint QemuInjStats(TUint& aQueued, TUint& aDelivered, TUint& aDropped)
    {
    aQueued = gInjQueued;
    aDelivered = gInjDelivered;
    aDropped = gInjDropped;
    return gInjQueued;
    }

// Resolve the \KBINJ.TXT gate + hook the DFC to Kern::DfcQue0().  Called from
// QemuKbdArm(): first at Asic::Init3 (before FatMount, so no volume yet — the
// gate stays unresolved and is resolved by the shell-time call), then from
// ShellMain / `kbd arm` once C: is mounted.  File content selects the source
// mask: "ptr" -> pointer only (1), "key" -> keyboard only (2), anything else
// -> both (3); absent file -> 0 (OFF).
void QemuKbdInjArm()
    {
    if (!gInjDfcQueued && Kern::DfcQue0())
        {
        gInjDfc.SetDfcQ(Kern::DfcQue0());   // exists only after K::Init3
        gInjDfcQueued = ETrue;
        }
    if ((gInjGate & 0x80000000u) && FatCurLetter())
        {
        TUint8 buf[32];
        TUint got = 0;
        for (TUint i = 0; i < sizeof(buf); ++i) buf[i] = 0;
        char prev = FatCurLetter();
        if (!prev) FatUse('C');
        TInt r = FatReadPath("\\KBINJ.TXT", buf, sizeof(buf), got);
        if (prev) FatUse(prev);
        TUint mask = 0;
        if (r == KErrNone && got > 0)
            {
            // lowercase copy for the content match
            TUint8 lb[32];
            for (TUint i = 0; i < got && i < sizeof(lb); ++i)
                {
                TUint8 c = buf[i];
                lb[i] = (c >= 'A' && c <= 'Z') ? (TUint8)(c + 32) : c;
                }
            TBool hasPtr = EFalse, hasKey = EFalse;
            for (TUint i = 0; i + 2 < got; ++i)
                {
                if (lb[i] == 'p' && lb[i+1] == 't' && lb[i+2] == 'r') hasPtr = ETrue;
                if (lb[i] == 'k' && lb[i+1] == 'e' && lb[i+2] == 'y') hasKey = ETrue;
                }
            if (hasPtr && !hasKey) mask = 1u;
            else if (hasKey && !hasPtr) mask = 2u;
            else mask = 3u;
            }
        gInjGate = mask;
        KPrintf("KBINJ: genuine EWSRV input bridge gate %s mask=%u "
                "(\\KBINJ.TXT r=%d got=%u; 1=ptr 2=key)\r\n",
                mask ? "ON" : "OFF", mask, r, got);
        }
    }

// --- PS/2 keyboard ISR: Set-1 decode into the shell ring (kern_shell.cpp) ---
extern "C" void QemuKbdPush(char aC);  // kern_shell.cpp (shared input ring)

// --- COM1 RDA ISR: drain RBR into the shell ring (IRQ4, 16550A) ---
extern "C" void QemuUartIsr(TAny* /*aPtr*/)
    {
    for (int i = 0; i < 16; ++i)
        {
        if (!(In8(0x3FD) & 0x01))  // LSR.DR
            break;
        char c = (char)In8(0x3F8);  // RBR read clears the interrupt
        QemuKbdPush(c);
        }
    }
extern "C" void QemuKbdIsr(TAny* /*aPtr*/)
    {
    ++gKbdIsrCount;
    static const char kMap[128] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
        'q','w','e','r','t','y','u','i','o','p','[',']', '\n', 0,
        'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
        'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };
    static unsigned char sShift = 0, sE0 = 0;
    sE0 = 0;   // 0xE0 is a prefix only within ONE ISR batch.  A trailing E0
               // left set at batch end would swallow the next batch's first
               // scancode -- possibly a genuine break -- so clear it here.
    for (int i = 0; i < 8; ++i)
        {
        if (!(In8(0x64) & 0x01))
            break;
        unsigned char sc = In8(0x60);
        ++gKbdScancodes;
        if (sc == 0xE0) { sE0 = 1; continue; }
        TBool ext = sE0; sE0 = 0;
        // Genuine EWSRV input bridge: every decoded scancode (including
        // E0-extended keys, which have no shell ASCII entry) also becomes a
        // TRawEvent(EKeyDown/EKeyUp, TStdScanCode) queued for the kernel event
        // queue.  No-op unless the \KBINJ.TXT gate is ON.
        TInt std = KbdStdScancode(ext, sc);
        if (std)
            QemuEwsrvPushKey((sc & 0x80u) == 0u, (TUint)std);
        if (ext)
            continue;   // extended keys were always skipped for the shell ring
        if (sc == 0x2A || sc == 0x36) { sShift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { sShift = 0; continue; }
        // Set-1 break scancodes have bit 7 set; the matching make scancode is
        // sc & 0x7F.  Decode it with the same table + shift rules as a make so
        // the low 7 bits of the key-up byte equal the make byte, then tag bit 7
        // to form a distinct, shell-inert key-release byte (0x80 | c).  Both
        // line editors ignore bytes outside 32..126 (kern_shell.cpp:2775,
        // runtime.c:259); COM1, mouse and USB input are untouched.
        char c = kMap[sc & 0x7F];
        if (!c) continue;
        if (sShift)
            {
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            else switch (c)
                {
                case '1': c='!'; break; case '2': c='@'; break;
                case '3': c='#'; break; case '4': c='$'; break;
                case '5': c='%'; break; case '6': c='^'; break;
                case '7': c='&'; break; case '8': c='*'; break;
                case '9': c='('; break; case '0': c=')'; break;
                case '-': c='_'; break; case '=': c='+'; break;
                case '[': c='{'; break; case ']': c='}'; break;
                case ';': c=':'; break; case '\'': c='\"'; break;
                case '`': c='~'; break; case '\\': c='|'; break;
                case ',': c='<'; break; case '.': c='>'; break;
                case '/': c='?'; break;
                }
            }
        if (sc & 0x80)
            {
            // A physical release ends the hold: clear the per-key DOWN bit so
            // the next genuine make of this scancode is accepted again, and
            // arm the post-break guard for any queued stray make.  Forwarding
            // of the break byte itself is unchanged.
            unsigned char mk = (unsigned char)(sc & 0x7F);
            if (mk < 128)
                {
                sKeyDown[mk] = 0;
                sLastBreakAt[mk] = NTickCount();
                }
            QemuKbdPush((char)(0x80u | (unsigned char)c));  // key-up: 0x80 | ascii
            continue;
            }
        const int suppress = QemuKbdTypematicSuppress();
        // Post-break guard: swallow any make of this key that arrives inside
        // the window opened by its last break (typically a typematic make
        // queued ahead of the break in the 8042 buffer).  This runs BEFORE the
        // DOWN bit is set so a stray repeat cannot latch it and re-arm DOOM.
        if (suppress && sc < 128 && sLastBreakAt[sc] &&
            (unsigned int)(NTickCount() - sLastBreakAt[sc]) < kPostBreakGapMs)
            { KbdRepeatDropMark((unsigned char)sc); continue; }
        // Per-key DOWN bitmap: a second make of a scancode that is already
        // down is a hardware typematic repeat -> DROP it (no push, no counter).
        // The break clears the bit, so one physical press == one make byte.
        // Text mode (suppress==0) deliberately keeps hardware typematic.
        if (suppress && sc < 128 && sKeyDown[sc])
            { KbdRepeatDropMark((unsigned char)sc); continue; }
        if (sc < 128) sKeyDown[sc] = 1;
        ++gKbdKeys;
        QemuKbdPush(c);
        }
    }

// Re-arm PS/2 IRQ1 from a shell-safe context.  Asic::Init1 re-masks the whole
// 8259 after boot constructors run (the mouse driver documents the same trap
// for IRQ12), so a boot-time unmask alone is not durable.  More importantly,
// a key pressed before IRQ1 was unmasked can leave a scancode in the 8042
// output buffer: that holds the edge-triggered IRQ1 line high and the 8259
// never sees the low->high edge of the next keypress, i.e. the QEMU window
// looks dead while COM1 still works.  Drain the buffer first, then unmask.
extern "C" void QemuKbdArm()
    {
    QemuIrq::Disable(1);                    // silence IRQ1 while we poke 8042
    for (int i = 0; i < 32; ++i)            // drop stale scancodes -> line low
        {
        if (!(In8(0x64) & 0x01))
            break;
        (void)In8(0x60);
        }
    Out8(0x64, 0x20);                       // read 8042 config byte
    for (int i = 0; i < 100000 && !(In8(0x64) & 0x02); ++i) { }
    unsigned char cfg = In8(0x60);
    if (!(cfg & 0x01))                      // bit0 = keyboard IRQ enable
        {
        Out8(0x64, 0x60);                   // write config
        for (int i = 0; i < 100000 && (In8(0x64) & 0x02); ++i) { }
        Out8(0x60, (unsigned char)(cfg | 0x01u));
        }
    Out8(0x64, 0xAE);                       // enable keyboard (mouse init
    for (int i = 0; i < 100000 && (In8(0x64) & 0x02); ++i) { }  // toggles AD/AE)
    Interrupt::Bind(1, QemuKbdIsr, 0);
    Interrupt::Clear(1);                    // drop stale request/in-service
    QemuIrq::Enable(1);                     // read-modify-write: spares IRQs 0/4
    // Genuine EWSRV input bridge: hook the drain DFC to Kern::DfcQue0() (exists
    // after K::Init3) and resolve the \KBINJ.TXT gate once a volume is mounted
    // (at Init3 FAT is not mounted yet; the shell-time call resolves it).
    QemuKbdInjArm();
    }

extern "C" void QemuKbdStats(TUint& aIsrCount, TUint& aScancodes, TUint& aKeys,
                             TUint& aPicMask, TUint& aKbdStatus)
    {
    aIsrCount = gKbdIsrCount;
    aScancodes = gKbdScancodes;
    aKeys = gKbdKeys;
    aPicMask = In8(0x21);                   // master 8259 IMR
    aKbdStatus = In8(0x64);                 // 8042 status register
    }

// Binds the three QEMU IRQs + installs user exec gates (0x20/0x21, DPL3).
// Called from Asic::Init1 (variant/qemu_variant_asic.cpp).
extern "C" void SetupExecGates();
extern "C" void QemuIrqBindTickKbd()
    {
    SetupExecGates();  // genuine __X86Vector20/21 handle user int 0x20/0x21
    Interrupt::Bind(0, QemuTickIsr, 0);
    Interrupt::Bind(1, QemuKbdIsr, 0);
    Interrupt::Bind(4, QemuUartIsr, 0);
    }

// (Naked dispatcher deleted: plain-C X86IrqDispatcher above is immune to the
// genuine entry frame layout. Kept: QemuIrqBindTickKbd below.)

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
#include "qemu_variant.h"

static inline void Out8(unsigned short aPort, unsigned char aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
static inline unsigned char In8(unsigned short aPort)
    { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

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
    static const char kMap[128] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
        'q','w','e','r','t','y','u','i','o','p','[',']', '\n', 0,
        'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
        'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };
    static unsigned char sShift = 0, sE0 = 0;
    for (int i = 0; i < 8; ++i)
        {
        if (!(In8(0x64) & 0x01))
            break;
        unsigned char sc = In8(0x60);
        if (sc == 0xE0) { sE0 = 1; continue; }
        if (sE0) { sE0 = 0; continue; }
        if (sc == 0x2A || sc == 0x36) { sShift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { sShift = 0; continue; }
        if (sc & 0x80) continue;
        if (sc >= 128) continue;
        char c = kMap[sc];
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
        QemuKbdPush(c);
        }
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

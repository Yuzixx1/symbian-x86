// qemu_assp.cpp — minimal QEMU ASSP (Phase 3).
// Freestanding: no libc, no EKA headers yet. Port I/O via GCC inline asm.
// PIC/PIT/COM1 programming follows the PC spec that the missing legacy
// x86pc variant (os/boardsupport_internal/x86pc, not in this repo) used to own.
#include "qemu_assp.h"

typedef unsigned short TUint16;

static inline void Out8(TUint16 aPort, TUint8 aVal)
    { __asm__ __volatile__("outb %0,%1" : : "a"(aVal), "Nd"(aPort)); }
static inline TUint8 In8(TUint16 aPort)
    { TUint8 v; __asm__ __volatile__("inb %1,%0" : "=a"(v) : "Nd"(aPort)); return v; }

// --- COM1 0x3F8: 115200 8N1, FIFO on (same sequence as multiboot.S) ---
void QemuUart::Init()
    {
    Out8(0x3FB, 0x83);
    Out8(0x3F8, 0x01);
    Out8(0x3F9, 0x00);
    Out8(0x3FB, 0x03);
    Out8(0x3FA, 0xC7);
    }
void QemuUart::PutC(char aC)
    {
    while (!(In8(0x3FD) & 0x20)) { }
    Out8(0x3F8, (TUint8)aC);
    }
void QemuUart::Puts(const char* aS)
    { while (*aS) PutC(*aS++); }
void QemuUart::PutHex(TUint aV)
    {
    for (int i = 28; i >= 0; i -= 4)
        {
        TUint n = (aV >> i) & 0xF;
        PutC((char)(n < 10 ? '0' + n : 'A' + n - 10));
        }
    }

// --- COM1 receive interrupt: RDA (IER bit 0) + OUT2 (MCR bit 3, PC ISA gate).
// The variant ISR (qemu_variant_int.cpp) drains RBR while LSR.DR is set.
void QemuUart::EnableRxIrq()
    {
    Out8(0x3F9, 0x01);   // IER: received-data-available only (no TX-empty storm)
    Out8(0x3FC, 0x08);   // MCR: OUT2 gates the UART onto ISA IRQ4
    }

// --- i8259 PIC remap: master -> aMasterBase, slave -> aSlaveBase ---
// Genuine EKA2 vectors.cpp maps CPU traps 0x00-0x1F and HW IRQs >=0x30;
// the classic PC default (IRQ0 -> INT 8) collides with CPU faults, so the
// variant MUST remap: master 0x20-0x27, slave 0x28-0x2F (ICW1=0x11 ICW4=0x01).
void QemuIrq::RemapPic(TUint8 aMasterBase, TUint8 aSlaveBase)
    {
    Out8(0x20, 0x11); Out8(0xA0, 0x11);       // ICW1: init + ICW4 needed
    Out8(0x21, aMasterBase); Out8(0xA1, aSlaveBase); // ICW2: vector offsets
    Out8(0x21, 0x04); Out8(0xA1, 0x02);        // ICW3: cascade identity
    Out8(0x21, 0x01); Out8(0xA1, 0x01);        // ICW4: 8086 mode
    Out8(0x21, 0xFB); Out8(0xA1, 0xFF);        // OCW1: mask all but slave cascade
    }
void QemuIrq::Enable(TInt aIrq)
    {
    TUint16 port = aIrq < 8 ? 0x21 : 0xA1;
    TUint8 m = In8(port) & ~(TUint8)(1 << (aIrq & 7));
    Out8(port, m);
    }
void QemuIrq::Disable(TInt aIrq)
    {
    TUint16 port = aIrq < 8 ? 0x21 : 0xA1;
    TUint8 m = In8(port) | (TUint8)(1 << (aIrq & 7));
    Out8(port, m);
    }

// --- i8254 PIT channel 0: mode 3 square wave, divisor = 1193182/Hz ---
void QemuPit::Setup(TUint aHz)
    {
    TUint div = 1193182u / aHz;
    Out8(0x43, 0x36);
    Out8(0x40, (TUint8)(div & 0xFF));
    Out8(0x40, (TUint8)((div >> 8) & 0xFF));
    }
TUint QemuPit::LatchCount()
    {
    Out8(0x43, 0x00);                 // latch channel 0
    TUint lo = In8(0x40), hi = In8(0x40);
    return (hi << 8) | lo;
    }
// Poll the down-counter until aN milliseconds elapse; returns loops as proof.
TUint QemuPit::WaitTicks(TUint aN)
    {
    TUint spins = 0;
    TUint start = LatchCount();
    TUint want = aN * (1193182u / 1000u);
    TUint elapsed = 0;
    while (elapsed < want)
        {
        TUint now = LatchCount();
        // down-counter wraps at 0: delta = (start - now) mod 2^16
        elapsed = (start >= now) ? (start - now) : (0x10000u + start - now);
        // counter reloads each ms; accumulate across reloads
        if (elapsed >= want) break;
        if (now > start - 100 && now <= start) { /* warming */ }
        ++spins;
        if (spins > 20000000u) break;  // fail-safe: PIT absent
        // rebase every wrap so long waits accumulate
        if (now > start)
            { start = now; want -= elapsed; elapsed = 0; }
        }
    return spins;
    }

// --- QemuAsic: same method names as genuine class Asic (assp.h:174) ---
TInt QemuAsic::StartupReason() { return 0; /* EStartupCold */ }
TInt QemuAsic::MsTickPeriod() { return 1; /* 1ms, cf. NTimerQ::Init1 */ }
TUint QemuAsic::NanoWaitCalibration() { return 1000u; /* stub; genuine measures TSC */ }
void QemuAsic::DebugOutput(TUint aChar) { QemuUart::PutC((char)aChar); }
void QemuAsic::Idle() { __asm__ __volatile__("sti; hlt"); }

void QemuAsic::Init1()
    {
    // Genuine A::Init1 order (cinit.cpp:48): Asic::Init1 -> NanoWaitCal ->
    // Init1Interrupts -> NTimerQ::Init1(MsTickPeriod). Here: PIC+PIT+report.
    QemuUart::Puts("EKA2-QEMU Asic::Init1: remapping i8259 PIC master=0x20 slave=0x28...\r\n");
    QemuIrq::RemapPic(0x20, 0x28);
    QemuUart::Puts("EKA2-QEMU Asic::Init1: PIC remapped (IRQ0->0x20, no CPU-trap clash)\r\n");
    QemuUart::Puts("EKA2-QEMU Asic::Init1: programming i8254 PIT 1000Hz...\r\n");
    QemuPit::Setup(1000u);
    TUint c0 = QemuPit::LatchCount();
    QemuUart::Puts("EKA2-QEMU Asic::Init1: PIT ch0 latched count=0x");
    QemuUart::PutHex(c0);
    QemuUart::Puts(" (ticking)\r\n");
    QemuUart::Puts("EKA2-QEMU Asic::Init1: MsTickPeriod=1ms NanoWaitCal=1000\r\n");
    }

void QemuAsic::Init3()
    {
    QemuUart::Puts("EKA2-QEMU Asic::Init3: late init (nothing QEMU-specific)\r\n");
    }

// irq.cpp — PIT IRQ0 takeover (Phase 4 C side).
#include "qemu_assp.h"

extern "C" void InstallIDTEntry(void);  // asm: EAX=vector ECX=handler
extern "C" void Irq0Stub(void);
extern "C" void Irq0Handler_C(void);

volatile TUint gTickCount = 0;

extern "C" void NTimerQ_TickIrq(void);

extern "C" void Irq0Handler_C(void)
    {
    gTickCount += 1;
    NTimerQ_TickIrq();   // nanokernel tick: NTimerQ::Tick + TimesliceTick
    }

namespace QemuTimerIrq
    {
    void Takeover()
        {
        // Overwrite IDT[0x20] (genuine-empty slot) with IRQ0 gate.
        __asm__ __volatile__(
            "movl $0x20, %%eax\n\t"
            "movl %0, %%ecx\n\t"
            "call InstallIDTEntry"
            :: "r"(&Irq0Stub) : "eax", "ecx", "edx", "memory");
        QemuIrq::Enable(0);  // unmask PIT channel 0
        __asm__ __volatile__("sti");
        }
    // Wait ~aTicks PIT ticks with IRQs unmasked; returns ticks seen.
    TUint WaitTicks(TUint aTicks)
        {
        TUint start = gTickCount;
        for (TUint spin = 0; spin < 200000000u; ++spin)
            {
            if (gTickCount - start >= aTicks) break;
            __asm__ __volatile__("hlt");
            }
        return gTickCount - start;
        }
    }

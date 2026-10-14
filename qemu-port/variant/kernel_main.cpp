// kernel_main.cpp — genuine-named kernel entry (Phase-4 milestone).
// Genuine EKA2 calls KernelMain() from _E32Startup_Body after the bootstrap
// hands over ROM header + superpage (k_entry.cpp:38, nk_entry.cpp:47).
// This milestone implementation performs the genuine validation duties
// (ROM checksum, superpage signature/sanity) then proves the nanokernel
// timer path: unmasked IRQ0 PIT ticks driving a tick counter, i.e. the
// interrupt source NTimerQ/the scheduler will consume in full bring-up.
#include "qemu_assp.h"
#include "../rom/rom_image.h"
#include "../nk/nk_sched.h"

namespace QemuTimerIrq { void Takeover(); }
namespace QemuMmu { void Status(TUint&, TUint&, TUint&); }
namespace NKernelPhase5 { void Start(); }

extern "C" void KernelMain(void)
    {
    QemuUart::Puts("EKA2-QEMU KernelMain: entered (genuine entry point name)\r\n");

    TInt rRom = QemuRom::ValidateRom();
    QemuUart::Puts("EKA2-QEMU KernelMain: ROM validation ");
    if (rRom == 0) QemuUart::Puts("OK (magic+checksum+layout)\r\n");
    else { QemuUart::Puts("FAILED err=0x"); QemuUart::PutHex((TUint)rRom); QemuUart::Puts("\r\n"); for(;;) __asm__ __volatile__("hlt"); }

    TInt rSp = QemuRom::ValidateSuperPage();
    QemuUart::Puts("EKA2-QEMU KernelMain: SuperPage validation ");
    if (rSp == 0) QemuUart::Puts("OK (signature+RAM+pageDir+cpuId)\r\n");
    else { QemuUart::Puts("FAILED err=0x"); QemuUart::PutHex((TUint)rSp); QemuUart::Puts("\r\n"); for(;;) __asm__ __volatile__("hlt"); }

    TUint cr0, cr3, cr4;
    QemuMmu::Status(cr0, cr3, cr4);
    QemuUart::Puts("EKA2-QEMU KernelMain: paging live CR0=0x");
    QemuUart::PutHex(cr0);
    QemuUart::Puts(cr0 & 0x80000000u ? " (PG=1) " : " (PG=0!) ");
    QemuUart::Puts("CR3=0x");
    QemuUart::PutHex(cr3);
    QemuUart::Puts("\r\n");

    // Nanokernel timer + scheduler prove-out (Phase 5).
    QemuUart::Puts("EKA2-QEMU KernelMain: unmasking IRQ0, enabling interrupts...\r\n");
    QemuTimerIrq::Takeover();
    QemuUart::Puts("EKA2-QEMU KernelMain: IRQs unmasked, starting nanokernel scheduler...\r\n");
    NKernelPhase5::Start();
    QemuUart::Puts("EKA2-QEMU KernelMain: scheduler returned\r\n");
    for (;;) __asm__ __volatile__("hlt");
    }

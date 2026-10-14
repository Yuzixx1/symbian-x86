// qemu_kernel.cpp — Phase-4 boot flow (ROM -> MMU -> KernelMain).
// QemuKernelMain is entered from ported ResetEntry (post-InitCpu, paging off).
// It synthesizes the rombuild structures, enables paging via the ported
// Direct-model MMU, then transitions to the genuine-named KernelMain().
#include "qemu_assp.h"
#include "bootinfo.h"
#include "../rom/rom_image.h"

extern "C" TUint qemu_cpu_features;
namespace QemuMmu { void BuildIdentityMap(); TUint Enable(); TInt TouchTest(); }
extern "C" void KernelMain(void);

extern "C" void QemuKernelMain(TUint aMagic, TUint aInfo)
    {
    QemuUart::Init();
    QemuUart::Puts("\r\nEKA2-QEMU kernel side: entered QemuKernelMain (bootstrap handoff)\r\n");
    BootInfo::Capture(aInfo);
    QemuUart::Puts("EKA2-QEMU kernel side: multiboot magic=0x");
    QemuUart::PutHex(aMagic);
    QemuUart::Puts(" info=0x");
    QemuUart::PutHex(aInfo);
    QemuUart::Puts("\r\n");

    // Genuine A::Init1 duties first (PIC/PIT/COM1), as in Phase 3:
    QemuAsic::Init1();

    // Phase 4: synthesize rombuild output in memory.
    QemuRom::Synthesize(qemu_cpu_features);
    QemuUart::Puts("EKA2-QEMU ROM synth: TRomHeader+SuperPage built (rombuild stand-in)\r\n");
    TInt rR = QemuRom::ValidateRom();
    TInt rS = QemuRom::ValidateSuperPage();
    QemuUart::Puts("EKA2-QEMU ROM synth: ROM check ");
    QemuUart::Puts(rR == 0 ? "OK" : "FAIL");
    QemuUart::Puts(" SuperPage check ");
    QemuUart::Puts(rS == 0 ? "OK" : "FAIL");
    QemuUart::Puts("\r\n");
    if (rR != 0 || rS != 0)
        { QemuUart::Puts("EKA2-QEMU FATAL: synth validation failed\r\n"); for(;;) __asm__ __volatile__("hlt"); }
    QemuRom::Report();

    // Phase 4: ported Direct-model MMU — identity map + CR0.PG/CR4.PGE.
    QemuUart::Puts("EKA2-QEMU MMU: building 32x4M identity map (0-128M)...\r\n");
    QemuMmu::BuildIdentityMap();
    QemuUart::Puts("EKA2-QEMU MMU: enabling paging (CR3 load, CR0.PG|WP, CR4.PGE)...\r\n");
    TUint cr0 = QemuMmu::Enable();
    QemuUart::Puts("EKA2-QEMU MMU: paging on, CR0=0x");
    QemuUart::PutHex(cr0);
    QemuUart::Puts(cr0 & 0x80000000u ? " (PG=1)\r\n" : " (PG=0 FAILED)\r\n");
    if (!(cr0 & 0x80000000u))
        { QemuUart::Puts("EKA2-QEMU FATAL: paging not enabled\r\n"); for(;;) __asm__ __volatile__("hlt"); }
    TInt rT = QemuMmu::TouchTest();
    QemuUart::Puts("EKA2-QEMU MMU: touch test through paging ");
    QemuUart::Puts(rT == 0 ? "OK\r\n" : "FAILED\r\n");
    if (rT != 0) { for(;;) __asm__ __volatile__("hlt"); }

    // Transition to the genuine entry point (cf. _E32Startup_Body->KernelMain).
    QemuUart::Puts("EKA2-QEMU: transitioning QemuKernelMain -> KernelMain...\r\n");
    KernelMain();
    QemuUart::Puts("EKA2-QEMU FATAL: KernelMain returned\r\n");
    for (;;) __asm__ __volatile__("hlt");
    }

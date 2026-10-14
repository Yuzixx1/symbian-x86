/* bootmain_gas.S — GAS port of the ResetEntry skeleton from genuine
 * kernel/eka/kernel/x86/bootmain.asm:60.
 *
 * Genuine ResetEntry sequence (preserved in order, trimmed for QEMU):
 *   cli, cld -> JMP InitialiseHardware -> ESI=ROM header, ESP=boot stack
 *   -> RAM/ROM bank setup -> BMA alloc -> InitMemMapSystem -> MapContiguous
 *   -> SwitchToVirtual/EnableMmu -> _E32Startup_Body(KernelMain)
 *
 * Phase-3 scope: protected-mode entry already guaranteed by multiboot.S,
 * ROM image/page tables don't exist yet (Phase 4), so this port executes
 * the REAL first three steps (entry register contract, QEMU hardware probe,
 * InitCpu GDT/IDT) and then calls the kernel-side milestone entry
 * QemuKernelMain (which stands in for _E32Startup_Body/KernelMain until the
 * ROM image exists). Anything it prints comes from ported EKA2 init code.
 */
.code32

.set COM1_BASE, 0x03F8
.set MB_BOOT_MAGIC, 0x2BADB002
.set HANDOFF_ADDR, 0x9000
.set HANDOFF_MAGIC, 0x51454B32

.section .bss.reset, "aw", @nobits
.align 4
.globl reset_saved_magic
reset_saved_magic:
    .space 4
.globl reset_saved_info
reset_saved_info:
    .space 4

.section .text.reset, "ax"
.code32
.globl ResetEntry
.type ResetEntry, @function
ResetEntry:
    cli
    cld

    /* Genuine contract: entered with EAX=magic(from multiboot), EBX=info*.
     * Save to .bss NOW: the stack switch below would orphan stack-saved regs
     * (InitCpu also clobbers EAX/EBX internally). */
    movl %eax, reset_saved_magic
    movl %ebx, reset_saved_info

    /* QEMU InitialiseHardware: replaces variant-specific MASM routine.
     * Verifies multiboot magic + publishes RAM summary on COM1. */
    call QemuInitialiseHardware
    /* EAX = 0 on success; halt with report otherwise */
    testl %eax, %eax
    jz reset_hw_ok
    leal reset_badmagic_msg, %esi
    call bootcpu_com1_puts
1:  hlt
    jmp 1b

reset_hw_ok:
    /* Genuine: ESP = boot stack top (cf. bootmain.asm:88, CpuBootStackTop) */
    movl $qemu_boot_stack_top, %esp

    /* Genuine: InitCpu before MMU (cf. bootmain flow via BTF_EnableMMU) */
    leal reset_initcpu_msg, %esi
    call bootcpu_com1_puts
    call InitCpu
    leal reset_cpu_ok_msg, %esi
    call bootcpu_com1_puts
    movl qemu_cpu_features, %eax
    call bootcpu_com1_puthex
    movb $0x0D, %al
    call bootcpu_com1_putc
    movb $0x0A, %al
    call bootcpu_com1_putc

    /* Genuine nanokernel entry: _E32Startup_Body(romHeader, superPage).
     * Reserves live in variant/genuine_glue.cpp until ROM synthesis moves over. */
    pushl $qemu_superpage_reserve
    pushl $qemu_rom_header_reserve
    call _E32Startup_Body

    /* must not return */
    leal reset_returned_msg, %esi
    call bootcpu_com1_puts
2:  hlt
    jmp 2b

/* ----------------------------------------------------------------
 * QemuInitialiseHardware — minimal variant of MASM InitialiseHardware.
 * Reads entry regs from .bss (saved before the stack switch).
 * Output: EAX=0 ok / nonzero fail. Prints RAM summary.
 */
.globl QemuInitialiseHardware
.type QemuInitialiseHardware, @function
QemuInitialiseHardware:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edx

    movl reset_saved_magic, %eax
    cmpl $MB_BOOT_MAGIC, %eax
    jne hw_fail
    /* handoff page must carry our magic (written by multiboot.S) */
    movl $HANDOFF_ADDR, %edx
    movl (%edx), %ebx
    cmpl $HANDOFF_MAGIC, %ebx
    jne hw_fail

    leal hw_ok_msg, %esi
    call bootcpu_com1_puts
    xorl %eax, %eax
    jmp hw_done
hw_fail:
    movl $1, %eax
hw_done:
    popl %edx
    popl %esi
    popl %ebx
    popl %ebp
    ret

.section .rodata.reset, "a"
reset_initcpu_msg:
    .asciz "EKA2-QEMU ResetEntry: calling ported InitCpu (GDT/IDT)...\r\n"
reset_cpu_ok_msg:
    .asciz "EKA2-QEMU ResetEntry: InitCpu done, cpu_features=0x"
reset_badmagic_msg:
    .asciz "EKA2-QEMU ResetEntry: FATAL bad multiboot magic\r\n"
reset_returned_msg:
    .asciz "EKA2-QEMU ResetEntry: FATAL kernel returned\r\n"
hw_ok_msg:
    .asciz "EKA2-QEMU ResetEntry: multiboot handoff OK (128M RAM, ROM-in-RAM deferred)\r\n"

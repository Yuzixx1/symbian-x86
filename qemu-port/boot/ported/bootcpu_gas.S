/* bootcpu_gas.S — GAS port of the essential subset of genuine
 * kernel/eka/kernel/x86/bootcpu.asm (MASM32).
 *
 * Ported (faithful logic, GAS Intel-syntax):
 *   InitCpu    <- bootcpu.asm:44   (CPUID detect, CR4.PSE enable, SetupGDT+SetupIDT)
 *   SetupGDT   <- bootcpu.asm:638  (flat 0-4G: R0CS/R0DS/R3CS/R3DS + TSS slot)
 *   SetupIDT   <- bootcpu.asm:577  (256-entry IDT, 32 CPU stubs -> VectorCommon)
 *   InstallIDTEntry <- bootcpu.asm:537
 *   Vec00-Vec1F + VectorCommon <- bootcpu.asm:378 (debug stubs; COM1 fault report)
 *
 * Deliberate QEMU simplifications (full MMU/ROM paging is Phase 4):
 *   - GDT/IDT live in .bss (qemu_gdt/qemu_idt), not superpage CPU page.
 *   - No SSuperPage writes; CPU features go to qemu_cpu_features (cf. iCpuId).
 *   - EnableMmu/PageTableUpdate/GetPdeValue NOT ported yet (need ROM page tables).
 *
 * Conventions: .code32, AT&T syntax, i486 baseline, no libc.
 */
.code32

.set COM1_BASE, 0x03F8
/* CR4 / CPUID feature bits (cf. include/kernel/x86/bootcpu.inc:74) */
.set CR4_PSE, 0x10
.set EX86Feat_PSE, 0x08
.set EFLAGS_ID, 0x200000

/* Genuine selectors (x86boot.h:219): R0CS 0x08 R0DS 0x10 R3CS 0x1B R3DS 0x23 TSS 0x28 */
.set KRing0CS, 0x08
.set KRing0DS, 0x10
.set KGdtSize, 6
.set KIdtSize, 256

.section .bss.bootcpu, "aw", @nobits
.align 16
.globl qemu_gdt
qemu_gdt:
    .space KGdtSize*8
.globl qemu_gdtr
qemu_gdtr:
    .space 6
.globl qemu_idt
qemu_idt:
    .space KIdtSize*8
.globl qemu_idtr
qemu_idtr:
    .space 6
.globl qemu_cpu_features
qemu_cpu_features:
    .long 0
/* Bootstrap TSS for selector 0x28: genuine X86::Init1Interrupts LTRs the
 * CURRENT (bootstrap) GDT's TSS slot, so it must be valid (cf. real estrt).
 * Zeroed 104-byte 32-bit available-TSS; genuine later installs its own. */
.globl qemu_tss
qemu_tss:
    .space 104
.globl qemu_boot_stack
    .space 8192
.globl qemu_boot_stack_top
qemu_boot_stack_top:

.section .text.bootcpu, "ax"
.code32

/* -- tiny COM1 polled helpers for fault paths (no C runtime yet) -- */
.globl bootcpu_com1_init
.type bootcpu_com1_init, @function
bootcpu_com1_init:
    pushl %edx
    pushl %eax
    movw $COM1_BASE+3, %dx
    movb $0x83, %al
    outb %al, %dx
    movw $COM1_BASE, %dx
    movb $0x01, %al
    outb %al, %dx
    movw $COM1_BASE+1, %dx
    xorb %al, %al
    outb %al, %dx
    movw $COM1_BASE+3, %dx
    movb $0x03, %al
    outb %al, %dx
    movw $COM1_BASE+2, %dx
    movb $0xC7, %al
    outb %al, %dx
    popl %eax
    popl %edx
    ret

/* AL = char */
.globl bootcpu_com1_putc
.type bootcpu_com1_putc, @function
bootcpu_com1_putc:
    pushl %edx
    movb %al, %ah
    movw $COM1_BASE+5, %dx
1:  inb %dx, %al
    testb $0x20, %al
    jz 1b
    movb %ah, %al
    movw $COM1_BASE, %dx
    outb %al, %dx
    popl %edx
    ret

/* ESI = NUL string */
.globl bootcpu_com1_puts
.type bootcpu_com1_puts, @function
bootcpu_com1_puts:
    pushl %eax
    pushl %esi
1:  lodsb
    testb %al, %al
    jz 2f
    call bootcpu_com1_putc
    jmp 1b
2:  popl %esi
    popl %eax
    ret

/* EAX = 32-bit hex */
.globl bootcpu_com1_puthex
.type bootcpu_com1_puthex, @function
bootcpu_com1_puthex:
    pushl %eax
    pushl %ecx
    pushl %edx
    movl %eax, %edx
    movl $8, %ecx
1:  roll $4, %edx
    movl %edx, %eax
    andl $0xF, %eax
    cmpl $10, %eax
    jb 2f
    addl $7, %eax
2:  addl $'0', %eax
    call bootcpu_com1_putc
    decl %ecx
    jnz 1b
    popl %edx
    popl %ecx
    popl %eax
    ret

/* ---------------------------------------------------------------- InitCpu
 * Genuine logic: EFLAGS.ID test -> CPUID.01 -> EDX features -> CR4.PSE if
 * EX86Feat_PSE -> SetupGDT -> SetupIDT. Superpage writes replaced by
 * qemu_cpu_features store (Phase 4 restores SSuperPageBase::iCpuId).
 */
.globl InitCpu
.type InitCpu, @function
InitCpu:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %esi
    pushl %edi

    movl $0, %eax
    movl %eax, qemu_cpu_features

    /* EFLAGS.ID toggle test for CPUID support */
    pushfl
    movl (%esp), %edx
    orl $EFLAGS_ID, %edx
    pushl %edx
    popfl
    pushfl
    popl %ecx
    testl $EFLAGS_ID, %ecx
    jz no_cpuid
    xorl $EFLAGS_ID, %ecx
    pushl %ecx
    popfl
    pushfl
    popl %edx
    testl $EFLAGS_ID, %edx
    jnz no_cpuid

    /* CPUID valid */
    xorl %eax, %eax
    cpuid
    cmpl $0, %eax
    je only_0_supported
    movl $1, %eax
    cpuid
    /* EAX = signature (skip store), EDX = features (genuine stores both) */
    movl %edx, qemu_cpu_features
    /* Enable 4M pages if supported */
    testl $EX86Feat_PSE, %eax
    testl $EX86Feat_PSE, %edx
    jz 1f
    movl %cr4, %ecx
    orl $CR4_PSE, %ecx
    movl %ecx, %cr4
1:
only_0_supported:
no_cpuid:
    popfl
    movl qemu_cpu_features, %eax

    call SetupGDT
    call SetupIDT

    popl %edi
    popl %esi
    popl %edx
    popl %ecx
    popl %ebx
    popl %ebp
    ret

/* ---------------------------------------------------------------- SetupGDT
 * Genuine bytes preserved: R0CS 00cf9b00/R0DS 00cf9300/R3CS 00cbfb00/
 * R3DS 00cbf300 over limit 0000ffff (non-flexible direct model).
 */
.globl SetupGDT
.type SetupGDT, @function
SetupGDT:
    pushl %eax
    pushl %ebx
    pushl %edx

    /* zero 6 entries */
    movl $qemu_gdt, %ebx
    movl $0, %eax
    movl $KGdtSize*2, %ecx
1:  movl %eax, (%ebx)
    addl $4, %ebx
    decl %ecx
    jnz 1b

    movl $qemu_gdt, %ebx
    /* seg1 R0 code @0x08 */
    movl $0x0000ffff, 8(%ebx)
    movl $0x00cf9b00, 12(%ebx)
    /* seg2 R0 data @0x10 */
    movl $0x0000ffff, 16(%ebx)
    movl $0x00cf9300, 20(%ebx)
    /* seg3 R3 code @0x18 */
    movl $0x0000ffff, 24(%ebx)
    movl $0x00cbfb00, 28(%ebx)
    /* seg4 R3 data @0x20 */
    movl $0x0000ffff, 32(%ebx)
    movl $0x00cbf300, 36(%ebx)
    /* seg5 TSS @0x28: bootstrap 32-bit available-TSS over qemu_tss
     * (descriptor format mirrors genuine SetTssDescriptor: limit 103,
     * type 0x89 present). LTR in X86::Init1Interrupts needs this valid. */
    pushl %ecx
    movl $qemu_tss, %eax
    movl %eax, %edx
    shrl $16, %edx
    andl $0xff, %edx              /* edx = base[23:16] at bits 0-7 */
    movl %eax, %ecx
    shrl $24, %ecx               /* ecx = base[31:24] */
    shll $24, %ecx
    orl $0x00108900, %edx        /* access byte 0x89 at bits 8-15 */
    orl %ecx, %edx               /* base[31:24] at bits 24-31 */
    movl %edx, 44(%ebx)
    andl $0xffff, %eax
    shll $16, %eax
    orl $103, %eax
    movl %eax, 40(%ebx)
    popl %ecx

    movw $(KGdtSize*8-1), qemu_gdtr
    movl $qemu_gdt, %eax
    movl %eax, qemu_gdtr+2

    lgdt qemu_gdtr
    /* far jump to reload CS (genuine uses iretd trick; ljmp is equivalent) */
    ljmp $KRing0CS, $1f
1:
    movw $KRing0DS, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    xorl %eax, %eax
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ax
    lldt %ax

    popl %edx
    popl %ebx
    popl %eax
    ret

/* ---------------------------------------------------------------- IDT */
.macro VEC_STUB num
.globl Vec\()\num
Vec\()\num:
    pushl $0
    pushl $\num
    jmp VectorCommon
.endm

/* generate Vec00..Vec31 (enough for CPU traps; HW IRQs remapped in Phase-3 ASSP) */
VEC_STUB 0
VEC_STUB 1
VEC_STUB 2
VEC_STUB 3
VEC_STUB 4
VEC_STUB 5
VEC_STUB 6
VEC_STUB 7
VEC_STUB 8
VEC_STUB 9
VEC_STUB 10
VEC_STUB 11
VEC_STUB 12
VEC_STUB 13
VEC_STUB 14
VEC_STUB 15
VEC_STUB 16
VEC_STUB 17
VEC_STUB 18
VEC_STUB 19
VEC_STUB 20
VEC_STUB 21
VEC_STUB 22
VEC_STUB 23
VEC_STUB 24
VEC_STUB 25
VEC_STUB 26
VEC_STUB 27
VEC_STUB 28
VEC_STUB 29
VEC_STUB 30
VEC_STUB 31

.section .rodata.bootcpu, "a"
vec_table:
    .long Vec0, Vec1, Vec2, Vec3, Vec4, Vec5, Vec6, Vec7
    .long Vec8, Vec9, Vec10, Vec11, Vec12, Vec13, Vec14, Vec15
    .long Vec16, Vec17, Vec18, Vec19, Vec20, Vec21, Vec22, Vec23
    .long Vec24, Vec25, Vec26, Vec27, Vec28, Vec29, Vec30, Vec31

.section .text.bootcpu, "ax"
.code32

/* EAX=vector, ECX=handler -> install 32-bit interrupt gate DPL0 (cf. InstallIDTEntry) */
.globl InstallIDTEntry
.type InstallIDTEntry, @function
InstallIDTEntry:
    pushl %edx
    /* entry = qemu_idt + vector*8 */
    shll $3, %eax
    addl $qemu_idt, %eax
    movw %cx, (%eax)             /* off 0-15 */
    movw $KRing0CS, 2(%eax)      /* selector */
    movb $0, 4(%eax)
    movb $0x8E, 5(%eax)          /* P=1 DPL=0 32-bit int gate */
    shrl $16, %ecx
    movw %cx, 6(%eax)            /* off 16-31 */
    popl %edx
    ret

/* Install user exec gates 0x20/0x21 (DPL3) -> genuine __X86Vector20/21.
 * Called from variant init (our IDT is runtime-owned, unlike the GDT).
 * The handlers are C++-mangled; reference by explicit symbol names. */
.extern _Z13__X86Vector20v
.extern _Z13__X86Vector21v
.globl SetupExecGates
.type SetupExecGates, @function
SetupExecGates:
    pushl %eax
    pushl %ecx
    pushl %edx
    movl $0x20, %eax
    movl $_Z13__X86Vector20v, %ecx
    call InstallIDTUser
    movl $0x21, %eax
    movl $_Z13__X86Vector21v, %ecx
    call InstallIDTUser
    popl %edx
    popl %ecx
    popl %eax
    ret

/* EAX=vector, ECX=handler -> install 32-bit interrupt gate DPL3 */
.globl InstallIDTUser
.type InstallIDTUser, @function
InstallIDTUser:
    pushl %edx
    shll $3, %eax
    addl $qemu_idt, %eax
    movw %cx, (%eax)
    movw $KRing0CS, 2(%eax)
    movb $0, 4(%eax)
    movb $0xEE, 5(%eax)          /* P=1 DPL=3 32-bit int gate */
    shrl $16, %ecx
    movw %cx, 6(%eax)
    popl %edx
    ret

.globl SetupIDT
.type SetupIDT, @function
SetupIDT:
    pushl %eax
    pushl %ecx
    pushl %edx
    /* zero IDT */
    movl $qemu_idt, %edx
    xorl %eax, %eax
    movl $KIdtSize*2, %ecx
1:  movl %eax, (%edx)
    addl $4, %edx
    decl %ecx
    jnz 1b
    /* install 32 stubs */
    movl $31, %eax
2:  movl vec_table(,%eax,4), %ecx
    pushl %eax
    pushl %ecx
    movl %eax, %ebx
    movl %ecx, %ecx
    /* InstallIDTEntry expects EAX=vec ECX=handler */
    movl %ebx, %eax
    popl %ecx
    call InstallIDTEntry
    popl %eax
    decl %eax
    jns 2b
    /* lidt */
    movw $(KIdtSize*8-1), qemu_idtr
    movl $qemu_idt, %ecx
    movl %ecx, qemu_idtr+2
    lidt qemu_idtr
    popl %edx
    popl %ecx
    popl %eax
    ret

/* Bootstrap fault handler: COM1 report + halt (genuine: BasicFaultHandler/DoFault) */
.globl VectorCommon
.type VectorCommon, @function
VectorCommon:
    cli
    popl %eax   /* vector */
    popl %ecx   /* dummy error */
    pushl %eax
    leal vec_msg, %esi
    call bootcpu_com1_puts
    popl %eax
    call bootcpu_com1_puthex
    movl $0x0A, %eax
    movb $0x0D, %al
    call bootcpu_com1_putc
    movb $0x0A, %al
    call bootcpu_com1_putc
1:  hlt
    jmp 1b

.section .rodata.bootcpu, "a"
vec_msg:
    .asciz "EKA2-QEMU bootstrap FAULT vector=0x"

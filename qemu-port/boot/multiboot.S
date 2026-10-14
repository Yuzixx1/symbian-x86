/* multiboot.S — Multiboot1 bridge: QEMU -> (Phase 3) EKA2 ResetEntry.
 *
 * This is BOOT INFRASTRUCTURE, not a kernel. It proves the QEMU->protected-mode
 * handoff that the genuine bootstrap (kernel/x86/bootmain.asm:ResetEntry)
 * requires, and halts with a serial banner until Phase 3 wires in the port.
 *
 * Spec: Multiboot 0.6.96 §3.1.1. QEMU -kernel loads ELF, verifies the header
 * in the first 8KiB, then enters _mb_entry with:
 *   EAX = 0x2BADB002, EBX = multiboot_info*, CS/DS/ES/FS/GS = 32-bit segments,
 *   interrupts OFF, paging OFF, A20 enabled.
 */
.code32

/* --- Multiboot1 header constants --- */
.set MB_MAGIC,    0x1BADB002
.set MB_FLAG_ALIGN,  1 << 0   /* align modules to 4K */
.set MB_FLAG_MEMINFO, 1 << 1 /* request mem_lower/mem_upper */
.set MB_FLAGS,    MB_FLAG_ALIGN | MB_FLAG_MEMINFO
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)
.set MB_BOOT_MAGIC, 0x2BADB002

/* --- QEMU constants (mirror inc/qemu_config.h) --- */
.set COM1_BASE,   0x03F8
.set HANDOFF_ADDR, 0x9000
.set HANDOFF_MAGIC, 0x51454B32

.section .multiboot, "a"
.align 4
mb_header:
    .long MB_MAGIC
    .long MB_FLAGS
    .long MB_CHECKSUM
    /* a.out kludge fields zeroed: ELF kernel, QEMU derives load info from ELF */
    .long 0, 0, 0, 0, 0
    .long 0  /* mode: linear framebuffer off */
    .long 0, 0, 0

.section .bss.stack, "aw", @nobits
.align 16
    .space 16384
mb_stack_top:

.section .data.handoff, "aw"
.align 4
.globl qemu_handoff
qemu_handoff:          /* also mirrored at HANDOFF_ADDR for the ported ResetEntry */
    .long 0  /* +0: QEMU_HANDOFF_MAGIC once validated */
    .long 0  /* +4: saved EAX (must be MB_BOOT_MAGIC) */
    .long 0  /* +8: saved EBX (multiboot_info*) */

.section .text.entry, "ax"
.code32
.globl _mb_entry
.type _mb_entry, @function
_mb_entry:
    /* PROOF-OF-ENTRY marker first (preserve live regs: EAX=magic below). */
    pushl %eax
    pushl %edx
    movb $'K', %al
    movw $0x3F8, %dx
    outb %al, %dx
    popl %edx
    popl %eax
    cli
    cld
    movl $mb_stack_top, %esp
    xorl %ebp, %ebp

    /* stash boot registers before we clobber anything */
    pushl %ebx
    pushl %eax

    /* validate magic; EDI = 1 if ok, else 0 (no %dil: i486 has no DIL) */
    movl $0, %edi
    cmpl $MB_BOOT_MAGIC, %eax
    jne 0f
    movl $1, %edi
0:

    /* publish handoff block */
    movl %eax, qemu_handoff+4
    popl %eax               /* drop saved copy depth; EAX still holds magic */
    pushl %eax
    movl %eax, %ecx         /* ECX = magic */
    popl %eax
    popl %ebx               /* EBX = info ptr (restored) */
    pushl %ebx
    pushl %ecx
    movl %ebx, qemu_handoff+8
    cmpl $1, %edi
    jne mb_no_magic
    movl $HANDOFF_MAGIC, qemu_handoff
    jmp mb_handoff_low
mb_no_magic:
    movl $0, qemu_handoff
mb_handoff_low:
    /* mirror to fixed low-mem page 0x9000 for the Phase-3 ResetEntry port */
    movl $HANDOFF_ADDR, %edx
    movl qemu_handoff, %ecx
    movl %ecx, (%edx)
    movl qemu_handoff+4, %ecx
    movl %ecx, 4(%edx)
    movl qemu_handoff+8, %ecx
    movl %ecx, 8(%edx)
    popl %ecx
    popl %ebx

    /* early COM1 init: 115200 8N1, FIFOs on */
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

    /* banner on serial: proves protected-mode entry + COM1 before EKA2 exists */
    pushl %edi
    leal mb_banner, %esi
    call mb_puts
    cmpl $1, (%esp)
    je mb_ok_msg
    leal mb_badmagic, %esi
    call mb_puts
    jmp mb_halt
mb_ok_msg:
    leal mb_ok, %esi
    call mb_puts
mb_halt:
    addl $4, %esp
    /* Phase 3: enter the ported genuine bootstrap (never returns) */
    leal mb_jump, %esi
    call mb_puts
    movl qemu_handoff+4, %eax   /* multiboot magic -> ResetEntry EAX */
    movl qemu_handoff+8, %ebx   /* info ptr      -> ResetEntry EBX */
    call ResetEntry
    /* must not return */
1:  hlt
    jmp 1b

/* ESI = NUL-terminated string; clobbers EAX/EDX */
mb_puts:
    pushl %esi
.Lnext:
    lodsb
    testb %al, %al
    jz .Ldone
    movw $COM1_BASE+5, %dx
.Lwait:
    inb %dx, %al
    testb $0x20, %al
    jz .Lwait
    movb -1(%esi), %al
    movw $COM1_BASE, %dx
    outb %al, %dx
    jmp .Lnext
.Ldone:
    popl %esi
    ret

.section .rodata, "a"
mb_banner:
    .asciz "EKA2-QEMU bridge: 32-bit protected mode, paging off, COM1 ok\r\n"
mb_ok:
    .asciz "EKA2-QEMU bridge: multiboot magic 2BADB002 OK\r\n"
mb_badmagic:
    .asciz "EKA2-QEMU bridge: WARNING bad multiboot magic\r\n"
mb_phase3:
    .asciz "EKA2-QEMU bridge: halting (Phase 3: JMP ported ResetEntry)\r\n"
mb_jump:
    .asciz "EKA2-QEMU bridge: jumping to ported ResetEntry...\r\n"

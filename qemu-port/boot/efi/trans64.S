/* trans64.S — long-mode to 32-bit protected-mode drop (AT&T, GNU as).
 * MS ABI: RCX=entry (<4G), RDX=mbi, R8=&gdt_ptr (10-byte lgdt image).
 * Preserves entry/mbi in RDI/RSI across the mode switch (GPRs survive),
 * then jumps to the 32-bit kernel entry with EAX=2BADB002, EBX=mbi,
 * ESP=0x9000, paging off — the same contract as the BIOS path. */
.code64
.globl drop_to_pm
drop_to_pm:
    cli
    mov %rcx, %rdi
    mov %rdx, %rsi
    lgdt (%r8)
    /* far return into the 32-bit code segment (selector 0x08) */
    leaq compat32(%rip), %rax
    pushq $0x08
    pushq %rax
    retfq
.code32
compat32:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    /* leave long mode: PG off, then PAE off, then LME off */
    mov %cr0, %eax
    and $0x7FFFFFFF, %eax
    mov %eax, %cr0
    mov %cr4, %eax
    and $0xFFFFFFDF, %eax
    mov %eax, %cr4
    mov $0xC0000080, %ecx
    rdmsr
    and $0xFFFFFEFF, %eax
    wrmsr
    /* flat 32-bit world now: handoff registers + jump */
    mov $0x9000, %esp
    mov $0x2BADB002, %eax
    mov %esi, %ebx
    jmp *%edi

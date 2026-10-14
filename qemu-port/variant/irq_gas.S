/* irq_gas.S — PIT IRQ0 takeover (Phase 4).
 * Overwrites IDT entry 0x20 (genuine leaves 0x20-0x2F empty: vectors.cpp only
 * installs <0x20 traps and >=0x30 HW gates) with an interrupt gate to Irq0Stub.
 * Stub preserves all GPRs + segments, calls C handler, sends PIC EOI, iret.
 */
.code32

.section .text.irq, "ax"
.code32

.globl Irq0Stub
.type Irq0Stub, @function
Irq0Stub:
    pushal
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    call Irq0Handler_C
    /* EOI to master PIC */
    movb $0x20, %al
    outb %al, $0x20
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popal
    iret

/* IRQ1 (PS/2 keyboard, vector 0x21): same frame, EOI to master only
 * (IRQ1 < 8 lives on the master PIC). */
.globl Irq1Stub
.type Irq1Stub, @function
Irq1Stub:
    pushal
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    call Irq1Handler_C
    movb $0x20, %al
    outb %al, $0x20
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popal
    iret

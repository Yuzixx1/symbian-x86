/* mmu_gas.S — GAS port of the genuine Direct-model MMU enable path.
 * Genuine sources: kernel/x86/bootcpu.asm EnableMmu:277 (CR3 load, CR0.PG|WP,
 * CR4.PGE, TLB flush) + bootutil.asm MapSingleSection (4M PS pages when PSE).
 *
 * Phase-4 scope: identity-map low 128M RAM with 32x 4M PDEs (P|RW|PS),
 * load CR3, set CR0.PG|WP + CR4.PGE (if CPU supports PGE — QEMU does).
 * No temp-identity dance: load==link (@1M) so the running code is already
 * identity-mapped; genuine temp-PDE removal is a no-op here (documented).
 */
.code32

.set CR0_PG, 0x80000000
.set CR0_WP, 0x00010000
.set CR4_PSE, 0x10
.set CR4_PGE, 0x80
.set PDE_P,  0x01
.set PDE_W,  0x02
.set PDE_PS, 0x80
.set PDE_G,  0x100
.set EX86Feat_PGE, 0x2000

/* 4K-aligned page directory: 1024 x 4B = 4K, plus end marker */
.section .bss.mmu, "aw", @nobits
.align 4096
.globl qemu_page_dir
qemu_page_dir:
    .space 4096
.globl qemu_mmu_on
qemu_mmu_on:
    .long 0

.section .text.mmu, "ax"
.code32

/* Build 32x 4M identity PDEs covering 0x00000000-0x07FFFFFF (128M).
 * EAX untouched except flags. Mirrors genuine MapSingleSection PDE value:
 * PDE = phys | P|W | PS (| G if PGE). */
.globl Mmu_BuildIdentityMap
.type Mmu_BuildIdentityMap, @function
Mmu_BuildIdentityMap:
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %edi
    /* zero the directory first */
    movl $qemu_page_dir, %edi
    xorl %eax, %eax
    movl $1024, %ecx
1:  movl %eax, (%edi)
    addl $4, %edi
    decl %ecx
    jnz 1b
    /* global-bit choice matches CPU (genuine GetPdeValue checks PGE) */
    movl qemu_cpu_features, %edx
    movl $(PDE_P|PDE_W|PDE_PS), %eax
    testl $EX86Feat_PGE, %edx
    jz 2f
    orl $PDE_G, %eax
2:  movl %eax, %edx   /* template PDE flags */
    movl $qemu_page_dir, %edi
    xorl %ecx, %ecx   /* PDE index 0..31 */
    xorl %eax, %eax   /* phys base */
3:  movl %eax, %ebx
    orl %edx, %ebx
    movl %ebx, (%edi,%ecx,4)
    addl $0x400000, %eax
    incl %ecx
    cmpl $32, %ecx
    jb 3b
    popl %edi
    popl %edx
    popl %ecx
    popl %eax
    ret

/* Enable paging. Returns EAX=CR0 after enable. Genuine order: CR3 -> CR0.PG|WP
 * -> CR4.PGE + CR3 reload (flush non-global TLB). wbinvd omitted (QEMU). */
.globl Mmu_Enable
.type Mmu_Enable, @function
Mmu_Enable:
    movl $qemu_page_dir, %eax
    movl %eax, %cr3
    movl %cr0, %eax
    orl $(CR0_PG|CR0_WP), %eax
    movl %eax, %cr0
    /* far jump not needed (GAS ljmp unnecessary: EIP identity-mapped) */
    movl qemu_cpu_features, %eax
    testl $EX86Feat_PGE, %eax
    jz 1f
    movl %cr4, %eax
    orl $CR4_PGE, %eax
    movl %eax, %cr4
    movl %cr3, %eax
    movl %eax, %cr3
1:  movl $1, qemu_mmu_on
    movl %cr0, %eax
    ret

/* EAX=CR0, EDX=CR3, ECX=CR4 (status proof for serial log). */
.globl Mmu_Status
.type Mmu_Status, @function
Mmu_Status:
    movl %cr0, %eax
    movl %cr3, %edx
    movl %cr4, %ecx
    ret

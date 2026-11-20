/* mmu_gas.S — GAS port of the genuine Direct-model MMU enable path, upgraded
 * to real 4 KiB paging with per-page U/S protection (OS-CORE upgrade).
 *
 * Genuine sources: kernel/x86/bootcpu.asm EnableMmu:277 (CR3 load, CR0.PG|WP,
 * CR4.PGE, TLB flush) + bootutil.asm MapSingleSection (4M PS pages when PSE).
 *
 * Layout (identity mapped, phys==virt):
 *   qemu_page_dir  : 1024 x PDE. PDE[0..31] -> 4K page tables (P|RW|US, PS=0),
 *                    covering 0x00000000..0x07FFFFFF (low 128 MiB: ALL of the
 *                    128 MiB QEMU RAM — kernel image, kernel heap/data at
 *                    0x1000000, E32 user code/data at 0x2200000, DLL BSS).
 *                    PDE[32..1023] -> fallback 4M identity PDEs (P|RW|PS|G),
 *                    U/S=0: high MMIO/APIC, ring 0 only.
 *   qemu_page_tables: 32 x 4 KiB PTs (32768 PTEs), identity PTEs P|RW|US.
 *                    pd.cpp clears U/S on known kernel-only pages at boot.
 *
 * Mmu_BuildIdentityMap() is called from HwInit0 before any dynamic allocation;
 * the PT pool is static BSS so it cannot fragment or fail.
 */
.code32

.set CR0_PG, 0x80000000
.set CR0_WP, 0x00010000
.set CR4_PSE, 0x10
.set CR4_PGE, 0x80
.set PDE_P,  0x01
.set PDE_W,  0x02
.set PDE_U,  0x04
.set PDE_PS, 0x80
.set PDE_G,  0x100
.set PTE_P,  0x01
.set PTE_W,  0x02
.set PTE_U,  0x04
.set EX86Feat_PGE, 0x2000
.set K_NUM_PT, 32

/* 4K-aligned page directory + the 4K page-table pool. */
.section .bss.mmu, "aw", @nobits
.align 4096
.globl qemu_page_dir
qemu_page_dir:
    .space 4096
.globl qemu_page_tables
qemu_page_tables:
    .space (K_NUM_PT * 4096)
.globl qemu_mmu_on
qemu_mmu_on:
    .long 0

.section .text.mmu, "ax"
.code32

/* Build the 4K PD + 32 identity PTs. EAX/ECX/EDX/EDI/EBX clobbered (flags too).
 * PTEs default to U/S=1 (matches the old flat low-RAM map, so nothing regresses);
 * pd.cpp then clears U/S on kernel-only pages with 4K granularity. */
.globl Mmu_BuildIdentityMap
.type Mmu_BuildIdentityMap, @function
Mmu_BuildIdentityMap:
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %edi
    /* zero the page directory */
    movl $qemu_page_dir, %edi
    xorl %eax, %eax
    movl $1024, %ecx
1:  movl %eax, (%edi)
    addl $4, %edi
    decl %ecx
    jnz 1b
    /* zero all 32 page tables (32*1024 PTEs) */
    movl $qemu_page_tables, %edi
    movl $(K_NUM_PT * 1024), %ecx
2:  movl %eax, (%edi)
    addl $4, %edi
    decl %ecx
    jnz 2b
    /* fill each PT with identity PTEs: P|RW|US */
    movl $qemu_page_tables, %esi        /* ESI = current PT base */
    xorl %ecx, %ecx                     /* ECX = PT index 0..31 */
3:  movl %esi, %edi
    movl %ecx, %edx                     /* EDX = PT index -> this PT's 4M phys */
    shll $22, %edx
    xorl %ebx, %ebx                     /* EBX = PTE index 0..1023 */
4:  movl %ebx, %eax
    shll $12, %eax                      /* EA = (index << 12) + PT phys base */
    orl %edx, %eax
    orl $(PTE_P|PTE_W|PTE_U), %eax
    movl %eax, (%edi,%ebx,4)
    incl %ebx
    cmpl $1024, %ebx
    jb 4b
    addl $4096, %esi
    incl %ecx
    cmpl $K_NUM_PT, %ecx
    jb 3b
    /* fill the PD: low 128 MiB via 4K PTs, everything else via 4M fallback */
    movl $(PDE_P|PDE_W|PDE_PS), %eax    /* fallback template: U/S=0 */
    movl qemu_cpu_features, %edx
    testl $EX86Feat_PGE, %edx
    jz 5f
    orl $PDE_G, %eax
5:  movl %eax, %edx                     /* EDX = fallback PDE flags */
    movl $qemu_page_dir, %edi
    movl $qemu_page_tables, %esi        /* ESI = next PT phys */
    xorl %ecx, %ecx                     /* ECX = PDE index 0..1023 */
6:  cmpl $K_NUM_PT, %ecx
    jae 7f
    movl %esi, %ebx
    orl $(PDE_P|PDE_W|PDE_U), %ebx      /* 4K PT PDE: P|RW|US, PS=0 */
    jmp 8f
7:  movl %ecx, %ebx
    shll $22, %ebx
    orl %edx, %ebx                      /* 4M identity PDE, U/S=0, PS|G */
8:  movl %ebx, (%edi,%ecx,4)
    addl $4096, %esi
    incl %ecx
    cmpl $1024, %ecx
    jb 6b
    popl %edi
    popl %edx
    popl %ecx
    popl %ebx
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

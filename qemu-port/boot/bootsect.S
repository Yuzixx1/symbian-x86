/* bootsect.S — El Torito no-emulation boot sector (Phase 7, GRUB-free).
 *
 * WHY: GRUB2's ISO9660 reader corrupts our kernel past the first 2048 bytes
 * on pycdlib-mastered ISOs (verified by guest-memory forensics), while
 * single-sector reads are reliable. This sector IS the El Torito boot image:
 * SeaBIOS loads it at 0x7C00 with DL=boot drive, and it pulls the kernel
 * with BIOS INT13 EDD packets — no filesystem, no GRUB.
 *
 * Steps: COM1+VGA announce -> A20 -> E820 memory map -> EDD check ->
 * streaming chunk load (bounce 0x20000 -> PT_LOAD addresses, PHDR table
 * parsed from chunk 0, BSS zero) -> synthesize minimal MBI at 0x8000
 * (below bounce) -> flat GDT -> protected mode -> jump to ELF e_entry
 * with EAX=2BADB002, EBX=MBI (kernel can't tell the difference from a
 * Multiboot1 loader).
 *
 * Patched post-mastering by tools/mkiso.py (search markers, LE32):
 *   0x514C4241 ('QLBA') -> ISO LBA of /BOOT/EKERN.ELF
 *   0x514E5343 ('QNSC') -> kernel file size in 2048-byte ISO sectors
 *
 * Assembled 16-bit real mode, linked flat at 0x7C00, padded to 2048 bytes
 * (El Torito boot image = 4 x 512B sectors).
 */
.code16
.text
.globl _start
_start:
    cli
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw $0x6000, %sp
    movb %dl, %cs:boot_drive
    /* TEMP-DIAG IRQ test: stay cli (IF=0) for the whole load. If the hang
       disappears, a PIT IRQ0 BIOS interaction was derailing execution. */
    /* sti */

    /* COM1 115200 8N1 for serial log */
    movw $0x3FB, %dx
    movb $0x83, %al
    outb %al, %dx
    movw $0x3F8, %dx
    movb $0x01, %al
    outb %al, %dx
    movw $0x3F9, %dx
    xorb %al, %al
    outb %al, %dx
    movw $0x3FB, %dx
    movb $0x03, %al
    outb %al, %dx
    movw $0x3FA, %dx
    movb $0xC7, %al
    outb %al, %dx

    leaw msg_boot, %si
    call puts_rm
    movl %cs:kern_bdt_marker, %eax
    call puthex32_rm                 /* DIAG: build timestamp (which ISO?) */
    leaw msg_nl, %si
    call puts_rm

    /* A20 via fast gate 0x92 (SeaBIOS leaves it on; belt-and-braces). */
    inb $0x92, %al
    orb $0x02, %al
    outb %al, $0x92
    /* A20 via 8042 KBC fallback (harmless if already on; some real
       chipsets ignore 0x92, and USB-booted boxes vary). No stack/calls. */
1:  inb $0x64, %al
    testb $0x02, %al
    jnz 1b
    movb $0xD1, %al
    outb %al, $0x64
2:  inb $0x64, %al
    testb $0x02, %al
    jnz 2b
    movb $0xDF, %al
    outb %al, $0x60
3:  inb $0x64, %al
    testb $0x02, %al
    jnz 3b
    leaw msg_a20, %si
    call puts_rm

    /* E820 memory map at 0x5000 (up to 16 entries x 20B) */
    call do_e820
    leaw msg_mem, %si
    call puts_rm

    /* EDD extensions present? (IF=1 across BIOS calls: some firmware
       hangs disk I/O with interrupts disabled — observed on VirtualBox.) */
    xorw %ax, %ax                   /* pin segs: prior INT10/INT15 may trash DS */
    movw %ax, %ds
    movw %ax, %es
    sti
    movb $0x41, %ah
    movw $0x55AA, %bx
    movb %cs:boot_drive, %dl
    int $0x13
    cli
    jc disk_fail
    cmpw $0xAA55, %bx
    jne disk_fail
    movb $'+', %al                  /* DIAG: EDD present */
    movw $0x3F8, %dx
    outb %al, %dx
    leaw msg_eddok, %si
    call puts_rm

    /* Load kernel: LBA units depend on the boot medium. El Torito CD
       (drive >= 0xE0) addresses 2048B CD sectors (QLBA as-is); HDD/USB
       (drive < 0xE0) addresses 512B sectors (QHBA = QLBA*4, patched). */
    movl %cs:kern_lba_marker, %eax
    cmpb $0xE0, %cs:boot_drive
    jae cd_lba_ok
    movl %cs:kern_hba_marker, %eax
cd_lba_ok:
    movl %cs:kern_secs_marker, %ecx
    call load_kernel
    /* load_kernel never returns (jumps to pm_entry_rm when done). */
1:  hlt
    jmp 1b

/* ---------------- real-mode helpers ---------------- */
puts_rm:                            /* DS:SI NUL string -> VGA teletype + COM1 */
    pushw %ax
    pushw %dx
    pushw %si
    /* NOTE: no lodsb (string ops misbehave in real mode under QEMU
       here); explicit byte load instead. DF state irrelevant. */
1:  movb %ds:(%si), %al
    incl %esi
    testb %al, %al
    jz 2f
    movb $0x0E, %ah
    int $0x10
    movb %al, %ah
    movw $0x3FD, %dx
3:  inb %dx, %al
    testb $0x20, %al
    jz 3b
    movb %ah, %al
    movw $0x3F8, %dx
    outb %al, %dx
    jmp 1b
2:  popw %si
    popw %dx
    popw %ax
    ret

disk_fail:
    xorw %ax, %ax                   /* pin segs first: nothing on stack matters */
    movw %ax, %ds
    movw %ax, %es
    leaw msg_diskfail, %si
    call puts_rm
    leaw msg_errcode, %si
    call puts_rm
    movb %cs:cell_status, %al
    call puthex8_rm                  /* BIOS status */
    leaw msg_drive, %si
    call puts_rm
    movb %cs:boot_drive, %al
    call puthex8_rm                  /* drive number */
    leaw msg_lba, %si
    call puts_rm
    movl %cs:cell_lba, %eax
    call puthex32_rm                 /* failing LBA */
    leaw msg_chunk, %si
    call puts_rm
    movl %cs:cell_chunk, %eax
    call puthex32_rm                 /* failing count */
    leaw msg_nl, %si
    call puts_rm
1:  hlt
    jmp 1b

/* puthex32_rm: EAX -> 8 hex chars. puthex8_rm: AL -> 2 hex chars. */
puthex32_rm:
    pushl %eax
    pushl %ebx
    movl %eax, %ebx
    shrl $24, %eax
    call puthex8_rm
    movl %ebx, %eax
    shrl $16, %eax
    call puthex8_rm
    movl %ebx, %eax
    shrl $8, %eax
    call puthex8_rm
    movl %ebx, %eax
    call puthex8_rm
    popl %ebx
    popl %eax
    ret
puthex8_rm:                         /* AL = byte -> 2 hex chars (COM1+VGA) */
    pushw %ax
    shrb $4, %al
    call putnib_rm
    popw %ax
    pushw %ax
    andb $0x0F, %al
    call putnib_rm
    popw %ax
    ret
putnib_rm:                          /* AL = 0..15 value */
    cmpb $10, %al
    jb 1f
    addb $7, %al
1:  addb $'0', %al
    pushw %dx
    movb %al, %ah                    /* AH = ASCII */
    movw $0x3FD, %dx
2:  inb %dx, %al
    testb $0x20, %al
    jz 2b
    movb %ah, %al
    movw $0x3F8, %dx
    outb %al, %dx                    /* serial */
    movb %ah, %al
    movb $0x0E, %ah
    int $0x10                        /* VGA */
    popw %dx
    ret

do_e820:                            /* fills e820 table at 0x5000, count in BX */
    xorw %ax, %ax
    movw %ax, %es
    movw $0x5000, %di
    xorl %ebx, %ebx
    movw $0, %cs:e820_count
1:  movl $0xE820, %eax
    movl $0x534D4150, %edx
    movl $20, %ecx
    int $0x15
    jc 2f
    cmpl $0x534D4150, %eax
    jne 2f
    incw %cs:e820_count
    addw $20, %di
    testl %ebx, %ebx
    jnz 1b
    cmpw $16, %cs:e820_count
    jb 1b
2:  /* fallback: if E820 gave nothing, INT15 AH=88h (KB above 1M) */
    cmpw $0, %cs:e820_count
    jne 3f
    movb $0x88, %ah
    int $0x15
    jnc 4f
    movw $0x7F00, %ax               /* 32512K = ~31M fallback, still boots */
4:  movw %ax, %cs:e820_fallback_kb
3:  ret

load_kernel:                        /* Streaming EDD loader (no big staging).
       Kernel size varies (patched ISO-sector count). Each <=8-sector chunk
       lands in the 16KB bounce at 0x20000 via EDD v1 seg:off (the proven
       path: 64-bit EDD packets hang SeaBIOS here), then streams straight to
       its PT_LOAD addresses (parsed from chunk 0 into phdr_tab). Nothing
       accumulates: any kernel size fits; BSS zeroed per table afterwards.
       NO calls, NO rets, NO stack data across INT13s: loop state lives in
       image cells reloaded every use (SeaBIOS EDD was observed trashing
       caller stack words/cells). */
    /* Save caller LBA/count FIRST: the DS/ES pins below clobber AX. */
    movl %eax, %cs:cell_lba
    movl %ecx, %cs:cell_rem
    movl $0, %cs:cell_fileoff
    movl $0xFFFFFFFF, %cs:cell_parsed
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    /* Chunked EDD reads to bounce 0x20000, one ISO sector per transfer:
       single-sector transfers are the most compatible shape (multi-sector
       EDD hangs some BIOSes, observed on VirtualBox). */
rd_loop:
    movl %cs:cell_rem, %ecx
    testl %ecx, %ecx
    jz rd_done
    movl $1, %eax
    cmpl %eax, %ecx
    jae rd_chunk
    movl %ecx, %eax                 /* final partial chunk */
rd_chunk:
    movl %eax, %cs:cell_chunk
rd_issue:
    movw $0x0010, %cs:cell_pkt      /* size = 16: EDD v1 seg:off */
    movl %cs:cell_chunk, %eax
    /* EDD count units are medium-dependent like LBA (CD: ISO sectors,
       HDD: 512B sectors): scale up off-CD. */
    cmpb $0xE0, %cs:boot_drive
    jae cd_count_ok
    shll $2, %eax
cd_count_ok:
    movw %ax, %cs:cell_pkt+2        /* count */
    movl %cs:cell_lba, %eax
    movl %eax, %cs:cell_pkt+8       /* LBA32 */
    movl $0, %cs:cell_pkt+12
    movw $0x2000, %cs:cell_pkt+6    /* seg (bounce 0x20000) */
    movw $0, %cs:cell_pkt+4         /* off */
    xorw %ax, %ax                   /* re-pin: prior BIOS call may trash DS */
    movw %ax, %ds
    movw %ax, %es
    movw $cell_pkt, %si             /* DS=0: linear packet addr */
    movb $'R', %al                  /* DIAG: about to issue a read */
    movw $0x3F8, %dx
    outb %al, %dx
    movb $'R', %al
    movb $0x0E, %ah
    int $0x10
    xorw %ax, %ax                   /* re-pin AFTER video BIOS: INT10 may trash */
    movw %ax, %ds                   /* DS/ES/SI, and the packet addr must be */
    movw %ax, %es                   /* the last thing set before INT13 */
    movw $cell_pkt, %si             /* DS=0: linear packet addr */
    movb $0x42, %ah
    movb %cs:boot_drive, %dl
    sti                             /* BIOS disk I/O expects IF=1 */
    int $0x13
    cli
    movb %ah, %cs:cell_status
    setc %bl
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    testb %bl, %bl
    jz rd_ok
    movw $0xB800, %ax                /* DIAG: INT13 returned CF (not a hang) */
    movw %ax, %es                    /* direct VGA cell, no BIOS needed */
    movb $'e', %es:0x0640
    movb $0x4F, %es:0x0641
    xorw %ax, %ax
    movw %ax, %es
    /* Reset + retry (up to 3): flaky firmware gets a second chance. */
    movl %cs:cell_retry, %eax
    incl %eax
    movl %eax, %cs:cell_retry
    cmpl $3, %eax
    ja disk_fail
    sti
    movb $0, %ah
    movb %cs:boot_drive, %dl
    int $0x13
    cli
    jmp rd_issue
rd_ok:
    movl $0, %cs:cell_retry
    inb $0x92, %al                    /* re-assert A20: EDD may gate it, and the */
    orb $0x02, %al                    /* stream copy below targets above 1M */
    outb %al, $0x92                   /* (no stack: BIOS may trash SS:SP) */
    movb $'.', %al                  /* DIAG: chunk done */
    movw $0x3F8, %dx
    outb %al, %dx
    movb $'.', %al                  /* VGA copy: VBox screen shows progress */
    movb $0x0E, %ah
    int $0x10
    /* First chunk carries ELF header + PHDR table: parse PT_LOADs once. */
    cmpl $0xFFFFFFFF, %cs:cell_parsed
    je rd_parse
    jmp rd_stream
rd_parse:
    movl $0x20000, %ebx             /* bounce = file bytes [0,16K) */
    movl 28(%ebx), %eax             /* e_phoff */
    cmpl $512, %eax
    ja elf_bad                      /* sanity: table near header */
    movzwl 44(%ebx), %ecx           /* e_phnum */
    cmpl $8, %ecx
    ja elf_bad                      /* sanity: few segments */
    testl %ecx, %ecx
    jz elf_bad
    addl %ebx, %eax                 /* EAX = PHDR table */
    xorl %ebx, %ebx
    xorl %ebx, %ebx                 /* zero-extend: movw leaves high garbage */
    movw $phdr_tab, %bx
    movl $0, %cs:cell_nload
phdr_scan:
    testl %ecx, %ecx
    jz phdr_scan_done
    cmpl $1, (%eax)                 /* PT_LOAD? */
    jne phdr_scan_next
    movl 8(%eax), %edx              /* vaddr */
    cmpl $0x100000, %edx
    jb elf_bad                      /* sanity: dest >= 1M */
    cmpl $0x1000000, %edx
    jae elf_bad                     /* sanity: dest < 16M (KernData) */
    movl 4(%eax), %esi              /* file offset */
    movl %esi, (%ebx)
    movl %edx, 4(%ebx)              /* vaddr */
    movl 16(%eax), %edx
    movl %edx, 8(%ebx)              /* filesz */
    movl 20(%eax), %edx
    movl %edx, 12(%ebx)             /* memsz */
    addl $16, %ebx
    movl %cs:cell_nload, %edx
    incl %edx
    movl %edx, %cs:cell_nload
    cmpl $8, %edx
    ja elf_bad
phdr_scan_next:
    addl $32, %eax                  /* sizeof Elf32_Phdr */
    decl %ecx
    jmp phdr_scan
phdr_scan_done:
    cmpl $0, %cs:cell_nload
    je elf_bad
    movl $0, %cs:cell_parsed
    jmp rd_stream
rd_stream:
    /* Copy this chunk's overlap with every PT_LOAD (F=fileoff,L=bytes). */
    movl %cs:cell_fileoff, %eax
    movl %cs:cell_chunk, %ecx
    shll $11, %ecx
    movl %eax, %cs:cell_tmpF
    movl %ecx, %cs:cell_tmpL
    movl %cs:cell_nload, %edx
    xorl %ebx, %ebx
    xorl %ebx, %ebx                 /* zero-extend: movw leaves high garbage */
    movw $phdr_tab, %bx
stream_next:
    testl %edx, %edx
    jz stream_done
    movl (%ebx), %esi               /* o1 = max(F, off) */
    movl %cs:cell_tmpF, %edi
    cmpl %edi, %esi
    jae stream_o1ok
    movl %edi, %esi
stream_o1ok:
    movl (%ebx), %edi               /* o2 = min(F+L, off+filesz) */
    addl 8(%ebx), %edi
    movl %cs:cell_tmpF, %eax
    addl %cs:cell_tmpL, %eax
    cmpl %eax, %edi
    jbe stream_o2ok
    movl %eax, %edi
stream_o2ok:
    cmpl %edi, %esi
    jae stream_skip                 /* o1 >= o2: no overlap */
    movl %edi, %ecx
    subl %esi, %ecx                 /* n bytes */
    movl %esi, %eax
    subl %cs:cell_tmpF, %eax
    addl $0x20000, %eax             /* src = bounce+(o1-F) */
    movl %esi, %edi
    subl (%ebx), %edi
    addl 4(%ebx), %edi              /* dst = vaddr+(o1-off) */
    movl %ebx, %cs:cell_tmpE
    movl %edx, %cs:cell_tmpN
    movl %ecx, %edx
    shrl $2, %edx                   /* dwords */
    jz stream_tail
stream_dw:
    movl %ds:(%eax), %esi
    movl %esi, %es:(%edi)
    addl $4, %eax
    addl $4, %edi
    decl %edx
    jnz stream_dw
stream_tail:
    andl $3, %ecx
    jz stream_copied
stream_byte:
    movb %ds:(%eax), %dl
    movb %dl, %es:(%edi)
    incl %eax
    incl %edi
    decl %ecx
    jnz stream_byte
stream_copied:
    movl %cs:cell_tmpE, %ebx
    movl %cs:cell_tmpN, %edx
stream_skip:
    addl $16, %ebx
    decl %edx
    jmp stream_next
stream_done:
    movl %cs:cell_chunk, %eax
    movl %eax, %edx                 /* EDX = ISO-unit chunk (rem/fileoff) */
    cmpb $0xE0, %cs:boot_drive
    jae cd_lba_adv
    shll $2, %eax                   /* HDD LBA advances 4x (512B units) */
cd_lba_adv:
    addl %eax, %cs:cell_lba
    movl %cs:cell_rem, %ecx
    subl %edx, %ecx                 /* rem stays in ISO units */
    movl %ecx, %cs:cell_rem
    movl %cs:cell_chunk, %eax
    shll $11, %eax                  /* chunk bytes */
    addl %eax, %cs:cell_fileoff
    jmp rd_loop
rd_done:
/* --- BSS zero from phdr_tab (pure CPU) --- */
    /* Re-enable A20: SeaBIOS EDD may leave it gated (writes would wrap). */
    pushl %eax
    inb $0x92, %al
    orb $0x02, %al
    outb %al, $0x92
    popl %eax
    movl %cs:cell_nload, %edx
    xorl %ebx, %ebx
    xorl %ebx, %ebx                 /* zero-extend: movw leaves high garbage */
    movw $phdr_tab, %bx
zero_next:
    testl %edx, %edx
    jz zero_done
    movl 12(%ebx), %ecx             /* memsz - filesz = bss bytes */
    subl 8(%ebx), %ecx
    jbe zero_step
    cmpl $0x100000, %ecx             /* sanity: BSS < 1M */
    ja elf_bad
    movl 4(%ebx), %edi              /* vaddr + filesz = bss start */
    addl 8(%ebx), %edi
    xorb %al, %al
zero_byte:
    movb %al, %es:(%edi)
    incl %edi
    decl %ecx
    jnz zero_byte
zero_step:
    addl $16, %ebx
    decl %edx
    jmp zero_next
zero_done:
    movb $'C', %al                  /* DIAG: ELF load done */
    movw $0x3F8, %dx
    outb %al, %dx
    jmp 8f
elf_bad:
    leaw msg_elfbad, %si
    call puts_rm
1:  hlt
    jmp 1b
msg_elfbad:
    .asciz "BOOT: FATAL bad ELF headers
\n"
8:  /* --- hardcoded MBI at 0x8000 (QEMU -m 128 E820-measured) --- */
    movl $0x8000, %edi
    movl $0x1, (%edi)               /* flags = MEMORY */
    movl $639, 4(%edi)              /* mem_lower KB (full dword: no residue) */
    movl $129920, 8(%edi)           /* mem_upper KB */
    movb $'M', %al                  /* DIAG: MBI done */
    movw $0x3F8, %dx
    outb %al, %dx
    jmp pm_entry_rm                 /* never returns */

zero_bss:                           /* walk ELF PHDRs at 0x100000, zero BSS tails */
    movl $0x100000, %ebx
    movl 28(%ebx), %eax             /* e_phoff */
    movzwl 44(%ebx), %edx           /* e_phnum */
    addl %ebx, %eax                 /* PHDR table */
1:  testl %edx, %edx
    jz 2f
    cmpl $1, (%eax)                 /* PT_LOAD? */
    jne 3f
    movl 16(%eax), %ecx             /* p_filesz */
    movl 20(%eax), %edi             /* p_memsz */
    subl %ecx, %edi                 /* bss bytes */
    jbe 3f
    movl 8(%eax), %ecx              /* p_vaddr */
    addl 16(%eax), %ecx             /* + filesz */
    xchgl %ecx, %edi                /* EDI=dest, ECX=len */
    xorb %al, %al
    rep stosb                       /* ES=0, 32-bit EDI */
    jmp 3f
3:  addl $32, %eax                  /* sizeof Elf32_Phdr (phentsize==32 here) */
    decl %edx
    jmp 1b
2:  ret

build_mbi:                          /* MBI at 0x8000: flags/mem_lower/mem_upper */
    movl $0x8000, %edi
    movl $0x1, (%edi)               /* flags = MEMORY */
    /* mem_lower: INT12h KB below 640K */
    int $0x12
    jnc 1f
    movw $639, %ax
1:  movl $0, %ecx
    movw %ax, 4(%edi)
    /* mem_upper: sum E820 type-1 above 1M, else fallback */
    movl $0, %ecx
    cmpw $0, %cs:e820_count
    je 2f
    pushw %ds
    xorw %ax, %ax
    movw %ax, %ds
    movw $0x5000, %si
    movw %cs:e820_count, %dx
3:  cmpl $1, 16(%si)                /* type == RAM? */
    jne 4f
    movl (%si), %eax                /* base lo */
    cmpl $0x100000, %eax
    jb 4f
    movl 8(%si), %eax               /* len lo -> KB */
    shrl $10, %eax
    addl %eax, %ecx
4:  addw $20, %si
    decw %dx
    jnz 3b
    popw %ds
    jmp 5f
2:  movzwl %cs:e820_fallback_kb, %ecx
5:  movl %ecx, 8(%edi)
    ret

/* ---------------- protected-mode handoff ---------------- */
.code16
pm_entry_rm:
    cli
    lgdtl %cs:gdt_ptr
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmp $0x08, $pm_entry32
.code32
pm_entry32:
    movb $'G', %al                  /* DIAG: PM32 reached */
    movw $0x3F8, %dx
    outb %al, %dx
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    movl $0x9000, %esp              /* scratch stack (above MBI+handoff) */
    /* ELF e_entry: hardcoded 0x100030 (_mb_entry; Makefile asserts it).
       (Reading it from the image proved unreliable; see Phase 7 notes.) */
    movl $0x100030, %ecx
    /* DIAG: raw-hex ECX (e_entry) then J marker, no calls/BIOS. */
    pushl %ecx
    movl %ecx, %ebx
    shrl $28, %ebx
    call pm_hex1
    popl %ecx
    pushl %ecx
    movl %ecx, %ebx
    shrl $24, %ebx
    call pm_hex1
    popl %ecx
    pushl %ecx
    movl %ecx, %ebx
    shrl $20, %ebx
    call pm_hex1
    popl %ecx
    pushl %ecx
    movl %ecx, %ebx
    shrl $16, %ebx
    call pm_hex1
    popl %ecx
    pushl %ecx
    movl %ecx, %ebx
    shrl $12, %ebx
    call pm_hex1
    popl %ecx
    pushl %ecx
    movl %ecx, %ebx
    shrl $8, %ebx
    call pm_hex1
    popl %ecx
    pushl %ecx
    movl %ecx, %ebx
    shrl $4, %ebx
    call pm_hex1
    popl %ecx
    movl %ecx, %ebx
    call pm_hex1
    movb $'J', %al
    movw $0x3F8, %dx
    outb %al, %dx
    /* multiboot contract for _mb_entry (ECX still = e_entry). */
    movl $0x2BADB002, %eax
    movl $0x8000, %ebx
    movb %cs:boot_drive, %dl
    jmp *%ecx
pm_hex1:                            /* BL low nibble -> hex via COM1 */
    andb $0x0F, %bl
    cmpb $10, %bl
    jb pm_hex2
    addb $7, %bl
pm_hex2:
    addb $'0', %bl
    movb %bl, %al
    movw $0x3F8, %dx
    outb %al, %dx
    ret

/* ---------------- data ---------------- */
.code16
gdt:
    .long 0, 0
    .long 0x0000FFFF, 0x00CF9A00    /* 0x08: 32-bit code */
    .long 0x0000FFFF, 0x00CF9200    /* 0x10: 32-bit data */
gdt_ptr:
    .word gdt_ptr - gdt - 1
    .long gdt

boot_drive:
    .byte 0
    .align 4
cell_lba:
    .long 0                         /* INT13 LBA cursor (cells survive BIOS) */
cell_fileoff:
    .long 0                         /* file offset of current chunk */
cell_rem:
    .long 0                         /* ISO sectors remaining */
cell_total:
    .long 0                         /* total ISO sectors (for final copy) */
cell_chunk:
    .long 0                         /* current chunk */
cell_retry:
    .long 0                         /* EDD retries for this chunk */
cell_parsed:
    .long 0                         /* 0xFFFFFFFF until PHDRs parsed */
cell_nload:
    .long 0                         /* PT_LOAD entries saved */
cell_tmpF:
    .long 0                         /* stream: chunk file offset */
cell_tmpL:
    .long 0                         /* stream: chunk byte length */
cell_tmpE:
    .long 0                         /* stream: saved table pointer */
cell_tmpN:
    .long 0                         /* stream: saved entry counter */
phdr_tab:
    .space 128                      /* 8 x (off, vaddr, filesz, memsz) */
cell_status:
    .byte 0                         /* last INT13 AH status */
    .align 4
cell_pkt:
    .space 16                       /* EDD v1 packet (in-image, not scratch) */
e820_count:
    .word 0
e820_fallback_kb:
    .word 0

/* Patch markers (LE32), replaced by tools/mkiso.py */
kern_lba_marker:
    .long 0x514C4241               /* 'QLBA' -> ISO LBA of /BOOT/EKERN.ELF */
kern_secs_marker:
    .long 0x514E5343               /* 'QNSC' -> kernel size in ISO sectors */
kern_hba_marker:
    .long 0x51484241               /* 'QHBA' -> HDD LBA (QLBA*4, 512B units) */
kern_bdt_marker:
    .long 0x51424454               /* 'QBDT' -> Unix build time (mkiso) */

msg_boot:
    .asciz "BOOT: EKA2 CD boot, loading kernel...\r\n"
msg_a20:
    .asciz "BOOT: A20+E820...\r\n"
msg_mem:
    .asciz "BOOT: EDD read...\r\n"
msg_eddok:
    .asciz "BOOT: EDD ok, loading (dots) ...\r\n"
msg_loaded:
    .asciz "BOOT: kernel in, PM jump...\r\n"
msg_elf:
    .asciz "BOOT: ELF@1M magic=0x"
msg_entry:
    .asciz " entry=0x"
msg_nl2:
    .asciz "\r\n"
msg_diskfail:
    .asciz "BOOT: FATAL disk read error"
copy_bad:
    xorw %ax, %ax                   /* pin segs; cells hold the evidence */
    movw %ax, %ds
    movw %ax, %es
    leaw msg_copybad, %si
    call puts_rm
    movl %cs:cell_chunk, %eax
    call puthex32_rm
    leaw msg_nl, %si
    call puts_rm
1:  hlt
    jmp 1b
msg_copybad:
    .asciz "BOOT: FATAL copy bounds! chunk=0x"
msg_errcode:
    .asciz " status=0x"
msg_drive:
    .asciz " drive=0x"
msg_lba:
    .asciz " lba=0x"
msg_chunk:
    .asciz " count=0x"
msg_nl:
    .asciz "\r\n"

    /* pad boot image to 4096 bytes (El Torito multi-sector image) */
    .org 4096

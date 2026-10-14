/* mbr.S — USB/HDD MBR companion for the hybrid ISO (real hardware + VMs).
 *
 * CD boot uses El Torito (bootsect.S); a raw `dd` of the ISO to USB boots
 * as a hard disk, where the BIOS loads THIS sector (LBA 0) at 0x7C00 with
 * DL=drive number.
 *
 * Flow: copy self [0x7C00,0x7E00) down to 0x6000 and continue there (the
 * sector reads below land on 0x7C00, so running from 0x7C00 would eat its
 * own tail — observed), then EDD-load the El Torito boot image (always
 * 4096B = 8 sectors; size asserted by the Makefile) straight to its final
 * 0x7C00 in one-sector reads (single-sector transfers are the most
 * compatible shape; multi-sector EDD hangs some IDE paths here), then
 * far-jump to it with DL preserved. No bounce, no copy-back.
 *
 * Linked at 0x6000 (boot/mbr.ld) so every label addresses the relocated
 * copy directly with DS=CS=0; only the copy source (0x7C00) and the final
 * jump target are literals.
 *
 * Patched post-mastering by tools/mkiso.py (search marker, LE32):
 *   0x5142424C ('QBBL') -> ISO LBA of /BOOT/BOOTSECT.BIN (the boot image)
 *
 * The partition table (446-509) and 0x55AA signature are written by
 * mkiso.py, not here: this file only supplies the first 446 bytes.
 * Assembled 16-bit real mode (`.org 446` fails the build if the code
 * outgrows the MBR area). NO calls/rets/stack across INT13s; loop state
 * lives in image cells (SeaBIOS trashes segment regs and caller stack
 * words across EDD calls — observed).
 */
.code16
.text
.globl _start
_start:
    /* Entry runs at 0x7C00 (whatever CS the BIOS used); everything linked
       at 0x6000. Normalize CS via retf, using folded absolute addresses
       (GAS folds same-section link-time arithmetic). */
    cli
    xorw %ax, %ax
    movw %ax, %ss
    movw $0x6000, %sp
    pushw $0
    pushw $(mbr_norm - _start + 0x7C00)
    retf
mbr_norm:
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    movb %dl, %ds:(mbr_drive - _start + 0x7C00)
    sti

    /* Relocate [0x7C00,0x7E00) -> [0x6000,0x6200), then continue at 0x6000.
       Forward copy is safe (disjoint, running code untouched). */
    movw $0x7C00, %si
    movw $0x6000, %di
    movw $256, %cx                  /* 512 bytes in words */
mbr_reloc:
    movw %ds:(%si), %ax
    movw %ax, %es:(%di)
    addw $2, %si
    addw $2, %di
    decw %cx
    jnz mbr_reloc
    ljmp $0, $reloc_cont            /* linked at 0x6000: direct and correct */
reloc_cont:
    leaw mbr_banner, %si
    call mbr_puts

    /* EDD extensions present? (USB-HDD BIOSes all have them; bail loudly.) */
    xorw %ax, %ax                   /* pin segs: INT10 puts may trash DS */
    movw %ax, %ds
    movw %ax, %es
    movb $0x41, %ah
    movw $0x55AA, %bx
    movb mbr_drive, %dl
    int $0x13
    jc mbr_disk_fail
    cmpw $0xAA55, %bx
    jne mbr_disk_fail

    /* 8x single-sector EDD reads straight to 0x7C00+i*512. */
    movl mbr_bbl, %eax
    movl %eax, mbr_lba
    movl $0, mbr_cnt
mbr_rd:
    cmpl $8, mbr_cnt
    je mbr_done
    xorw %ax, %ax                   /* re-pin every iteration */
    movw %ax, %ds
    movw %ax, %es
    movw $0x0010, mbr_pkt           /* size = 16: EDD v1 seg:off */
    movw $1, mbr_pkt+2              /* count */
    movl mbr_lba, %eax
    movl %eax, mbr_pkt+8            /* LBA32 */
    movl $0, mbr_pkt+12
    movl mbr_cnt, %eax
    shll $5, %eax
    addl $0x07C0, %eax
    movw %ax, mbr_pkt+6             /* seg = 0x07C0+i*0x20 */
    movw $0, mbr_pkt+4              /* off */
    movb $0x42, %ah
    movb mbr_drive, %dl
    movw $mbr_pkt, %si              /* DS=0: relocated packet addr */
    int $0x13
    jc mbr_disk_fail
    movb $'.', %al                  /* DIAG: sector done */
    movw $0x3F8, %dx
    outb %al, %dx
    movl mbr_lba, %eax
    incl %eax
    movl %eax, mbr_lba
    movl mbr_cnt, %eax
    incl %eax
    movl %eax, mbr_cnt
    jmp mbr_rd
mbr_done:
    /* Hand off with DL=drive. Far jump serializes the icache. */
    movb mbr_drive, %dl
    ljmp $0, $0x7C00

mbr_disk_fail:
    xorw %ax, %ax
    movw %ax, %ds
    movw %ax, %es
    leaw mbr_msg, %si
    call mbr_puts
1:  hlt
    jmp 1b

mbr_puts:                           /* DS:SI NUL string -> VGA teletype + COM1 */
    pushw %ax
    pushw %dx
    pushw %si
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

mbr_drive:
    .byte 0
    .align 4
mbr_pkt:
    .space 16
mbr_bbl:
    .long 0x5142424C               /* 'QBBL' -> boot image LBA */
mbr_lba:
    .long 0                         /* EDD LBA cursor */
mbr_cnt:
    .long 0                         /* sectors loaded */
mbr_msg:
    .asciz "MBR: disk read error\r\n"
mbr_banner:
    .asciz "MBR: EKA2 USB/HDD boot, loading boot image...\r\n"

    /* code must fit the 446-byte MBR area (table+sig by mkiso.py) */
    .org 446

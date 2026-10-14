# qemu_boot.inc — GAS-side replacement for the missing legacy variant config.inc
# (MASM side). Included by hand-ported bootstrap .S files in Phase 3.
#
# Background: genuine bootcpu.inc does `INCLUDE config.inc` and expects the
# base-port to define CFG_MMDirect / CFG_RomSizeAlign / CFG_CopyRomToAddress /
# BPR_* parameters plus BTF_RamBanks/BTF_HwBanks tables. The original x86pc
# variant lived outside kernelhwsrv (os/boardsupport_internal/x86pc) and is
# NOT in this repo, so the QEMU port authors its own here.
#
# This file is NOT MASM: it is GAS syntax, included with `.include`.
# Values mirror inc/qemu_config.h so C and asm agree.

.ifndef __QEMU_BOOT_INC__
.equ __QEMU_BOOT_INC__, 1

# --- Memory model: Direct only (Phase 2 decision) ---
.equ CFG_MMDirect, 1
.equ CFG_RomSizeAlign, 16

# --- Load / identity-map layout (must match boot/link.ld) ---
.equ QEMU_RAM_BASE,      0x00000000
.equ QEMU_RAM_SIZE,      0x08000000
.equ QEMU_KERNEL_LOAD,   0x00100000
.equ QEMU_VGA_HOLE_BASE, 0x000A0000
.equ QEMU_VGA_HOLE_SIZE, 0x00060000

# --- Early serial (COM1 0x3F8, 115200 8N1), polled, no interrupts ---
.equ QEMU_COM1_BASE, 0x03F8

# --- i8259 remap target: IRQ0 -> INT 0x20 (see vectors.cpp Init1Interrupts) ---
.equ QEMU_IRQ_REMAP_BASE, 0x20

# --- Handoff page written by multiboot.S for ported ResetEntry (Phase 3) ---
.equ QEMU_HANDOFF_ADDR,  0x9000
.equ QEMU_HANDOFF_MAGIC, 0x51454B32

# --- MASM->GAS porting macros used by Phase 3 boot ports ---
# MASM `ADR reg, label` (EIP-relative lea) becomes:
.macro ADR reg, label
    lea \label, \reg
.endm

.endif

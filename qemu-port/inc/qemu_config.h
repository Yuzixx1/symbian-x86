// qemu_config.h — QEMU i386 base-port constants (C side).
// Replaces the missing legacy variant config.inc for C/C++ code.
// MASM-side equivalent for the bootstrap lives in inc/qemu_boot.inc.
// Memory model: Direct (CFG_MMDirect), identity-mapped RAM, ROM-in-RAM.
//
// Genuine EKA2 sources that consume these:
//   kernel/eka/kernel/x86/bootmain.asm  (BTF_RamBanks / BTF_HwBanks tables)
//   kernel/eka/memmodel/epoc/direct/x86/xinit.cpp (RamBlockSize, heap sizing)
//   kernel/eka/include/kernel/x86/x86boot.h (KPageSize, segment selectors)
#ifndef __QEMU_PORT_CONFIG_H__
#define __QEMU_PORT_CONFIG_H__

#include <stdint.h>

// --- Memory-model selection (mirrors legacy config.inc CFG_ symbols) ---
#define CFG_MMDirect 1
#define CFG_RomSizeAlign 16        // 64K ROM granularity (same default as bootcpu.inc)
#define CFG_MinimiseKernelHeap 1   // keep initial kernel heap tight for 128M QEMU guest

// --- QEMU 'pc' physical map (i440FX, single CPU, direct model: linear==physical) ---
#define QEMU_RAM_BASE       0x00000000u
#define QEMU_RAM_SIZE       0x08000000u   // 128M; matches -m 128 in Makefile qemu target
#define QEMU_KERNEL_LOAD    0x00100000u   // 1M: multiboot load address (see boot/link.ld)
#define QEMU_VGA_HOLE_BASE  0x000A0000u   // must NEVER be mapped as RAM (VGA/BIOS hole)
#define QEMU_VGA_HOLE_SIZE  0x00060000u
#define QEMU_MMIO_HOLE      0xF0000000u   // PCI/MMIO above RAM, identity-map on demand only

// --- 16550A COM1: early debug port (replaces legacy TRomHeader::iDebugPort UART) ---
#define QEMU_COM1_BASE      0x03F8u
#define QEMU_COM1_BAUDDIV   1             // 115200 baud

// --- i8259 PIC (used until Phase 3 APIC work; vectors remapped to 0x20-0x2F) ---
#define QEMU_PIC1_CMD       0x0020u
#define QEMU_PIC1_DATA      0x0021u
#define QEMU_PIC2_CMD       0x00A0u
#define QEMU_PIC2_DATA      0x00A1u
#define QEMU_IRQ_REMAP_BASE 0x20u         // EKA2 vectors.cpp maps HW IRQs at >=0x30;
                                          // QEMU PIC must be remapped so IRQ0 -> 0x20

// --- i8254 PIT channel 0: EKA2 MsTickPeriod source (A::Init1 -> NTimerQ::Init1) ---
#define QEMU_PIT_CH0        0x0040u
#define QEMU_PIT_CMD        0x0043u
#define QEMU_PIT_HZ         1193182u
#define QEMU_TICK_HZ        1000u         // 1ms tick; Asic::MsTickPeriod() returns 1

// --- CMOS RTC (Asic::SystemTimeInSecondsFrom2000) ---
#define QEMU_CMOS_ADDR      0x0070u
#define QEMU_CMOS_DATA      0x0071u

// --- Multiboot->EKA2 handoff page (filled by boot/multiboot.S, read by ported ResetEntry) ---
#define QEMU_HANDOFF_MAGIC  0x51454B32u   // 'QEK2'
#define QEMU_HANDOFF_ADDR   0x00009000u   // free low-mem page below 640K, clear of EBDA

#endif // __QEMU_PORT_CONFIG_H__

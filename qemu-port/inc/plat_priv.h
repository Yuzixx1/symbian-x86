// plat_priv.h — QEMU variant private export (Phase 11).
//
// What bldmake generates per-variant from variant.mmh, hand-written here:
// the symbols genuine X86 kernel headers expect from <plat_priv.h>.
// Grown strictly compiler-driven: every declaration below answers a real
// `undefined/undeclared' error from a genuine kernel/ or memmodel compile.
// Nothing here duplicates genuine logic; variant values (memory map, caps)
// follow our QEMU i386 target (128MB RAM, 486+, PSE/PGE, 16550A, i8259/54).
#ifndef __QEMU_PLAT_PRIV_H__
#define __QEMU_PLAT_PRIV_H__

#endif // __QEMU_PLAT_PRIV_H__

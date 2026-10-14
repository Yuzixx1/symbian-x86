// platform.h — QEMU variant platform export (Phase 11).
//
// What bldmake generates per-variant (CPU/ASIC identity, capabilities),
// hand-written here for QEMU i386. Grown strictly compiler-driven, like
// inc/plat_priv.h: every declaration answers a real error from a genuine
// kernel/ or memmodel compile.
#ifndef __QEMU_PLATFORM_H__
#define __QEMU_PLATFORM_H__

// Genuine assp.h (via x86.h) needs TMachineStartupType + TPtr8 before any
// other kernel header is included. The genuine variant export provides them;
// here they come from their genuine homes (cf. bldmake variant export).
#include <e32hal.h>
#include <e32cmn.h>

#endif // __QEMU_PLATFORM_H__

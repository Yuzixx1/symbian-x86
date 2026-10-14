// bootinfo.h — Multiboot-provided memory map (Phase 7).
// Captured in QemuKernelMain while paging is still off (identity == direct).
#ifndef __QEMU_BOOTINFO_H__
#define __QEMU_BOOTINFO_H__

#include "qemu_assp.h"

namespace BootInfo
    {
    extern TUint flags;       // multiboot info flags word
    extern TUint memLowerKB;  // conventional memory below 640K (flags&1)
    extern TUint memUpperKB;  // extended memory above 1M in KB (flags&1)
    extern TUint infoAddr;    // multiboot_info* as passed in EBX
    void Capture(TUint aInfoAddr);
    }

#endif

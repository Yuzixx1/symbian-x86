// kernfull_glue.cpp — minimal bootstrap glue for the full Kern image.
//
// The genuine kernel now provides KPrintf/NKFault/ExcFault/KernelMain/
// RomHeaderAddress/SuperPageAddress/BTrace itself, so this file keeps only
// what is genuinely variant-owned: early UART, ROM synthesis kickoff,
// .init_array construction, the reserve blobs, and a bump allocator for
// ::malloc/::free (nk_entry operator new, pre-heap BTrace).

#include <x86.h>
#include <kernel/kern_priv.h>
#include <kernel/kernel.h>
#include "qemu_variant.h"

namespace QemuUart
    {
    void Init();
    void PutC(char aC);
    void Puts(const char* aS);
    void PutHex(unsigned aV);
    }

extern "C"
    {
    extern TLinAddr RomHeaderAddress;
    extern TLinAddr SuperPageAddress;
    }

// Reserve blobs: ResetEntry pushes these to _E32Startup_Body (bootmain_gas.S).
// Sizes cover QRomBlob (~2KB) and TSuperPage + CpuPage + machine config.
TUint8 qemu_rom_header_reserve[4096];
TUint8 qemu_superpage_reserve[16384];

namespace
    {
    TUint8 gHeapPool[262144];
    TUint gHeapUsed = 0;
    }

extern "C"
    {
    TAny* malloc(TUint32 aSize)
        {
        // Genuine objects such as TServerMessage use global new but are
        // released with Kern::AsyncFree. Once the kernel heap exists they
        // must come from that heap, not the bootstrap bump pool.
        if (K::Allocator)
            return Kern::Alloc(aSize);
        TUint size = (TUint)((aSize + 7u) & ~7u);
        if (gHeapUsed + size > sizeof(gHeapPool))
            return 0;
        TAny* p = &gHeapPool[gHeapUsed];
        gHeapUsed += size;
        return p;
        }
    // Global operator delete lands here. Route to the kernel heap once it
    // exists; the pre-heap bump pool is append-only (never freed) and must
    // never reach Kern::Free (foreign pool -> heap corruption). Range check
    // keeps both pools disjoint. This fixes the per-exec heap drain: every
    // DObject delete previously vanished here while its AllocZ stayed live.
    void free(TAny* aCell)
        {
        TUint8* p = (TUint8*)aCell;
        if (!p)
            return;
        if (p >= gHeapPool && p < gHeapPool + sizeof(gHeapPool))
            return;  // pre-heap bump pool: no-op by design
        if (K::Allocator)
            Kern::Free(p);
        }
    }

typedef void (*CtorFn)(void);
extern "C"
    {
    extern CtorFn __init_array_start[];
    extern CtorFn __init_array_end[];
    }

extern "C" void* memclr(void* aTrg, unsigned aLength);

// Genuine variant entry, called by _E32Startup_Body (nk_entry.cpp) after the
// ROM/SuperPage addresses land but before KernelMain. Runs before any genuine
// kernel code, so: UART first, ROM synth, then static construction.
extern "C" void HwInit0()
    {
    QemuUart::Init();
    QemuRomInit();
    // Bootstrap duty (cf. estrt clearing kernel RAM): the kernel data, stacks
    // and heap live in fixed RAM, not in our ELF .bss, so clear the region
    // before genuine code takes uninitialized reads from it.
    memclr((void*)kQemuKernDataAddress, (unsigned)kQemuKernRegionSize);
    // Publish the variant ASIC before _E32Startup reaches KernelMain: the
    // ROM path (P::CreateVariant -> A::CreateVariant) sets the same pointer
    // later, but early KBOOT traces would otherwise be dropped (DebugOutput
    // null-checks TheAsic). Variant-owned debug enablement, not a bypass.
    X86::TheAsic = (Asic*)QemuVariantInitialise(0);
    QemuUart::Puts("QEMU-HwInit0: ROM synth ok\r\nRomHeaderAddress = ");
    QemuUart::PutHex((unsigned)RomHeaderAddress);
    QemuUart::Puts("\r\nSuperPageAddress = ");
    QemuUart::PutHex((unsigned)SuperPageAddress);
    QemuUart::Puts("\r\n");
    for (CtorFn* p = __init_array_start; p != __init_array_end; ++p)
        (*p)();
    }

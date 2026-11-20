// pd.h — port-side 4K page-table / per-page U/S management (OS-CORE upgrade).
//
// kernelhwsrv/ is read-only and the genuine DProcess has no portable PD slot
// (only the flexible memmodel adds iPageDir/iOsAsid). Ownership therefore lives
// in this port-side table, keyed by DProcess*, mirroring the sExecs/ProcExt
// pattern in variant/e32load.cpp.
//
// Design: mmu/mmu_gas.S builds ONE global 4K page directory + 32 shared 4K
// identity page tables covering the low 128 MiB (all of QEMU's 128 MiB RAM).
// Per-process PDs (KMaxPdProcs) point at those SAME shared PTs for PDE[0..31]
// and use 4M identity PDEs (U/S=0) for the high MMIO/APIC half. U/S bits are
// therefore global, not per-process — documented remainder: true per-process
// isolation still needs private PT copies. The PT pool cap is 32 pages; the
// PD pool cap is KMaxPdProcs (16) pages.

#ifndef QEMU_MMU_PD_H__
#define QEMU_MMU_PD_H__

// TInt/TUint/TBool come from the kernel headers that must be included before
// this file (variant/e32load.cpp includes kern_priv.h/kernel.h first).
class DProcess;

TBool PdAvail();       // a free side-table row exists
TInt  PdCreate(DProcess* aProc, TUint aCodeRun, TUint aCodeLen,
               TUint aDataRun, TUint aDataLen,
               TUint aStackRun, TUint aStackLen);
TUint PdPhys(DProcess* aProc);  // CR3 value (4K-aligned PD phys); 0 if unknown
void  PdDestroy(DProcess* aProc);

// MMU infrastructure ---------------------------------------------------------
void  PdInit();            // early boot: lock kernel pages U/S=0, install hooks
void  PdSwitch(TUint aPdPhys);  // naked-style CR3 reload (helper)
TUint PdCurrent();         // read CR3

// Per-page U/S (4K granularity) on the SHARED page tables.
// PdMapUser  sets U/S=1 for [aVaddr,aVaddr+aSize).
// PdMapKernel clears U/S (supervisor-only) for that range.
// For ranges >= 128 MiB (outside the 32-PT pool) we fall back to flipping the
// U/S bit of the coarse 4M identity PDE for that 4M region.
void  PdMapUser(DProcess* aProc, TUint aVaddr, TUint aPhys, TUint aSize);
void  PdMapData(DProcess* aProc, TUint aVaddr, TUint aPhys, TUint aSize);
void  PdMapKernel(DProcess* aProc, TUint aVaddr, TUint aSize);
void  PdMapUserRange(TUint aVaddr, TUint aSize);
void  PdMapKernelRange(TUint aVaddr, TUint aSize);

// Boot-time diagnostic: dump CR3 / PDE[0] / sample PTE U/S bits to serial.
void  PdDump4K(const char* aTag);

#endif // QEMU_MMU_PD_H__

// pd.h — port-side per-process page directory side-table (M-03).
//
// kernelhwsrv/ is read-only and the genuine DProcess has no portable PD slot
// (only the flexible memmodel adds iPageDir/iOsAsid). Ownership therefore lives
// in this port-side table, keyed by DProcess*, mirroring the sExecs/ProcExt
// pattern in variant/e32load.cpp.

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

// M-03 step 2: MMU infrastructure -------------------------------------------
void  PdInit();            // early boot: init pool, install Vec14 + switch hook
void  PdSwitch(TUint aPdPhys);  // naked: mov cr3, eax (caller-saved EAX)
TUint PdCurrent();         // read CR3
void  PdMapUser(DProcess* aProc, TUint aVaddr, TUint aPhys, TUint aSize);
void  PdMapData(DProcess* aProc, TUint aVaddr, TUint aPhys, TUint aSize);
                            // record/map a user page range U/S=1 (kernel U/S=0
                            // is deferred: flat 4M PDE map keeps ring-3 live)

#endif // QEMU_MMU_PD_H__

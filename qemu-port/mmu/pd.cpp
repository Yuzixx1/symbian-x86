// pd.cpp — port-side M-03 page directory allocation + ownership side-table.
//
// Scope of this step (design docs/r1_mmu_design_v1.md, tasks #1-#2):
//   - one 4K-aligned page directory per E32 process, tracked by DProcess*
//   - PD starts as a flat 4M identity map with U/S=1 so enabling CR3 switching
//     later cannot change today's ring-3 behaviour
//   - 4K page tables + per-page U/S enforcement (kernel U/S=0) and the #PF
//     policy are deliberately deferred to the next M-03 step
//
// No kernelhwsrv/ source is modified. A fixed, 4K-aligned BSS page pool is used
// instead of Kern::AllocZ(4096): the hybrid heap is not page-aligned, and the
// design doc explicitly allows a fixed page pool as the fragmentation fallback.
#include <e32err.h>
#include <kernel/kern_priv.h>
#include <kernel/kernel.h>
#include "pd.h"

namespace
    {
    const TInt KMaxPdProcs = 16;
    const TUint KPdEntries = 1024;  // 4 KiB / 4 bytes

    struct TProcPd
        {
        DProcess* iProc;
        TUint     iPdPhys;   // CR3 value; 0 marks a free row
        // Recorded for the later PdMapUser() step (4K PTEs, U/S=1).
        TUint     iCodeRun, iCodeLen;
        TUint     iDataRun, iDataLen;
        TUint     iStackRun, iStackLen;
        };

    static TProcPd sProcs[KMaxPdProcs];
    static TUint32 sPdPages[KMaxPdProcs][KPdEntries]
        __attribute__((aligned(4096)));
    }

TBool PdAvail()
    {
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        if (!sProcs[i].iProc)
            return ETrue;
    return EFalse;
    }

TInt PdCreate(DProcess* aProc, TUint aCodeRun, TUint aCodeLen,
              TUint aDataRun, TUint aDataLen,
              TUint aStackRun, TUint aStackLen)
    {
    if (!aProc)
        return KErrArgument;
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        {
        if (sProcs[i].iProc)
            continue;
        TUint32* pd = sPdPages[i];
        // Identity map 0-128 MiB with 32 x 4 MiB PDEs. 0x87 = P|RW|U/S|PS:
        // matches the boot map's reachability (ring 3 must keep working until
        // the 4K PTEs with U/S=0 for kernel pages are installed).
        for (TUint n = 0; n < KPdEntries; ++n)
            pd[n] = 0;
        for (TUint n = 0; n < 32; ++n)
            pd[n] = ((TUint32)n << 22) | 0x87u;
        TProcPd& r = sProcs[i];
        r.iProc = aProc;
        r.iPdPhys = (TUint)pd;
        r.iCodeRun = aCodeRun;  r.iCodeLen = aCodeLen;
        r.iDataRun = aDataRun;  r.iDataLen = aDataLen;
        r.iStackRun = aStackRun; r.iStackLen = aStackLen;
        return KErrNone;
        }
    return KErrOverflow;
    }

TUint PdPhys(DProcess* aProc)
    {
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        if (sProcs[i].iProc == aProc)
            return sProcs[i].iPdPhys;
    return 0;
    }

void PdDestroy(DProcess* aProc)
    {
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        {
        if (sProcs[i].iProc != aProc)
            continue;
        TProcPd& r = sProcs[i];
        r.iProc = 0;
        r.iPdPhys = 0;
        r.iCodeRun = r.iCodeLen = 0;
        r.iDataRun = r.iDataLen = 0;
        r.iStackRun = r.iStackLen = 0;
        return;
        }
    }

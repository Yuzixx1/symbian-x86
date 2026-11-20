// qemu_ps_exec.cpp -- expose a genuine userland process census to ring-3 E32
// programs (the PS process viewer, user/ps.c).
//
// The kernel shell's `ps`, `segs` and PortFile channel counter read the genuine
// EKA2 containers directly (variant/kern_shell.cpp). There is no executive that
// returns a process list to userland, so this file adds one small dedicated
// slow executive that snapshots exactly what the shell prints:
//
//   processes = K::Containers[EProcess]->Count()
//   channels  = UserFilesChannelCount()
//   segs      = DCodeSeg::GlobalList length
//   per-process id + FullName from K::Containers[EProcess]
//
// The snapshot is written into a static struct in low, identity-mapped RAM
// (U/S=1 in user PDs -- mmu/pd.cpp). This is the proven netbridge (247/248)
// hand-off pattern; kernelhwsrv/ is left untouched.
//
// Slot choice: the only invalid entries left in EpocSlowExecTable are 240
// (audio tone) and 247..251 (netbridge + GUI), all already taken. Slots
// 252..267 are genuine execs and 268..270 belong to SvcMgr. Slot 241
// (EExecGetBTraceId) is unused in this port -- BTrace is stubbed out
// (genuine_glue.cpp: BTrace::IsSupported) -- so repurposing it is safe and
// does not collide with any other worker's exec.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include <kernel/kern_priv.h>
#include <memmodel.h>
#include "qemu_variant.h"
#include "../user/pshost.h"

#define QE_EXEC_PS 241u

extern const TUint32 EpocSlowExecTable[];

namespace
    {
    __attribute__((aligned(4096))) static struct ps_census gPsCensus;

    /* Take a snapshot of the same three counts the shell `ps` prints, plus a
     * bounded list of process ids/names. Called from slow-exec context on the
     * requesting user thread, with the same container locking the shell uses. */
    static TInt PsRefresh()
        {
        struct ps_census* c = &gPsCensus;
        c->magic = 0;                       /* invalidate while rebuilding */

        c->channels = UserFilesChannelCount();

        DObjectCon* pc = K::Containers[EProcess];
        pc->Wait();
        TInt n = pc->Count();
        c->processes = n;
        TInt listed = 0;
        for (TInt i = 0; i < n && listed < PS_MAX_PROCS; ++i)
            {
            DProcess* p = (DProcess*)(*pc)[i];
            struct ps_process* e = &c->proc[listed];
            e->pid = (int)p->iId;
            TBuf<PS_NAME_LEN> name;
            p->FullName(name);
            TInt k = 0;
            for (; k < name.Length() && k < PS_NAME_LEN - 1; ++k)
                e->name[k] = (char)(name[k] & 0x7F);
            e->name[k] = 0;
            ++listed;
            }
        pc->Signal();
        c->listed = listed;

        DCodeSeg::Wait();
        TInt ns = 0;
        for (SDblQueLink* pL = DCodeSeg::GlobalList.First();
             pL != &DCodeSeg::GlobalList.iA; pL = pL->iNext)
            ++ns;
        DCodeSeg::Signal();
        c->segs = ns;

        __asm__ __volatile__("" : : : "memory");   /* publish after fill */
        c->magic = PS_MAGIC;
        return n;
        }

    /* Slow exec ABI: TInt handler(TInt aOp, TInt aUnused).
     *   aOp 0 -> identity-mapped base address of gPsCensus
     *   aOp 1 -> refresh snapshot, return process count
     *   else  -> KErrArgument */
    static TInt PsExec(TInt aOp, TInt)
        {
        if (aOp == 0) return (TInt)(TLinAddr)&gPsCensus;
        if (aOp == 1) return PsRefresh();
        return KErrArgument;
        }

    static void PsExecInstall()
        {
        TUint32* tab = (TUint32*)EpocSlowExecTable;
        if (tab[0] < QE_EXEC_PS + 1u)
            {
            KPrintf("ps exec: table count %u, install skipped\r\n", tab[0]);
            return;
            }
        const TUint s = QE_EXEC_PS;
        tab[3u + 2u * s] = 0;                       /* flags */
        tab[4u + 2u * s] = (TUint32)(TLinAddr)&PsExec;
        KPrintf("ps exec: census installed at slot %u\r\n", s);
        }
    }  /* namespace */

TUint PsExecSlot() { return QE_EXEC_PS; }

/* Boot hook: HwInit0 walks .init_array after paging + TheAsic are ready. */
__attribute__((constructor)) static void PsExecBootInit()
    { PsExecInstall(); }

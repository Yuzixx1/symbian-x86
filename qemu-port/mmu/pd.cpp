// pd.cpp — port-side M-03 page directory allocation + ownership side-table.
//
// kernelhwsrv/ is READ-ONLY. A fixed, 4K-aligned BSS page pool is used instead
// of Kern::AllocZ(4096): the hybrid heap is not page-aligned. The boot PD is
// built by mmu/mmu_gas.S (32x4M identity, U/S=1) and process PDs are identical
// copies so a CR3 reload never unmaps running kernel/user code. Real 4K PTEs
// with per-page U/S are deferred (see docs/r1_mmu_feasibility.md caveat).
#include <e32err.h>
#include <kernel/kern_priv.h>
#include <kernel/kernel.h>
#include "pd.h"

// Genuine exception vector table (vectors.cia; non-const despite the comment).
extern "C" void (*TheExcVectors[64])();

namespace
    {
    const TInt KMaxPdProcs = 16;
    const TUint KPdEntries = 1024;  // 4 KiB / 4 bytes

    struct TProcPd
        {
        DProcess* iProc;
        TUint     iPdPhys;   // CR3 value; 0 marks a free row
        TUint     iCodeRun, iCodeLen;
        TUint     iDataRun, iDataLen;
        TUint     iStackRun, iStackLen;
        };

    static TProcPd sProcs[KMaxPdProcs];
    static TUint32 sPdPages[KMaxPdProcs][KPdEntries]
        __attribute__((aligned(4096)));
    static TBool sPdInitialised = EFalse;
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
        for (TUint n = 0; n < KPdEntries; ++n)
            pd[n] = 0;
        // Identity map the full flat 4 GiB with 1024 x 4 MiB PDEs (matches the
        // boot PD / GDT and keeps high MMIO reachable from ring 0). Coarse
        // M-03 split: below 0x80000000 is U/S=1 (user code/heap live there),
        // the high half is U/S=0 supervisor-only (kernel MMIO/APIC).
        for (TUint n = 0; n < KPdEntries; ++n)
            pd[n] = ((TUint32)n << 22) | (n < 512u ? 0x87u : 0x83u);
        TProcPd& r = sProcs[i];
        r.iProc = aProc;
        r.iPdPhys = (TUint)pd;
        r.iCodeRun = aCodeRun;  r.iCodeLen = aCodeLen;
        r.iDataRun = aDataRun;  r.iDataLen = aDataLen;
        r.iStackRun = aStackRun; r.iStackLen = aStackLen;
        // Install the recorded user ranges (4K PTE U/S=1 is deferred).
        PdMapUser(aProc, aCodeRun, aCodeRun, aCodeLen);
        PdMapUser(aProc, aDataRun, aDataRun, aDataLen);
        if (aStackLen)
            PdMapUser(aProc, aStackRun, aStackRun, aStackLen);
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
        // Zero the page unless it is still live in CR3 (E32Reap can run with a
        // dying process's PD loaded — emptying it there would fault instantly).
        if (r.iPdPhys && PdCurrent() != r.iPdPhys)
            {
            TUint32* pd = (TUint32*)r.iPdPhys;
            for (TUint n = 0; n < KPdEntries; ++n)
                pd[n] = 0;   // free the PD page: invalidate every PDE
            }
        r.iProc = 0;
        r.iPdPhys = 0;
        r.iCodeRun = r.iCodeLen = 0;
        r.iDataRun = r.iDataLen = 0;
        r.iStackRun = r.iStackLen = 0;
        return;
        }
    }

// --- M-03 step 2: CR3 switching + #PF containment --------------------------

// Naked switch hook. Genuine ncsched.cia calls iProcessHandler with EBX = new
// NThread, EDI = scheduler; the handler must preserve EBX/EDI/ESI and return
// via ret. We reload CR3 from NThread.iAddressSpace (0 == keep current PD).
extern "C" __attribute__((naked)) void PdProcessSwitch()
    {
    asm("mov eax, [ebx + %c0]\n\t"
        "test eax, eax\n\t"
        "jz 1f\n\t"
        "mov cr3, eax\n\t"
        "1:\n\t"
        "ret\n\t"
        : : "i"((TUint)_FOFF(NThread, iAddressSpace)));
    }

// Naked #PF stub, installed as TheExcVectors[14] before X86::Init1Interrupts
// builds the IDT. CPU pushes: [esp]=error, +4=EIP, +8=CS, +12=EFLAGS,
// (+16=ESP, +20=SS on CPL change). U/S error bit (bit 2) selects policy.
// Kernel faults and user faults both terminate the offending context; the
// genuine exception machinery is bypassed only for the #PF vector.
extern "C" void PdPageFaultC(TUint aCr2, TUint aErr);

extern "C" __attribute__((naked)) void PdPageFaultStub()
    {
    asm("cld\n\t"
        "push ds\n\t"
        "push es\n\t"
        "push eax\n\t"
        "push edx\n\t"
        "mov eax, cr2\n\t"
        "mov edx, [esp+16]\n\t"   // page-fault error code
        "mov ax, ss\n\t"          // kernel data segments for the C++ helper
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "push edx\n\t"            // arg2: error code
        "push eax\n\t"            // arg1: CR2
        "call PdPageFaultC\n\t"
        "add esp, 8\n\t"
        "pop edx\n\t"
        "pop eax\n\t"
        "pop es\n\t"
        "pop ds\n\t"
        "iretd\n\t");
    }

extern "C" void PdPageFaultC(TUint aCr2, TUint aErr)
    {
    const TBool user = (aErr & 4u) != 0;
    KPrintf("MMU #PF: cr2=0x%08x err=0x%02x %s\r\n", aCr2, aErr,
            user ? "(user) killing process" : "(kernel) PANIC");
    if (user)
        {
        // Panics the *current thread* (the owning process dies, machine lives).
        K::PanicKernExec(KErrGeneral);  // does not return
        }
    else
        Kern::Fault("MMU-KernelPF", K::ESystemException);  // kernel bug
    }

void PdInit()
    {
    if (sPdInitialised)
        return;
    sPdInitialised = ETrue;
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        {
        sProcs[i].iProc = 0;
        sProcs[i].iPdPhys = 0;
        }
    TheExcVectors[14] = (void (*)())&PdPageFaultStub;
    TheScheduler.SetProcessHandler((TLinAddr)&PdProcessSwitch);
    KPrintf("M-03: PdInit PD pool=%d procs, Vec14=%x, processHandler=%x\r\n",
            KMaxPdProcs, (TUint)TheExcVectors[14], (TUint)&PdProcessSwitch);
    }

void PdSwitch(TUint aPdPhys)
    {
    if (aPdPhys)
        asm volatile("mov cr3, %0" : : "r"(aPdPhys) : "memory");
    }

TUint PdCurrent()
    {
    TUint cr3;
    asm volatile("mov %0, cr3" : "=r"(cr3));
    return cr3;
    }

void PdMapUser(DProcess* aProc, TUint aVaddr, TUint aPhys, TUint aSize)
    {
    // The flat 4M identity PDE map already maps low RAM with U/S=1; recording
    // the range is enough today. Genuine 4K PTE U/S enforcement needs a 4K PT
    // per 4M region (4 MiB of PTs/PD) which the fixed pool cannot host without
    // whitelisting kernel-heap user code — see docs/r1_mmu_feasibility.md.
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        if (sProcs[i].iProc == aProc && sProcs[i].iPdPhys)
            {
            KPrintf("M-03: PdMapUser proc=%x v=%x p=%x len=%u (U/S=1 flat)\r\n",
                    (TUint)aProc, aVaddr, aPhys, aSize);
            return;
            }
    }

void PdMapData(DProcess* aProc, TUint aVaddr, TUint aPhys, TUint aSize)
    {
    // User data pages share the same U/S=1 flat-PDE treatment as code.
    PdMapUser(aProc, aVaddr, aPhys, aSize);
    }

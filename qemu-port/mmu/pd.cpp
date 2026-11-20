// pd.cpp — port-side 4K page-table ownership + per-page U/S (OS-CORE upgrade).
//
// kernelhwsrv/ is READ-ONLY. The boot PD (mmu/mmu_gas.S) is now a real 4K
// directory: PDE[0..31] point at STATIC 4K identity page tables covering the
// low 128 MiB (all of QEMU's 128 MiB RAM); PDE[32..1023] are 4M identity
// fallback PDEs (U/S=0) for high MMIO/APIC. A fixed, 4K-aligned BSS pool holds
// 16 per-process PDs; they point at the SAME shared PTs, so U/S is a global
// per-page policy (documented remainder: no private PTs => no per-process AS).
//
// U/S policy: PTEs start U/S=1 (old flat low-RAM behaviour, no regression);
// PdInit() clears U/S on known kernel-only pages (kernel text/rodata except the
// trampoline page, the kernel heap/data region at 0x1000000, the page pool and
// the boot PD/TSS pages). E32Run's PdMapUser() re-asserts U/S=1 on user ranges.

#include <e32err.h>
#include <kernel/kern_priv.h>
#include <kernel/kernel.h>
#include "pd.h"

// Genuine exception vector table (vectors.cia; non-const despite the comment).
extern "C" void (*TheExcVectors[64])();
extern "C" TUint qemu_cpu_features;
extern "C" TUint32 qemu_page_dir[1024];
extern "C" TUint32 qemu_page_tables[32 * 1024];
extern "C" TUint8 qemu_tss[];
extern "C" void Mmu_Status(void);   // EAX=CR0 EDX=CR3 ECX=CR4 (asm)
namespace QemuUart { void Puts(const char* aS); void PutHex(unsigned aV); }

namespace
    {
    const TInt  KMaxPdProcs = 16;
    const TUint KPdEntries  = 1024;   // 4 KiB / 4 bytes
    const TUint KNumPt      = 32;     // 32 x 4 KiB PT = low 128 MiB
    const TUint KPdePtLimit = 32;     // PDE index where 4M fallback begins
    const TUint K4K         = 4096;

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

    // Fill one process PD: 4K PT PDEs for low RAM, 4M identity PDEs (U/S=0,
    // PS|G) for high MMIO. G is only valid when CR4.PGE was enabled by
    // Mmu_Enable (which checks the same CPUID feature bit).
    void FillPd(TUint32* aPd)
        {
        const TUint g = (qemu_cpu_features & 0x2000u) ? 0x100u : 0u;
        for (TUint n = 0; n < KPdEntries; ++n)
            {
            if (n < KPdePtLimit)
                aPd[n] = ((TUint)&qemu_page_tables[n * KPdEntries]) | 0x07u;
            else
                aPd[n] = (n << 22) | 0x83u | g;
            }
        }

    // Set or clear the U/S bit on every 4K page in [aVirt,aVirt+aSize).
    // Ranges outside the shared-PT window fall back to flipping the U/S bit
    // of the coarse 4M identity PDE in the boot PD and every process PD.
    void SetUsRange(TUint aVirt, TUint aSize, TBool aUser)
        {
        if (!aSize)
            return;
        TUint end = aVirt + aSize;
        if (end < aVirt)                 // overflow: clamp to 4G
            end = 0xFFFFFFFFu;
        for (TUint p = (aVirt & ~(K4K - 1)); p < end; p += K4K)
            {
            TUint pdi = p >> 22;
            if (pdi < KPdePtLimit)
                {
                TUint32* pt = &qemu_page_tables[pdi * KPdEntries];
                TUint pti = (p >> 12) & (KPdEntries - 1);
                if (!(pt[pti] & 1u))     // not present: nothing to protect
                    continue;
                if (aUser) pt[pti] |= 0x04u;
                else       pt[pti] &= ~0x04u;
                }
            else
                {
                // Coarse fallback: one PDE covers a whole 4M region.
                TUint32 mask = aUser ? 0x04u : ~0x04u;
                if (aUser) qemu_page_dir[pdi] |= mask;
                else       qemu_page_dir[pdi] &= mask;
                for (TInt i = 0; i < KMaxPdProcs; ++i)
                    if (sProcs[i].iProc && sProcs[i].iPdPhys)
                        {
                        TUint32* pd = (TUint32*)sProcs[i].iPdPhys;
                        if (aUser) pd[pdi] |= mask;
                        else       pd[pdi] &= mask;
                        }
                }
            }
        // Flush non-global TLB entries so the new U/S bits take effect at once.
        TUint cr3;
        asm volatile("mov %0, cr3" : "=r"(cr3));
        asm volatile("mov cr3, %0" : : "r"(cr3) : "memory");
        }

    // Count supervisor-only PTEs in the 32 shared PTs (serial proof).
    TUint CountSupervisorPtes()
        {
        TUint n = 0;
        for (TUint i = 0; i < KPdePtLimit * KPdEntries; ++i)
            if ((qemu_page_tables[i] & 1u) && !(qemu_page_tables[i] & 0x04u))
                ++n;
        return n;
        }

    // Lock the kernel-only regions down to U/S=0. The trampoline page at
    // 0x18c000 (sTrampExit/sTrampPutc in .rodata) is deliberately EXCLUDED:
    // ring 3 calls those stubs. The EUSER.DLL code lives in kernel .bss
    // (0x2f3840..0x373840) and is also left U/S=1.
    void LockKernelPages()
        {
        SetUsRange(0x100000u, 0x18c000u - 0x100000u, EFalse); // kernel text+rodata
        SetUsRange(0x1000000u, 0x200000u, EFalse);            // kernel heap/data
        SetUsRange((TUint)qemu_page_tables, KPdePtLimit * K4K, EFalse);
        SetUsRange((TUint)&qemu_page_dir[0], K4K, EFalse);
        SetUsRange((TUint)qemu_tss, K4K, EFalse);
        }
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
        FillPd(pd);
        TProcPd& r = sProcs[i];
        r.iProc = aProc;
        r.iPdPhys = (TUint)pd;
        r.iCodeRun = aCodeRun;  r.iCodeLen = aCodeLen;
        r.iDataRun = aDataRun;  r.iDataLen = aDataLen;
        r.iStackRun = aStackRun; r.iStackLen = aStackLen;
        // Re-assert U/S=1 on the exact user ranges (4K granularity).
        PdMapUser(aProc, aCodeRun, aCodeRun, aCodeLen);
        PdMapUser(aProc, aDataRun, aDataRun, aDataLen);
        if (aStackLen)
            PdMapUser(aProc, aStackRun, aStackRun, aStackLen);
        KPrintf("PD: create proc=%x CR3=%x PDE[0]=%x PDE[8]=%x (4K PTs, not 4M)\r\n",
                (TUint)aProc, (TUint)pd, pd[0], pd[8]);
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
        // Zero the PD unless it is still live in CR3 (E32Reap can run with a
        // dying process's PD loaded — emptying it there would fault instantly).
        if (r.iPdPhys && PdCurrent() != r.iPdPhys)
            {
            TUint32* pd = (TUint32*)r.iPdPhys;
            for (TUint n = 0; n < KPdEntries; ++n)
                pd[n] = 0;
            }
        r.iProc = 0;
        r.iPdPhys = 0;
        r.iCodeRun = r.iCodeLen = 0;
        r.iDataRun = r.iDataLen = 0;
        r.iStackRun = r.iStackLen = 0;
        return;
        }
    }

// --- CR3 switching + #PF containment ---------------------------------------

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

// Resolve the owning process from the live CR3 (our side-table only — no
// kernel object walk in fault context). Returns -1 for the boot/kernel PD.
static TInt PdOwnerPid(TUint aCr3)
    {
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        if (sProcs[i].iProc && sProcs[i].iPdPhys == aCr3)
            return (TInt)sProcs[i].iProc->iId;
    return -1;
    }

static TBool PdAddrInUserRange(TUint aCr3, TUint aAddr)
    {
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        if (sProcs[i].iProc && sProcs[i].iPdPhys == aCr3)
            {
            if (aAddr >= sProcs[i].iCodeRun &&
                aAddr < sProcs[i].iCodeRun + sProcs[i].iCodeLen) return ETrue;
            if (aAddr >= sProcs[i].iDataRun &&
                aAddr < sProcs[i].iDataRun + sProcs[i].iDataLen) return ETrue;
            if (aAddr >= sProcs[i].iStackRun &&
                aAddr < sProcs[i].iStackRun + sProcs[i].iStackLen) return ETrue;
            return EFalse;
            }
    return EFalse;
    }

extern "C" void PdPageFaultC(TUint aCr2, TUint aErr)
    {
    const TBool user = (aErr & 4u) != 0;
    const TUint cr3 = PdCurrent();
    const TInt pid = PdOwnerPid(cr3);
    const TBool inUser = PdAddrInUserRange(cr3, aCr2);
    KPrintf("PD: #PF pid=%d cr2=0x%08x err=0x%02x user=%d inUserRange=%d cr3=0x%x\r\n",
            pid, aCr2, aErr, (TInt)user, (TInt)inUser, cr3);
    if (user)
        {
        // Panics the *current thread* (the owning process dies, machine lives).
        // Full EKA2 exception delivery to the owning DThread (via the genuine
        // TPagingExcTrap in ckernel.cia) is out of scope: that machinery is not
        // linked into this image and cannot be reached from the raw IDT stub
        // without kernelhwsrv edits. Documented in docs/r1_mmu_4k_upgrade.md.
        K::PanicKernExec(KErrGeneral);  // does not return
        }
    else
        Kern::Fault("PD-KernelPF", K::ESystemException);  // kernel bug
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
    // Demote known kernel-only pages to supervisor (4K granularity) before
    // any user process is created.
    LockKernelPages();
    TheExcVectors[14] = (void (*)())&PdPageFaultStub;
    TheScheduler.SetProcessHandler((TLinAddr)&PdProcessSwitch);
    KPrintf("PD: PdInit 4K paging: PD pool=%d procs, PT pool=%d pages, Vec14=%x, processHandler=%x\r\n",
            KMaxPdProcs, KNumPt, (TUint)TheExcVectors[14], (TUint)&PdProcessSwitch);
    PdDump4K("boot");
    }

// Direct-UART proof (KPrintf output is not yet wired at HwInit0 time).
// Prints CR3, CR0, PDE[0] (must be a PT phys | 7 with PS=0 — not a 4M 0x87
// PDE), the high 4M fallback PDE, and sample kernel/user PTE U/S bits.
void PdDump4K(const char* aTag)
    {
    TUint cr0, cr3, cr4;
    __asm__ __volatile__("call Mmu_Status" : "=a"(cr0), "=d"(cr3), "=c"(cr4) :: "memory");
    TUint32 pde0 = qemu_page_dir[0];
    TUint32 pdeHi = qemu_page_dir[1006];  // local APIC 4M region
    TUint32 pteKernel = qemu_page_tables[0x100];      // 0x100000 kernel text
    TUint32 pteUser = qemu_page_tables[8 * 1024];     // 0x2000000 user run
    QemuUart::Puts("PD: "); QemuUart::Puts(aTag);
    QemuUart::Puts(" CR3="); QemuUart::PutHex(cr3);
    QemuUart::Puts(" CR0="); QemuUart::PutHex(cr0);
    QemuUart::Puts(" PDE0="); QemuUart::PutHex(pde0);
    QemuUart::Puts(" PDEhi="); QemuUart::PutHex(pdeHi);
    QemuUart::Puts(" KPTE="); QemuUart::PutHex(pteKernel);
    QemuUart::Puts(" UPTE="); QemuUart::PutHex(pteUser);
    QemuUart::Puts(" supPtes="); QemuUart::PutHex(CountSupervisorPtes());
    QemuUart::Puts("\r\n");
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

void PdMapUserRange(TUint aVaddr, TUint aSize)
    { SetUsRange(aVaddr, aSize, ETrue); }

void PdMapKernelRange(TUint aVaddr, TUint aSize)
    { SetUsRange(aVaddr, aSize, EFalse); }

void PdMapUser(DProcess* aProc, TUint aVaddr, TUint aPhys, TUint aSize)
    {
    SetUsRange(aVaddr, aSize, ETrue);
    for (TInt i = 0; i < KMaxPdProcs; ++i)
        if (sProcs[i].iProc == aProc && sProcs[i].iPdPhys)
            {
            KPrintf("PD: PdMapUser proc=%x v=%x p=%x len=%u -> U/S=1 (4K)\r\n",
                    (TUint)aProc, aVaddr, aPhys, aSize);
            return;
            }
    }

void PdMapData(DProcess* aProc, TUint aVaddr, TUint aPhys, TUint aSize)
    {
    // User data pages get the same U/S=1 treatment as code.
    PdMapUser(aProc, aVaddr, aPhys, aSize);
    }

void PdMapKernel(DProcess* aProc, TUint aVaddr, TUint aSize)
    {
    SetUsRange(aVaddr, aSize, EFalse);
    KPrintf("PD: PdMapKernel proc=%x v=%x len=%u -> U/S=0 (4K)\r\n",
            (TUint)aProc, aVaddr, aSize);
    }

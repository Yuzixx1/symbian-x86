// nk_sched.cpp — genuine-named nanokernel scheduler harness (Phase 5).
#include "nk_sched.h"
#include "../console/shell.h"

extern "C" TUint qemu_boot_stack_top;

NThread* TScheduler::iCurrentThread = 0;
TUint8 TScheduler::iRescheduleNeededFlag = 0;
TUint64 TScheduler::iReadyBitmap = 0;
NThread* TScheduler::iReadyByPri[KNumPriorities] = {0};

TUint NTimerQ::iTickPeriodMs = 1;
volatile TUint NTimerQ::iTickCount = 0;

void TScheduler::Init()
    {
    iCurrentThread = 0;
    iRescheduleNeededFlag = 0;
    iReadyBitmap = 0;
    for (int i = 0; i < KNumPriorities; ++i) iReadyByPri[i] = 0;
    }

void TScheduler::AddReady(NThread* aThread)
    {
    aThread->iState = EReady;
    iReadyByPri[aThread->iPriority] = aThread;
    iReadyBitmap |= (1ULL << aThread->iPriority);
    }

NThread* TScheduler::HighestReady()
    {
    for (int p = KNumPriorities - 1; p >= 0; --p)
        if ((iReadyBitmap >> p) & 1ULL)
            return iReadyByPri[p];
    return 0;
    }

// Cooperative round-robin: highest READY thread != current (genuine would
// pick strictly-highest priority; equal-priority rotation matches EKA2
// timeslice behavior for the demo pair).
void TScheduler::Reschedule()
    {
    NThread* next = 0;
    for (int p = KNumPriorities - 1; p >= 0; --p)
        {
        if (!((iReadyBitmap >> p) & 1ULL)) continue;
        NThread* cand = iReadyByPri[p];
        if (cand && cand != iCurrentThread && cand->iState == EReady)
            { next = cand; break; }
        }
    if (!next)
        { iRescheduleNeededFlag = 0; return; }  // only current is ready: keep it
    NThread* prev = iCurrentThread;
    iCurrentThread = next;
    ++next->iSwitches;
    iRescheduleNeededFlag = 0;
    NThread_Switch(&prev->iSavedEsp, next->iSavedEsp);
    }

void TScheduler::YieldTo(NThread* aThread)
    {
    (void)aThread;
    iRescheduleNeededFlag = 1;
    Reschedule();
    }

static NThread* sThreadReg[8];
static TUint sThreadRegCount = 0;

TUint TScheduler::Register(NThread* aThread)
    {
    if (sThreadRegCount >= 8) return 0xFFFFFFFFu;
    sThreadReg[sThreadRegCount] = aThread;
    return sThreadRegCount++;
    }

NThread* TScheduler::ThreadAt(TUint aIdx)
    {
    if (aIdx >= sThreadRegCount) return 0;
    return sThreadReg[aIdx];
    }

const char* TScheduler::StateName(TInt aState)
    {
    switch (aState)
        {
        case EReady: return "READY";
        case ESuspended: return "SUSP";
        case EWaitFastSemaphore: return "WSEM";
        case ESleep: return "SLEEP";
        case EBlocked: return "BLOCK";
        case EDead: return "DEAD";
        case EWaitDfc: return "WDFC";
        default: return "?";
        }
    }

// Genuine TimesliceTick: quantum expiry requests a reschedule; the switch
// itself happens at the thread yield boundary (as here).
void TScheduler::TimesliceTick()
    {
    if (!iCurrentThread) return;
    if (iCurrentThread->iTimeslice > 0)
        --iCurrentThread->iTimeslice;
    if (iCurrentThread->iTimeslice == 0)
        {
        iCurrentThread->iTimeslice = 10;   // refill quantum (10ms)
        iRescheduleNeededFlag = 1;         // preemption requested by timer
        }
    }

void NTimerQ::Init1(TInt aTickPeriodMs)
    {
    iTickPeriodMs = (TUint)aTickPeriodMs;
    iTickCount = 0;
    }

// Called from IRQ0 at 1000Hz (cf. nk_priv.h:449 NTimerQ::Tick).
void NTimerQ::Tick()
    {
    iTickCount += 1;
    TScheduler::TimesliceTick();
    }

TUint NKern::TickCount() { return NTimerQ::iTickCount; }
void NKern::YieldTimeslice()
    {
    TScheduler::iRescheduleNeededFlag = 1;
    TScheduler::Reschedule();
    }

// IRQ0 -> nanokernel tick bridge (called by Irq0Handler_C in variant/irq.cpp).
extern "C" void NTimerQ_TickIrq(void) { NTimerQ::Tick(); }

// --- Phase-5 demo threads (8K stacks, equal priority => round-robin) ---
static TUint8 sStackA[8192] __attribute__((aligned(16)));
static TUint8 sStackB[8192] __attribute__((aligned(16)));
static NThread sThreadMain = {0, 0, EReady, 0, 0, 0, 10, 0};
static NThread sThreadA = {1, 10, EReady, 0, 0, 0, 10, 0};
static NThread sThreadB = {2, 10, EReady, 0, 0, 0, 10, 0};
static volatile TUint sIters = 0;
static volatile TUint sPreemptReqSeen = 0;

static void UartPuts(const char* s) { QemuUart::Puts(s); }
static void UartHex(TUint v) { QemuUart::PutHex(v); }

static void ThreadA(void* /*aArg*/)
    {
    for (TUint i = 0; i < 3; ++i)
        {
        UartPuts("EKA2-QEMU NKern Thread A: iter=0x");
        UartHex(i);
        UartPuts(" tick=0x");
        UartHex(NKern::TickCount());
        UartPuts(" yield->B\r\n");
        if (TScheduler::iRescheduleNeededFlag) sPreemptReqSeen += 1;
        NThread* prev = &sThreadA;
        TScheduler::iCurrentThread = &sThreadB;
        ++sThreadB.iSwitches;
        NThread_Switch(&prev->iSavedEsp, sThreadB.iSavedEsp);
        }
    UartPuts("EKA2-QEMU NKern Thread A: done, switch->main\r\n");
    NThread* prev = &sThreadA;
    TScheduler::iCurrentThread = &sThreadMain;
    NThread_Switch(&prev->iSavedEsp, sThreadMain.iSavedEsp);
    }

static void ThreadB(void* /*aArg*/)
    {
    for (TUint i = 0; i < 3; ++i)
        {
        // Simulate worker load while ticks preempt-request in background.
        volatile TUint work = 0;
        TUint t0 = NKern::TickCount();
        while (NKern::TickCount() - t0 < 50) { work += 1; }
        (void)work;
        sIters += 1;
        UartPuts("EKA2-QEMU NKern Thread B: iter=0x");
        UartHex(i);
        UartPuts(" tick=0x");
        UartHex(NKern::TickCount());
        UartPuts(" worked, yield->A\r\n");
        NThread* prev = &sThreadB;
        TScheduler::iCurrentThread = &sThreadA;
        ++sThreadA.iSwitches;
        NThread_Switch(&prev->iSavedEsp, sThreadA.iSavedEsp);
        }
    UartPuts("EKA2-QEMU NKern Thread B: done\r\n");
    // Return into ThreadA just after its switch call, so A can finish.
    NThread* prev = &sThreadB;
    TScheduler::iCurrentThread = &sThreadA;
    NThread_Switch(&prev->iSavedEsp, sThreadA.iSavedEsp);
    }

void NKernelPhase5::Start()
    {
    UartPuts("EKA2-QEMU NKern: scheduler init (64 pri, quantum 10ms)...\r\n");
    TScheduler::Init();
    NTimerQ::Init1(1);
    sThreadMain.iId = 0;
    sThreadMain.iStackTop = (TUint)&qemu_boot_stack_top;
    TScheduler::Register(&sThreadMain);
    TScheduler::Register(&sThreadA);
    TScheduler::Register(&sThreadB);
    sThreadA.iStackBase = (TUint)sStackA;
    sThreadA.iStackTop = (TUint)(sStackA + sizeof(sStackA));
    sThreadB.iStackBase = (TUint)sStackB;
    sThreadB.iStackTop = (TUint)(sStackB + sizeof(sStackB));
    sThreadA.iSavedEsp = NThread_InitStack(sThreadA.iStackTop, ThreadA, 0);
    sThreadB.iSavedEsp = NThread_InitStack(sThreadB.iStackTop, ThreadB, 0);
    TScheduler::AddReady(&sThreadA);
    TScheduler::AddReady(&sThreadB);
    UartPuts("EKA2-QEMU NKern: threads ready A(pri10) B(pri10), IRQs live, start...\r\n");
    TScheduler::iCurrentThread = &sThreadMain;
    NThread* prev = &sThreadMain;
    TScheduler::iCurrentThread = &sThreadA;
    ++sThreadA.iSwitches;
    NThread_Switch(&prev->iSavedEsp, sThreadA.iSavedEsp);
    // Back in main: both threads finished (A switched home).
    UartPuts("EKA2-QEMU NKern: back in main thread, iters=0x");
    UartHex(sIters);
    UartPuts(" A_switches=0x");
    UartHex(sThreadA.iSwitches);
    UartPuts(" B_switches=0x");
    UartHex(sThreadB.iSwitches);
    UartPuts("\r\n");
    if (sIters == 3 && sThreadA.iSwitches >= 3 && sThreadB.iSwitches >= 3)
        UartPuts("EKA2-QEMU MILESTONE: thread switch A->B->A OK under paging\r\n");
    else
        UartPuts("EKA2-QEMU MILESTONE: FAILED switch counts\r\n");
    // Phase 6: hand the console to the interactive shell (never returns).
    Shell::SpawnAndEnter();
    }

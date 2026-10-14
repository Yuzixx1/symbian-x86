// nk_sched.h — nanokernel scheduler harness (Phase 5).
// Genuine names/layouts reused (nkern/nk_priv.h, nkern.h, nkern/x86/nk_plat.h):
//   NThreadState enum values, KNumPriorities=64, SThreadStack field ORDER,
//   TScheduler::Reschedule/YieldTo/TimesliceTick + iRescheduleNeededFlag +
//   iCurrentThread, NTimerQ::Init1/Tick, NKern::TickCount/YieldTimeslice.
// Simplifications (documented): single shared address space (no MMU per-thread,
// no TSS.esp0 switch), cooperative+tick-requested switching (genuine defers the
// switch to unlock/yield boundaries too), no DFC/semaphore wait objects.
#ifndef __QEMU_NK_SCHED_H__
#define __QEMU_NK_SCHED_H__

#include "qemu_assp.h"

typedef unsigned long long TUint64;

static const TInt KNumPriorities = 64;

// Genuine NThreadBase::NThreadState values (nk_priv.h:48).
enum NThreadState
    {
    EReady = 0,
    ESuspended = 1,
    EWaitFastSemaphore = 2,
    ESleep = 3,
    EBlocked = 4,
    EDead = 5,
    EWaitDfc = 6,
    ENumNStates = 7
    };

// EXACT genuine layout (nkern/x86/nk_plat.h:59). ESP points at iCR0.
struct SThreadStack
    {
    TUint iCR0;
    TUint iEbx;
    TUint iEsi;
    TUint iEdi;
    TUint iEbp;
    TUint iGs;
    TUint iFs;
    TUint iReschedFlag;
    TUint iEip;
    };

struct NThread
    {
    TInt iId;
    TInt iPriority;      // 0..63, higher runs first (genuine: KNumPriorities)
    TInt iState;         // NThreadState
    TUint iSavedEsp;     // ESP of suspended SThreadStack image
    TUint iStackBase;
    TUint iStackTop;
    TUint iTimeslice;    // ticks remaining in quantum
    TUint iSwitches;     // times switched in
    };

struct TScheduler
    {
    static NThread* iCurrentThread;
    static TUint8 iRescheduleNeededFlag;
    static TUint64 iReadyBitmap;   // bit p = a ready thread at priority p
    static NThread* iReadyByPri[KNumPriorities];
    static void Init();
    static void AddReady(NThread* aThread);
    static NThread* HighestReady();
    static void Reschedule();            // switch to highest ready thread
    static void YieldTo(NThread* aThread);
    static void TimesliceTick();        // from NTimerQ::Tick
    static TUint Register(NThread* aThread);  // thread table for `threads`
    static NThread* ThreadAt(TUint aIdx);
    static const char* StateName(TInt aState);
    };

struct NTimerQ
    {
    static TUint iTickPeriodMs;
    static volatile TUint iTickCount;
    static void Init1(TInt aTickPeriodMs);
    static void Tick();                  // called from IRQ0, cf. nk_priv.h:449
    };

struct NKern
    {
    static TUint TickCount();
    static void YieldTimeslice();        // cf. nkern.h:945
    };

// Low-level context primitives (nk_ctx_gas.S, SThreadStack-exact).
extern "C" void NThread_Switch(TUint* aOldEspOut, TUint aNewEsp);
extern "C" TUint NThread_InitStack(TUint aStackTop,
                                  void (*aEntry)(void*), void* aArg);
extern "C" void ThreadExitTrap(void);    // fault if a thread returns

// Phase-5 demo entry, called from KernelMain after IRQ takeover.
namespace NKernelPhase5 { void Start(); }

#endif

/*
 * ca_test.cpp -- I-07 userland active-object scheduler fixture.
 *
 * Exercises the genuine-ABI CActive/CActiveScheduler C++ layer in e32base.h:
 *   CActive::CActive -> CActiveScheduler::Install/Add -> SetActive -> Start
 *   -> WaitForAnyRequest() blocks on the thread request semaphore
 *   -> a kernel timer request (the RTimer/CTimer executive beneath EUSER)
 *      writes iStatus and wakes the wait
 *   -> RunL() is dispatched through the active's vptr
 *   -> Stop() ends the loop -> Start() returns.
 *
 * The six markers match tools/mke32_ca_test.py (the genuine-ordinal PE2
 * fixture), so both worlds prove the same Start()/Stop() round trip.
 */
#include "e32base.h"
#include "runtime.h"

extern "C" int exec4(unsigned, unsigned, unsigned, unsigned, unsigned);

class CTimerActive : public CActive {
public:
    CTimerActive() : CActive(EPriorityStandard) {
        CActiveScheduler::Add(this);
    }

    void RunL() {
        print("ca_test: RunL() reached\n");
        CActiveScheduler::Stop();
        print("ca_test: Stop() called\n");
    }

    void DoCancel() {}

    /* EExecTimerCreate/EExecTimerAfter are the kernel timer device behind
     * EUSER::RTimer::After (ord 928); completion writes iStatus and signals
     * this thread, exactly like the ordinal-driven fixture. */
    void After(TInt aMicroseconds) {
        iStatus.iInt = KRequestPending;
        TInt timer = exec0(133);
        if (timer < 0) { iStatus.iInt = timer; return; }
        exec4(53, (unsigned)timer, (unsigned)&iStatus.iInt,
              (unsigned)aMicroseconds, 0);
        SetActive();
    }
};

int app_main() {
    CActiveScheduler sched;
    CActiveScheduler::Install(&sched);
    print("ca_test: CActiveScheduler::Install ok\n");

    CTimerActive active;
    active.After(50000);
    print("ca_test: Start() entered\n");

    CActiveScheduler::Start();
    print("ca_test: Start() returned\n");
    print("ca_test: clean exit\n");
    return 0;
}

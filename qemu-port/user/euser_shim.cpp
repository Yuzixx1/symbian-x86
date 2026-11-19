/*
 * euser_shim.cpp — static member definitions for EUSER/CBase shim.
 * C++98, -ffreestanding -fno-exceptions -fno-rtti.
 */
#include "e32base.h"
#include "e32cmn.h"
#include "ipc.h"

/* Static member definitions */
CActiveScheduler* CActiveScheduler::iScheduler = nullptr;
CBase* CleanupStack::iStack = nullptr;
TInt User::iTlsIndex = 0;

/* P-06 wait/exit-status shim. process_wait_exit() posts a genuine ProcessLogon
 * executive (123), lets the request semaphore block until the target's last
 * thread exits, and cancels the logon (124) if the timeout expires first. */
TInt User::WaitForExit(TUint aPid, TRequestStatus& aStatus, TInt aTimeoutMs)
    {
    TInt reason = QE_KREQUEST_PENDING;
    TInt type = 0;
    TInt r = process_wait_exit(aPid, &reason, &type, aTimeoutMs);
    (void)type;
    aStatus.iInt = r;
    return r;
    }

/* GCC emits a weak reference to this for abstract classes (CActive::RunL/
 * DoCancel are pure virtual). It must be defined for elf2e32's reloc check. */
extern "C" void __cxa_pure_virtual() { process_exit(KErrGeneral); }
/* ELeave is an enumerator in e32base.h; no out-of-line definition is needed. */

/* No hand-written RTTI/vtable arrays: the build uses -fno-rtti and the
 * compiler emits real vtables for CBase/CServer2/CSession2 and their derived
 * classes. Fake __ZTV/__ZTI symbols only confused the linker. */
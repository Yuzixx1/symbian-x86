/*
 * euser_shim.cpp — static member definitions for EUSER/CBase shim.
 * C++98, -ffreestanding -fno-exceptions -fno-rtti.
 */
#include "e32base.h"
#include "e32cmn.h"
#include "ipc.h"

/* Static member definitions */
CActiveScheduler* CActiveScheduler::iScheduler = nullptr;
CActive* CActiveScheduler::iActiveQ = nullptr;
CBase* CleanupStack::iStack = nullptr;
TInt User::iTlsIndex = 0;
/* ELeave is an enumerator in e32base.h; no out-of-line definition is needed. */

/* No hand-written RTTI/vtable arrays: the build uses -fno-rtti and the
 * compiler emits real vtables for CBase/CServer2/CSession2 and their derived
 * classes. Fake __ZTV/__ZTI symbols only confused the linker. */
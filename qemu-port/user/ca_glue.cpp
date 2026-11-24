/*
 * ca_glue.cpp — PE2 conversion glue for the ca_test fixture (2026-11-24).
 *
 * Replaces the retired user/euser_shim.cpp as the last consumer of its
 * out-of-line symbols (svcmgr.elf is gone since dd22ae2).  It provides:
 *
 *   1. web_euser_printf() — the C entry point user/ca_test.c calls under
 *      -DWEB_USE_EUSER.  It tail-calls the genuine EUSER.DLL ordinal 915
 *      (RDebug::Printf) through the PE2 IAT slot web_rdebug_printf_slot,
 *      a SHN_ABS symbol at 0x408000 (codeBase+IAT_OFF) defined by
 *      --defsym=web_rdebug_printf_slot=0x408000 on the ca_test.elf link.
 *      elf2e32.py --euser-import 915 seeds the slot with the 1-based ordinal
 *      and the E32 loader rebinds it to the real export before resume —
 *      the same path as the ~40 other user E32s (selftest, textedit, ...).
 *   2. __cxa_pure_virtual — GCC emits a weak reference for abstract classes
 *      (CActive::RunL/DoCancel); it must be defined for elf2e32's reloc
 *      check.  Same body as the deleted shim.
 *   3. User::WaitForExit — P-06 wait/exit-status helper, moved verbatim out
 *      of the deleted euser_shim.cpp (genuine ProcessLogon execs 123/124).
 *
 * The static members (CActiveScheduler::iScheduler, CleanupStack::iStack,
 * User::iTlsIndex) are deliberately NOT defined here: no linked image
 * references them anymore.
 *
 * C++98, -ffreestanding -fno-exceptions -fno-rtti (USER_FLAGS).
 */
#include "e32base.h"

extern "C" {

/* PE2 IAT slot for EUSER.DLL ordinal 915 (RDebug::Printf); SHN_ABS at
 * 0x408000 via --defsym on the link line.  Reading it yields the bound
 * export address once the loader has patched the slot. */
extern unsigned int web_rdebug_printf_slot;

void web_euser_printf(const char* text)
{
    typedef void (*TPrintf)(const char*);
    unsigned int fn = *(volatile unsigned int*)&web_rdebug_printf_slot;
    ((TPrintf)fn)(text);
}

/* GCC emits a weak reference to this for abstract classes (CActive::RunL/
 * DoCancel are pure virtual). It must be defined for elf2e32's reloc check. */
void __cxa_pure_virtual() { process_exit(KErrGeneral); }

} /* extern "C" */

/* P-06 wait/exit-status helper. process_wait_exit() posts a genuine
 * ProcessLogon executive (123), lets the request semaphore block until the
 * target's last thread exits, and cancels the logon (124) if the timeout
 * expires first. */
TInt User::WaitForExit(TUint aPid, TRequestStatus& aStatus, TInt aTimeoutMs)
{
    TInt reason = QE_KREQUEST_PENDING;
    TInt type = 0;
    TInt r = process_wait_exit(aPid, &reason, &type, aTimeoutMs);
    (void)type;
    aStatus.iInt = r;
    return r;
}

# R1 reliable wait / exit-status design (v1) — P-06 + P-08

Status: investigation only, no source changes. kernelhwsrv is READ-ONLY.

## Current state

**User exit path (works).** `_start` (user/start.S:3-7) calls `app_main`, then
`process_exit` (start.S:67-74): EBX=code, ECX=0xffff8001 (KCurrentThread),
EDX=ESI=0, EAX=115 (`EExecThreadKill`), `int $0x21` (SLOW_EXEC4). Kernel side
`ExecHandler::ThreadKill` (server.cpp:835) -> `Kern::ThreadKill` -> `DThread::Die`
(sthread.cpp:855) -> `SetExitInfo` (sthread.cpp:1661) stores iExitType /
iExitReason / iExitCategory, then `NKern::ThreadKill`. `DThread::Exit`
(sthread.cpp:714) sets `iExiting` early (:761) and completes target logons via
`TLogon::CompleteAll(iTargetLogons, TLogon::ETarget, iExitReason)` (:802).
`SvKill` (sthread.cpp:882) copies the last thread's exit info into the process
(:910-916) and completes `pP->iTargetLogons` (:919). `DProcess::Die`
(sprocess.cpp:528) stores process exit info under the process lock and
`KillAllThreads` (:577); `DProcess::Logon` (:707) attaches TLogon::ETargetProcess.

**Wait primitive (works, genuine).** `CActiveScheduler::WaitForAnyRequest`
(user/e32base.h:144) -> `User::WaitForAnyRequest` (:66) -> `ipc_wait_any`
(start.S:10-14: `xor eax,eax; int $0x20`) -> fast exec 0
`EFastExecWaitForAnyRequest` -> `NKern::WaitForAnyRequest`
(nkern/nkern.cpp:442) -> `pC->iRequestSemaphore.Wait()` (:448), a real blocking
kernel semaphore. Completion: `Kern::RequestComplete` ->
`NKern::ThreadRequestSignal` -> semaphore Signal.

**What is missing (P-06).** No user-mode wait/exit-status API. The genuine
user layer `RThread::Logon` / `RProcess::Logon` (euser/us_ksvr.cpp:3347-3427,
3963-4042) and `ExitType/ExitReason/ExitCategory` map to execs that the kernel
implements (`ExecHandler::ProcessLogon/ThreadLogon` + Cancel, sexec.cpp:1137-1204;
getters at :323-353, :733-763). The generated `build/export/exec_user.h`
declares them as SLOW_EXEC3: `EExecThreadLogon=116`, `ThreadLogonCancel=117`,
`ProcessLogon=123`, `ProcessLogonCancel=124` (`exec_enum.h:178-187`). The
port's user runtime (e32base.h, runtime.c) exposes none of these — no RThread /
RProcess logon veneer, no `User::WaitForExit`. So the kernel execs are unwired
to user. NOTE: parent brief said "execs 24/25"; in this tree 24/25 are fast
atomics (Axo64/Cas64), the logon execs are 116/117/123/124 — reconcile.

**Exit ring (weak).** `variant/e32load.cpp`: `sExits[16]` (:629),
`ExitRecRecord` (:632), `E32FindExit` (:1044). Records are written ONLY from
`E32Reap` (:963, :999-1004), i.e. post-mortem. Shell `wait id [secs]`
(variant/kern_shell.cpp:~1239) polls `E32ProcessDead` (:1023) with `hlt`.
Recording at reap time leaves a window where a death is not yet observable and
risks losing completions under concurrent load (P-08).

**Tests.** `tools/test_wait.py` covers wait-timeout, unknown id, kill record,
reason=0. `tools/test_conc.py` covers server+clients+shell, kill recovery,
exit ring. Neither covers blocking wait-before-exit, multiple concurrent
waiters, post-reap lookup, multi-process stress, or lost completions.

## Concrete next steps

1. Add user exec shims in start.S/runtime.c for `ThreadLogon`/`ProcessLogon`
   (116/123) + Cancel (117/124) and the ExitType/Reason/Category getters, using
   a genuine-shaped SLOW_EXEC3 frame (TRequestStatus* + TBool, handle in arg1).
2. Add `User::WaitForExit(pid, &status, timeout)`: post a stack TRequestStatus
   via ProcessLogon, block in `WaitForRequest` (existing int 0x20 path), read
   iExitType/iExitReason on wake; support timeout via LogonCancel + timer.
3. Move `ExitRecRecord` into the death path under critical section (DThread::Exit
   / SvKill or a variant death hook) instead of only E32Reap, so status is
   reliable before reap and cannot be lost.
4. Strengthen `test_wait.py`: wait issued before target exits (blocking waiter
   released on death), several concurrent waiters on one pid, post-reap lookup.
5. Strengthen `test_conc.py`: multi-process concurrent create/exit, assert every
   waiter is released (no lost completions), no `Exc `/`FAULT:` under load.

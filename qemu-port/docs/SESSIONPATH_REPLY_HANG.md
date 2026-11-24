# SessionPath reply hang — root cause and minimal port fix

Date 2026-11-24 ~03:42 EET. Observer subagent. `kernelhwsrv/` READ-ONLY and
untouched; no QEMU launched, no rebuild, no commit/push, no gate edit. Only this
report was written. Evidence is from existing raw logs in
`~/.cache/sybian-r1/efile-reentry-accept{3,4,5}/`, `efile-final-{prove,accept}/`
and read-only source inspection.

## 0. Bottom line

The last 8/9 criterion is **not** in the re-entry guard, the reply exec, or the
scheduler. The genuine F32 request dies inside
`CFsMessageRequest::Dispatch()` at **`FsPluginManager::ReadLockChain()`
(`RReadWriteLock::ReadLock`)**: it never returns in this x86 port, so
`DoRequestL`/`Complete` are never reached and no `SessionPath r=` is ever
printed. The port fix already landed in the owner's 03:39/03:40 build —
a no-op chain-lock shim in `genuine/efile/patched/sf_request.cpp` — and the
observation is independently visible: accept4 stops exactly at
`before ReadLockChain`, accept5 and final-prove/final-accept print
`SessionPath r=0` and `IPC round trip OK` with a 9/9 strict count.

**Remaining risk (not fixed by this report):** the no-op shim is compiled only
when `EFILE_DEBUG_MAIN` is set (`genuine/build_efile.py:82,111`). A default
build uses real `sf_request.o` and will hang again. Make the port patch
unconditional (or rename it from "debug" to a permanent port patch).

## 1. Log evidence: where the request dies

| Log | Exact tail markers | Meaning |
|---|---|---|
| accept3 (03:34 build) | `:1370 EFILE-DBG: ServiceL func 0xe main=1`; no `after Dispatch`; then `e32run: resumed`, `f32client started`, `Null calling Idle()` | inside `pR->Dispatch()` |
| accept4 (DBG2 build, no shim) | `:1372 Dispatch enter state=0`, `:1374 Dispatch after DoInitialise r=0 state=2`, `:1376 Dispatch before ReadLockChain`, **then nothing** | exact hang site = `ReadLockChain()` |
| accept5 (shim + DBG2) | `:1378 after ReadLockChain`, `:1380 before DispatchSync/Drive state=2`, `:1382 ProcessDriveOperation before DoRequestL`, `:1384 after DoRequestL err=0 leave=0`, `:1386 after DispatchSync`, `:1388 ServiceL ... after Dispatch`, `:1390-91 SessionPath r=0` ×2, `:1392-93 IPC round trip OK` ×2 | reply path intact once lock is bypassed |
| final-prove / final-accept (shim, DBG2 removed) | `SessionPath r=0` ×2, `IPC round trip OK` ×2 | fix survives cleanup |

Strict count in accept5 and final-accept (criteria at
`tools/r1_efile_acceptance.py:77`): start=1, registered=1, returned=0, `#PF`=0,
port-shim=0, `IPC round trip OK`=2, `SessionPath r=-15`=0, Exc/Panic/FAULT=0,
boot=1 → **9/9**.

Corroboration that the generic IPC path is fine: the client reached opcode 14,
therefore `EConnect` (`ipc_call(handle,-1,1)`, `fsclient.cpp:89`) already
completed through exec 65 (`EExecMessageComplete`, `exec_enum.h:128`) →
`Kern::QueueRequestComplete` (`kernelhwsrv/kernel/eka/kernel/sipc.cpp:1729`),
and `ipc_wait_for`/`ipc_wait_any` (`user/ipc.c:19-29`, `user/start.S:11-15`)
woke correctly. `IsMainThread()=1` in `ServiceL` proves the scheduler delivered
the message on the main server thread, so "scheduler not running" is false.

## 2. Ranked hypotheses

1. **CONFIRMED — `FsPluginManager::iChainLock.ReadLock()` deadlocks/spins.**
   `genuine/efile/patched/sf_request.cpp:1543` (before patch: genuine
   `kernelhwsrv/.../sfile/sf_request.cpp:1541`; also `:1033,1081,1095,1168,1484,1748`)
   takes the plugin-chain read lock from `Dispatch()`;
   `ReadLockChain()` is `iChainLock.ReadLock()`
   (`kernelhwsrv/.../sfile/sf_plugin_man.cpp:584-587`), a `RReadWriteLock`
   created at `sf_plugin_man.cpp:102`. Its implementation loops on a 64-bit CAS
   and may block on `iReaderSem.Wait()`
   (`kernelhwsrv/kernel/eka/euser/us_rwlock.cpp:84-106`). No plugin thread exists
   to hold a writer, and EUSER absolute low-address TLS (`0x110010`) is
   supervisor-only in this port (`docs/EFILE_REENTRY_FIX.md` residual,
   `mmu/pd.cpp` U/S=0), so the wait/`WaitForRequest` completion never resolves.
   accept4's last marker is exactly this call.
2. **Secondary — reply silently skipped if message handle is 0/local.**
   `patched/sf_request.cpp:1298-1303` guards `Message().Complete()` with
   `(msgHandle != KLocalMessageHandle) && (msgHandle != 0)`. Would produce a
   hang with no `r=` line, but accept5 disproves it: with #1 removed the same
   code replies `r=0`. Note if a future change zeroes `iMessage.iHandle`
   (the client-side `ipc_complete()` at `user/ipc.c:74-78` does, but the genuine
   server never calls it).
3. **Ruled out — request TLS/heap or dead scheduler.** `ServiceL func 0xe
   main=1` and the successful EConnect reply prove the EFILE main thread is
   live and scheduled; `Null calling Idle()` shows the kernel idle loop runs,
   i.e. the server thread is parked, not faulting (`#PF=0`, port-shim=0).
4. **Ruled out — client descriptor width / exec-77 claim-guard handle.**
   `flags=5` + `IpcBuffer16{0x20000000,32,...}` (`genuine/f32_probe/fsclient.cpp:106-109`)
   is the same descriptor that EFSRVMIN replies to with `r=0`; accept5 shows
   genuine `DoRequestL err=0 leave=0` with it. Re-entry suppression
   (`genuine/efile/efile_glue.cpp:151-159`) leaves exactly one `E32Main`, so no
   competing owner invalidates the handle. Do **not** re-add the rejected
   trampoline.

## 3. Minimal port-side fixes (file:line + owner)

- **A — landed, make permanent (EFILE glue/build owner).**
  `genuine/efile/patched/sf_request.cpp:2` defines
  `#define EFILE_PORT_NOOP_LOCK() ((void)0)` and replaces every
  `FsPluginManager::ReadLockChain()/UnlockChain()` in that TU (`:1035,1039,
  1045,1083,1088,1097,1104,1111,1170,1177,1181,1486,1492,1495,1543,1574,1580,
  1748,1750`). Safe here: the port has no plugin threads and one EFILE server
  thread, so the chain needs no synchronization. Make it unconditional in
  `genuine/build_efile.py:82,111` (drop the `EFILE_DEBUG_MAIN` gate or add a
  dedicated `sf_request_port.o`) so `make isogrub` cannot silently revert.
  Keep `kernelhwsrv/` read-only; patch is side-by-side, never in-tree.
- **B — only if real synchronization is required (kernel/TLS owner).**
  Fix `RReadWriteLock::ReadLock()`'s wait/CAS path or map EUSER TLS user-accessible
  per process (`mmu/pd.cpp` / EUSER loader). Larger, explicitly a separate
  approved task; not needed for the 9/9 gate.
- **C — unchanged (re-entry owner).** Keep guard-only suppression in
  `efile_glue.cpp:151-159`; no trampoline.

## 4. One discriminating test (exact markers)

Re-add the DBG2 prints to `genuine/efile/patched/sf_request.cpp` around lines
`1518,1528,1543,1545,1547,1599,1604,1128,1130` (as in accept4/5), rebuild
EFILE with the shim, boot once via `tools/r1_efile_acceptance.py`, and require
this sequence in one raw log:

```
EFILE-DBG: ServiceL func 0xe main=1
EFILE-DBG2: Dispatch enter state=0
EFILE-DBG2: Dispatch after DoInitialise r=0 state=2
EFILE-DBG2: Dispatch after ReadLockChain        <-- absent in accept4, present in accept5
EFILE-DBG2: ProcessDriveOperation after DoRequestL err=0 leave=0
EFILE-DBG: ServiceL func 0xe after Dispatch
F32CLIENT: SessionPath r=0
F32CLIENT: IPC round trip OK
```

Decision table: stop at `before ReadLockChain` ⇒ #1 returns (lock shim missing
or ineffective); reach `after DoRequestL` but no `r=` ⇒ #2; `Send77 r<0` or
`r=-15` ⇒ server-lifetime regression, not this bug.

**GREEN criteria:** fresh full run prints `SessionPath r=0` before `IPC round
trip OK`, and strict checker reports `RESULT: PASS (9/9 ...)` with
`F32CLIENT: IPC round trip OK >= 1`, `SessionPath r=-15 == 0`, start=1,
returned=0, `#PF=0`, port-shim=0, Exc/Panic/FAULT=0. Already observed in
`~/.cache/sybian-r1/efile-reentry-accept5/efile_acceptance.log` and
`.../efile-final-accept/efile_acceptance.log`; re-verify after making fix A
unconditional.

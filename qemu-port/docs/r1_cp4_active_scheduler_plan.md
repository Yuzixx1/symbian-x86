# R1 Checkpoint 4 — Genuine `CActiveScheduler` / `CActive` bring-up

Read-only research document. Grounded in `kernelhwsrv/` (untouched) and `qemu-port/`.
Nothing here changes source; every claim cites a file.

## 1. What already exists (baseline)

| Artefact | Evidence |
|---|---|
| `portfilesrv` runs as a genuine user process | `variant/e32load.cpp` `E32Run()` — `ProcessCreate r=0`, `Loaded r=0`, `Resume()`; smoke log `/tmp/opencode/r1_qemu_smoke.log` |
| Genuine EUSER.DLL (2503 exports) loaded resident | `E32LoadDll r=0`, `exports=2503`; binary `/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll` (412104 B) |
| PE2 import resolution by ordinal | `e32load.cpp` PE2 branch: IAT slot `textSize+4*k` holds 1-based ordinal; binds `sDllExp[ord-1]`. Verified: slot 672 ord 1221 → `002562c8`, slot 676 ord 1953 → `0025994a` |
| Automated smoke test | `tools/r1_qemu_smoke.py`, 10 markers, exit 0 |

Open defect inherited into CP4: the `portfilesrv` fixture still faults `Exc 06 EIP=3` right after `Resume()` because it bakes absolute PE addresses (`0x402000`, `0x403040`/`0x40303c`) with `codeRelocOff=0`. A flat, reloc-fixed fixture is being produced separately (`r1-portfilesrv-flat-v1`).

## 2. Real CActive / CActiveScheduler sources in `kernelhwsrv`

| Symbol | File:line | Notes |
|---|---|---|
| `class CActive : public CBase` | `kernelhwsrv/kernel/eka/include/e32base.h:1541` | pure-virtual `RunL()`, `iStatus` (`TRequestStatus`), `iPriority`, `iActive` |
| `class CActiveScheduler : public CBase` | `kernelhwsrv/kernel/eka/include/e32base.h:2788` | static `Install`/`Current`/`Replace`/`Add`/`Start`/`Stop`/`RunIfReady`; virtual `WaitForAnyRequest()`; `TActiveQueue iActiveQ` |
| `class CActiveSchedulerWait` | `e32base.h:2863` | `Start()`/`AsyncStop()` |
| `CActiveScheduler::WaitForAnyRequest()` (virtual) | `e32base.h:2831` | `IMPORT_C virtual void WaitForAnyRequest();` — derived schedulers override to block |
| `User::WaitForAnyRequest()` | `euseru.def` ord 626 (`_ZN4User17WaitForAnyRequestEv @ 626`) | the global wait called by request-serving threads (e.g. `sfile/sf_thread.cpp:557`) |
| Kernel scheduler impl | `kernel/eka/nkern/x86/ncsched.cpp` (102 lines) | x86 half of `CActiveScheduler` machinery; the ARM twin `ncsched.cpp` has the active queue |
| `TRequestStatus` | `include/e32cmn.h` / `u32std.h` | request-status type used by `CActive::iStatus` |

Key architectural fact: `CActiveScheduler::WaitForAnyRequest()` is **virtual**, and `Start()` (ord 426) idles by calling it. A derived scheduler must implement the blocking wait — in real EKA2 that waits on the current thread's request semaphore. Our port has **no user-mode semaphore/wait primitive yet** (nothing in `qemu-port/user/` or `qemu-port/services/` mentions `WaitForAnyRequest`/`RequestSemaphore`).

## 3. Ordinals from `bx86gcc/euseru.def`

All of these are `@ N NONAME` — exported by ordinal, no name decoration needed on the import side.

**CActiveScheduler**
| Ordinal | Symbol |
|---|---|
| 428 | `CActiveScheduler::Install(CActiveScheduler*)` |
| 429 | `CActiveScheduler::Replace(CActiveScheduler*)` |
| 427 | `CActiveScheduler::Current()` |
| 426 | `CActiveScheduler::Start()` |
| 425 | `CActiveScheduler::Stop()` |
| 424 | `CActiveScheduler::Add(CActive*)` |
| 422 | `CActiveScheduler::RunIfReady(int&, int)` |
| 423 | `CActiveScheduler::WaitForAnyRequest()` |
| 418 | `CActiveScheduler::OnStarting()` |
| 419 | `CActiveScheduler::OnStopping()` |
| 1579 | `CActiveScheduler::Error(int) const` |
| 430 | `CActiveScheduler::CActiveScheduler()` |
| 432 | `~CActiveScheduler()` (D0/D1/D2 @ 432-434) |
| 1932 | vtable for CActiveScheduler |

**CActive**
| Ordinal | Symbol |
|---|---|
| 1091 | `CActive::CActive(int)` |
| 1090 | `CActive::SetActive()` |
| 1087 | `CActive::Deque()` |
| 1088 | `CActive::Cancel()` |
| 1089 | `CActive::RunError(int)` |
| 1086 | `CActive::SetPriority(int)` |
| 1092-1094 | `~CActive()` (D0/D1/D2) |
| 1940 | vtable for CActive |

**Supporting**
| Ordinal | Symbol |
|---|---|
| 626 | `User::WaitForAnyRequest()` |
| 1953 | `memcpy` (already bound by portfilesrv) |
| 1221 | `UserSvr::FsRegisterThread()` (already bound by portfileserver) |

Export verification command:
```
grep -nE "CActive|WaitForAnyRequest|memcpy|FsRegisterThread" \
  /home/yuzi/Documents/Sybian REAL/kernelhwsrv/kernel/eka/bx86gcc/euseru.def
```

Status: every symbol above is exported by ordinal in `euseru.def` → **no ordinal blockers** for CP4.

## 4. Dispatch problem — how `RunL` reaches fixture code

`CActive::RunL()` is pure virtual (`= 0`) on `CActive`. The scheduler calls it through `iActiveQ`, i.e. through the **object's vtable pointer**, not by ordinal.

Concretely:
1. `CActive::CActive(int aPriority)` (ord 1091) is called by fixture code. The real constructor stores `this` into `iActive` and inserts into the scheduler's queue only when `SetActive()` is later called.
2. The fixture derives a C++ class from `CActive` and overrides `RunL()`. The object's vptr therefore points at the fixture's vtable.
3. `CActiveScheduler::Start()` (ord 426) walks `iActiveQ` and calls `(*it)->RunL()` via that vptr → lands in fixture code automatically, **provided** the object lives in memory the CPU can execute from and the vtable pointer was set by EUSER's genuine `CActive` constructor.

What our fixture must do:
- link against genuine `EUSER.DLL` (import `CActive::CActive(int)`, `SetActive`, `Cancel`, `CActiveScheduler::Install`/`Add`/`Start`/`Stop` by ordinal),
- define a `CActive` subclass whose `RunL` posts a completion and calls `CActiveScheduler::Stop()`,
- construct it, `SetActive()`, `Install(&scheduler)`, `Start()`,
- from a second thread/timer complete the request so `WaitForAnyRequest()` returns and `Start()` exits.

The vtable for `CActive` (ord 1940) and `CActiveScheduler` (ord 1932) are exported but are **data**, not code — importing them by ordinal is unnecessary; the compiler emits the subclass vtable in the fixture itself.

## 5. What our port must already provide (gaps to close)

| Primitive | Needed for | Provided today? | Where to add |
|---|---|---|---|
| `CActiveScheduler::Start`/`Stop`/`Add`/`Install`/`WaitForAnyRequest` | scheduler loop | EUSER ordinals exist; EUSER.DLL loads | none — bind by ordinal |
| `CActive::CActive(int)`/`SetActive`/`Cancel`/`Deque` | active objects | EUSER ordinals exist | none — bind by ordinal |
| User-mode request semaphore / thread wait | `WaitForAnyRequest()` must block the scheduler thread | **NO** — nothing in `qemu-port/user/` or `services/` | new EUSER ordinal or kernel export |
| Second thread / timer to complete a request | prove async completion → `Stop()` | `portfilesrv` is single-threaded today | fixture spawns a 2nd thread or uses a timer ordinal |
| `memcpy` (ord 1953) | fixture internal copies | already bound | none |

The real blocker is the wait primitive. In genuine EKA2, `CActiveScheduler::WaitForAnyRequest()` (the derived implementation) waits on `Kern::RequestSemaphore`/`DThread::iRequestSem`, signaled by `DThread::RequestComplete` when an async request finishes. Our kernel's user-mode thread/exit path (`qemu-port/variant/e32load.cpp` `E32Run`) creates threads but there is no request-semaphore wait exposed to EUSER.

## 6. Minimal fixture spec (the CP4 proof)

Build a new E32 EXE `ca_test.exe` (PE2, single DLL import = EUSER, ordinals only):

```
Main thread:
  CActiveScheduler* sched = new CActiveScheduler();   // ord 430
  CActiveScheduler::Install(sched);                   // ord 428
  MyActive* a = new MyActive(EPriorityForeground);    // ord 1091, subclass
  a->SetActive();                                     // ord 1090 -> enqueued on iActiveQ
  CActiveScheduler::Start();                          // ord 426 -> blocks in WaitForAnyRequest

MyActive::RunL():                                     // fixture override, vtable dispatch
  // do work
  CActiveScheduler::Stop();                           // ord 425 -> Start() returns

Second thread (or timer) started before Start():
  sleeps briefly, then completes a->iStatus (KErrNone) and calls SetActive again,
  OR simply calls CActiveScheduler::Stop() directly to prove Start()/Stop() round-trip.

Expected serial markers (kernel log):
  "e32run: bind slot ..." for each ordinal
  "ca_test: CActiveScheduler::Install ok"
  "ca_test: Start() entered"
  "ca_test: RunL() reached"           <-- proves vtable dispatch into fixture
  "ca_test: Stop() called"
  "ca_test: Start() returned"
  "ca_test: clean exit"
```

Failure modes to watch for (all pre-identified, not speculative):
- `Exc 06` after `Resume()` — same stale-absolute-address class as portfilesrv; fix the fixture relocs first.
- `RunL` never reached — vptr not installed (constructor didn't run, or object in non-RWX/static buffer); check `E32Run` copies code and imports before `Resume()`.
- `Start()` returns immediately with no `RunL` — `iActiveQ` empty because `SetActive()` wasn't bound or `Add` never called (real `CActive::SetActive` calls `iScheduler->Add(this)` internally; verify the def ordinal actually does the enqueue).

## 7. Test scaffolding

Reuse `tools/r1_qemu_smoke.py` structure:
- disk: MBR + FAT32 at LBA 2048 (`@@1048576`), stage `ca_test` + `EUSER.DLL`
- QEMU: `q35 -m 128 -display none -serial stdio -kernel build/kernfull.elf -drive ...snapshot=on ... -device ich9-ahci ... -monitor none -no-reboot`
- feed `mount\n` then `ca_test\n`
- new script `tools/r1_cp4_ca_test.py` with markers listed in §6; exit 0 only if all pass

## 8. Ordered work breakdown (each step independently testable)

| Step | Work | Exit criterion |
|---|---|---|
| 1 | Verify ordinals 422-434, 1086-1095, 626 are present in a built `euser.dll` (not just the def) | `grep`/`nm` on `euser.dll` export table matches |
| 2 | Fix portfilesrv fixture relocs (flat 0x400000 image + reloc list) so `Resume()` no longer `Exc 06` | smoke test passes, no `Exc 06` |
| 3 | Add user-mode wait/semaphore primitive exposed to EUSER (kernel side in `qemu-port/` only; `kernelhwsrv` untouched) | `CActiveScheduler::WaitForAnyRequest()` can block a user thread |
| 4 | Write `ca_test` fixture: CActive subclass, `RunL`, `SetActive`/`Start`/`Stop` round-trip | serial shows `RunL()` reached and `Start()` returned |
| 5 | Add 2nd thread/timer that completes the request asynchronously | `RunL` runs after completion, not immediately |
| 6 | `tools/r1_cp4_ca_test.py` automated QEMU test, all markers pass | exit 0 |
| 7 | Re-measure hybrid-heap ceiling (EUSER code lives in a static RWX buffer; confirm no `Kern::Alloc(408144)` for scheduler code) | no `HEAP CORRUPTED` / `KERN-HEAP` on load |

## 9. Risks

- `CActiveScheduler::Start()` may block forever if `WaitForAnyRequest()` has no real wait → fixture must guarantee a completion or a `Stop()` from another context.
- `CActive::SetActive()` enqueues via `CActiveScheduler::Add(this)`; if our bound `Add` (ord 424) differs from the real one, `iActiveQ` stays empty. Verify by reading the real `SetActive`/`Add` implementation once the def ordinals are confirmed present in a built DLL.
- The scheduler's virtual `WaitForAnyRequest()` dispatch requires the installed scheduler object's vptr to point at a real `CActiveScheduler` vtable — constructing via the genuine `CActiveScheduler()` ctor (ord 430) handles this.

## 10. Definition of done for CP4

1. `tools/r1_cp4_ca_test.py` exits 0 (all markers).
2. Kernel log shows `RunL()` reached via vtable dispatch (marker from fixture code, not the loader).
3. No `Exc 06`/`KERN-HEAP`/`HEAP CORRUPTED` across the whole boot + `ca_test` run.
4. Ordinals 422-434, 1086-1095, 626 verified present in `euser.dll`.

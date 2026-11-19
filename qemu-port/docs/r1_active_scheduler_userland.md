# I-07 — Active-object scheduler in userland (design / read-only investigation)

## Done criteria (archived `docs/full_os_checklist.md`, retrieved from `dabf70b^`)
"I-07 Active-object scheduler in userland — TODO. Needed before any real service loop."
Order: S-04 → I-08 → I-07. Checkpoint-4 gate: Exc 06 gone, bound calls work end-to-end,
TLS assessed, exit/panic carried to shell `wait`, genuine-toolchain PE2 fixture + markers.
Note: the checklist file itself is currently absent from the worktree and HEAD.

## Current state (two separate worlds)
- Shim `user/e32base.h`: `CActive::RunL()` is **non-virtual**, there is no `iStatus`;
  `CActiveScheduler::Start()` calls `WaitForAnyRequest()` → `ipc_wait_any()` then **directly**
  calls `a->RunL()`; `Add` uses a singly-linked `iNext` queue. This is a stub, not genuine ABI.
- CP4 fixture `tools/mke32_ca_test.py` + `tools/r1_cp4_ca_test.py`: hand-built PE2 E32 that
  imports **genuine EUSER** ordinals and installs its **own** vtable. Its `Start()` is the real
  `CActiveScheduler::Start` (@426); wait is the genuine `int 0x20` fast exec →
  `NKern::WaitForAnyRequest`, woken by kernel exec 133 `TimerCreate` / 53 `TimerAfter`, and
  `RunL()` is dispatched by the scheduler through the object vptr. Six ordered markers prove
  Install → Start → RunL → Stop → return. So genuine scheduler + vtable dispatch + an async
  completion source **already work at exec level**; what is missing is the userland C++ layer.

## Genuine ABI/declarations (`kernelhwsrv/kernel/eka/include/e32base.h`)
`CActive : CBase` — protected ctor `CActive(TInt)`, `SetActive()`, pure virtual `RunL()`/`DoCancel()`,
public `iStatus`, `iLink` (TPriQueLink), friend `CActiveScheduler`. `CActiveScheduler : CBase` —
static `Install/Add/Start/Stop/RunIfReady/Current/Replace`, virtual `WaitForAnyRequest/Error`,
`TPriQue<CActive> iActiveQ`, private `DoRunL`. `ub_act.cpp:795,814,1083,1091-1108`:
`WaitForAnyRequest()` → `User::WaitForAnyRequest()`; `DoRunL` loops `WaitForAnyRequest()`, scans
`iActiveQ` for `IsActive() && iStatus != KRequestPending`, clears the flags, then `pR->RunL()`.
Ordinals (`bx86gcc/euseru.def`): scheduler 418–434 (422 RunIfReady, 423 WaitForAnyRequest,
424 Add, 425 Stop, 426 Start, 427 Current, 428 Install, 430/431 ctor); CActive 1086–1095
(1087 Deque, 1088 Cancel, 1090 SetActive, 1091 ctor); `User::RequestComplete` 617,
`User::WaitForAnyRequest` 626; CTimer 881–890 (881 ConstructL, 885 After, 887 ctor);
RTimer 924–930 (925 CreateLocal, 926 At, 928 After, 929 Cancel).

## Needed for a real service loop
1. RTimer/CTimer wrapper classes bound to the ordinals above (currently only raw exec 133/53 in
   `runtime.c:sleep_us`); 2. user thread creation — `runtime.c/h` has only `sleep_us`,
   `thread_yield`, `thread_park`, no thread API. Kernel path exists:
   `ExecHandler::ThreadCreate` (`sexec.cpp:1919`) → `DProcess::NewThread` (`sprocess.cpp:622`)
   → `GetNewThread` + `pT->Create` + `EUserThreadCreated`; `RThread::Create` @2405,
   `Kern::ThreadCreate` (`sexec.cpp:2022`). 3. genuine `Add`/`SetActive` calls and a real
   `iStatus` field; 4. `User::RequestComplete` (ord 617, signature `TRequestStatus*&`) as the
   completion producer.

## Concrete next steps
1. Bind CTimer/RTimer ordinals (887 ctor, 881 ConstructL, 885/928 After) in a new fixture.
2. Write `ca_test2` E32: install scheduler, CActive subclass with genuine vtable + `iStatus`,
   `Add`/`SetActive`/`Start`, complete via RTimer → `RunL` fires through vptr, `Stop`, exit.
3. Add markers + a `r1_cp4_ca_test.py`-style QEMU test; run it (not done in this read-only pass).

## Caveats found
- `user/e32cmn.h:56` defines `KRequestPending=0x80000000`, but genuine value is
  `-KMaxTInt = 0x80000001` (`e32const.h:977`) and `user/e32base.h:34` + CP4 fixture use
  0x80000001; the e32cmn constant is off by one.
- Shim `CActiveScheduler::Add` uses a plain `iNext` chain, genuine uses a priority queue —
  priority ordering (EPriorityIdle/High) is not represented in the shim.

# EFILE duplicate-launch analysis / port-side single-owner plan

Scope: read-only diagnosis plus a port-side guard experiment in
`variant/e32load.cpp`. `kernelhwsrv/` untouched. No commit, no push, no
`make clean`. Date 2026-11-24.

## 1. Root cause (corrected)

The "3x `EFILE: genuine file server starting` / 2x `E32Main returned 0`"
signature is **not three process launches**. In the original 1248-line
`/tmp/opencode/fallback_efile_genuine.log` there is exactly **one**
`DProcess::Create` for EFILE (`pid=9`, `proc=102cf10`) and exactly one
`e32run: ... Resume()`. The three starts are three executions of
`app_main()` inside that one process:

1. first thread: `starting` -> `File server thread registered` -> blocked
   in `E32Main`;
2. second entry: `starting` -> `E32Main returned 0`;
3. third entry: `starting` -> `E32Main returned 0`.

Genuine EKA2 x86 enters **every** user thread at the process entry veneer
(`kernelhwsrv/kernel/eka/kernel/x86/ckernel.cia:36-130`: EBX=0 first thread,
EBX=1 later threads; `SThreadCreateInfo` copied to the new user stack).
Genuine `_E32Startup`/`RunThread`
(`kernelhwsrv/kernel/eka/euser/epoc/x86/uc_exe.cia:29-54`,
`uc_exe.cpp:37-61`) dispatches reentries to `iFunction(iPtr)`. Our port's
minimal `user/start.S` ignores EBX and always calls `app_main()`, so the
genuine `RThread::Create` calls in `sfile/sf_main.cpp` re-enter `E32Main`.
Instrumented runs with a loader-installed entry trampoline confirmed two
reentries with `reason=1`, `iFunction=0x02a1a59c` and `0x02a20bf2`
(from genuine `Exec::ThreadCreate`, slow exec 104). This is a **entry-ABI
shim gap**, not a second boot launch.

Launch paths checked (no duplicate launch from these):

* `variant/e32load.cpp` `GenuineBootLaunch` already has a `genuineFs`
  guard for the EFILE/EFSRV slot and skips `SvcMgrBootLaunch` when a
  genuine FS started; a second guard for an already-published `!FileServer`
  was added.
* `genuine/prove_efile_genuine.py:34-49,83-94` stages the server under the
  alias `EFILEX.E32` (deliberately not auto-launched by the boot table),
  types `efilex` **once**, then `f32cli` once. It does **not** duplicate the
  launch. `F32CLIENT: start` appearing twice is the probe's dual output
  (`marker()` calls both the EUSER ord-915 printf slot and `print()`),
  not two client processes.
* `tools/r1_genuine_retire_test.py` EFSRV case stages `EFILE.E32` under its
  genuine name and relies on the boot-table auto-launch; it types
  `f32client` once. No duplicate process launch there either.
* Direct verification: booting the prove disk and typing `efilex` twice
  produced `single-owner ... launch refused` and only one
  `ProcessCreate r=0`, so a real second process launch is now refused.

## 2. Port-side fix implemented (launch guard)

`variant/e32load.cpp`:

* `PortIsEfileName()` recognises EFILE / EFILEX / EFILERFS / EFSRV basenames.
* `PortFileServerPublished()` checks `K::Containers[EServer]` for
  `!FileServer` under the container mutex.
* `PortEfileProcessRunning()` checks `K::Containers[EProcess]` for a live
  EFILE-family process.
* `E32Run()` refuses a second EFILE-family image with `KErrAlreadyExists`
  when either condition holds; the svcmgr shim fallback is untouched.
* `GenuineBootLaunch()` skips the file-server slot and the fallback when
  `!FileServer` is already published.

A concurrent writer replaced this region in `variant/e32load.cpp`
around 02:47 with a UID3-keyed atomic claim plus an entry trampoline
(`sPortOrigEntry`, `PortE32EntryTrampoline`) that dispatches reentries to
their genuine `iFunction`. That version is currently in the file and in
`build/kernfull.elf`. **This is an ownership conflict**: the brief assigned
`variant/e32load.cpp` exclusively to this agent, but another agent edited
the same file while this work was in flight. I did not overwrite their
version; the parent must reconcile.

## 3. Evidence / measured results

Build: `make build/kernfull.elf` (no `make clean`, kernelhwsrv untouched).

Launch-guard-only build (my version, before the concurrent rewrite):

* `genuine/prove_efile_genuine.py`: 464 binds PASS, 1x
  `File server thread registered`, port-shim 0, `IPC round trip OK` x2
  (dual output), no guest fault, 3x `starting` / 2x `E32Main returned`
  (intra-process reentries). `SessionPath r=-15` also appears on one of the
  dual-output paths, so the client's verdict is mixed; the gate marker
  `F32CLIENT: IPC round trip OK` is nevertheless present.
* `guard_duplicate_test`: two `efilex` commands -> one `ProcessCreate`,
  second refused by the guard.

Current (concurrent writer's trampoline) build:

* `genuine/prove_efile_genuine.py`: 464 binds PASS, 1x `starting`, 1x
  `thread registered`, 0x `E32Main returned`, 0 guest faults, but
  `EFILE: port-shim` x2 (`User::Panic reason=19` = `ELdrRestartSemaphore`,
  `reason=30` = `EMainCreateStartupThread3`), two reentries faulting at
  `0x00110010`, `F32CLIENT: SessionPath` never printed and no IPC round
  trip. Dispatching genuine thread functions without EUSER's
  `UserHeap::SetupThreadHeap` / `_E32Startup` setup does not work in this
  port. Killing reentries instead also hung EConnect. So the trampoline
  yields one owner and one start but **breaks the IPC proof**.

Required gates (current build):

* `python3 tools/r1_qemu_smoke.py` -> **PASS** (all 10 markers, no faults).
* `python3 tools/r1_genuine_retire_test.py` -> `efsrv FAIL`, `ewsrv PASS`,
  `estart PASS`, `euser PASS` (first full run had a truncated boot log for
  efsrv/euser; `--only efsrv,euser` re-run gave the results above).
  EFSRV FAIL reasons: missing `F32CLIENT: IPC round trip OK` with the
  gate's older `genuine/f32_probe/F32CLIENT.E32` client and
  `EFILE: port-shim count == 2`.

## 4. Blockers / recommendation

1. **Choose one implementation of `variant/e32load.cpp`.** The guard-only
   version preserved the IPC marker; the trampoline version achieves the
   1x-start marker but loses IPC. They need to be combined carefully:
   keep the UID3-keyed single-owner claim, drop or gate the reentry
   dispatch, and re-run prove.
2. The genuine `StartupThread`/loader threads cannot run in this port
   without EUSER's full per-thread init (`RunThread`) and ESTART/media
   drivers; panics 19/30 are from that gap. The entry-ABI shim belongs in
   `user/start.S` (not owned by this agent) if genuine reentries must run.
3. Prove script (`genuine/prove_efile_genuine.py`) needs no change: it does
   not duplicate the launch. The gate's older F32CLIENT image is a
   separate blocker for the retire test.
4. No shim was retired; `kernelhwsrv/` remains read-only.

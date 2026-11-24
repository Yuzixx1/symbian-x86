# EFILE single-owner / reentry trampoline fix

Date 2026-11-24. Owner: `variant/e32load.cpp` only. `kernelhwsrv/` untouched;
no gate-assertion edits; shim fallback path unchanged.

## Root cause

1. **Cross-launcher duplicate** (latent): boot table (`\EFILE.E32`), prove
   alias (`efilex`), and the `svcmgr` fallback could each start a process
   publishing the one protected `!FileServer` DServer. Losing owners tear the
   name down (`KErrServerTerminated`, F32CLIENT `r=-15`). The old guard
   (`e32load.cpp:779`) was a TOCTOU check outside CS and matched only 3 names,
   missing `EFILERFS`.
2. **Intra-process reentry** (observed): one E32Run enters `app_main` 3x
   because `user/start.S:3-8` ignores EBX (EKA2 entry reason), so each genuine
   `RThread::Create` (`sf_main.cpp:517`, `InitLoader` `sf_ldr.cpp:367`)
   re-runs `app_main`/`E32Main` instead of the new thread's own entry.
3. **Old magic-offset trampoline** read `iFunction` from the wrong slot and
   produced `E32ENTRY iFunction=00000000` -> `#PF` -> `port-shim 19/38`.

## Fix (file:line in `variant/e32load.cpp`)

- `:40` `KPortEfileUid3 = 0x100000bf`; `:62-65` `PortIsEfileImage()` keys on
  the genuine UID3 (covers all aliases incl. `EFILERFS`) with a name fallback,
  explicitly excluding EWSRV which shares the UID3.
- `:845-847` probe published name / live process unlocked; `:931` atomic
  check-and-claim `sPortEfileOwner` under `NKern::ThreadEnterCS()` (closes
  TOCTOU); failed `HBuf`/`ProcessCreate` paths release the claim.
- `:105-161` `PortE32EntryTrampoline`: first thread jumps to real `_start`;
  reentries read `iFunction`/`iPtr` from the user-stack `SThreadCreateInfo`
  copy using `_FOFF(SThreadCreateInfo,iFunction/iPtr)` (verified 0x08/0x0c in
  `objdump -d build/e32load.o`), call the genuine function, then exit via
  exec 115. A NULL `iFunction` is never called (no null-pointer `#PF`).
- `:1077-1088` install the veneer as `iEntryPtVeneer` for EFILE images only.
- `:1405` exec 270 op 5 logs `E32ENTRY: reentry ... iFunction=... iPtr=...`
  from ring 0, so the offset bug is directly observable.

## Verification evidence (raw serial logs)

D2 — `python3 genuine/prove_efile_genuine.py`,
`/tmp/opencode/fallback_efile_genuine.log`, kernel rebuilt with no `make clean`:
`e32run: single-owner: efilex.E32 uid3=100000bf claimed` =1;
`EFILE single-entry veneer` =1; `genuine file server starting` =1;
`File server thread registered` =1; `E32Main returned` =0;
`E32ENTRY ... iFunction=02a1a59c` and `iFunction=02a20bf2` (LoaderThread and
StartupThread) — **nonzero**; reentry no longer calls `app_main`.
`F32CLIENT: IPC round trip OK` =0; `EFILE: port-shim` =2; `#PF` =2
(`cr2=0x00110010`, `err=0x05`, `inUserRange=0`). No `r=-15` on this log.
`EXC_KEYS` check passed only because the harness looks for `MMU #PF` while
the kernel prints `PD: #PF` — the fault is real.

D1 — alias disk with both `\EFILE.E32` and `EFILEX.E32`
(`/tmp/opencode/d1_prove2.log`): boot claim =1, `efilex` refused =1
(`single-owner: ... already running or !FileServer published`),
1 start / 1 registered / 0 `E32Main returned`. IPC still blocked by the same
fault described below.

Gates: `tools/r1_qemu_smoke.py` PASS; `tools/r1_genuine_retire_test.py`
EFSRV/EWSRV/EUSER PASS (estart gate reported a sporadic `Exc 00` in
`r1_retire_estart.log`, unrelated to EFILE — no estart/ESHELL files edited).

## Residual blockers (not fixable inside `e32load.cpp` alone)

- Genuine EUSER's reentry path uses absolute low addresses for thread-local
  state (`0x100000`, `0x110010`, `0x210010`, `0x310010`, `0x120010`,
  `0x220010`, `0x320010`, and `0xf0000010` seen in earlier logs). In this
  port those addresses land in kernel text/rodata, which `mmu/pd.cpp:LockKernelPages`
  marks U/S=0, so the genuine `LoaderThread`/`StartupThread` take a user-mode
  write `#PF` on first TLS access; `InitLoader()` then fails and genuine
  EFILE calls `Fault(ELdrRestartInit=19)` / `Fault(...=30)`. The server thread
  therefore dies before it can serve the F32CLIENT session.
- Fixing this needs a port-side TLS mapping change (per-process page tables
  or a user-scratch relocation of EUSER absolute addresses), i.e. `mmu/pd.cpp`
  / EUSER loader ownership, outside this subagent's file ownership.
- `F32CLIENT: IPC round trip OK` >= 1 and port-shim=0 therefore remain FAIL;
  they were already 0/2 in the pre-fix baseline and are not a regression.
- Workspace race: another process rebuilt `build/kernfull.elf` from a stale
  snapshot of `variant/e32load.cpp` twice during this work; final source and
  binary were re-verified to contain all three fix blocks before handoff.

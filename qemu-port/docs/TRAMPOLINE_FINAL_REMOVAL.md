# EFILE entry trampoline — final removal (2026-11-24)

Scope: `variant/e32load.cpp` only. `kernelhwsrv/` untouched; no commit/push;
no `make clean`. A late agent re-added the broken EFILE thread-entry
trampoline after the earlier resolution; `AGENTS.md` now permanently forbids
it. This is the definitive removal, kept guard-only.

## Blocks removed from `variant/e32load.cpp`

1. **Entry trampoline definition** (old lines 108–157): the `EKA2 x86 entry
   ABI` comment, the globals `sPortOrigEntry`, `sPortReentryFn`,
   `sPortReentryPtr`, and the entire `PortE32EntryTrampoline()` asm veneer
   (EBX dispatch to `SThreadCreateInfo.iFunction`, exec-270 op 5, exec-115
   thread kill).
2. **Call site in `E32Run`** (old lines 1076–1087): the `if (isEfile)` block
   that stored `sPortOrigEntry`, issued two `PdMapUser(0, …)` mappings to make
   the veneer and its entry pointer user-executable, replaced
   `pinfo.iEntryPtVeneer` with the veneer address and printed
   `EFILE single-entry veneer`.
3. **exec-270 op 5 trace** in `SvcMgrCtlExec` (old lines 1401–1410): the
   `E32ENTRY: reentry reason=1 iFunction=… iPtr=…` KPrintf branch.

Verification of the source: 0 matches for `PortE32EntryTrampoline`,
`sPortOrigEntry`, `sPortReentry*`, `E32ENTRY`, `SThreadCreateInfo`.

## Blocks deliberately KEPT

* UID3 `0x100000bf` single-owner guard incl. EFILEX/EFILERFS/EFSRV aliases,
  EWSRV exclusion, published-name/process probes, atomic claim under NKern CS.
* `ECapabilityProtServ` grant; PE2 512 import slots; PTRINJ pointer bridge
  (`PortEwsrvPosHook`, ord-1868 rebinding, marker gate); exec-81
  `PortFsRegisterThread` wrapper clearing system-permanent/critical flags;
  `SvcMgrBootLaunch` shim fallback; legacy `sTrampExit`/`sTrampPutc` EUSER
  import stubs (unrelated to the EFILE thread-entry veneer — see
  `GUARD_TRAMPOLINE_REVIEW.md`).

## Rebuild

`make kernfull isogrub`, 2026-11-24 03:15:14–03:15:27 EET (no `make clean`).
`build/kernfull.elf` 03:15:15 (1025004 B), `build/symbian-grub.iso` 03:15:27
(75882496 B), source 03:15:01 — binary newer than source. No agent restored
the trampoline during the test window (re-checked mtime/grep afterwards).

ELF string audit: 0 hits for all six trampoline/reentry markers; present:
`single-owner:` (2), `claimed` (4), `ProtServ` (1), `PTRINJ` (4). `nm` shows
only `PortEwsrvPosHook` (PTRINJ) and genuine `ExecHandler::SetReentryPoint`.

## QEMU evidence — new kernel

`genuine/prove_efile_genuine.py` against the rebuilt `build/kernfull.elf`
(the prove stages its own FAT disk and boots the ELF with `-kernel`);
log `/tmp/opencode/trampoline_removal_prove_final.log`:

| Marker | Count | Verdict |
|---|---|---|
| `e32run: bind slot` | 464 (463 EFILE + 1 F32CLI) | PASS |
| `File server thread registered` | 1 | PASS |
| `E32Main returned` | 2 | accepted intra-process `RThread::Create` re-entry markers |
| `F32CLIENT: IPC round trip OK` | 2 | PASS (>=1) |
| `EFILE: port-shim` | 0 | PASS |
| `#PF` / `FAULT:` / `Panic` / `Abort` / `Exc ` | 0 / 0 / 0 / 0 / 0 | PASS |
| `single-owner:` claim / refusal | 1 / 0 | PASS |

`SessionPath r=-15` appears **2×** in this log — reported honestly, not
faked; another agent is investigating (see Blockers).

## Gate status

* `tools/r1_genuine_retire_test.py --log-dir /tmp/opencode/trampoline_removal_gates`
  — **EFSRV/F32: FAIL**, **EWSRV: PASS** (212 binds, no faults),
  **ESTART/ESHELL: PASS** (no `Exc 00` in this run — in-flight other-agent
  work has landed), **EUSER: PASS** (2503 exports in 3/3 logs).
  Overall `RESULT: FAIL (3 passed, 0 skipped, 1 failed)`.
  EFSRV fails solely on the missing `F32CLIENT: IPC round trip OK` marker:
  its client `genuine/f32_probe/F32CLIENT.E32` was rebuilt at 03:11:31 with
  strict `r == KErrNone` (fsclient.cpp:111-125), so the two `r=-15` replies
  print `SessionPath FAILED` instead. The prove uses the older permissive
  `/tmp/opencode/efile_client_bang.E32` (02:43) and therefore reports 2 round
  trips. This is a client-binary/status discrepancy, not a trampoline effect.
* `tools/r1_qemu_smoke.py` — **RESULT: PASS** (all loader/env markers, 2503
  exports, no Exc/PanicKernExec). Log `/tmp/opencode/trampoline_removal_smoke.log`.

## Blockers / hand-off

1. `SessionPath r=-15` ×2: unresolved genuine-EFILE/EUSER reply-status issue
   under investigation by another agent; the trampoline cannot fix it and
   must not be reintroduced for it (trampoline gives port-shim=2, #PF=2,
   IPC=0).
2. Retire EFSRV gate is FAIL because the staged client now requires `r == 0`.
   `genuine/` is not owned by this task; not modified here.
3. Logs for the prove end with the harness `terminate()` after `C:/>` + IPC,
   as in prior runs; no uninterrupted boot was obtained this cycle.

# SHIM_RETIREMENT_PLAN — auditable, dry-run-only retirement of the hand-written userspace

**Status:** planning / audit document. Nothing in this file or in
`tools/r1_shim_retire_plan.py` changes source. The eventual switch is a separate,
reviewed hand-applied change. `kernelhwsrv/` is read-only and untouched.

This is the *operational* companion to
[`GENUINE_USERSPACE.md`](GENUINE_USERSPACE.md) (the rule, the subsystem map and
the ordered plan in §4) and [`GENUINE_PROGRESS.md`](GENUINE_PROGRESS.md) (the
live gate verdicts and blockers B1–B7). The point of this document is that when
a gate finally passes, the switch is one reviewed step with a known rollback —
not a scramble.

## 0. Hard contract

`tools/r1_shim_retire_plan.py` is **DRY-RUN ONLY**. It asserts `DRY_RUN_ONLY` in
code, contains no `--apply` / `--delete` / `--edit` / `--commit` mode, never
opens a file for writing, never stages anything, and never launches QEMU. Its
only side effect is reading:

- the retire gate's verdicts, either by spawning
  `python3 tools/r1_genuine_retire_test.py --dry-run` (the gate's own dry-run
  path skips `stage_disk()` and `run_qemu()`), or by parsing a captured log via
  `--from-file`;
- the shim paths and boot-list lines it reports, via `Path.is_file()` /
  `Path.read_text()`.

READY means *only* "that subsystem's retire gate reported PASS". BLOCKED means
the gate reported SKIP or FAIL. Chain-level blockers (B1 client side, B3
follow-up, B5 plus the EFILE `-fshort-wchar`/`-freg-struct-return` rebuild fixing
`CServer2::Start` KErrBadName; B1 caps/B4/B6/B7 and the exec-81 teardown fix are
FIXED) and the manual acceptance checks are printed separately as an extra review
checklist; they never silently flip a verdict.

## 1. Inventory — four shims still in the production path

All paths are relative to `qemu-port/`. Line numbers refer to the tree audited
on 2026-11-24.

| Shim | Files (would be removed / bypassed, NOT touched by the tool) | What it provides | Genuine replacement | Gate | Other dependents / call sites |
|---|---|---|---|---|---|
| **portfilesrv** | `user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `services/r1_service_host.cpp`, `services/r1_service_host.h`, `:portfilesrv` (flat image) | Ring-3 owner of the hand-rolled `!FileServer` name; `CServer2::Start` with an injected in-memory `FatBackend` | `kernelhwsrv/userlibandfileserver/fileserver/`: genuine `efile.exe` (`sfile/sf_main.cpp` publishes `!FileServer`) + `efat32.fsy` + `efsrv.dll` | **EFSRV/F32** (`r1_genuine_retire_test.py gate_efsrv`) | `variant/e32load.cpp:1217-1229` `SvcMgrBootLaunch()` reads `\SVCMGR.E32`; `user/svcmgr.cpp:247-255` `kServices[]` launches `\PORTFILESRV.E32`; `variant/e32load.cpp:1304` fallback; `r1_genuine_retire_test.py` stages the prebuilt flat binary for the EWSRV gate |
| **r1_fs_* (core / server / client)** | `fs/r1_fs_core.{cpp,h,mmp}`, `fs/r1_fs_backend.h`, `fs/r1_fs_server.{cpp,h}`, `fs/r1_fs_fat.{cpp,h}`, `fs/r1_fs_channel*.{cpp,h}`, `fs/r1_fs_channel_state.h`, `fs/r1_fs_wire_logic.h`, `variant/r1_fs_channel.{cpp,h}`, `variant/fat32.cpp` (`FatReadPath`/`FatWritePath`/`FatMount`) | Private `R1Fs` wire opcodes (`FileOpen=30`, `FileRead=34`, …), `CServer2` server, client channel, in-memory FAT backend and kernel-side FAT glue | genuine `RFs`/`RFile` ordinals from `efsrv.dll` + real `TFsMessage` server `efile.exe`/`efat32.fsy`. The 13/13 opcode check is the ABI seam that makes the swap possible | **EFSRV/F32** | `user/svcmgr.cpp` includes `r1_fs_core.h` + `r1_fs_server.h`, defines `FatBackend : R1Fs::Backend`, calls `R1Fs::FileServer::NewL`; `Makefile:326-331` links `r1_fs_core.o`/`r1_fs_fat.o` into `KERNFULL_OBJS`; `tests/r1_fs_core/`, `genuine/efsrv_min/efsrv_min.cpp`, `services/r1_service_host.*` |
| **winsrv** | `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` (GUI execs 249/250/251, event ring 271/272/273) | Hand-written PE2 compositor and `gui_shared` display-list transport; not the EKA2 `RWindow`/`CWsScreenDevice` ABI | genuine `kernelhwsrv/kernel/eka/ewsrv/`: `ewsrv.exe` (`ws_main.cpp`) plus a QEMU `edisp` LDD/PSL | **EWSRV** (`gate_ewsrv`) | `user/{desktop,guishell,guihello,guimenu,guilib,breakout,saver,shot,dtcalc,dtedit,gcblittest,fbblittest}.c` and `user/wsclient.h` include `guishost.h` / `qemu_windowsrv.h`; `ports/doom/e32_platform.c` has the `guishost.h` GC_RECT fallback; `variant/qemu_variant.h:112,250`; `Makefile:258,261` |
| **svcmgr** | `user/svcmgr.cpp`; inside `variant/e32load.cpp`: `SvcMgrExecInstall`, `SvcMgrLaunchExec`, `SvcMgrKillExec`, `SvcMgrCtlExec`, `SvcMgrBootLaunch` and slow execs 268/269/270; the `SvcMgrExecInstall()` call in `variant/qemu_variant_asic.cpp:194` | Ordered startup / supervision / shutdown; repurposes three genuine test-only slow-exec slots to launch, kill and reap E32 images | genuine `fileserver/estart/` `e32strt.exe` + `fileserver/etshell/` `eshell.exe` + `domainmgr/` `domainsrv.exe`/`domainpolicy.dll`, launched from the boot image | **ESTART/ESHELL** (`gate_estart_eshell`) | `variant/e32load.cpp:1172,1222,1266` all go through `FatReadPath` + `E32Run`; `variant/e32load.cpp:1304` fallback; `variant/qemu_variant_asic.cpp:186-196`; `variant/e32load.cpp:22` `qemu_fbblit.h` appends new ops to exec 270 |

Adjacent (not one of the four): the **EUSER** gate covers removal of the
*static* `user/euser_shim.cpp` / `e32base.h` linkage (plan step 1). `EUSER.DLL`
itself is already genuine and resident; an EUSER PASS retires none of the four
shims above. The tool reports its verdict as context only.

### Boot-list edits — CORRECTED 2026-11-24, synced to `r1_shim_retire_apply.py`

The apply tool edits ONLY hand-written shim lines, by exact-text replacement
(each string asserts exactly one occurrence). It NEVER edits the genuine
`kGenuineBootList[]` entries (`\EFILE`, `\EFSRV`, `\ESTART`, `\E32STRT`,
`\EWSRV`, `\ESHELL`) or the genuine launch path (`GenuineBootSpawn()` /
`ShellSpawnFull()` in `qemu_variant_asic.cpp`): the retire gate needs those to
boot. The genuine chain is the target state, not something to disable.

| Gate / shim | Edit set | Boot-line edits actually made by `--apply` | Genuine lines explicitly KEPT |
|---|---|---|---|
| **EFSRV/F32** (`portfilesrv`, `r1_fs_*`) | `(1, 2)` | 1. `SvcMgrBootLaunch()` fallback in `variant/e32load.cpp` (tool records `:1304`, live `:1438`) — the `svcmgr -> \PORTFILESRV.E32` escape hatch. 2. `\PORTFILESRV.E32` service row at `user/svcmgr.cpp:250`. | `\EFILE.E32` + `\EFSRV.E32` stay enabled. |
| **EWSRV** (`winsrv`) | `()` | NONE. `winsrv.c`, `guishost.h`, `qemu_windowsrv.*` and the GUI execs are retired by **file removal** per §3 step 5; **human review** required — no apply token edits a boot line here. | `\EWSRV.E32` entry and the `GenuineBootSpawn()` / `ShellSpawnFull()` block stay. |
| **ESTART/ESHELL** (`svcmgr`) | `(3)` | 1. `SvcMgrExecInstall();` call at `variant/qemu_variant_asic.cpp:194`; the dead `SvcMgr*` handlers / execs 268–270 then go with the file. | `\ESTART.E32`, `\E32STRT.E32` and `\ESHELL.E32` stay enabled. |

An earlier draft disabled the genuine `EFILE/EFSRV/ESTART/ESHELL/EWSRV`
entries and rewrote the "skip condition"; that is WRONG and explicitly
rejected — see `docs/SHIM_RETIRE_APPLY_REVIEW.md` (HIGH #2). No skip-condition
line is edited. Line numbers are audit-time identifiers (the tool prints the
exact strings with their recorded locations); matching is by exact text, so
live line drift does not change the edit sets.

## 2. Current verdict: 0 READY / 4 BLOCKED

Captured `python3 tools/r1_genuine_retire_test.py --dry-run` (exit 0):

```
  efsrv    SKIP
  ewsrv    SKIP
  estart   SKIP
  euser    SKIP
RESULT: PASS  (0 passed, 4 skipped [not built yet], 0 failed)
```

All four shims are BLOCKED, even though the pre-G-01 full run once reported
EWSRV PASS, ESTART/ESHELL PASS and EUSER PASS:

1. **A dry-run never executes anything.** By design it reports BUILT and static
   checks only; runtime is SKIP. SKIP is explicitly "nothing to retire yet", so
   no dry-run can ever make a shim READY.
2. **The full chain is incomplete.** The EFSRV gate is SKIP even in a full run:
   `genuine/build/` has archives, probes, `EFSRVMIN.E32` and now a full
   `EFILE.E32` (273,068 B; 464 EUSER imports bound); with the port-side exec-81
   teardown fix it reaches `E32Main` (`File server thread registered`,
   `E32Main returned=0`) with **0 FAULT**. But `CServer2::Start("!FileServer")`
   returns **KErrBadName (-28)** — GDB proved the `_LIT` name descriptor is
   UTF-32 (4-byte chars), while prebuilt EUSER reads 16-bit chars; the
   `-fshort-wchar` rebuild is IN FLIGHT. `docs/EFILE_START_ANALYSIS.md`'s ranked
   cause #1 (EUSER return translation of a successful create) was superseded:
   exec 126/127 were never reached, and no shim collision occurred (SVCMGR.E32
   absent). There is still no runnable `EFILE.E32`, `EFAT32.FSY` or `EFSRV.DLL`;
   boot falls back to `svcmgr -> \PORTFILESRV.E32`. Partial markers are not proof.
3. **The full run now passes, but a dry-run still cannot certify it.** The
   `KERN 0x62` boot fault (B7) is FIXED — the genuine chain runs on a supervisor
   DThread from `Asic::Init3()` with `ECapabilityProtServ`, and the latest full
   run reports ewsrv PASS / estart PASS / euser PASS / efsrv SKIP (exit 0,
   0 faults). A dry-run by design SKIPs all runtime, so it never promotes a
   shim to READY.
4. **PASS alone is not sufficient for the later steps.** EWSRV external client
   sessions (`!E32WindowServer` name lookup, IN FLIGHT) and a ported `edisp`
   LDD plus window tests and an unchanged DOOM frame are still needed; EUSER
   PASS only governs the separate static-shim removal. The file-server data
   path is still blocked: full `EFILE.E32`'s `CServer2::Start("!FileServer")`
   returns KErrBadName from UTF-32 literals (`-fshort-wchar` rebuild IN FLIGHT
   in tools/genuine_build.py, genuine/f32_efsrv_build.py, genuine/build_efile.py,
   genuine/efat32/build_fsy_dll.py); B3's `Backend` follow-up is IN FLIGHT, and
   B5's ring-3 FAT adapter is OPEN. These are the "extra conditions" the tool prints.

**Latest reconciled status (2026-11-24):** no shim retired. A full run now has
EFSRV **FAIL / IN FLIGHT** (F32CLIENT fix in flight) and EWSRV/ESTART/EUSER PASS;
the dry-run above still reports all SKIP, so READY stays 0. Re-check §6 before
treating any of these numbers as current.

## 3. Ordered retirement sequence

Sequence is by dependency (a server goes only after the library its clients
bind is genuine). Execute one step at a time, re-run both the retire gate and
`python3 tools/r1_os_selfcheck.py` after each, and keep the previous step's
image/ISO for rollback.

0. **Baseline.** Record `r1_os_selfcheck.py`, `r1_doom_test.py`,
   `r1_window_system_test.py`, `r1_winsrv_test.py` results and the
   `symbian-grub.iso` hash.
1. **B7 / B4 — DONE.** The `KERN 0x62` boot fault is fixed: genuine chain runs
   on a supervisor DThread from `Asic::Init3()` with `ECapabilityProtServ`;
   EWSRV/ESTART/EUSER gates PASS in a full run with 0 faults. No longer a step.
2. **EFILE UTF-16 rebuild + B3 follow-up (both IN FLIGHT).** The exec-81 teardown
   fix LANDED (0 FAULT); add `-fshort-wchar` alongside `-freg-struct-return` to
   the GCC flags and rebuild (tools/genuine_build.py, genuine/f32_efsrv_build.py,
   genuine/build_efile.py, genuine/efat32/build_fsy_dll.py) so `_LIT` literals are
   UTF-16 and `TThreadId` returns in EDX:EAX, clearing the `CServer2::Start`
   KErrBadName (-28). Land non-virtual `Backend` methods in `fs/r1_fs_backend.h`;
   B1 capabilities and the const-descriptor root cause are already fixed.
   Remaining: EWSRV external client name lookup (IN FLIGHT).
3. **File server (retire `portfilesrv` and `r1_fs_*`).** Build/link and boot
   `EFILE.E32` + `EFAT32.FSY` + `EFSRV.DLL`; require a full-run EFSRV/F32 PASS
   with `F32CLIENT: IPC round trip OK` and `SvcMgr: boot launch r=0`, plus B5's
   ring-3 FAT adapter. Then remove the shim files; the ONLY boot-line edits
   are the `\PORTFILESRV.E32` row (`user/svcmgr.cpp:250`) and the
   `SvcMgrBootLaunch()` fallback (`e32load.cpp`) — genuine `\EFILE`/`\EFSRV`
   entries stay. Keep `R1Fs::Backend` only as a unit-test seam. Gate:
   `r1_fcopy_test.py`, `r1_largefile_test.py`, `tests/r1_file_regression/`, DOOM
   WAD still loads from FAT32.
4. **Startup/shell (retire `svcmgr`).** With the file server genuine, boot
   `E32STRT.E32`/`ESTART.E32`, `ESHELL.E32` and `dm_domainsrv.E32` from the
   image; require a full-run ESTART/ESHELL PASS. The ONLY boot-line edit is
   the `SvcMgrExecInstall();` call at `variant/qemu_variant_asic.cpp:194`;
   then delete execs 268–270 and the `SvcMgr*` handlers with the file.
   Genuine `\ESTART`/`\E32STRT`/`\ESHELL` entries stay. Gate: selfcheck + file
   tests + DOOM + the `domainpolicy.dll` shutdown path (B4).
5. **Window server (retire `winsrv`).** Port `edisp` to the QEMU framebuffer,
   register `!E32WindowServer`, require a full-run EWSRV PASS with 4/4 markers
   and a rendered screendump; then delete `winsrv.c`, `guishost.h`,
   `qemu_windowsrv.*` and the GUI execs. **No boot-line edit** (edit set `()`):
   file removal only, with **human review**; genuine `\EWSRV.E32` and
   `GenuineBootSpawn()`/`ShellSpawnFull()` stay. Gate: `kerneltest/e32test/window`,
   `r1_window_system_test.py`, `r1_winsrv_test.py`, `r1_desktop_test.py`, DOOM
   graphical frame unchanged.
6. **EUSER closure (separate from the four).** Make `elf2e32.py` emit real PE2
   imports to the genuine DLL and delete `euser_shim.cpp`.
7. **Networking stays authored.** ESOCK is absent from this tree; `netstack.c`
   is not a retirement target and never becomes a "genuine" gate.

After step 5 the console may remain as a debug aid; it is not a userspace fork
of a Symbian server.

## 4. How to use the tool

**Safety gates (apply tool).** `r1_shim_retire_apply.py` is preview-only unless
given `--shim <name> --apply --confirm <TOKEN>`; it then re-runs the real
(non-dry-run) gate and requires PASS, refuses a dirty `kernelhwsrv/` or any
already-modified edit target, and asserts a unique exact-text match per edit.
Before the first write it snapshots every touched file to
`/tmp/opencode/shim_retire_backup_<timestamp>/` with a sha256 manifest and a
rollback token; all post-write work (writes, post gate, rollback) is wrapped so
ANY failure auto-rolls back, and the rollback rc plus the post-rollback gate are
checked (simulated failure returned rc=5 and restored the files). Manual undo:
`--rollback <dir> --confirm ROLLBACK-<shim>-<ts>`, or
`git checkout archive/handwritten-r25 -- qemu-port/<path>`. The review that
drove these fixes and the corrected boot-line sets is
`docs/SHIM_RETIRE_APPLY_REVIEW.md`.

```bash
cd qemu-port
python3 -m py_compile tools/r1_shim_retire_plan.py
python3 tools/r1_shim_retire_plan.py                       # fresh gate --dry-run
python3 tools/r1_genuine_retire_test.py --dry-run > /tmp/opencode/retire.txt
python3 tools/r1_shim_retire_plan.py --from-file /tmp/opencode/retire.txt
python3 tools/r1_shim_retire_plan.py --json > /tmp/opencode/retire-plan.json
```

The human-readable output prints, per shim: `READY`/`BLOCKED`, the gate and its
verdict, the reasons, every file that would be removed or bypassed, every
boot-list line that would change (with a `present` / `CHANGED?` freshness check
against the current source), the extra chain-level conditions and the dependent
call sites. `--json` emits schema `r1-shim-retire-plan/v1` with the same data
plus `dry_run_only`, `mutation_guarantee` and a `summary` of READY/BLOCKED
counts. The exit code is always 0: this is a planner, not a gate.

## 5. Rollback

The complete pre-pivot state is preserved at git tag and branch
`archive/handwritten-r25` (`56fbaff`) and in
`symbian-x86-archive-56fbaff.tar.gz` (git source only; binaries are reproducible
with `make`). Nothing is committed or pushed by the tool or by this plan.

Rollback of a completed switch:

1. `git diff` / `git status` the step's edits; save them to a patch under
   `/tmp/opencode/` before reverting.
2. Restore the shim files and the exact `e32load.cpp` / `qemu_variant_asic.cpp`
   lines from `git show archive/handwritten-r25:qemu-port/<path>` (or check out
   that tag/branch into a worktree for comparison).
3. Rebuild with `make isogrub` and re-run `r1_os_selfcheck.py` plus the affected
   subsystem test. Shim-only disks boot exactly as before because boot
   detection is by FAT presence: with no genuine-named E32 staged,
   `GenuineBootLaunch()` falls back to `SvcMgrBootLaunch()`.
4. Verify `git status -- kernelhwsrv/` is clean — the kernel tree is never part
   of a rollback or a switch.

## 6. Current dry-run output (captured 2026-11-24)

```
$ python3 tools/r1_shim_retire_plan.py
r1_shim_retire_plan -- DRY-RUN ONLY: no file is deleted, edited or staged; no QEMU is booted.
gate status source: live gate --dry-run (no QEMU)

=== portfilesrv: BLOCKED  (gate EFSRV/F32 = SKIP) ===
  reason: gate EFSRV/F32 = SKIP
  reason: SKIP never proves readiness (component not built or not executed; a --dry-run always SKIPs runtime)
  reason: BUILT:   NO   missing: efsrv client (EFSRV.DLL), efat32 fsy (EFAT32.FSY)
  ...
=== r1_fs_* (core/server/client): BLOCKED  (gate EFSRV/F32 = SKIP) ===
  reason: BUILT:   NO   missing: efsrv client (EFSRV.DLL), efat32 fsy (EFAT32.FSY)
  ...
=== winsrv: BLOCKED  (gate EWSRV = SKIP) ===
  reason: BUILT:   YES  genuine/build/EWSRV.E32
  ...
=== svcmgr: BLOCKED  (gate ESTART/ESHELL = SKIP) ===
  reason: BUILT:   YES  e32strt=estart_.E32 eshell=etshell_.E32
  ...
EUSER gate this run: SKIP
SUMMARY: 0 READY, 4 BLOCKED. Rollback: everything is archived at git tag/branch archive/handwritten-r25 (56fbaff)
```

A *full* run on the same tree reports `RESULT: PASS (3 passed, 1 skipped,
0 failed)` — ewsrv/estart/euser PASS, efsrv SKIP — but no dry-run can turn a
shim READY. Reconciled 2026-11-24 after the boot-chain fix; see
`GENUINE_PROGRESS.md` §2–§3.

## 7. References

- `docs/GENUINE_USERSPACE.md` §2 (subsystem map), §4 (ordered plan), §6 (harness semantics).
- `docs/GENUINE_PROGRESS.md` §1–§2 (verdicts and blockers B1–B7).
- `tools/r1_genuine_retire_test.py` (the source of truth parsed by the tool).
- `variant/e32load.cpp:1126-1305`, `variant/qemu_variant_asic.cpp:183-197`,
  `user/svcmgr.cpp:247-255` (boot list and call sites).
- `Makefile:258-262,318-331` (kernel objects that must drop out at retirement).

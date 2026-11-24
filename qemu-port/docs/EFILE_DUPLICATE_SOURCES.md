# EFILE launch sources and duplicate-owner audit

Date: 2026-11-24. Scope: `qemu-port/` source plus existing `/tmp/opencode/*.log`.
`kernelhwsrv/` is untouched; this audit ran **no QEMU** and made no code edits.

## 1. Bottom line

1. The duplicate actually observed in the logs is **not two launchers**: it is
   *one* `E32Run` of EFILE (one `e32run: creating process`, pid=9, alias
   `efilex` or genuine name `EFILE.E32`) that entered `app_main` **three times**.
   `user/start.S:3-8` ignores the EKA2 per-thread entry reason in EBX, so each
   genuine `RThread::Create` (`genuine/efile/patched/sf_main.cpp:515-519`)
   re-enters `app_main` (`genuine/efile/efile_glue.cpp:122-132`), prints
   `EFILE: genuine file server starting`, runs `E32Main`, prints
   `EFILE: E32Main returned 0`, and calls `process_exit(r)` (`user/start.S:68-77`,
   exec 115 kills that *thread*). The losing threads' `DServer::Close` detaches
   every session with `KErrServerTerminated`, which is exactly the `SessionPath
   r=-15` symptom. Counts: 3 starts / 1 `File server thread registered` / 2
   `E32Main returned` from a single launch.
2. Cross-launcher duplication is a **latent** risk: the boot table auto-launches
   `\EFILE.E32` and the prove scripts explicitly type `efilex`/`efilerfs`.
   Today the scripts keep those on *separate* FAT images, so no log shows both.
   The in-flight `variant/e32load.cpp` guard (`single-owner`, lines 815-827)
   refuses a second EFILE-family process; that closes the cross-launcher path
   but does not by itself fix the intra-process re-entry.
3. The in-flight trampoline (`variant/e32load.cpp:94-131,1036-1049`) does remove
   the re-entry (`diag5_prove.log`, `diag6_prove.log`,
   `fallback_efile_genuine.log` at 02:38-02:40: 1 start / 1 registered / 0
   returns), but `diag6_prove.log:1239-1247` shows
   `E32ENTRY: reentry reason=1 iFunction=00000000` → `PD: #PF ... cr2=...` →
   `EFILE: port-shim User::Panic reason=19/38`. The new-thread dispatch reads a
   null function pointer; that must be fixed before claiming the duplicate fix
   is complete.

## 2. Complete source table

| # | Source (path:line) | Owner | Fires when | Duplicate risk / notes |
|---|--------------------|-------|------------|------------------------|
| 1 | `variant/e32load.cpp:1519-1527,1530-1553,1556-1584` (`kGenuineBootList`, `GenuineBootTry`, `GenuineBootLaunch`) | port loader / boot chain | Boot, on supervisor DThread spawned by `variant/qemu_variant_asic.cpp:186-196` | Two file-server rows (`\EFILE.E32`, `\EFSRV.E32`) but `genuineFs`/`serverUp` skip the second. Launches under the **real** name, so any disk carrying `\EFILE.E32` auto-starts the server before the prompt. `E32Run` success alone sets `genuineFs` even if the process later dies; no liveness check. |
| 2 | `variant/e32load.cpp:1485-1498` `SvcMgrBootLaunch()` → `user/svcmgr.cpp:234-242,247-255,288-335` | port shim (to be retired) | Boot **fallback** when no genuine FS was launched and `!FileServer` was not published at `e32load.cpp:1560` | `kServices[1]` launches `\PORTFILESRV.E32` and publishes the **same** protected `!FileServer` (`fs/r1_fs_server.cpp:32`); supervisor restarts it up to 4× (`:307-329`). Two owners of one name if a genuine EFILE is alive but its name was not visible at the probe, or if `svcmgr` is typed from the shell. |
| 3 | `variant/e32load.cpp:1299-1337` `SvcMgrLaunchExec` (exec 268) | port loader | Every svcmgr launch/restart | Generic `FatReadPath` + `E32Run` path. Covered by the new name-based guard **only** for EFILE/EFILEX/EFSRV names; `\PORTFILESRV.E32` is not covered. |
| 4 | `variant/kern_shell.cpp:1435-1526` `RunImage`, dispatch at `:2554-2556` | port shell | Any unrecognised command typed at `C:/>` (`efilex`, `efilerfs`, `EFILE.E32`, `PORTFILESRV.E32`, `f32cli`, …) | Second `E32Run` while boot EFILE is live; guard now refuses EFILE-family names. `desktop=1` auto-run at `:2613-2620` runs only `DESKTOP.E32`, not EFILE. |
| 5 | `genuine/prove_efile_genuine.py:41-52,94-103,113-126` | genuine-EFILE prove owner | Manual QEMU proof; stages `EFILEX.E32` at `:48`, types `efilex` at `:96`, `f32cli` at `:101` | Deliberately uses an alias so boot FAT probe cannot auto-launch. Risk only if the same disk is reused with `\EFILE.E32` present — then boot + shell launch a pair. Marker checks are presence/counts of `bind slot`, not duplicate-owner counts. |
| 6 | `genuine/gdb_efile.py:55-58`, `genuine/gdb_handles.py:60-63` | GDB diagnostics | Manual QEMU proof; same `efilex` command via the prove disk builder | Same as #5; not in `r1_final_verify.py`, but still a live shell launch source. |
| 7 | `genuine/prove_efile_qemu.py:5-7,41-42,90-103` | older name-patched prove | Manual QEMU proof; stages `EFILERFS.E32`, types `efilerfs` | Image patched from `!FileServer` to unprotected `RFileServer`, so it is not a protected-name owner. **Alias `EFILERFS` is not matched by `PortIsEfileName` (`e32load.cpp:47`)**, so the guard and trampoline do not apply to it. |
| 8 | `tools/r1_genuine_retire_test.py:66-89,413-444` (and `:520-521` for EWSRV gate) | authoritative retire gate | `r1_final_verify.py` test 3; stages `EUSER.DLL`, `EFILE.E32`, `EFAT32.FSY`, `F32CLIENT.E32` and expects boot auto-launch | Correctly refuses to send `EFILE` from the shell (comment at `:439-441`). Harness counts are presence checks on a *captured* log; note marker text mismatch: `:77` expects `GenuineBoot: genuine file server up`, kernel at `e32load.cpp:1580` prints `GenuineBoot: file server up`. |
| 9 | `tools/r1_genuine_fullchain_test.py:45-72` | full-chain proof | Manual; stages `EFILE.E32` under genuine name | Boot-only launch; chain expects exactly one `GenuineBoot: EFILE.E32 r=0`. |
| 10 | `tools/r1_genuine_efsrv_test.py:90-93,150-156`; `tools/r1_genuine_fat_io_test.py:94,159` | F32 / FAT-I/O regression gates | `r1_final_verify.py` tests 4 and 7; stage `EFSRVMIN.E32` as `PORTFILESRV.E32`, type `PORTFILESRV.E32` from shell | These disks carry the **shim** owner (`GENUINEFS: starting`), not genuine EFILE. If a full disk with `\EFILE.E32` were ever combined with this staging, two `!FileServer` owners result. Keep disks separate. |
| 11 | `genuine/efile/patched/sf_main.cpp:515-527,552-576`; `genuine/efile/efile_glue.cpp:122-132`; `user/start.S:3-8,68-77` | genuine EFILE image + port runtime | Inside every EFILE launch (not a separate `E32Run`) | **Actual observed duplicate source**: 3 `app_main` entries / 1 launch. Fix belongs in the loader trampoline (`e32load.cpp:94-131`), not in genuine sources. |
| 12 | `services/r1_service_host.cpp:129-143` (`B3StartFileServer`) | service-host design | Only if called; no boot caller found in `qemu-port/` | Would create a third `!FileServer` publisher if wired up while EFILE or the shim is live; keep gated. |

## 3. Log cross-reference (raw serial logs)

Counts are literal substring counts in the **raw serial log**, not in harness
stdout: harness files (e.g. `verify_retire_full_final.log`, `efile_prove_final.log`)
repeat marker strings in PASS/FAIL/WARN lines and overcount them.

| Log (time) | `genuine file server starting` | `File server thread registered` | `E32Main returned` | launch evidence | IPC |
|---|---|---|---|---|---|
| `r1_fullchain.log` (02:21) | 3 | 1 | 2 | `GenuineBoot: EFILE.E32 r=0 pid=9` =1; one `e32run: creating process` for EFILE | no round-trip |
| `r1_retire_efsrv.log` (02:02) | 3 | 1 | 2 | boot launch =1 | no round-trip; `exit: ... reason=111 prog=EFILE.E32` |
| `diag_prove.log`, `diag2/3/4_prove.log`, `efile_session_path_final.log` (02:32-02:36) | 3 | 1 | 2 | `efilex started (user process, pid=9)` =1, boot launch =0 | `SessionPath r=-15` ×2 in `efile_session_path_final.log` |
| `diag5_prove.log` (02:38), `diag6_prove.log` (02:40), `fallback_efile_genuine.log` (02:40) | **1** | **1** | **0** | `e32run: EFILE single-entry veneer` =1, `efilex started` =1 | no round-trip; `E32ENTRY ... iFunction=00000000` + `#PF` + `port-shim` 19/38 in `diag6` |
| `r1_genuine_efsrv.log`, `r1_fat_io.log` | 0 (shim) | n/a | n/a | `PORTFILESRV.E32 started (user process, pid=9)` =1, `GENUINEFS: starting` =1 | shim IPC PASS |

No existing log contains both `GenuineBoot: EFILE.E32 r=0` and `efilex started`,
and no log contains `e32run: single-owner: ... launch refused`. The
cross-launcher guard path has therefore **never been exercised**.

## 4. Complete fix checklist by owner

- **`variant/e32load.cpp` (in-flight owner):** keep the cross-launcher guard,
  but make check-and-create atomic (a static owner flag set under CS before
  `ProcessCreate`; current `PortEfileProcessRunning()`/`PortFileServerPublished()`
  at `:821` is a TOCTOU window between boot DThread and shell). Extend
  guard/trampoline selection from names (`:40-56`) to image UID3 `0x100000bf`
  from `E32Validate`, so aliases such as `EFILERFS.E32` cannot slip through.
  Fix the trampoline's `SThreadCreateInfo` offsets (`:113-122` currently yields
  `iFunction=00000000` and `#PF`) and verify non-first threads call their real
  `iFunction`, never `app_main`, and exit via exec 115. Do not change the shim
  fallback behaviour for disks with no genuine FS.
- **Boot-table owner (`e32load.cpp:1519-1584`):** retain the single file-server
  slot and the pre-loop `!FileServer` probe; optionally re-check publication
  after a successful launch before setting `genuineFs`, so a stillborn EFILE
  does not suppress both FS rows and the fallback.
- **Shim owner (`user/svcmgr.cpp:247-255,288-335`):** do not launch/restart
  `\PORTFILESRV.E32` when a genuine `!FileServer` is published; retire the row
  only when `r1_genuine_retire_test.py` EFSRV gate PASSes (never before).
- **Prove owners (`genuine/prove_efile_genuine.py`, `gdb_efile.py`,
  `gdb_handles.py`):** keep alias staging; add exact marker-count assertions
  (below); never type `efilex` on a disk containing `\EFILE.E32`; let the IPC
  client run to completion before terminating QEMU.
- **`genuine/prove_efile_qemu.py`:** either switch to the alias/UID-covered path
  or document that the name-patched `RFileServer` image is out of scope for the
  protected-name guard.
- **Gate owners (`r1_genuine_retire_test.py`, `r1_genuine_fullchain_test.py`):**
  stage only under the genuine name, never send an `EFILE` shell command; add
  count assertions, not just presence; fix the `GenuineBoot: ... file server up`
  marker typo at `r1_genuine_retire_test.py:77`.
- **Shim regression owners (`r1_genuine_efsrv_test.py`,
  `r1_genuine_fat_io_test.py`):** keep their disks shim-only; assert no
  `GenuineBoot: EFILE.E32` marker so a mixed disk cannot silently create two
  owners.
- **Genuine sources:** no edits to `genuine/efile/*`, `user/start.S` or
  `kernelhwsrv/` for the duplicate fix; port-side loader dispatch is the fix.

## 5. Discriminating tests and exact marker counts

All counts below are from one raw serial log.

**D1 — cross-launcher duplicate (never yet run).** Build one FAT disk containing
`EUSER.DLL`, `F32CLI.E32`, genuine `EFILE.E32` **and** the same image as
`EFILEX.E32`; boot, then type `efilex`, then `f32cli`. Expected:
`GenuineBoot: EFILE.E32 r=0` = 1; `e32run: single-owner: EFILEX.E32 ... refused`
= 1; `e32run: single-owner: EFILE.E32 ... refused` = 0;
`EFILE: genuine file server starting` = 1; `File server thread registered` = 1;
`EFILE: E32Main returned` = 0; `exit: id=... prog=efilex` = 0;
`EFILE: port-shim` = 0; `F32CLIENT: IPC round trip OK` >= 1; no `Exc`/`FAULT`.

**D2 — intra-process re-entry regression (standard prove disk).** Disk with
`EFILEX.E32`, `F32CLI.E32`, `EUSER.DLL`; boot, type `efilex`, then `f32cli`.
Expected: `e32run: EFILE single-entry veneer` = 1;
`EFILE: genuine file server starting` = 1; `File server thread registered` = 1;
`EFILE: E32Main returned` = 0; `E32ENTRY: reentry ...` lines must show a
nonzero `iFunction` and **no** `PD: #PF` after them; `SessionPath r=-15` = 0;
`F32CLIENT: IPC round trip OK` >= 1; `EFILE: port-shim` = 0.

**D3 — shim/genuine collision.** Disk with `EFILE.E32` plus `PORTFILESRV.E32`
(or manually type `svcmgr` while genuine EFILE is alive). Expected:
`GenuineBoot: EFILE.E32 r=0` = 1; `SvcMgr: boot launch r=0` = 0;
`GENUINEFS: starting` = 0; `File server thread registered` = 1; zero
`SVCMGR: restarted a service`; one live `!FileServer` owner.

Current status of D1: **untested**. D2: duplicate counts fixed, but trampoline
`#PF`/`iFunction=0` and `port-shim` 19/38 still FAIL. D3: **untested**.

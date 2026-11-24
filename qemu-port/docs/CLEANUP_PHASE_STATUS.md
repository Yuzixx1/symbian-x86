# CLEANUP_PHASE_STATUS — read-only snapshot of the whole cleanup phase (2026-11-24 ~19:03 +02:00)

Monitor-only snapshot. No edits except this file; no QEMU run; no commit/push;
`kernelhwsrv/` untouched (`git status --short` shows zero kernelhwsrv entries).
Sources: `git status`/`git diff`, file mtimes, `/tmp/opencode/` + `~/.cache/sybian-r1/`
logs, `docs/CLEANUP_*.md`, `docs/SVCMGR_*.md`, `docs/WINSRV_EDISP_SCOPE.md`.

## Tree state at snapshot

- **HEAD:** `e29a072` "Cleanup: remove retired portfilesrv artifacts, add cleanup docs"
  (committed 2026-11-24 19:01:09 by the user; it explicitly deferred the svcmgr-apply
  files to a follow-up commit).
- **Uncommitted (svcmgr apply, landed 19:00:54–19:01:31):** `M qemu-port/Makefile`,
  `qemu-port/user/user.mk`, `qemu-port/variant/e32load.cpp`,
  `qemu-port/variant/qemu_variant.h`, `qemu-port/variant/qemu_variant_asic.cpp`
  (+ `AGENTS.md`, `.sybian/coordinator/state.json`); diffstat 88+/192−.
  `?? qemu-port/.sybian/qemu-slot.lock`, `?? symbian-x86-archive-56fbaff.tar.gz`.
- **Rollback point:** `/tmp/opencode/svcmgr_retire_backup_20261124_185621/`
  (MANIFEST.sha256 + copies of e32load.cpp, qemu_variant_asic.cpp, qemu_variant.h,
  user.mk, svcmgr.cpp — 5 sha256 entries, taken 18:56:21); also `git show e29a072:<path>`.

## Per-workstream table

| Workstream | Status | Evidence |
|---|---|---|
| **svcmgr apply** (owns e32load.cpp, qemu_variant_asic.cpp, qemu_variant.h, user.mk, user/svcmgr.cpp) | **EDITS LANDED (uncommitted, 19:00:54–19:01:31); post-apply gates only partially run; svcmgr.cpp deletion still pending** | `SvcMgrExecInstall` → `PortTrapExecInstall()` (`variant/e32load.cpp:1377`, live call `variant/qemu_variant_asic.cpp:171` before `GenuineBootSpawn()`); exec slots 268/269 (launch/kill) and ctl ops 0/1 removed, slot 270 slimmed to ops 2/3/4 (FBO_FBINFO/FBBLIT/PTRINJ) — grep `PortTrapExecInstall` = 3 hits in e32load.cpp + 1 live call in asic; `qemu_variant.h` decls swapped; `user.mk` SVCMGR.E32/PORTFILESRV.E32 targets+rules gone; `Makefile` r1_fs_core.o/r1_fs_fat.o kernel objects removed, Init3 mount is now direct `FatMount(4,0)` (R1Fs globals/exports deleted). `user/svcmgr.cpp` still present (14059 B, mtime 08:45, in backup manifest, not deleted). Post-apply: `r1_os_selfcheck_selftest.log`/`r1_selftest.log` (19:01:39–40) **RESULT: PASS** — all core services, "EUSER ord 915 bound", "no guest exceptions (0)"; raw `r1_qemu_smoke.log` (19:02:34) boots to `C:/>` (its "portfilesrv started pid=9" line is the smoke fixture's own staged binary from `~/.cache/sybian-r1/r1-portfilesrv-flat-v1`, not the boot path — fullchain prints "svcmgr fallback skipped"). Pre-apply baseline green: strict 11/11, retire 4/4, fullchain 14/14 (below). **Not yet re-run post-apply:** retire, fullchain, strict 11/11, pointer/PTRINJ (last PTRINJ evidence `genuine_pointer.log` + `PTRINJ.TXT` 02:30, `80 40` — slot 270 op 4 kept, must be re-proven). |
| **edisp port** (owns genuine/ewsrv/, qemu_display.cpp) | **Client blockers FIXED + gate PASS; EDISP.DLL factorization and winsrv migration NOT started** | `variant/qemu_display.cpp` mtime 02:00:46 (EDisplayHalScreenInfo bridge at :493–514 answers VBE LFB). `genuine/ewsrv/`: `build_ewsrv.py` 01:32:37, `build_client.py` 01:32:40, `ewsrv_client.cpp` 01:19:54, `obj/` 18:52:34 (ewsrv.elf 119460 B, ewclient.elf, co_cli.o…). Both blockers fixed per `docs/WINSRV_EDISP_SCOPE.md` §3.2 (identical `!E32WindowServer` name both sides; explicit `TSize` field arithmetic — no sret). Evidence: `ewsrv_final_test2.out` (01:30:40) **RESULT: PASS** — full EWCLIENT marker set (ScreenSize/Init/SetTitle/Write/SetCursorPos/Write2 all r=0, "window drawn"), 232 PE2 imports bound, 0 faults, desktop PNG; `final_verify_genuine_ewsrv.log` (18:52:52) **RESULT: PASS** — no guest exception/panic/FAULT, screendump renders desktop; `genuine_ewsrv.log` (18:52) ewclient.e32 pid=11 window drawn. Remaining per scope doc: factor edisp as real EDISP.DLL (ordinal-1 `CScreenDriver::New`) or keep in-image patched `wd_generic.cpp`; ~12 GUI users + DOOM `guishost.h` fallback still on execs 249/250/251 + 271–273; winsrv deletion set untouched (needs gate + human review). |
| **authority runs** | **CLEAN at `d26fade`; post-apply tree NOT yet re-certified** | `docs/CLEANUP_AUTHORITY_CHECK.md` (19:02:03): strict `--strict-skip` **11/11 PASS** at `d26fade` (selfcheck run-1 FAIL attributed to concurrency, isolated re-run PASS), retire **4/4**, fullchain **14/14**, kernelhwsrv clean — but the verdict certifies `d26fade` only. `/tmp/opencode/final_verify_strict_20261124.log` (03:55) "OVERALL: PASS (11 pass, 0 skip, 0 fail)"; per-test `final_verify_*.log` (18:50–18:57) all RESULT: PASS — qemu_smoke, genuine_retire (18:52, "4 passed 0 failed"), genuine_efsrv (IPC round trip OK, ord 915 bound), genuine_ewsrv, genuine_eshell (8/8 commands), genuine_fat_io (FATIO-MARKER on volume), doom_main/audio/playtest/multikey (all PASS, 0 Exc/Panic/FAULT/#PF). `r1_fullchain.log` (18:58): ESHELL.E32 r=0 pid=17, "file server up, svcmgr fallback skipped". Skipped >50 MB logs (existence noted only): `ewsrv_exec.log` 136M, `trampoline_removal_gates` 99M, `ewsrv_exec2.log` 86M. |
| **commits** | **One cleanup commit landed (`e29a072`, 19:01:09, user-authored); svcmgr apply edits uncommitted by design** | `git log`: e29a072 on top of d26fade/7f76900/56fbaff; commit message defers Makefile/e32load/asic/variant.h/user.mk/svcmgr.cpp to a follow-up "in-flight svcmgr-apply verification". No push observed this session; per rules no commit/push without explicit request. Preservation intact: branch `archive/handwritten-r25` @ `56fbaff` + tarball `symbian-x86-archive-56fbaff.tar.gz` (untracked). |

## Next actions

1. **svcmgr apply agent:** re-run the full gate suite on the applied tree —
   `r1_genuine_retire_test.py` (4/4), `r1_genuine_fullchain_test.py` (14/14),
   `r1_final_verify.py --strict-skip` (11/11), and the pointer test
   (`r1_genuine_pointer_test.py`, PTRINJ via slot 270 op 4) — then delete
   `user/svcmgr.cpp`; commit as the follow-up to `e29a072` only when the user
   authorizes. Rollback: `/tmp/opencode/svcmgr_retire_backup_20261124_185621/`.
2. **edisp agent:** decide EDISP.DLL vs in-image patched driver (scope doc §3),
   then plan the winsrv migration (~12 GUI users + DOOM fallback off execs
   249/250/251 + 271–273 onto genuine `RWsSession`/`RWindowGroup`); no winsrv
   deletion until the EWCLIENT window gate + fresh strict run pass.
3. **Authority:** fresh `--strict-skip` run on the post-apply tree before any
   further deletion (explicit caveat in `CLEANUP_AUTHORITY_CHECK.md`).
4. Standing caveats: never accept `SessionPath r=-15` as IPC success; never
   re-add the EFILE trampoline; `!`-servers need `ECapabilityProtServ`;
   netstack is never retired; `kernelhwsrv/` read-only.

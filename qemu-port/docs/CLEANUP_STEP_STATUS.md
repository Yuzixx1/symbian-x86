# CLEANUP_STEP_STATUS — read-only snapshot (2026-11-24, ~18:50 +02:00)

Produced by a read-only monitor agent. No edits other than this file; no QEMU
runs; `kernelhwsrv/` untouched (`git status kernelhwsrv/` clean). Concurrent
cleanup agents are active — statuses reflect the working tree at snapshot time.

## Step status

| Step | Status | Evidence (file + mtime) | Next action |
|---|---|---|---|
| 1. Authority: fresh `r1_final_verify.py --strict-skip` 11/11 on current tree | **IN PROGRESS** | `/tmp/opencode/final_verify_strict_regression_d26fade.log` (18:37:43): selfcheck PASS 237s, qemu_smoke PASS 50s, genuine_retire PASS 46s, genuine_efsrv PASS 15s; ewsrv log `final_verify_genuine_ewsrv.log` (18:38:01) RESULT: PASS. No full 11/11 summary line yet — run still in flight | Await final summary (11 tests incl. fat_io + 4 DOOM); do not treat as gate until RESULT line present |
| 2. Remove dead portfilesrv build rules/files + stray artifacts | **IN PROGRESS** (uncommitted) | `git status`: `D user/portfilesrv.cpp`, `D tools/mke32_portfilesrv.py`, `D :portfilesrv`, `M user/user.mk` (all 18:47:0x); `build/user/PORTFILESRV.E32` **gone**; `services/r1_service_host.{cpp,h}` still present (10-28) | Finish user.mk rule removal + r1_service_host, then re-run strict verify before commit |
| 3. `r1_fs` kernel glue removal feasibility | **PENDING** | `docs/R1FS_GLUE_FEASIBILITY.md` does NOT exist; `fs/r1_fs_core.cpp` still present (10-31) | Write feasibility doc; gate = IPC ×2, port-shim=0, #PF=0 |
| 4. svcmgr retirement | **NOT STARTED** (as planned) | `user/svcmgr.cpp` present (mtime 11-24 08:45); `svcmgr.cpp:250` + `e32load.cpp:1552` still commented; `SvcMgrExecInstall()` at `qemu_variant_asic.cpp:194` LIVE | Keep exec-81 wrapper; no blind removal |
| 5. winsrv retirement | **NOT STARTED** | `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.cpp`, `variant/qemu_gui_exec.cpp` all present | Needs edisp port + EWSRV external-client window gate |
| 6. euser_shim removal | **NOT STARTED** | `user/euser_shim.cpp` present (11-19); genuine EUSER.DLL resident, 2503 exports | Separate closure via real PE2 imports in `elf2e32.py` |
| 7. netstack | **KEEP — NEVER** | `user/netstack.c` + tools present | Authored exception; no action |

## Test evidence (newest logs, `/tmp/opencode/`)

- **Retire 4/4:** `final_verify_genuine_retire.log` (18:37:28) — efsrv/ewsrv/estart/euser all PASS, "RESULT: PASS (4 passed, 0 failed)".
- **Smoke PASS:** `final_verify_qemu_smoke.log` (18:36:42) — RESULT: PASS, exports=2503, EUSER.DLL resident.
- **Fullchain:** `r1_fullchain.log` (09:22:51, newest) — `GenuineBoot: ESHELL.E32 r=0 pid=17`, "file server up, svcmgr fallback skipped" (14/14 record per AGENTS.md; no post-deletion rerun yet).
- **⚠ Watch item:** `final_verify_selfcheck.log` (18:47:31) — fast selfcheck snapshot shows `qemu_smoke` FAIL (EUSER marker lines missing) in a boot log that still prints `portfilesrv started (pid=9)` (`r1_qemu_smoke.log` 18:47:31). Coincides exactly with the 18:47 portfilesrv deletions — likely transient mid-edit state, not a gate result. Re-run after step 2 settles.

## Docs present / absent

- Present: `docs/CLEANUP_LOG.md` (18:47:05, step table), `docs/REMAINING_SHIMS_AUDIT.md` (18:30:41, cleanup order §3), `docs/SHIM_RETIREMENT_RESULT.md` (08:59), `docs/FINAL_VERIFICATION_RESULT.md` (03:55).
- Absent (pending): `docs/CLEANUP_AUTHORITY_CHECK.md`, `docs/R1FS_GLUE_FEASIBILITY.md`, `docs/SVCMGR_RETIREMENT_PLAN.md`.

## Overall verdict

Cleanup is **mid-flight on step 2 only**: portfilesrv shim files deleted in the
working tree (uncommitted), strict authority run (step 1) green so far but
unfinished, retire gate 4/4 PASS, smoke PASS at 18:36. Steps 3–6 untouched as
planned, netstack permanently kept. **No commit/push has happened** (HEAD still
`d26fade`); the 18:47 selfcheck smoke FAIL should be re-checked once step 2
completes. Rollback snapshot `/tmp/opencode/shim_retire_backup_20261124_083011`
remains valid.

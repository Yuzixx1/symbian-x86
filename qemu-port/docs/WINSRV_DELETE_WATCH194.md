# WINSRV_DELETE_WATCH194 — re-apply status (2026-11-24)

- APPLY_RC: **not yet printed** — apply still in flight (pid 1286837, 33:54 elapsed, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`); no ROLLBACK in log.
- Phases: P1 PASS, P2 PASS (make + selfcheck + ewsrv retire gate each); P3 running — final line is `r1_final_verify.py --strict-skip`, earlier P3 gates pending.
- Deletes: 7 confirmed in `git status` (winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp); edits 18/18 applied.
- kernelhwsrv/: clean — `git status --short kernelhwsrv/` = 0 lines; log also reports "kernelhwsrv/ clean: clean".
- HEAD: 0f03f7e (doom e32_platform.c restore) on top of 97f224b (GUI-off-winsrv migration).
- Snapshot/rollback token: /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).

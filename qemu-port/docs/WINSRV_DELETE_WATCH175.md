# WINSRV_DELETE_WATCH175 — re-apply status (2026-11-24)

- APPLY_RC: NOT YET EMITTED — apply still running (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`, ~33 min elapsed), currently inside phase P3 gate list (final gate: `r1_final_verify.py --strict-skip`).
- No rollback triggered; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436) intact.
- Gates so far: pre-apply ewsrv PASS, P1 (make+selfcheck+ewsrv) PASS, P2 (isogrub+selfcheck+ewsrv) PASS.
- 7 deletes confirmed in worktree: winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp (`git status` D count = 7).
- kernelhwsrv/ untouched: `git status --short kernelhwsrv/` = 0 lines; log tail also printed "kernelhwsrv/ clean: clean".
- HEAD: 0f03f7e (restore doom e32_platform.c hunk), parent 97f224b — no new commit; deletes uncommitted as required.

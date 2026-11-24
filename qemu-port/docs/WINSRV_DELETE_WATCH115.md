# WINSRV_DELETE_WATCH115 — re-apply status (2026-11-24)

**APPLY_RC: NOT YET EMITTED — apply still in progress.**

- Log `/tmp/opencode/winsrv_delete_apply2.log` (3.2 KB, far under 50 MB cap): last lines show P3 gate list invoked (selfcheck, smoke, ewsrv/input/doom tests); no `APPLY_RC`, no rollback, no FAILED/Traceback.
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) alive, ~21m41s elapsed — P3 verification gates still running.
- `git status --short | grep -cE '^ ?D'` = **7**: winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp (all under qemu-port/).
- `git log -2`: `0f03f7e` (doom e32_platform.c restore), `97f224b` (GUI→EWSRV migration). No new commit from this apply.
- Pre-apply + P1 + P2 gates all PASS; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (rollback token ROLLBACK-WINSRV-20261124_215436).
- `kernelhwsrv/` git status: clean/untouched.

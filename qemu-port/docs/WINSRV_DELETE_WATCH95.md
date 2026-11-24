# WINSRV_DELETE_WATCH95 — re-apply status (2026-11-24 ~22:30)

- APPLY_RC: **NOT YET EMITTED** — apply still running, currently in phase P3 gates
  (`make`, selfcheck, qemu_smoke, ewsrv/input/doom tests); log ends at P3 gate list.
- Process: PID 1286837 alive (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`, ~18 min elapsed).
- Log: `/tmp/opencode/winsrv_delete_apply2.log` (2990 bytes, 50 lines); P1+P2 gates PASS, 7 files deleted, no FAIL/ROLLBACK/Traceback lines.
- Snapshot/rollback token: `ROLLBACK-WINSRV-20261124_215436` at `/tmp/opencode/winsrv_retire_backup_20261124_215436`.
- git: `git status --short | grep -cE '^ ?D'` = **7** (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp).
- HEAD: `0f03f7e` (doom e32_platform.c restore) on top of `97f224b` (winsrv→EWSRV migration).
- kernelhwsrv/ untouched (git status clean there). No QEMU run; read-only monitor.

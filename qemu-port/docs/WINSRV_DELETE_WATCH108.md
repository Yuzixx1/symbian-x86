# WINSRV_DELETE_WATCH108 — re-apply status (2026-11-24 22:13)

- APPLY_RC: NOT YET — pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still alive (~20 min), currently in phase P3 gates (make, selfcheck, smoke, ewsrv/input/doom tests).
- Log /tmp/opencode/winsrv_delete_apply2.log is only 3102 bytes; no APPLY_RC line (appended by wrapper on exit); no ERROR/FAIL/rollback lines.
- Phases P1+P2 gates PASS; 7 files deleted (winsrv.c, wsclient.c/.h, guishost.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp) → `git status` shows 7 `D` entries; edits in variant/ + user.mk + Makefile.
- git log: HEAD 0f03f7e (prior 97f224b) — apply makes no commit. kernelhwsrv/ clean (read-only respected).
- Rollback snapshot intact: /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436).

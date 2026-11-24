# WINSRV_DELETE_WATCH170 — re-apply status (2026-11-24)

- APPLY_RC: **NOT YET** — apply still running (pid 1286837, ~32 min elapsed); last log line shows P3 gate `r1_final_verify.py --strict-skip` in progress. No rollback lines in log.
- Phases done: P1 (variant edits + gates PASS), P2 (Makefile/user.mk + isogrub gate PASS), P3 deletions done (7 files: winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp), P3 gates running.
- `git status --short | grep -cE '^ ?D'` = **7**; kernelhwsrv/ clean per log; HEAD = 0f03f7e (pre-apply commits 0f03f7e, 97f224b).
- Rollback snapshot: /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436).
- kernelhwsrv/ untouched; no QEMU run by this monitor.

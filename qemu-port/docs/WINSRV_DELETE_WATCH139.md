# WINSRV_DELETE_WATCH139 — re-apply status (2026-11-24)

- APPLY_RC: **NOT YET EMITTED** — apply still running (pid 1286837 alive). No APPLY_RC, no rollback in /tmp/opencode/winsrv_delete_apply2.log (56 lines, 3.3 KB).
- Progress: P1 gate PASS, P2 gate PASS; all 18 edits + 7 deletes applied; P3 gate (10 tests incl. r1_final_verify --strict-skip) in flight.
- Deletions visible in git: 7 (`^ ?D` count = 7): winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp.
- kernelhwsrv/: untouched (0 changes in git status; log lines 2/3/5 report clean).
- Snapshot/rollback token: /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).
- HEAD: 0f03f7e (doom e32_platform.c restore) on 97f224b (EWSRV migration).

# WINSRV delete watch #188 (read-only monitor)

- APPLY_RC: **not yet printed** — apply still running in phase P3 gates (last log line: `r1_final_verify.py --strict-skip` gate queued); no rollback/FAILED lines in log.
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): **alive**, elapsed 33:30.
- Deletes staged: **7** (`git status --short | grep -cE '^ ?D'` = 7): winsrv.c, guishost.h, wsclient.c, wsclient.h, qemu_windowsrv.cpp, qemu_windowsrv.h, qemu_gui_exec.cpp — matches the 7-deletes plan.
- kernelhwsrv/: `git status --short kernelhwsrv/ | wc -l` = **0** — untouched/clean.
- HEAD: 0f03f7e (doom e32_platform.c restore on top of 97f224b winsrv→EWSRV migration).
- Snapshot/rollback token: ROLLBACK-WINSRV-20261124_215436 at /tmp/opencode/winsrv_retire_backup_20261124_215436.
- Pre-apply and P1/P2 gates all PASS; P3 full gate suite in progress. No edits made by this monitor except this report.

# WINSRV_DELETE_WATCH186 — read-only monitor report (2026-11-24)

- APPLY_RC: NOT YET EMITTED — apply still in progress. Log `/tmp/opencode/winsrv_delete_apply2.log` (3.3 KB) ends at `+ gate[P3]: r1_final_verify.py --strict-skip`; no APPLY_RC/rollback line. No rollback triggered; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436) exists but unused.
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still RUNNING, elapsed 33:21.
- Phases so far: 18 edits + 7 deletes applied; pre/P1/P2 ewsrv gates PASS; P3 gates launched (selfcheck, smoke, ewsrv, input, doom x4, fullchain, retire, final_verify).
- Deletes confirmed: `git status --short | grep -cE '^ ?D'` = 7 — user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp, variant/qemu_windowsrv.h, variant/qemu_gui_exec.cpp.
- kernelhwsrv/ untouched: `git status --short kernelhwsrv/ | wc -l` = 0.
- HEAD: 0f03f7e (doom e32_platform.c restore) on 97f224b (GUI→EWSRV migration). No commit/push made by monitor.

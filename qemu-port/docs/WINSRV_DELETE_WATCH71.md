# WINSRV_DELETE_WATCH71 — re-apply status (2026-11-24)

- Log `/tmp/opencode/winsrv_delete_apply2.log` (2.7 KB, fully read): 25 ops (18 edits, 7 deletes); P1 and P2 gates PASS (make, isogrub, selfcheck, ewsrv retire gate); deletes applied; P3 gate (`make` + selfcheck) started at line 46 — **no APPLY_RC yet**.
- Process pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still RUNNING — apply in flight, P3 gate pending.
- git status: 7 deletes present (user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp/.h, variant/qemu_gui_exec.cpp) + M tools/r1_winsrv_delete.py; HEAD 0f03f7e / 97f224b.
- kernelhwsrv/ untouched (log: "kernelhwsrv/ clean: clean"). Rollback token: ROLLBACK-WINSRV-20261124_215436, snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436.

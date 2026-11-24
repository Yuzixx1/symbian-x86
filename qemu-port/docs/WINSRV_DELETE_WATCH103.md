# WINSRV_DELETE_WATCH103 — re-apply status (2026-11-24)

- APPLY_RC: NOT YET EMITTED — apply still running (pid 1286837 alive, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`, ~19 min elapsed); no rollback in log.
- Log tail (`/tmp/opencode/winsrv_delete_apply2.log`, 3.1 KB, well under 50 MB): P1 gate PASS, P2 gate PASS, 7 deletes applied, P3 gates in progress (make / selfcheck / qemu_smoke / genuine_ewsrv / genuine_input / doom / doom_audio / doom_playtest); no APPLY_RC line yet.
- Deletions staged (7): user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp, variant/qemu_windowsrv.h, variant/qemu_gui_exec.cpp — `git status --short | grep -cE '^ ?D'` = 7.
- kernelhwsrv/ clean (log-verified); HEAD = 0f03f7e, prior 97f224b.
- Snapshot/rollback token: /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).

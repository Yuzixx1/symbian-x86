# WINSRV_DELETE_WATCH195 — winsrv re-apply status

- APPLY_RC=0 — re-apply FINISHED, no rollback; all P1–P3 phase gates PASS (ewsrv retire gate, selfcheck, full verify 11-test suite, DOOM tests).
- 7 deletes confirmed in `git status --short` (D count = 7): user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp, variant/qemu_windowsrv.h, variant/qemu_gui_exec.cpp.
- 18 edits applied (variant + Makefile + user.mk), all unique/allowlisted.
- pid 1286837 not running (apply process exited).
- HEAD: 0f03f7e (restore e32_platform.c) on top of 97f224b.
- kernelhwsrv/ untouched: `git status --short kernelhwsrv/` = 0 lines.
- Rollback snapshot (unused): /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436).

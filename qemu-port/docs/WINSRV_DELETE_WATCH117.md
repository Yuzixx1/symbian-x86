# WINSRV delete apply watch #117 (2026-11-24 22:15 UTC)

**APPLY_RC: NOT YET — apply still running.**

- pid 1286837 alive (~22 min): `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`; log `/tmp/opencode/winsrv_delete_apply2.log` (3.2 KB, no APPLY_RC line yet).
- Progress: pre-gate ewsrv PASS; P1 (18 edits) gate PASS; P2 (Makefile/user.mk) gate PASS; 7 files deleted; now in **P3 QEMU gates** (selfcheck → smoke → ewsrv → input → doom×4 → fullchain). No rollback triggered.
- `git status --short | grep -cE '^ ?D'` = **7** (winsrv.c, guishost.h, wsclient.c/.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp). HEAD: `0f03f7e` / `97f224b`.
- kernelhwsrv/ clean per apply log ("kernelhwsrv/ clean: clean"); snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, rollback token `ROLLBACK-WINSRV-20261124_215436`.

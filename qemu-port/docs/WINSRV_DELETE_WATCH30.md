# WINSRV_DELETE_WATCH30 — 2026-11-24 ~21:55

- Re-apply (apply2, pid 1286837) STILL RUNNING — no APPLY_RC in /tmp/opencode/winsrv_delete_apply2.log yet.
- Log tail: 18 edits applied (qemu_variant.h, qemu_mouse.cpp, qemu_display.cpp, kern_shell.cpp, qemu_variant_int.cpp); now at P1 gate `make` + r1_os_selfcheck.py. Pre-apply ewsrv gate PASS; snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).
- Deletes NOT in git yet: no staged/deleted winsrv entries in `git status`; winsrv sources still on disk (variant/qemu_windowsrv.*, user/guishost.h).
- HEAD: 0f03f7e (doom e32_platform restore); prior 97f224b (winsrv→EWSRV migration).
- kernelhwsrv/ untouched (log line 2: clean). Only pre-existing `M tools/r1_winsrv_delete.py` in status.

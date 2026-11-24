# WINSRV_DELETE_WATCH37 — re-apply status (2026-11-24)

- Re-apply still RUNNING: pid 1286837 alive (~3m21s), log ends at gate[P1] `make` + selfcheck; no APPLY_RC yet, no rollback.
- Pre-apply ewsrv gate PASS; snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).
- 13 edit ops applied so far (qemu_variant.h, qemu_mouse.cpp, qemu_display.cpp, kern_shell.cpp, qemu_variant_int.cpp); 7 deletes are in-file block deletions.
- git status: NO file deletes (no `D` entries); winsrv files still present/tracked: variant/qemu_windowsrv.{h,cpp}, user/guishost.h, user/winsrv.c, user/wsclient.{c,h}, variant/qemu_gui_exec.cpp.
- HEAD: 0f03f7e (doom e32_platform.c restore) on top of 97f224b (GUI apps migrated to EWSRV).
- kernelhwsrv/ clean/untouched. Next watch: APPLY_RC line + post-apply gates.

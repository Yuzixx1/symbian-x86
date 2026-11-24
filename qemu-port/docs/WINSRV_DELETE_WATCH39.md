# WINSRV_DELETE_WATCH39 — re-apply status (2026-11-24 ~21:58)

- Re-apply NOT finished: no `APPLY_RC` in /tmp/opencode/winsrv_delete_apply2.log (log 1.6 KB, ends at `gate[P1]: make` / selfcheck).
- Pid 1286837 still running (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`, elapsed ~3:44), currently in P1 build/selfcheck gate.
- 18 edits applied so far (qemu_variant.h, qemu_mouse.cpp, qemu_display.cpp, kern_shell.cpp, qemu_variant_int.cpp); the 7 deletes not yet executed.
- No deletions in git yet: 0 ` D` entries; winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.*, qemu_gui_exec.cpp still tracked.
- Pre-apply ewsrv gate PASS; snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).
- HEAD: 0f03f7e (doom e32_platform.c restore on top of 97f224b). kernelhwsrv/ clean/untouched.

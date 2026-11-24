# WINSRV_DELETE_WATCH70 — 2026-11-24 22:15 UTC

- APPLY_RC: **NOT YET** — pid 1286837 still running (10m35s); log ends at line 46 `+ gate[P3]: r1_os_selfcheck.py`, no APPLY_RC/rollback line.
- Deletes in git: YES, 7 files unstaged `D` — user/{winsrv.c,guishost.h,wsclient.c,wsclient.h}, variant/{qemu_windowsrv.cpp,qemu_windowsrv.h,qemu_gui_exec.cpp}; edits in qemu_variant.h, qemu_mouse.cpp, qemu_display.cpp, kern_shell.cpp, qemu_variant_int.cpp, Makefile, user/user.mk. Nothing staged, no new commit.
- Gates so far: pre-apply ewsrv PASS, P1 (make+selfcheck+ewsrv) PASS, P2 (isogrub+selfcheck+ewsrv) PASS; P3 in flight.
- Rollback snapshot: /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436).
- kernelhwsrv/: clean/untouched. git log -2: 0f03f7e, 97f224b (no commit/push).

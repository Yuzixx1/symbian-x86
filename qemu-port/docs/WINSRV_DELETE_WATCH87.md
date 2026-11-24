# WINSRV_DELETE_WATCH87 — re-apply status (2026-11-24 22:10)

- APPLY_RC: NOT finished. Log ends at P3 gate (`r1_genuine_ewsrv_test.py`); no APPLY_RC line, no rollback.
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): still running (~16 min).
- Progress: P1 (18 edits) PASS, P2 (Makefile/user.mk) PASS, 7 deletes applied (winsrv.c, guishost.h, wsclient.[ch], qemu_windowsrv.[ch], qemu_gui_exec.cpp); P3 gate in flight.
- `git status --short | grep -cE '^ ?D'` = 7 (all in qemu-port/, expected deletion set).
- HEAD: 0f03f7e (doom e32_platform.c restore) on top of 97f224b (winsrv→EWSRV migration).
- kernelhwsrv/: clean, 0 changes. Snapshot/rollback token: ROLLBACK-WINSRV-20261124_215436.

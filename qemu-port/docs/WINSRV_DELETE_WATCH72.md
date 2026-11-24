# WINSRV_DELETE_WATCH72 — re-apply status (2026-11-24 22:04)

- APPLY_RC: **not yet emitted** — apply2 log ends at `gate[P3]: r1_os_selfcheck.py`; no APPLY_RC/rollback line.
- Process pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still running (~11 min elapsed).
- Git deletes present (staged `D`): user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp/.h, variant/qemu_gui_exec.cpp; Makefile/user.mk/variant edits applied; P1+P2 gates PASS, P3 gate in flight.
- Snapshot/rollback token: ROLLBACK-WINSRV-20261124_215436 at /tmp/opencode/winsrv_retire_backup_20261124_215436.
- git log -2: 0f03f7e (doom e32_platform.c restore), 97f224b (EWSRV migration).
- kernelhwsrv/ untouched; no QEMU run; read-only monitor.

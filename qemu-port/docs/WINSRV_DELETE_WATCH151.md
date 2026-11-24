# WINSRV_DELETE_WATCH151 — re-apply status (2026-11-24)

- APPLY_RC: **not yet** — no APPLY_RC/rollback line in `/tmp/opencode/winsrv_delete_apply2.log` (log 3.3 KB, ends mid-P3 gates); no rollback occurred.
- pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still running, elapsed ~30 min; currently in phase P3 gates (`r1_final_verify.py --strict-skip` last logged).
- Progress: pre-gate ewsrv PASS; P1 (18 edits) gate PASS; P2 (Makefile/user.mk) gate PASS; P3 all 7 deletes done, gates in flight.
- Snapshot/rollback token: `/tmp/opencode/winsrv_retire_backup_20261124_215436`, `ROLLBACK-WINSRV-20261124_215436`.
- `git status --short | grep -cE '^ ?D'` = **7** (guishost.h, winsrv.c, wsclient.c/.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp).
- HEAD: `0f03f7e` Restore doom e32_platform.c EWSRV hunk; parent `97f224b` Migrate GUI apps off winsrv.
- kernelhwsrv/ untouched (script asserts clean pre/post); no QEMU run by this watcher; read-only except this report.

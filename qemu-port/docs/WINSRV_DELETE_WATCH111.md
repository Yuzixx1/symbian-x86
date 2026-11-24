# WINSRV_DELETE_WATCH111 — re-apply status (read-only monitor)

- APPLY_RC: **NOT YET** — no APPLY_RC/rollback line in /tmp/opencode/winsrv_delete_apply2.log (3,162 bytes, not >50 MB).
- pid 1286837 **alive** (etime 20:44): `python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`; log ends at phase P3 gate list (make/selfcheck/smoke/ewsrv/input/doom* tests) — still running, no rollback.
- Progress so far: pre-apply ewsrv gate PASS, P1 (18 edits) PASS, P2 (Makefile/user.mk edits + `make isogrub`) PASS, 7 deletes applied (winsrv.c, guishost.h, wsclient.c/.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp); `git status --short | grep -cE '^ ?D'` = **7**.
- Snapshot/rollback token: /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).
- kernelhwsrv/ untouched (0 modified/deleted entries). Latest commits: 0f03f7e, 97f224b. No QEMU run by this monitor.

# WINSRV_DELETE_WATCH140 — re-apply status (2026-11-24)

- APPLY_RC: **NOT YET** — no APPLY_RC/rollback line in `/tmp/opencode/winsrv_delete_apply2.log` (3,355 bytes; apply still in flight).
- PID 1286837 alive (27+ min): `python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`.
- Last log line: gate[P3] running `r1_final_verify.py --strict-skip`; P1 (edits) and P2 (Makefile/user.mk) gates PASS, all 7 file deletions applied.
- `git status --short | grep -cE '^ ?D'` = **7** (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp).
- `git log --oneline -2`: 0f03f7e, 97f224b. Snapshot: `/tmp/opencode/winsrv_retire_backup_20261124_215436` (ROLLBACK-WINSRV-20261124_215436).
- kernelhwsrv/ untouched (log: "kernelhwsrv/ clean: clean"); no QEMU run by this watch.

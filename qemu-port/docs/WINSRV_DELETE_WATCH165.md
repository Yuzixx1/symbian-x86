# WINSRV_DELETE_WATCH165 — re-apply status (2026-11-24 ~22:50)

- APPLY_RC: **NOT YET** — apply still running. `/tmp/opencode/winsrv_delete_apply2.log` (3.3 KB, well under 50 MB) ends at `+ gate[P3]: r1_final_verify.py --strict-skip`; no APPLY_RC/rollback line yet.
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): alive, 32:00 elapsed, state S.
- Progress: P1 (18 edits) and P2 (Makefile/user.mk) gates PASS; all 7 deletes done (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp); P3 full gate suite in progress.
- Rollback token if needed: `ROLLBACK-WINSRV-20261124_215436` (snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`).
- `git status --short | grep -cE '^ ?D'` = **7** staged/visible deletions; HEAD = `0f03f7e` (doom e32_platform.c restore), parent `97f224b`.
- `kernelhwsrv/` untouched (log: "kernelhwsrv/ clean" at every gate). No QEMU run by this watcher.

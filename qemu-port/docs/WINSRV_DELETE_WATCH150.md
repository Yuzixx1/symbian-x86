# WINSRV_DELETE_WATCH150 — re-apply status (2026-11-24)

- GOAL: APPLY_RC not yet reached — re-apply STILL RUNNING.
- Log /tmp/opencode/winsrv_delete_apply2.log (3.3 KB, read in full): ends at last P3 gate `r1_final_verify.py --strict-skip`; no APPLY_RC / ROLLBACK / FAIL lines.
- pid 1286837 alive (30+ min): `python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`.
- Progress: P1 (12 edits) and P2 (5 edits) gates PASS; 7 deletes done (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp); `git status` shows 7 deleted entries.
- Snapshot/rollback token: ROLLBACK-WINSRV-20261124_215436 at /tmp/opencode/winsrv_retire_backup_20261124_215436.
- HEAD: 0f03f7e (97f224b migrate GUI apps off winsrv). kernelhwsrv/ untouched (git status clean).

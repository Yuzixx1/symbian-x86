# WINSRV_DELETE_WATCH127 — re-apply status (2026-11-24)

- APPLY_RC: NOT YET — no APPLY_RC marker in /tmp/opencode/winsrv_delete_apply2.log; log ends at P3 gate list, apply still in flight.
- Process: pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) alive, elapsed 24:37.
- Progress: P1 (18 edits) PASS, P2 (Makefile/user.mk) PASS, P3 (7 deletes: winsrv.c, guishost.h, wsclient.{c,h}, qemu_windowsrv.{cpp,h}, qemu_gui_exec.cpp) gates running.
- Rollback: none; snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436).
- git: 7 deletions in `git status --short`; HEAD 0f03f7e / 97f224b. kernelhwsrv/ clean/untouched. No QEMU run by this watcher.

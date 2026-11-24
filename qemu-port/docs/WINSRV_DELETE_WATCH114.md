# WINSRV delete watch #114 — re-apply status (2026-11-24)

- APPLY_RC: **not yet emitted** — `/tmp/opencode/winsrv_delete_apply2.log` ends at the P3 gate list; no APPLY_RC, no rollback.
- pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still running (~21 min), currently in P3 gate battery.
- Progress so far: 18 edits (P1) + Makefile/user.mk edits (P2) applied; P1/P2 gates PASS (make, selfcheck, ewsrv retire gate).
- 7 deletes present in worktree (`git status`: winsrv.c, guishost.h, wsclient.c/.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp) — matches the 7-op delete set.
- Snapshot/rollback token: `ROLLBACK-WINSRV-20261124_215436` at `/tmp/opencode/winsrv_retire_backup_20261124_215436`; not invoked.
- kernelhwsrv/: 0 modified files — untouched. No commit/push made.

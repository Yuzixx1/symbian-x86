# WINSRV_DELETE_WATCH78 — re-apply status (2026-11-24)

- APPLY_RC: **not yet emitted** — apply still running; log ends at `gate[P3]: make` / selfcheck (log 2767 B, unchanged since 22:04).
- pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): alive, ~12 min elapsed.
- Rollback: none — snapshot taken at `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436), no rollback line in log.
- Gates so far: pre-apply ewsrv PASS, P1 PASS (make+selfcheck+ewsrv), P2 PASS (isogrub+selfcheck+ewsrv); P3 in progress.
- `git status --short | grep -cE '^ ?D'` = **7** (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp).
- HEAD: `0f03f7e` (doom e32_platform restore); prior `97f224b`.
- kernelhwsrv/ untouched: `git status --short -- kernelhwsrv/` = 0 entries. No commits made.

# WINSRV_DELETE_WATCH85 — re-apply status (2026-11-24)

- APPLY_RC: **NOT YET EMITTED** — apply still in flight.
- pid 1286837 alive (etime ~15 min): `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`.
- Log `/tmp/opencode/winsrv_delete_apply2.log` (2.8 KB): P1 gate PASS, P2 gate PASS (make isogrub + selfcheck + ewsrv retire gate), 7 deletes done, P3 gate in progress (make/selfcheck/smoke/ewsrv test running). No rollback lines.
- `git status --short | grep -cE '^ ?D'` = **7** (winsrv.c, wsclient.c/.h, guishost.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp).
- git log -2: `0f03f7e` (doom e32_platform.c restore), `97f224b` (GUI apps off winsrv onto EWSRV).
- kernelhwsrv/: **0 changes** (clean). Snapshot/rollback token: `ROLLBACK-WINSRV-20261124_215436` at `/tmp/opencode/winsrv_retire_backup_20261124_215436`.
- Conclusion: re-apply NOT finished; awaiting P3 gate + APPLY_RC. No QEMU run by this watcher.

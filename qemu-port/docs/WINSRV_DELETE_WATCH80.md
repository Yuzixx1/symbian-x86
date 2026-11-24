# WINSRV_DELETE_WATCH80 — re-apply status (2026-11-24 ~22:17)

- **APPLY_RC: NOT YET EMITTED** — re-apply still running; no rollback triggered.
- PID 1286837 alive (12m42s): `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`.
- Log `/tmp/opencode/winsrv_delete_apply2.log` (2.7 KB, far under 50 MB): 25 ops (18 edits, 7 deletes) applied; P1 gate PASS, P2 gate PASS; now at **P3 gate** (`make` + `r1_os_selfcheck.py` running).
- Pre-apply EWSRV retire gate: PASS. Snapshot: `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436).
- git: 7 staged deletes (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp); HEAD = 0f03f7e (apply does not commit).
- `kernelhwsrv/` untouched (git status clean there).

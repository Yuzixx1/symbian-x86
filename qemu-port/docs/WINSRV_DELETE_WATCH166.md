# WINSRV_DELETE_WATCH166 — re-apply status (2026-11-24 ~22:49)

**APPLY_RC: NOT YET EMITTED — apply still running.**

- pid 1286837 alive (31:56 elapsed, state S): `python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`
- Log `/tmp/opencode/winsrv_delete_apply2.log` (3,355 bytes, well under 50 MB): phases P1+P2 PASS (make, isogrub, selfcheck, ewsrv retire gate all PASS); 7 deletes applied (winsrv.c, guishost.h, wsclient.c/.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp); currently inside phase P3 gate list (smoke, ewsrv/input, doom*, fullchain, final_verify) — no APPLY_RC/rollback line yet.
- Rollback snapshot ready: `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436). No rollback triggered.
- `git status --short | grep -cE '^ ?D'` = 7 (the 7 winsrv deletions); HEAD = `0f03f7e`, prev `97f224b`.
- `kernelhwsrv/` dirty count: 0 — untouched.

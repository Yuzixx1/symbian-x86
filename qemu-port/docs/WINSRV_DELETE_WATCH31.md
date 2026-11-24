# WINSRV_DELETE_WATCH31 — 2026-11-24 ~21:56

Re-apply (winsrv_delete_apply2.log) **STILL RUNNING, no APPLY_RC yet** — log ends at `gate[P1]: make` / `r1_os_selfcheck.py`; pre-apply ewsrv gate PASS; snapshot `winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436).

- pid 1286837 alive (python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV, ~1m45s).
- No deletes in git: `git status` shows zero `D` entries for winsrv/guishost/wsclient/qemu_windowsrv/gui_exec; all tracked files still on disk. Only modified file is `qemu-port/tools/r1_winsrv_delete.py`.
- HEAD: `0f03f7e` (doom e32_platform.c restore) on top of `97f224b` (GUI apps off winsrv onto genuine EWSRV).
- kernelhwsrv/ untouched (git status clean).

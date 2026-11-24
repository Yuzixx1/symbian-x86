# WINSRV_DELETE_WATCH125 (2026-11-24)

**Question: did the re-apply finish (APPLY_RC)?** — **No, still running.**

- Log `/tmp/opencode/winsrv_delete_apply2.log` (56 lines, 3.3 KB): no `APPLY_RC`, no rollback; last line is `gate[P3]: r1_final_verify.py --strict-skip` in progress.
- Pid 1286837 alive (etime 24:14): `python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`.
- `git status --short | grep -cE '^ ?D'` = **7** staged deletions (winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp); P1/P2 gates PASS, P3 gates running.
- `git log --oneline -2`: `0f03f7e` (doom e32_platform.c restore), `97f224b` (GUI apps off winsrv).
- Snapshot/rollback token: `ROLLBACK-WINSRV-20261124_215436` at `/tmp/opencode/winsrv_retire_backup_20261124_215436`.
- `kernelhwsrv/` untouched (git status clean there).

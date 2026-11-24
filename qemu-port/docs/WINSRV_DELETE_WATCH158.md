# WINSRV_DELETE_WATCH158 — re-apply status (read-only monitor)

- **APPLY_RC: NOT YET EMITTED** — `/tmp/opencode/winsrv_delete_apply2.log` (3.4 KB, well under 50 MB) contains 0 `APPLY_RC` lines; the only `ROLLBACK` match is the snapshot token line, i.e. **no rollback occurred**.
- Process `pid 1286837` (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still alive, elapsed 31:19; last log line is `+ gate[P3]: r1_final_verify.py --strict-skip` — currently inside the P3 gate suite.
- Progress so far: pre-apply ewsrv gate PASS, P1 gate PASS (9 edits), P2 gate PASS (Makefile/user.mk edits), 7 files deleted (winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp).
- `git status --short | grep -cE '^ ?D'` = **7**; `git log --oneline -2`: `0f03f7e` (restore doom e32_platform.c hunk), `97f224b` (migrate GUI apps to EWSRV).
- Snapshot for rollback: `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token `ROLLBACK-WINSRV-20261124_215436`).
- `kernelhwsrv/` untouched: 0 modified/deleted lines under it. No QEMU run, no commits, no edits outside this report.

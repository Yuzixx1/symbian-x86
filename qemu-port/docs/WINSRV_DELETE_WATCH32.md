# WINSRV_DELETE_WATCH32 — re-apply status (2026-11-24)

- APPLY_RC: **not yet** — apply still running (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`, ~2 min elapsed).
- Log `/tmp/opencode/winsrv_delete_apply2.log` (1.6 KB, well under 50 MB): 25 ops (18 edits, 7 deletes); pre-gate ewsrv PASS; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`; 13 edits applied so far; now in gate[P1] `make` + `r1_os_selfcheck.py`. No rollback recorded.
- Deletes in git: **none yet** — `variant/qemu_windowsrv.cpp/.h` still present; `git status` shows only the 5 edited variant files + tool/docs, no `D` entries for winsrv/guishost/qemu_windowsrv/gui_exec.
- HEAD: `0f03f7e` (doom e32_platform.c restore), parent `97f224b` (winsrv→EWSRV migration). kernelhwsrv/ clean.

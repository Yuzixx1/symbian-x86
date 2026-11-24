# WINSRV_DELETE_WATCH179 — apply status (2026-11-24 22:27 EET)

- APPLY_RC: **not yet emitted** — apply still running (pid 1286837 alive, 32:45 elapsed); last log line is `+ gate[P3]: r1_final_verify.py --strict-skip`. No rollback triggered.
- Log: `/tmp/opencode/winsrv_delete_apply2.log` (3.3 KB, not truncated); snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, token `ROLLBACK-WINSRV-20261124_215436`.
- Deletes confirmed: `git status --short | grep -cE '^ ?D'` = **7** (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp).
- `kernelhwsrv/` untouched: `git status --short kernelhwsrv/ | wc -l` = **0**.
- HEAD: `0f03f7e` (restore doom e32_platform.c hunk) on top of `97f224b` (GUI→EWSRV migration). Phases P1+P2 gates passed; P3 gate in progress.

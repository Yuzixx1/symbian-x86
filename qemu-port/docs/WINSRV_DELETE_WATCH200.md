# WINSRV_DELETE_WATCH200 — winsrv re-apply status (2026-11-24)

- Re-apply finished: **YES** — `APPLY_RC=0` in `/tmp/opencode/winsrv_delete_apply2.log` (log 4 KB, tail read).
- Result line: "SUCCESS: winsrv deletion applied across P1-P3; all phase gates PASS."
- Gates: pre-apply ewsrv PASS; P1 (make + selfcheck + retire ewsrv) PASS; P2 (isogrub + selfcheck + retire ewsrv) PASS; P3 (selfcheck, smoke, ewsrv, input, doom ×4, fullchain, retire, final_verify --strict-skip) PASS.
- 7 deletes confirmed via `git status --short | grep -cE '^ ?D'` = **7** (winsrv.c, guishost.h, wsclient.c, wsclient.h, qemu_windowsrv.cpp, qemu_windowsrv.h, qemu_gui_exec.cpp).
- pid 1286837: **gone** (apply process exited after APPLY_RC=0).
- kernelhwsrv/ untouched: `git status --short kernelhwsrv/ | wc -l` = **0**.
- HEAD: `0f03f7e` (doom e32_platform.c restore) on top of `97f224b` (winsrv migration). No rollback triggered.
- Rollback snapshot retained: `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436).
- No QEMU runs performed by this watch; read-only monitor, no commits.

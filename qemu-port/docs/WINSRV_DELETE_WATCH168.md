# WINSRV_DELETE_WATCH168 — re-apply status (2026-11-24)

- **APPLY_RC: NOT YET EMITTED** — `/tmp/opencode/winsrv_delete_apply2.log` (3.4 KB) ends at line 56 mid-P3 gate (`r1_final_verify.py --strict-skip`); no `APPLY_RC=`/rollback line, so the apply is still IN PROGRESS, no rollback triggered.
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still alive, elapsed 32:01.
- `git status --short | grep -cE '^ ?D'` = **7** unstaged deletions, all in the winsrv deletion set: `user/{winsrv.c,guishost.h,wsclient.c,wsclient.h}`, `variant/{qemu_windowsrv.cpp,qemu_windowsrv.h,qemu_gui_exec.cpp}`.
- P1 and P2 phases logged PASS (make + selfcheck + ewsrv retire gate; isogrub + gates); pre-apply ewsrv gate PASS; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (rollback token `ROLLBACK-WINSRV-20261124_215436`).
- HEAD: `0f03f7e` (restore doom e32_platform.c hunk), parent `97f224b` (winsrv→EWSRV migration). `kernelhwsrv/` clean/untouched; no QEMU run by this watcher.

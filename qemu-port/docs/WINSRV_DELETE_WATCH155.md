# WINSRV_DELETE_WATCH155 — re-apply status (2026-11-24 ~22:49)

- APPLY_RC: **NOT YET EMITTED** — apply still in flight, currently in phase P3 gates (`r1_final_verify.py --strict-skip` is the last gate queued). No rollback triggered.
- Process: pid 1286837 alive (~31 min elapsed), `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`; log `/tmp/opencode/winsrv_delete_apply2.log` (3.4 KB, tailed).
- Gates so far: pre-apply ewsrv PASS, P1 (make + selfcheck + ewsrv retire) PASS, P2 (make isogrub + selfcheck + ewsrv retire) PASS; P3 running.
- Deletions staged in worktree: 7 files (`git status '^ ?D'` count = 7): winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp — matches the 7-delete op set.
- Snapshot/rollback token: `ROLLBACK-WINSRV-20261124_215436` (`/tmp/opencode/winsrv_retire_backup_20261124_215436`).
- kernelhwsrv/: clean, 0 changes — untouched.
- HEAD: `0f03f7e` (doom e32_platform.c restore) on top of `97f224b` (GUI→EWSRV migration). No commit/push made.

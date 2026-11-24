# WINSRV_DELETE_WATCH28 — re-apply monitor (2026-11-24)

- Verdict: YES — re-apply finished; all 7 deletions landed. APPLY_RC=0, pid 1286837 exited.
- Log /tmp/opencode/winsrv_delete_apply2.log: 25 ops (18 edits, 7 deletes); P1/P2/P3 gates all PASS incl. r1_final_verify.py --strict-skip.
- Deleted (on disk + `git status` D): user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp, variant/qemu_windowsrv.h, variant/qemu_gui_exec.cpp.
- Backup: /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436).
- HEAD: 0f03f7e; kernelhwsrv/ untouched (clean status, no diff). No commit/push made.

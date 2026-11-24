# WINSRV_DELETE_WATCH178 — re-apply status (read-only monitor, 2026-11-24)

- APPLY_RC: NOT YET — pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still running (~32 min); log `/tmp/opencode/winsrv_delete_apply2.log` (3.3 KB) ends at line 56 mid-P3 gates (`r1_final_verify.py --strict-skip`); no rollback triggered; snapshot `winsrv_retire_backup_20261124_215436` on standby.
- Gates so far: pre-ewsrv PASS, P1 (make+selfcheck+ewsrv) PASS, P2 (isogrub+selfcheck+ewsrv) PASS; P3 full gate suite in progress.
- Deletes confirmed: 7 (`git status --short | grep -cE '^ ?D'` = 7): user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp, variant/qemu_windowsrv.h, variant/qemu_gui_exec.cpp.
- kernelhwsrv/: clean — `git status --short kernelhwsrv/ | wc -l` = 0; log line 2/5 also report clean.
- HEAD: 0f03f7e (doom e32_platform.c restore), parent 97f224b (GUI→EWSRV migration). No commit/push made.

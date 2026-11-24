# WINSRV_DELETE_WATCH183 — re-apply status (2026-11-24)

- APPLY_RC: NOT YET — apply still running (pid 1286837, ~33 min); log ends at P3 gate `r1_final_verify.py --strict-skip`. No rollback so far (snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, token ROLLBACK-WINSRV-20261124_215436).
- Gates so far: pre-ewsrv PASS, P1 (make+selfcheck+ewsrv) PASS, P2 (isogrub+selfcheck+ewsrv) PASS; P3 gates in progress.
- Deletes confirmed: `git status --short | grep -cE '^ ?D'` = 7 (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp).
- kernelhwsrv/: `git status --short kernelhwsrv/ | wc -l` = 0 — untouched.
- HEAD: 0f03f7e (doom e32_platform.c restore on top of 97f224b winsrv migration).

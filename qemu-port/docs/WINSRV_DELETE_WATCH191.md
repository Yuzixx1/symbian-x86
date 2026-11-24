# WINSRV_DELETE_WATCH191 — re-apply status (2026-11-24 ~22:50)

- APPLY_RC: **NOT YET EMITTED** — apply still in flight (pid 1286837 alive, 33:44 elapsed); log ends at P3 gate list, no APPLY_RC/rollback line. No rollback observed.
- Deletes confirmed: 7 staged deletions (`git status` D count = 7): winsrv.c, wsclient.c/.h, guishost.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp.
- kernelhwsrv/: clean — `git status --short kernelhwsrv/ | wc -l` = 0; log also reports "kernelhwsrv/ clean".
- Gates so far: pre-apply ewsrv PASS; P1 (make+selfcheck+ewsrv) PASS; P2 (isogrub+selfcheck+ewsrv) PASS; P3 gate suite launched, pending.
- HEAD: 0f03f7e (doom e32_platform.c restore) on 97f224b. Rollback token: ROLLBACK-WINSRV-20261124_215436 (snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436).

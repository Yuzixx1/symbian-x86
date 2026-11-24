# WINSRV_DELETE_WATCH163 — re-apply status (2026-11-24)

- APPLY_RC: **not yet emitted** — apply still running (pid 1286837 alive, ~32 min).
- Log `/tmp/opencode/winsrv_delete_apply2.log`: P1 and P2 gates PASS; 7 winsrv files deleted; now in P3 gate (r1_final_verify.py --strict-skip in flight). No rollback triggered.
- Git: 7 deletions staged (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp); HEAD `0f03f7e`.
- kernelhwsrv/ clean/untouched. Rollback token: ROLLBACK-WINSRV-20261124_215436 (snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436).

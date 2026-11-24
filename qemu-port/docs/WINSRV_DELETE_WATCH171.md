# WINSRV_DELETE_WATCH171 — re-apply status (2026-11-24 ~22:49)

- APPLY_RC: NOT YET EMITTED — apply still running (pid 1286837, ~32 min elapsed, in gate[P3] full-verify phase of /tmp/opencode/winsrv_delete_apply2.log, 56 lines).
- No rollback triggered; snapshot taken: /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436).
- All 7 deletes present in git status: winsrv.c, wsclient.c/.h, guishost.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp (count = 7).
- kernelhwsrv/ untouched: `git status --short kernelhwsrv/` = 0 lines; log confirms "kernelhwsrv/ clean: clean" pre- and post-edit.
- Gates so far: pre-apply ewsrv PASS, P1 PASS, P2 PASS; P3 (make, selfcheck, smoke, ewsrv, input, doom*, fullchain, retire, final_verify --strict-skip) in flight.
- HEAD: 0f03f7e (doom e32_platform.c restore on top of 97f224b winsrv migration).

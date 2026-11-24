# WINSRV_DELETE_WATCH180 — apply2 status (2026-11-24 ~22:50)

- APPLY_RC: **NOT YET** — apply still running (pid 1286837, ~33 min elapsed); log ends at line 56, P3 gate list in progress (make, selfcheck, smoke, ewsrv, input, doom*, fullchain, retire, final_verify). No rollback triggered.
- Phases P1 (18 edits) and P2 (Makefile/user.mk edits) gates: PASS; pre-apply ewsrv gate: PASS.
- Deletes confirmed: 7 files staged as deleted (`git status` `D` count = 7): winsrv.c, guishost.h, wsclient.c, wsclient.h, qemu_windowsrv.cpp, qemu_windowsrv.h, qemu_gui_exec.cpp.
- kernelhwsrv/: clean — `git status --short kernelhwsrv/` = 0 lines (untouched).
- git log -2: 0f03f7e (doom e32_platform.c restore), 97f224b (EWSRV migration). No new commit from the apply yet.
- Snapshot/rollback token on file: ROLLBACK-WINSRV-20261124_215436 at /tmp/opencode/winsrv_retire_backup_20261124_215436.

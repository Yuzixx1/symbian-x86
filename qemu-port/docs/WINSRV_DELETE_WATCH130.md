# WINSRV_DELETE_WATCH130 — re-apply status (2026-11-24)

- APPLY_RC: NOT YET WRITTEN — apply still running; log ends at phase P3 gate list, no final APPLY_RC/rollback line.
- Process: pid 1286837 alive (25 min elapsed), `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`; no rollback triggered so far.
- Progress: P1 (18 edits) and P2 (Makefile/user.mk) gates PASS; 7 files deleted in git status (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp); P3 full gate suite (selfcheck, smoke, ewsrv, input, doom x4, fullchain, retire, final_verify) in flight.
- kernelhwsrv/ clean per pre-apply check; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, rollback token `ROLLBACK-WINSRV-20261124_215436`.
- Head: 0f03f7e; working tree shows expected M/D set plus untracked qemu-slot.lock. Read-only monitor; no edits outside this file.

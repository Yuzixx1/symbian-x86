# WINSRV_DELETE_WATCH121 — re-apply status (2026-11-24)

- APPLY_RC: **not yet emitted** — apply still running; log ends at `+ gate[P3]: r1_final_verify.py --strict-skip` (last P3 gate in flight). No rollback triggered.
- Process: pid 1286837 alive (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`, ~24 min elapsed).
- Log: `/tmp/opencode/winsrv_delete_apply2.log` (3.3 KB, fully read). P1 (18 edits) and P2 (Makefile/user.mk) gates PASS; 7 files deleted (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp); `git status` shows 7 deletions.
- Snapshot/rollback token: `ROLLBACK-WINSRV-20261124_215436` at `/tmp/opencode/winsrv_retire_backup_20261124_215436`.
- HEAD: `0f03f7e`; `kernelhwsrv/` clean/untouched. No QEMU run by this watcher.

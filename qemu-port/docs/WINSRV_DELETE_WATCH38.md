# WINSRV_DELETE_WATCH38 — 2026-11-24 ~21:58

- Re-apply RUNNING, not finished: pid 1286837 alive (3m32s), log ends mid-`gate[P1]` (make + r1_os_selfcheck). **No APPLY_RC, no rollback** in /tmp/opencode/winsrv_delete_apply2.log (1.6 KB total).
- Pre-apply gate ewsrv PASS; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436).
- **No deletes in git**: `git status` shows zero `D` entries; winsrv/guishost/qemu_windowsrv sources still on disk; only `tools/r1_winsrv_delete.py` modified + WATCH docs untracked.
- HEAD: 0f03f7e (doom e32_platform.c restore); prior 97f224b (EWSRV migration). kernelhwsrv/ untouched.

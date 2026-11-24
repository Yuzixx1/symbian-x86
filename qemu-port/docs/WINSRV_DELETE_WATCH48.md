# WINSRV_DELETE_WATCH48 — re-apply status (2026-11-24 ~22:05)

- APPLY_RC: NOT present. `winsrv_delete_apply2.log` (25 lines, 1.8 KB) ends mid-P1-gate (make → selfcheck → ewsrv retire gate); pid 1286837 still running (etime ~5:52), wrapper 1286836 will append `APPLY_RC=$?` on exit.
- Git deletes: NONE. `git status --short` shows 0 `D` entries; all winsrv targets still tracked/present (user/winsrv.c, user/wsclient.[ch], user/guishost.h, variant/qemu_windowsrv.[ch], variant/qemu_gui_exec.cpp). Only `M qemu-port/tools/r1_winsrv_delete.py` + untracked watch docs.
- Pre-apply state OK: ewsrv gate PASS, snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436), kernelhwsrv/ clean, 18 edits applied so far, 7 deletes pending.
- Note: a second concurrent apply exists (pid 1298363 → /tmp/opencode/winsrv_apply.log); watch for log clobbering/index races (cf. 0f03f7e).
- HEAD: 0f03f7e (parent 97f224b). kernelhwsrv/ untouched. No QEMU run.

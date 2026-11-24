# WINSRV_DELETE_WATCH164 — re-apply status (read-only monitor)

- APPLY_RC: **not yet emitted** — apply still running; log ends at `gate[P3]: r1_final_verify.py --strict-skip` (no PASS/FAIL/rollback line after it).
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): **alive**, elapsed ~31:45, currently in P3 full-gate phase.
- Deletions staged: **7** (`git status --short | grep -cE '^ ?D'` = 7): user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp/.h, variant/qemu_gui_exec.cpp.
- P1/P2 gates already PASS (make + selfcheck + ewsrv retire gate each phase); 18 edits + 7 deletes applied, all allowlisted; pre-apply ewsrv gate PASS.
- Rollback: none triggered; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token ROLLBACK-WINSRV-20261124_215436) intact.
- kernelhwsrv/: **untouched** (log reports clean; no edits outside qemu-port/).
- git log -2: `0f03f7e` (doom e32_platform.c restore), `97f224b` (GUI apps off winsrv onto EWSRV).

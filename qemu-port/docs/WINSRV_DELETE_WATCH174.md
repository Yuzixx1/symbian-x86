# WINSRV_DELETE_WATCH174 — re-apply status (2026-11-24 22:27 EET)

- APPLY_RC: **not yet emitted** — `/tmp/opencode/winsrv_delete_apply2.log` (3355 B, last write 22:17) ends at `+ gate[P3]: r1_final_verify.py --strict-skip`; no APPLY_RC, no ROLLBACK/FAIL/Traceback lines.
- Apply process pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still alive, ~32 min elapsed; P3 gate battery in progress.
- Progress so far: 18 edits + 7 deletes applied; P1 and P2 gates PASS (make, selfcheck, ewsrv retire gate, isogrub).
- 7 deletes confirmed in `git status`: winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp (`git status --short | grep -cE '^ ?D'` = 7).
- `kernelhwsrv/` clean: `git status --short kernelhwsrv/ | wc -l` = 0, no diff. HEAD: 0f03f7e, 97f224b.
- Snapshot/rollback token: `ROLLBACK-WINSRV-20261124_215436` (`/tmp/opencode/winsrv_retire_backup_20261124_215436`).
- Read-only monitor; no edits besides this report; no QEMU run.

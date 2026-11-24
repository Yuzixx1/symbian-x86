# WINSRV_DELETE_WATCH161 — re-apply status (read-only monitor)

- APPLY_RC: **NOT FINISHED** — pid 1286837 still running (31m37s), log ends at `+ gate[P3]: r1_final_verify.py --strict-skip` (final gate in flight). No APPLY_RC line, no rollback.
- Deletions staged: 7 (`git status` `^ ?D` count = 7) — winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp, all under qemu-port/.
- Phases P1/P2 gates PASS (make, isogrub, selfcheck, ewsrv retire gate); P3 gates started. Snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, token `ROLLBACK-WINSRV-20261124_215436`.
- kernelhwsrv/: clean/untouched. HEAD: 0f03f7e on top of 97f224b (winsrv migration).

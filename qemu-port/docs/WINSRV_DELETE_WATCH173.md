# WINSRV_DELETE_WATCH173 — re-apply status (2026-11-24 22:25 UTC)

- APPLY_RC: NOT YET WRITTEN — pid 1286837 (r1_winsrv_delete.py --apply) still alive (32m+), currently in P3 gate: r1_final_verify.py --strict-skip running (pid 1376334). No rollback lines in log; no APPLY_RC= line yet.
- Log: /tmp/opencode/winsrv_delete_apply2.log — P1 PASS (18 edits), P2 PASS (Makefile/user.mk), 7 deletes applied, P3 gates in progress. Snapshot: /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).
- Deletes confirmed: git status shows exactly 7 `D` entries (winsrv.c, guishost.h, wsclient.c, wsclient.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp).
- kernelhwsrv/: clean — `git status --short kernelhwsrv/` = 0 lines; untouched.
- HEAD: 0f03f7e (working tree holds the uncommitted delete set; no commit made).

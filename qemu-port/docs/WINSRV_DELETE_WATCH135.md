# WINSRV_DELETE_WATCH135 — re-apply status (2026-11-24 22:20 UTC)

- APPLY_RC: **not yet written** — pid 1286837 still running (~26 min), log ends at line 56 (gate P3 running `r1_final_verify.py --strict-skip`); no APPLY_RC/rollback line in log.
- Progress: P1 + P2 gates PASS; 18 edits applied; 7 files deleted (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp) — `git status --short | grep -cE '^ ?D'` = 7.
- Rollback available if needed: snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, token `ROLLBACK-WINSRV-20261124_215436`.
- kernelhwsrv/: clean/untouched (pre-flight + P6 checks in log all "clean").
- HEAD: 0f03f7e (restore doom e32_platform.c) on 97f224b (migrate GUI apps off winsrv).
- Verdict: re-apply **in progress, no failure/rollback so far**; final verdict awaits APPLY_RC.

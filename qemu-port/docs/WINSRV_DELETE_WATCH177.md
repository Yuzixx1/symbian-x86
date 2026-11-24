# WINSRV_DELETE_WATCH177 — re-apply status (2026-11-24)

- APPLY_RC: NOT YET WRITTEN — apply still in flight; pid 1286837 alive (32m41s), log ends at `+ gate[P3]` battery (final_verify last), no rollback lines.
- Log: /tmp/opencode/winsrv_delete_apply2.log (4 KB, fully read). Pre-gates PASS, P1/P2 gates PASS, snapshot ROLLBACK-WINSRV-20261124_215436 taken.
- Deletes confirmed: exactly 7 (`git status --short | grep -cE '^ ?D'` = 7): winsrv.c, wsclient.c/.h, guishost.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp.
- kernelhwsrv/: clean — `git status --short kernelhwsrv/ | wc -l` = 0; log line 2/5 also report clean.
- HEAD: 0f03f7e (doom EWSRV hunk restore) on 97f224b (winsrv→EWSRV migration); no new commit from this apply yet.

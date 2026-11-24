# WINSRV delete watch 107 — re-apply status

- APPLY_RC: **not yet emitted** — apply still running (pid 1286837 alive, ~20 min); log ends mid-P3 gates (doom/audio/playtest pending), no rollback, no FAIL/Traceback.
- Log: /tmp/opencode/winsrv_delete_apply2.log (3.1 KB, fully read). P1 + P2 gates PASS (ewsrv retire gate, selfcheck, isogrub); 7 deletes applied (winsrv.c, wsclient.{c,h}, guishost.h, qemu_windowsrv.{cpp,h}, qemu_gui_exec.cpp).
- git: 7 deletions in `git status --short` (`^ ?D` count = 7); HEAD = 0f03f7e (top), parent 97f224b; working tree dirty as expected mid-apply.
- Snapshot/rollback token: ROLLBACK-WINSRV-20261124_215436 at /tmp/opencode/winsrv_retire_backup_20261124_215436.
- kernelhwsrv/ untouched (apply log self-reports clean); no QEMU run by this watch.

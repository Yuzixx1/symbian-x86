# WINSRV_DELETE_WATCH152 — re-apply status (2026-11-24 ~22:47)

- APPLY_RC: NOT YET — apply still running, no APPLY_RC/rollback line in log tail.
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): alive, 30:23 elapsed.
- Log: /tmp/opencode/winsrv_delete_apply2.log (3.3 KB, fully tailed). P1+P2 gates PASS; P3 gates (selfcheck, smoke, ewsrv, input, doom x4, fullchain, retire, final_verify --strict-skip) in progress.
- git: 7 staged deletions (winsrv.c, wsclient.c/.h, guishost.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp); HEAD 0f03f7e / 97f224b.
- kernelhwsrv/: untouched (git diff HEAD = 0 lines). Snapshot/rollback token: ROLLBACK-WINSRV-20261124_215436.

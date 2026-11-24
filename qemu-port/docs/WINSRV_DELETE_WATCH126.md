# WINSRV_DELETE_WATCH126 — re-apply status (read-only monitor)

- APPLY_RC: **not yet written** — apply still running. Log ends at `+ gate[P3]: r1_final_verify.py --strict-skip` (last P3 gate); no APPLY_RC, no ROLLBACK in log.
- PID 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): alive, 24m27s elapsed, state S.
- Deletions staged: 7 (`git status --short | grep -cE '^ ?D'` = 7): winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp.
- Gates so far: pre-apply ewsrv retire PASS; P1 (make+selfcheck+ewsrv retire) PASS; P2 (isogrub+selfcheck+ewsrv retire) PASS; P3 in flight.
- Snapshot/rollback token: `/tmp/opencode/winsrv_retire_backup_20261124_215436` (ROLLBACK-WINSRV-20261124_215436).
- HEAD: 0f03f7e; kernelhwsrv/ untouched (clean per apply log). No QEMU run by this monitor.

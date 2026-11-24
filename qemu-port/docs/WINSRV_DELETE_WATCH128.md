# WINSRV_DELETE_WATCH128 — re-apply status

- APPLY_RC: **not yet written** — apply still running (pid 1286837, ~24 min; bash wrapper 1286836 appends `APPLY_RC=$?` on exit).
- Phase P3 in flight: last line is `+ gate[P3]: r1_final_verify.py --strict-skip` (child pid 1376334 active). No rollback triggered.
- Gates so far: pre-apply ewsrv PASS; P1 (make+selfcheck+ewsrv) PASS; P2 (isogrub+selfcheck+ewsrv) PASS; P3 pending.
- git status --short: 7 deletions staged (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.c/h, qemu_gui_exec.cpp); HEAD 0f03f7e.
- Log tiny (3.3 KB, no >50 MB skip needed). Snapshot: /tmp/opencode/winsrv_retire_backup_20261124_215436 (ROLLBACK-WINSRV-20261124_215436).
- kernelhwsrv/ untouched (log confirms clean). No QEMU run by this watcher.

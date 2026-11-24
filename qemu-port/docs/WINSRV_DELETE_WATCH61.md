# WINSRV_DELETE_WATCH61 — re-apply status (2026-11-24 ~22:09)

- Re-apply NOT finished: no APPLY_RC/rollback line in /tmp/opencode/winsrv_delete_apply2.log (2.2 KB, full tail read).
- PID 1286837 alive (8m26s, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`); last log line = P2 gate `make isogrub` in progress; P1 gate PASS, pre-apply ewsrv gate PASS.
- Snapshot/rollback token: ROLLBACK-WINSRV-20261124_215436 at /tmp/opencode/winsrv_retire_backup_20261124_215436.
- Git: no deletes yet — winsrv files still tracked/on disk (user/winsrv.c, user/wsclient.[ch], user/guishost.h, variant/qemu_windowsrv.[ch]); only M edits (18 applied so far: variant/*, Makefile, user/user.mk).
- HEAD: 0f03f7e / 97f224b. kernelhwsrv/ clean (no git changes). Monitor only; no QEMU, no edits outside this report.

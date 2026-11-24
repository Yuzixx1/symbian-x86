# WINSRV_DELETE_WATCH50 — re-apply status (2026-11-24 22:00 EET)

- APPLY_RC: NOT YET — /tmp/opencode/winsrv_delete_apply2.log (2.2 KB) ends at phase P2 gate (`make isogrub` + selfcheck); no APPLY_RC/rollback line.
- Process: pid 1286837 alive (06:22 elapsed), `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`.
- Git deletes: NONE — `git status --short` shows no ` D` entries; all winsrv files (user/winsrv.c, wsclient.*, guishost.h, variant/qemu_windowsrv.*, qemu_gui_exec.cpp) still tracked and present.
- git log -2: 0f03f7e (doom e32_platform.c restore), 97f224b (EWSRV migration).
- Pre-apply ewsrv gate: PASS; snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436).
- kernelhwsrv/: untouched (read-only honored). No QEMU run.

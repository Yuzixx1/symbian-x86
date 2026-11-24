# WINSRV_DELETE_WATCH67 — 2026-11-24 ~22:01 UTC

- APPLY_RC: **NOT YET** — /tmp/opencode/winsrv_delete_apply2.log (2.2 KB) ends mid phase P2 gate (`make isogrub` + selfcheck running); no APPLY_RC/rollback line yet.
- Process: pid 1286837 alive (python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV, ~10 min). Note: two other apply runs also alive (pids 1317751, 1323788 → winsrv_apply2.log / winsrv_apply.log) — possible index race.
- Pre-gates so far: ewsrv retire gate PASS pre-apply and PASS after P1; snapshot /tmp/opencode/winsrv_retire_backup_20261124_215436 (token ROLLBACK-WINSRV-20261124_215436); kernelhwsrv/ clean throughout.
- Git deletes: **none yet** — `git status --short` shows only `M qemu-port/tools/r1_winsrv_delete.py` + untracked WATCH docs; no D lines for winsrv/guishost/wsclient/qemu_windowsrv/gui_exec.
- HEAD: 0f03f7e (doom e32_platform.c restore), parent 97f224b (GUI-off-winsrv migrate).
- kernelhwsrv/: untouched (git diff empty). No QEMU run. Read-only watch only.

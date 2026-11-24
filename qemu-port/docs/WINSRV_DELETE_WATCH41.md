# WINSRV_DELETE_WATCH41 — 2026-11-24 21:59 UTC

- APPLY_RC: NOT YET — apply still running (pid 1286837 alive, ~4.5 min); log ends at `+ gate[P1]: make` / selfcheck, no APPLY_RC/rollback line.
- Progress: pre-apply ewsrv gate PASS; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (ROLLBACK-WINSRV-20261124_215436); 18 edits applied to 6 variant files, clean.
- Deletes in git: NONE yet — no `D` entries; qemu_windowsrv.{cpp,h}, user/guishost.h, user/wsclient.{c,h} all still tracked/present (7 deletes pending after gates).
- git log: 0f03f7e (doom e32_platform.c restore), 97f224b (winsrv→EWSRV migration). kernelhwsrv/ untouched. No QEMU run.

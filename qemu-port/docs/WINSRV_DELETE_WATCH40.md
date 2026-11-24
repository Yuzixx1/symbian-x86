# WINSRV_DELETE_WATCH40 — re-apply status (2026-11-24)

- APPLY_RC: NOT yet in log — apply still running, last line = gate[P1] `r1_os_selfcheck.py`; no APPLY_RC/rollback line emitted.
- pid 1286837 alive (~4 min): `python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`.
- Pre-apply ewsrv gate PASS; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (ROLLBACK-WINSRV-20261124_215436).
- 18 edits applied (variant/*); 7 deletes staged in op set but NO `D` entries in `git status` yet.
- git status winsrv-related: 37 hits, all `M` (r1_winsrv_delete.py) + untracked WATCH docs; zero deletions so far.
- HEAD: 0f03f7e (doom e32_platform.c restore); kernelhwsrv/ untouched (git diff empty, log "clean").

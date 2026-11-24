# WINSRV_DELETE_WATCH109 — re-apply status (2026-11-24)

- APPLY_RC: **not yet present** — apply still running; log ends mid-P3 gates
  (make / selfcheck / qemu_smoke / ewsrv / input / doom / doom_audio / doom_playtest).
- pid 1286837 alive (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`, ~20 min).
- git: 7 deletions staged (`winsrv.c`, `wsclient.c/h`, `guishost.h`, `qemu_windowsrv.cpp/h`, `qemu_gui_exec.cpp`); 18 edits across variant/user/Makefile; HEAD `0f03f7e`, prev `97f224b`.
- Pre-apply ewsrv gate PASS; P1/P2 gates PASS; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (ROLLBACK-WINSRV-20261124_215436) exists.
- kernelhwsrv/ untouched (apply log asserts clean); no rollback observed.

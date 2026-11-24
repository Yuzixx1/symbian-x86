# WINSRV_DELETE_WATCH10 — did the direct winsrv deletion apply land?

**NO — the deletion never executed; nothing landed.**

- `git status --short` shows zero tracked deletions for winsrv/guishost/wsclient/qemu_windowsrv/gui_exec; all 9 DELETE_SET files still on disk and tracked (`git ls-files` confirms).
- No `/tmp/opencode/winsrv_retire_backup_*` snapshot exists → `--apply` never ran (snapshot is created at apply time). Only unrelated `shim_retire_backup_*`/`svcmgr_retire_backup_*` dirs present.
- Newest logs (`winsrv_dryrun_1795549771.log` 21:49, `winsrv_dryrun_head.log` 21:48) end `REFUSE: no --apply given -- nothing changed`; every winsrv log is a dry-run/refusal, no APPLY/DELETED lines.
- HEAD `0f03f7e`; last commit touching `user/winsrv.c` is `7179d08` (pre-migration polish). No deletion commit.
- kernelhwsrv/ untouched; no QEMU run. Read-only watch only.

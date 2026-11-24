# WINSRV_DELETE_WATCH22 — did the final deletion apply land?

**NO — apply was REFUSED, nothing was deleted.**
- Apply log `/tmp/opencode/winsrv_delete_apply.log` (285 B): `REFUSE: P6 standing invariant violated: EFILE trampoline active at variant/e32load.cpp:1014 ... APPLY_RC=2` (trampoline KPrintf confirmed still present at that line).
- All 7 DELETE_SET targets still exist and are still git-tracked: `user/winsrv.c`, `user/guishost.h`, `user/wsclient.{c,h}`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp`.
- `git status --short` shows no winsrv deletions (only `M tools/r1_winsrv_delete.py` + untracked watch docs); `git log -2` = `0f03f7e`, `97f224b` (migration, not deletion).
- No snapshot dir `/tmp/opencode/winsrv_retire_backup_*` exists (consistent with a refused apply — snapshot would only be made on success).
- Newest winsrv log is still a dry-run (`winsrv_dryrun2.log`, 5.9 KB, ends `REFUSE: no --apply given`).
- `kernelhwsrv/` untouched (git status clean there). No QEMU run.

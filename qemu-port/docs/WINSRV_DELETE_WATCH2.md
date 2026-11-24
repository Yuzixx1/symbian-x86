# WINSRV_DELETE_WATCH2 — did the ses_df0e6e3a2 deletion apply land?

**Verdict: NO.** The apply did not land on the real winsrv sources.

- Deleted paths (`kernel/winsrv.c`, `include/guishost.h`, `kernel/wsclient.*`, `kernel/qemu_windowsrv.*`, `kernel/qemu_gui_exec.cpp`) are **untracked stale duplicates** — `git status` shows zero `D` entries; Makefile builds from `user/` + `variant/` only.
- Real tracked sources INTACT: `qemu-port/user/winsrv.c`, `user/guishost.h`, `user/wsclient.{c,h}`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` all still exist.
- No snapshot dir: `ls /tmp/opencode/winsrv_retire_backup_*` → none found.
- Delete tool log (`/tmp/opencode/winsrv_delete_noargs.log`) ends in **REFUSE** — plan shown, `--apply` never passed, nothing deleted by the tool.
- Post-gate logs in /tmp/opencode (final_verify_*, gate_*) predate/are unrelated to any winsrv apply; kernelhwsrv/ untouched.

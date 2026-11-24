# WINSRV_DELETE_WATCH — 2026-11-24 (read-only monitor)

**Verdict: APPLY NOT LANDED.** `tools/r1_winsrv_delete.py` exists (39 KB, 21:14) but only dry-run/no-args runs are logged (`/tmp/opencode/winsrv_delete_*.log` — both end in REFUSE, no writes).

- `git status`: zero `D` entries; `winsrv.c`, `guishost.h`, `wsclient.{c,h}`, `qemu_windowsrv.{cpp,h}`, `qemu_gui_exec.cpp` all still present (user/, variant/) and unmodified.
- No snapshot dir: `/tmp/opencode/winsrv_retire_backup_*` does not exist (only shim_retire/svcmgr_retire backups).
- Latest status docs (`DELETE_TOOL_STATUS5.md` etc.): tool LANDED, apply never invoked; blockers in `WINSRV_ENDGAME_STATUS.md` (Connect r=-28, KERN-8 #PF, P1 live execs) still open.
- `kernelhwsrv/` untouched (git status clean); no QEMU run by this monitor.

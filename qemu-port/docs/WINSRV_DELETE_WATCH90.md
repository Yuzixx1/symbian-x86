# WINSRV_DELETE_WATCH90 — re-apply status (2026-11-24 ~22:26)

- APPLY_RC: **NOT YET WRITTEN** — apply still running (pid 1286837, ~16 min elapsed); log ends at P3 gate batch launch (make, selfcheck, qemu smoke, ewsrv, input tests). No FAIL/Traceback; only "rollback" hit is the pre-apply snapshot token ROLLBACK-WINSRV-20261124_215436.
- Git: 7 deletions staged (` D` count = 7): winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp — matches the 7-delete op set; no rollback reverted.
- HEAD: 0f03f7e (doom e32_platform.c restore) on 97f224b (GUI→EWSRV migration). Branch: main.
- kernelhwsrv/: clean/untouched. Log: /tmp/opencode/winsrv_delete_apply2.log (2939 bytes, 49 lines, <50 MB so read in full).

# WINSRV_DELETE_WATCH64 — re-apply status (2026-11-24 ~22:10)

- APPLY_RC: NOT YET — log `/tmp/opencode/winsrv_delete_apply2.log` (2.2 KB) ends at `gate[P2]: make isogrub` + selfcheck; no APPLY_RC line, no rollback executed (only snapshot token ROLLBACK-WINSRV-20261124_215436).
- Process: pid 1286837 ALIVE (~9 min), `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV` — re-apply still in flight.
- P1 gate PASS (ewsrv retire gate + make + selfcheck PASS); P2 edits applied to Makefile/user/user.mk; P2 gate pending.
- Git deletes: NONE yet — `git status` has zero `D` lines; no winsrv/guishost/wsclient/qemu_windowsrv/gui_exec deletions staged or unstaged.
- Modified in-tree (P1/P2 edits): qemu-port/{Makefile,user/user.mk,variant/qemu_variant.h,qemu_mouse.cpp,qemu_display.cpp,kern_shell.cpp,qemu_variant_int.cpp} + r1_winsrv_delete.py.
- HEAD: 0f03f7e (doom e32_platform.c restore), parent 97f224b (EWSRV migration). kernelhwsrv/ untouched (0 changes).

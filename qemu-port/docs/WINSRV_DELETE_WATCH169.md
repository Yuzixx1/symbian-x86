# WINSRV DELETE WATCH 169 — apply final state (read-only monitor)

- Apply run `winsrv_delete_apply2.log` (pid 1286837) still RUNNING (~32 min), currently in P3 gate: `r1_final_verify.py --strict-skip` in flight. No `APPLY_RC=` line yet, no rollback lines. Snapshot/rollback token: `ROLLBACK-WINSRV-20261124_215436`.
- Earlier `winsrv_delete_apply.log` ended `APPLY_RC=2` (failed earlier attempt); apply2 is the live rerun. P1/P2 gates passed; all 7 winsrv deletions applied (winsrv.c, guishost.h, wsclient.*, qemu_windowsrv.*, qemu_gui_exec.cpp).
- `git status`: those 7 files show `D` in worktree; HEAD = `0f03f7e`; `qemu-port/user/winsrv.c` and `qemu-port/variant/qemu_gui_exec.cpp` confirmed absent on disk.
- `git status --short kernelhwsrv/ | wc -l` = 0 — kernelhwsrv untouched.

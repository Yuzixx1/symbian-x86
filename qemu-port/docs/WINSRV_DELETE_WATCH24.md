# WINSRV_DELETE_WATCH24 — apply2 status (2026-11-24 ~21:55)

- Re-apply (ses_df0e49d19) NOT finished: `r1_winsrv_delete.py --apply` (pid 1286837) still running, now in the EWSRV retire gate (`r1_genuine_retire_test.py`, pid 1287476); log ends at the gate line, no `APPLY_RC=` yet.
- Log `/tmp/opencode/winsrv_delete_apply2.log` (489 B): 25 ops (18 edits, 7 deletes), all unique/allowlisted; kernelhwsrv/ clean; P1 consumers grep-clean.
- Snapshot taken: `/tmp/opencode/winsrv_retire_backup_20261124_215436` (Makefile/manifest.json/user/variant).
- P3 deletions (7 files: user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.{cpp,h}, variant/qemu_gui_exec.cpp) NOT yet executed — those files still exist on disk; deletes run after P1+P2 gates.
- `git status`: only `qemu-port/tools/r1_winsrv_delete.py` modified among winsrv files (plus unrelated watch docs); kernelhwsrv/ untouched (clean).
- HEAD: 0f03f7e (doom e32_platform.c restore) on top of 97f224b (GUI→EWSRV migration). Separate pregate driver (pid 1286566) also running.

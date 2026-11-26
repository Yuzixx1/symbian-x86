# WINSRV_DELETE_WATCH27 — re-apply (pid 1286837) result

- **APPLY_RC=0** — `/tmp/opencode/winsrv_delete_apply2.log` ends with
  `SUCCESS: winsrv deletion applied across P1-P3; all phase gates PASS.` + `APPLY_RC=0`. No rollback.
- **Deletions landed (all 7):** `user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`,
  `user/wsclient.h`, `variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`,
  `variant/qemu_gui_exec.cpp` — confirmed absent from working tree.
- **Gates:** pre-apply ewsrv retire gate PASS; P1 gate (make+selfcheck+ewsrv) PASS;
  P2 gate (isogrub+selfcheck+ewsrv) PASS; P3 gate (make, selfcheck, smoke, ewsrv,
  input, doom ×4, fullchain 14/14, retire 4/4, `r1_final_verify.py --strict-skip`) PASS.
- **Edits landed:** 18 edits across `variant/qemu_variant.h`, `qemu_mouse.cpp`,
  `qemu_display.cpp`, `kern_shell.cpp`, `qemu_variant_int.cpp`, `Makefile`, `user/user.mk`.
- **Rollback snapshot:** `/tmp/opencode/winsrv_retire_backup_20261124_215436`
  (token `ROLLBACK-WINSRV-20261124_215436`).
- **kernelhwsrv/ untouched** (git status clean). HEAD: `0f03f7e` (+ working-tree changes,
  uncommitted as required). Note: stray untracked file `qemu-port/9` exists — not created by this monitor.

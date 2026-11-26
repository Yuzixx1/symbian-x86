# WINSRV_DELETE_WATCH238 — final state of winsrv deletion task

- HEAD = origin/main = `94ce9c9` "Retire winsrv/GUI desktop" (deletes winsrv.c, guishost.h,
  wsclient.*, qemu_windowsrv.*, qemu_gui_exec.cpp); both target files confirmed absent.
- kernelhwsrv/ untouched: `git status --short kernelhwsrv/` count = 0.
- Apply log: `winsrv_delete_apply2.log` has 1 `APPLY_RC=0`; smoke rerun RESULT=PASS RC=0.
- Post-delete full gate: `final_verify_post_winsrv_delete.log` OVERALL: FAIL (10 pass, 1 fail) —
  only `qemu_smoke` failed (50.1s, RESULT line FAIL) while standalone smoke rerun PASSes,
  so it looks like a transient, not a regression; all other 10 tests (incl. doom_*) PASS.
- `94ce9c9` referenced in GO_NO_GO.md, CLEANUP_LOG.md, FINAL_GATE_STATUS.md.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

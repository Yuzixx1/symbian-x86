# WINSRV_DELETE_WATCH237 — final state of winsrv deletion task

- HEAD = origin/main = `94ce9c9` "Retire winsrv/GUI desktop: delete winsrv.c guishost.h wsclient.* qemu_windowsrv.* qemu_gui_exec.cpp" (APPLY_RC=0, P1–P3 PASS, strict verify PASS at commit time).
- Files gone: `qemu-port/user/winsrv.c` and `qemu-port/variant/qemu_gui_exec.cpp` no longer exist.
- `git status --short kernelhwsrv/ | wc -l` = 0 → kernelhwsrv/ untouched.
- Working tree still has unrelated modified files (AGENTS.md, Makefile, user.mk, docs, state.json) — no winsrv leftovers among them.
- Post-delete final_verify: OVERALL FAIL (10 pass, 1 fail) — qemu_smoke FAIL ("imports bound dll", ProcessCreate/Loaded r=0); standalone rerun `r1_qemu_smoke_rerun.log` = RESULT: PASS, RC=0.
- winsrv_delete_apply2.log: APPLY_RC=0 count = 1 (apply succeeded).
- No QEMU run by this monitor; read-only except this report.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

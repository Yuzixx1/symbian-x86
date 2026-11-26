# WINSRV DELETE TOOL STATUS (2nd check)

**Status: NOT STARTED** — `tools/r1_winsrv_delete.py` did not land in this retry.

Evidence (read-only checks, no QEMU, kernelhwsrv/ untouched):
- `ls -la qemu-port/tools/r1_winsrv_delete.py` → "No such file or directory" (rc=2).
- `git log --all --grep=winsrv_delete` → no matching commits.
- Only related file present: `qemu-port/tools/r1_winsrv_test.py`.
- No py_compile / no-args / `--apply`-without-token runs performed — file absent, so nothing to execute.

Conclusion: the winsrv deletion tool was not created in this session/retry; winsrv cleanup remains pending per docs/REMAINING_SHIMS_AUDIT.md.

**2026-11-26 — correction:** the "NOT STARTED" status at line 3 is reversed — `tools/r1_winsrv_delete.py` is present (37017 bytes, 2026-11-24 21:56) and the winsrv/GUI deletion landed in commit `94ce9c9` (7 files removed: `user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`, `user/wsclient.h`, `variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`, `variant/qemu_gui_exec.cpp`), verified by the 13-row gate recorded in docs/FINAL_GATE_STATUS.md: PASS 13/13 (`OVERALL: PASS  (13 pass, 0 skip, 0 fail/not-run)`, `FINAL13_RC=0`).

**2026-11-26 (later run) — supersession (append-only; body and prior stanza above unchanged):** the 13-row figures above are **superseded by a 15-row gate**, recorded in `/tmp/opencode/final27_gate.log` (mtime 2026-11-26 03:26 EET): `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, at HEAD `ee8490e`, `kernelhwsrv/` untouched. Rows: g12_residue, g12_negative, missing_statics, selfcheck, qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey, hal_probe — all PASS. Treat **15/15** as the current gate verdict; the quoted 13/13 remains accurate only for the earlier `/tmp/opencode/final13_gate.log` run. The `tools/r1_winsrv_delete.py` presence (37017 bytes) and the 7-file winsrv/GUI deletion in `94ce9c9` are unaffected and still stand.

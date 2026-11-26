# WINSRV_DELETE_TOOL_STATUS — tools/r1_winsrv_delete.py

**Status: NOT STARTED** (as of 2026-11-24)

Evidence:
- `ls tools/r1_winsrv_delete.py` → "No such file or directory"; no matching file
  anywhere under `qemu-port/tools/` (only `r1_winsrv_test.py`,
  `r1_shim_retire_apply.py`, `r1_shim_retire_plan.py` present).
- No untracked or modified copy in `git status` for `qemu-port/tools/`.
- `docs/WINSRV_DELETION_EXECUTOR.md` (14 KB, Nov 24 20:38) exists as the
  *plan* only: it specifies a new token-gated tool reusing
  `r1_shim_retire_apply.py` helpers (`--apply --confirm DELETE-WINSRV`,
  dry-run default, ewsrv retire gate). The tool itself was never created.
- No dry-run safety checks could be run (nothing to compile/execute); nothing
  to refuse. `kernelhwsrv/` untouched; no QEMU run.

**2026-11-26 — correction (append-only; body above unchanged):** the `**Status: NOT STARTED**` line 3 is reversed — the winsrv/GUI deletion landed in `94ce9c9` (7 files deleted: `user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`, `user/wsclient.h`, `variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`, `variant/qemu_gui_exec.cpp`), verified by the 13-row gate at **13/13 PASS** (`/tmp/opencode/final13_gate.log`, `OVERALL: PASS  (13 pass, 0 skip, 0 fail/not-run)`, `FINAL13_RC=0`).

**2026-11-26 (later run) — supersession (append-only; body and prior stanza above unchanged):** the 13-row figures above are **superseded by a 15-row gate**, recorded in `/tmp/opencode/final27_gate.log` (mtime 2026-11-26 03:26 EET): `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, at HEAD `ee8490e`, `kernelhwsrv/` untouched. Rows: g12_residue, g12_negative, missing_statics, selfcheck, qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey, hal_probe — all PASS. Treat **15/15** as the current gate verdict; the quoted 13/13 remains accurate only for the earlier `/tmp/opencode/final13_gate.log` run. The `tools/r1_winsrv_delete.py` presence and the 7-file winsrv/GUI deletion in `94ce9c9` are unaffected and still stand.

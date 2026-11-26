# Overnight STOP report — 2026-11-26

**Gate:** FINAL27 **15/15 PASS**, `FINAL27_RC=0` — `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` in `/tmp/opencode/final27_gate.log`; rows match `tools/r1_final_verify.py:134-150`.
**Edits landed:** `tools/r1_genuine_ewsrv_test.py` portfilesrv decoupled (constant, staging and FAT assert all dropped; `EUSER_DLL = PORT / "EUSER.DLL"`); `tools/r1_selftest.py:37` and `tools/r1_cp4_ca_test.py:39/40` now resolve tracked repo paths (`qemu-port/EUSER.DLL`, `build/kernfull.elf`) instead of `~/.cache` copies.
**Docs:** authority promoted to 15 rows / FINAL27 in `AGENTS.md:62,88-93` and the status docs (`docs/KNOWN_GAPS_FINAL.md:127`, `docs/EWSRV_INPUT_INJECTION.md:157`).
**Deliberate gaps:** FreeDM user asset absent (`qemu-port/wads/` holds only `freedoom1.wad`, no `freedm.wad`); off-roster `tools/r1_genuine_input_test.py:59-60` and `tools/r1_genuine_pointer_test.py:47-48` still hardcode the retired portfilesrv cache; `tools/r1_genuine_retire_test.py:55` cache read is best-effort only (`.is_file()` guard at `:538`, never asserted); the roster scripts remain uncommitted. None affects the 15 rows.
**Tree:** HEAD == `origin/main` == `ee8490e`; `kernelhwsrv/` at `0c32086`, clean; ~309 uncommitted entries in the main repo; no commits, no pushes, no QEMU.

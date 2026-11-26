# G12 wiring plan — static residue gate first in DEFAULT_TESTS (design; r1_final_verify.py NOT edited)

Verified at HEAD ee8490e: `python3 tools/r1_shim_residue_sweep.py` = `RESULT: FAIL (28 residue)` — 18 file, 8 rule (`Makefile:342-351`), 2 include (`genuine/efsrv_min.cpp:30,32`). Wiring it now reds row 1, and default `--no-keep-going` marks the other 11 NOT RUN.

## Wire spec (one line, added later)
- `Test("g12_residue", "tools/r1_shim_residue_sweep.py", [], 120)` at index 0, above `selfcheck`; argv empty, no flags; <1 s real, 120 s is cold-tree headroom only.
- `--list` exits 0 with no verdict line, so run_test would print `PASS (exit status 0 (no explicit verdict))` — a false green. `--list`/`--json` stay human-only, never in DEFAULT_TESTS argv.
- No final_verify change needed: sweep prints `RESULT: PASS|FAIL` and rc 0/1 agree, and the `rc != 0` override at :382 can only demote a verdict, never promote.
- SKIP-clean by construction: the sweep has no SKIP path (missing Makefile = `RESULT: FAIL`), so strict-skip fatality cannot trigger. Add no `--skip-if-*` mode later.

## Add exactly when all four hold
1. S2 landed: `Makefile:342-351` gone, `make -n` never names `r1_fs_channel`.
2. S3 landed: `variant/r1_fs_channel.*`, the 6 `fs/r1_fs_channel*` + `r1_fs_wire_logic.h` files, and `tests/r1_fs_channel/` deleted.
3. Inventory narrowed FIRST: the live keepers (`fs/r1_fs_{core,server,backend,fat}*`, `tests/r1_fs_core`, the two `efsrv_min.cpp` includes) whitelisted-with-reason or off the retired list — S1+S2+S3 alone still leave 10 residue (8 files + 2 includes), so wiring stays NO-GO.
4. Settled tree prints `RESULT: PASS`, rc 0, captured to `/tmp/opencode/wave2_s3_g12.log`; then S reads 12 pass / 0 skip.

## Meanwhile
- No DEFAULT_TESTS entry at all: not FAIL, and not a placeholder SKIP (fatal under `--strict-skip`, silently green without it).
- Run G12 by hand at each WAVE2 step and record `g12_rc` beside R/F/S. D4 default unchanged: `ports/lua/e32_shim.c` stays whitelisted.
- This design writes no code, commits nothing, keeps `kernelhwsrv/` read-only, runs no make and no QEMU.

# G12 gap note — residue-sweep gate specified, not implemented

Open gap, recorded after the P1 step 1 audit (read-only).

- `tools/r1_shim_residue_sweep.py` is absent — `ls` reports No such file or directory.
- Specified at `docs/PLAN_SURVEY_GATES.md:78` (retired shim files absent, no live kernel
  hook references them, orphan `fs/r1_fs_*`/`services/r1_service_host*` deleted or
  whitelisted; <5 s, so it belongs first in DEFAULT_TESTS) and required as P1's per-step
  gate at `docs/PLAN_OVERNIGHT_WAVE1.md:40,46,51` — "G12 PASS + retire 4/4 + fullchain
  14/14 + strict-skip 11/11".
- `/tmp/opencode/p1_step1.rc` holds only retire/fullchain/strict/wrapper = 0
  (`strict_rc=0`). No `g12_rc`, so P1's green baseline carried no residue-sweep gate:
  deletion progress is not independently asserted, and a partial deletion reads green.
- Recommendation: implement G12, wire it first in DEFAULT_TESTS, and re-run the P1 step
  gates with it before P1 deletion is called complete.

## Correction (2026-11-26) — the gate is implemented and green

The gap above is closed. `tools/r1_shim_residue_sweep.py` exists and is wired as
`DEFAULT_TESTS[0]` (`g12_residue`) in `tools/r1_final_verify.py:126`, first so a
residue regression fails the run before anything expensive starts. Its
falsifiability row `g12_negative` (`tools/r1_g12_negative_test.py`) sits at
`[1]`: it seeds throwaway trees and asserts the sweep's verdict flips to FAIL
when `fs/r1_fs_channel.cpp` reappears, so a green row cannot be vacuous.

Verified 2026-11-26: `python3 tools/r1_shim_residue_sweep.py` prints
`RESULT: PASS (0 residue, 43 allowed comments)`, exit 0, scanned statically in
1.77 s with no QEMU. `DEFAULT_TESTS` carries 14 rows, and the final16 gate run
is 14/14 PASS with the sweep row green — the P1 green baseline now does assert
deletion progress, so the "partial deletion reads green" risk no longer holds.

## Supersession (2026-11-26) — the correction above pins a 14-row gate; it is now 15

The 2026-11-26 correction states `DEFAULT_TESTS` carries 14 rows and the `final16` run was
14/14. That count is superseded: `DEFAULT_TESTS` (`tools/r1_final_verify.py:134-150`) now
carries **15** rows (`hal_probe` added after `doom_multikey`), and the last full run
`/tmp/opencode/final27_gate.log` reports `OVERALL: PASS (15 pass, 0 skip, 0 fail/not-run)`,
`FINAL27_RC=0`, with `g12_residue` and `g12_negative` both green. The substance of the
correction still holds — the residue gate exists, runs first, and is falsifiable — so the
original gap remains closed. Prior text retained as history.

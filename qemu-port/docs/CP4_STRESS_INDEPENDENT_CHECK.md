# CP4_STRESS_INDEPENDENT_CHECK — 2026-11-24

Verdict: **claim REFUTED as stated.** cp4_stress does not fail on pristine
7f76900, and the two shim-retirement edits do not cause it. The sole cause is a
**stale build object** in the current working tree.

## Assertion
`tools/r1_cp4_ca_stress_test.py:194` — cycle 1 `done ids [1, 2, 3] != survivors
[1, 3, 4]`; selfcheck invokes it as step `cp4_stress` (r1_os_selfcheck.py:36).

## Results (serial, one QEMU at a time; pristine via detached git worktree)
- Current tree (2 retirement edits): **4/4 FAIL**, identical assertion
  (`cp4_current_run.out`, `cp4_current_trial{1,2,3}.out`; serial log
  `cp4_stress_639814.log`).
- Pristine `7f76900` fresh worktree: **4/4 PASS**
  (`cp4_pristine_run2.out`, `cp4_pristine_trial{1,2,3}.out`).
- Fresh worktree at `7f76900` **with** both retirement edits applied: **3/3 PASS**
  (`cp4_retired_trial{1,2,3}.out`).
- Same fresh tree seeded with current tree's stale object: **FAIL**, identical
  assertion (`cp4_staleo_trial2.out`); replacing that one object with a freshly
  built one → **PASS** (`cp4_staleo_fixed.out`).

## Root cause
`variant/qemu_variant_asic.cpp` is byte-identical in all trees (sha256
`ef3878be…`), but the current tree's `build/qemu_variant_asic.o` is stale:
built 08:31:59 during the svcmgr apply/rollback window, while its source mtime
is 2026-11-23 22:47:17 (older). `make` therefore never rebuilt it; sha256
`e441e39e…` (stale) vs `c6b6c4c3…` (correct). Only `qemu_variant_asic.o` and
`kernfull.elf` differ among common build outputs. A clean worktree rebuilds it
and passes.

## Consequences
- `docs/SHIM_RETIREMENT_RESULT.md` §5's stash A/B reused the same stale build
  directory, so both runs failed identically — that does **not** prove a
  pristine-baseline failure. `docs/FINAL_VERIFICATION_RESULT.md` 11/11 PASS is
  consistent with a correctly built tree.
- No retirement edit needs reverting. Force-rebuild `qemu_variant_asic.o`
  (touch source or delete just that .o, then `make build/kernfull.elf`), no
  `make clean`, then rerun selfcheck.
- `kernelhwsrv/` untouched (clean `git status`); scratch worktrees removed;
  no commit/push.

---

**Superseded 2026-11-26.** Any 11/11 strict-gate result recorded above is
**historical**. Superseded by the 15-row gate: **15 rows / `FINAL27_RC=0`**
(15 pass, 0 skip, 0 fail; `/tmp/opencode/final27_gate.log`). The body above is
left byte-identical and was not re-verified against the 15-row gate.

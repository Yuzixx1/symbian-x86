# PLAN_CONSISTENCY_CHECK — P0 commit plan vs P1 deletion plan

**Date:** 2026-11-25. Review-only task: this file is the only file written. No
commit, no push, no `make clean`, no QEMU, `kernelhwsrv/` untouched
(verified porcelain-clean, 0 lines). Inputs read:
[PLAN_P0_COMMIT.md](/home/yuzi/Documents/Sybian\ REAL/qemu-port/docs/PLAN_P0_COMMIT.md),
[PLAN_EXECUTION.md](/home/yuzi/Documents/Sybian\ REAL/qemu-port/docs/PLAN_EXECUTION.md) (P1 §),
[PLAN_SURVEY_SHIMS.md](/home/yuzi/Documents/Sybian\ REAL/qemu-port/docs/PLAN_SURVEY_SHIMS.md),
[PLAN_SURVEY_ROLLBACK.md](/home/yuzi/Documents/Sybian\ REAL/qemu-port/docs/PLAN_SURVEY_ROLLBACK.md),
live `git status` at HEAD `ee8490e` on `main`.

## P0->P1 dependency check

- P1's deletion order, gate-input risk, channel coupling, and freeze list all
  trace to PLAN_SURVEY_SHIMS: SHIMS deletion order 1–6 maps 1:1 onto P1 steps
  1–4, 6; SHIMS risk #1 (`fs/r1_fs_core.h` is read by
  `tools/r1_genuine_retire_test.py:46-48,127`) is exactly why P1 step 3 ports
  the opcode check off the shim header *before* deleting `fs/r1_fs_*`; SHIMS
  risk #4 (`variant/r1_fs_channel.*` includes `fs/r1_fs_channel*.h`) is why
  P1 step 4 removes the Makefile opt-in block together with the variant files.
  P1 step 8's freeze (exec 81/269/270, ewsrv_shims + HAL shim, netstack, doom
  shim) matches SHIMS "never in this pass" and P0's freeze register verbatim.
  CONSISTENT.
- P1 runs on the guarded executors documented in PLAN_SURVEY_ROLLBACK
  (`r1_shim_retire_plan.py` -> `r1_shim_retire_apply.py`, /tmp snapshots +
  `git show <sha>:<path>` fallback). P0's commit-first-then-archive order (§3)
  satisfies the rollback survey's requirement that evidence have a permanent
  git-history home before any destructive move. CONSISTENT.
- P0 commits every untracked `qemu-port/docs/PLAN_*.md` via the dynamic
  regeneration command (§1 step 2), so the SHIMS and ROLLBACK surveys P1 relies
  on are in git history before P1 starts. CONSISTENT.
- P1 deletes nothing P0 needs as evidence: P1's deletion set is `fs/r1_fs_*`,
  `tests/r1_fs_{core,channel}`, `services/r1_service_host.*`,
  `variant/r1_fs_channel.*`, the Makefile opt-in block, `user/user.mk` rules
  and stale comments, stale build artifacts, and (only per D4)
  `ports/lua/e32_shim.c`. Zero overlap with P0's committed set (PLAN docs,
  WINSRV_RETIREMENT_FINAL.md, 197 tracked WATCH files) or its preserved
  exclusions (tarball, lock, untracked WATCH). Neither SHIMS nor ROLLBACK
  references `WINSRV_DELETE_WATCH*`; the only WATCH mention in
  PLAN_EXECUTION.md is P0's own archive step. CONSISTENT.

## Staging correctness

- Group A (6): `AGENTS.md` + 4 docs are modified now; `.gitignore` becomes the
  6th when §4 appends its rules at execution time. `state.json` is correctly
  excluded (tracked, so no gitignore rule can untrack it; plan says leave the
  modification in place). Root `.gitignore` is indeed the only tracked
  gitignore (verified `git ls-files`). CONSISTENT.
- Group B: `qemu-port/docs/WINSRV_RETIREMENT_FINAL.md` exists untracked now and
  its lines 3–4 explicitly supersede the `WINSRV_DELETE_WATCH*` reports, which
  is what makes the §3 archive safe. CONSISTENT.
- Group C: enumerated 20 PLAN files at snapshot; 21 are untracked now. The §1
  step-2 command (`git status --porcelain | grep '^?? qemu-port/docs/PLAN_'`)
  is authoritative over the enumerated list, so the extra file is staged
  automatically — the live-tree caveat anticipated exactly this. CONSISTENT
  by design.
- Exclusions verified on disk: tarball `symbian-x86-archive-56fbaff.tar.gz`
  (2,481,838 B, 983 entries — both match §2), `qemu-port/.sybian/qemu-slot.lock`
  (0 B), `qemu-port/9` (0 B) all present and untracked; §5 guard (b) correctly
  refuses any of them (plus `state.json`, WATCH, `qemu-port/9`) in the staged
  set. CONSISTENT.
- One wart (fail-closed, needs a fix): §5 step (a) builds the expected staged
  set from `git status --porcelain | grep '^ M'`. After the §1 `git add`,
  staged files print as `M ` (column 1), not ` M`, so the grep would drop the
  5 modified docs and the diff would abort the commit. Correction: build the
  expected set before the adds, or use `grep -E '^(M |MM| M)'`. No correctness
  hole — the guard aborts rather than committing wrong — but as written the
  recipe cannot pass after staging.

## Archive/evidence check

- §3 archives only untracked WATCH files by explicit `git status`-derived list
  (never a blind glob, never `git clean`), into the §4-ignored
  `qemu-port/docs/archive/` — matching the ROLLBACK survey's advice to park
  manifests under `archive/`. Counts verified live: 128 untracked WATCH,
  197 tracked WATCH, 325 on disk; §5(d) expectations (197 tracked remain,
  128 archived) are arithmetically correct. CONSISTENT.
- Because the 128 WATCH files are untracked, they have no git-history fallback;
  the committed WINSRV_RETIREMENT_FINAL.md (APPLY_RC=0, P1–P3 PASS, rollback
  token, strict 11/11) plus PLAN_SURVEY_HYGIENE.md (128 reports md5-unique,
  8–30 lines) are the durable evidence — both are in P0's stage list, and §3
  step 0 asserts the closeout landed in HEAD before archiving. CONSISTENT.
- P0 never touches `kernelhwsrv/` (§5 guard (b) count 0, §5(e) porcelain
  check); P1's NO-GO includes the same check. Both consistent with the
  READ-ONLY boundary (verified clean now). CONSISTENT.

## Drift notes

- The claimed **128 untracked WATCH files is still exact** at review time
  (`git status --porcelain | grep -c '^?? qemu-port/docs/WINSRV_DELETE_WATCH'`
  = 128). No drift on the number P1/P0 gates key on. Total untracked is now
  155 = 128 WATCH + 22 PLAN_* + WINSRV_RETIREMENT_FINAL.md + ROADMAP_SUMMARY.md
  + tarball + lock + `qemu-port/9`.
- PLAN_* untracked count drifted 20 -> 22 since the P0 snapshot
  (`PLAN_RECONCILIATION.md`, `PLAN_DECISION_SHEET.md` appeared). Covered by the
  dynamic stage command; the enumerated Group C list is illustrative only. No
  action needed for those.
- New non-PLAN doc `qemu-port/docs/ROADMAP_SUMMARY.md` appeared after this
  review started. It matches neither the Group A/B fixed set nor the `PLAN_`
  grep, so P0 would leave it untracked and un-ignored — and §5(c)'s
  `git ls-files --others --exclude-standard` "must be empty" check would then
  FAIL (fail-closed abort before commit). Correction: add it explicitly to the
  §1 stage list or a §4 ignore rule before executing P0.
- PLAN_EXECUTION.md D1 row still says "4 PLAN_SURVEY files"; 17
  `PLAN_SURVEY_*.md` are now untracked. Cosmetic — P0's dynamic PLAN set is
  authoritative — but the D1 wording could say "all untracked PLAN_*.md".
- §5(a) `'^ M'` grep wart as described under Staging correctness.

## Verdict

CONSISTENT. P0 and P1 are mutually consistent: P0 commits exactly the docs
(P0 closeout + all PLAN_* surveys incl. SHIMS and ROLLBACK) that P1's gates,
ordering, and decisions rely on; P0's stage list excludes the tarball, lock,
stray `9`, `state.json`, and all WATCH files (guarded by §5(b)); P1 deletes
none of P0's evidence; the 128-WATCH claim is still exact. Three minor
corrections, all non-blocking and fail-closed: (1) fix the §5(a) expected-set
grep in PLAN_P0_COMMIT.md so it matches staged (`M `) as well as unstaged
(` M`) entries, or snapshot the expected set before the adds; (2) stage or
ignore the new `ROADMAP_SUMMARY.md`, else §5(c) aborts; (3) optionally reword
PLAN_EXECUTION.md D1 to "all untracked PLAN_*.md" instead of "4 PLAN_SURVEY
files". No changes required to the P1 deletion plan.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. The consistency-check reasoning that cites a strict 11/11 baseline is read against the 15-row gate. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

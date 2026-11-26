# PLAN_RECONCILIATION — contradictions in the execution plan vs. stated choices and surveys

**Date:** 2026-11-25. **Scope:** read-only review. Inputs read:
[`PLAN_EXECUTION.md`](PLAN_EXECUTION.md),
[`PLAN_DECISION_REGISTER.md`](PLAN_DECISION_REGISTER.md) (the live decision
register; `r1_decision_record_v1.md` is the older R1-era sign-off doc, see C6),
[`PLAN_SURVEY_GAME_ASSETS.md`](PLAN_SURVEY_GAME_ASSETS.md),
[`PLAN_SURVEY_FREE_GAMES.md`](PLAN_SURVEY_FREE_GAMES.md),
[`PLAN_SURVEY_NEXT_GAME.md`](PLAN_SURVEY_NEXT_GAME.md),
[`PLAN_SURVEY_GATES.md`](PLAN_SURVEY_GATES.md),
[`PLAN_SURVEY_GUI_MILESTONE.md`](PLAN_SURVEY_GUI_MILESTONE.md),
[`DOOM_PORTING.md`](DOOM_PORTING.md) §4, root
[`AGENTS.md`](/home/yuzi/Documents/Sybian REAL/AGENTS.md).
No code read beyond docs, no edits outside this file, no commit/push, no
`make clean`, no QEMU runs. `kernelhwsrv/` untouched (read-only).

## Contradictions found

- **C1 — P3 recommends Wolf3D shareware against the written free-license
  policy.** `PLAN_EXECUTION.md` P3 and decision D6 follow
  `PLAN_SURVEY_NEXT_GAME.md` rank 2 ("Wolfenstein 3D shareware"). But
  `DOOM_PORTING.md` §4 (~line 153): "Do NOT ship `DOOM.WAD`, `DOOM1.WAD` or any
  id Software / shareware WAD" (free-licensed only), and
  `PLAN_SURVEY_GAME_ASSETS.md` §5 states the id shareware WL1 "collides with the
  written asset policy", while `PLAN_SURVEY_FREE_GAMES.md` §5 blocker 1 ranks
  only license-clean candidates (C1 FreeDM, C2 free PWAD, C3 free Z-code,
  C6 libre raycaster packs) and explicitly excludes id shareware. The plan
  never resolves this; it schedules the shareware path by default.
- **C2 — "Never replace genuine logic" vs. the P2 port blit opcode.** Standing
  invariant 2 (`PLAN_EXECUTION.md`) and AGENTS.md: authored code is x86/PC glue,
  "never a replacement for real Symbian logic"; decision register entry 2 rules
  authored GDI behind a Symbian-shaped ABI forbidden. Yet P2 adds a
  **port-defined opcode inside `genuine/ewsrv/patched/ws_main.cpp` `ServiceL`**
  — authored logic compiled into the genuine window server. Patched copies are
  structurally allowed (invariant 1) and the plan labels the result
  "port-bridge", but the plan nowhere asks the user to sanction
  opcode-in-genuine-server as glue rather than replacement, and it dilutes the
  "genuine EWSRV" claim of gate 5 (`PLAN_SURVEY_GATES.md` risks section says
  exactly this).
- **C3 — P2 contradicts the frozen user decision.** AGENTS.md (2026-11-24,
  reaffirmed 2026-11-25 `94ce9c9`): the GUI/winsrv desktop was retired and
  "console shell + DOOM/Freedoom are the UI for now". `PLAN_DECISION_REGISTER.md`
  entry 2: GUI returns "only if the user wants the GUI back". But
  `PLAN_EXECUTION.md` P2 asserts "the user wants GUI back via genuine
  EWSRV+UIKON 'where possible'" and schedules P2 as an unattended overnight
  phase — no recorded decision re-authorizes GUI work; D5 only covers the
  purity axis, not whether P2 runs at all.
- **C4 — P3's zero-code smoke test does not fit the disk.** P3 step 1 stages
  Freedoom Phase 2 (`freedoom2.wad`, 28.8 MB) alongside Phase 1, but
  `PLAN_SURVEY_GAME_ASSETS.md` §3: only ~16.8 MB free — it needs an `FS_MB` grow
  or WAD swap, which D6 lists but the phase text assumes it "just fits".
- **C5 — Input list omission.** `PLAN_EXECUTION.md`'s header lists the
  `PLAN_SURVEY_*.md` inputs but omits `PLAN_SURVEY_FREE_GAMES.md` and
  `PLAN_SURVEY_GAME_ASSETS.md`, the two surveys that actually govern the P3
  decision (and contain the C1/C4 blockers above).
- **C6 — Decision-record confusion.** The task brief names
  `r1_decision_record_v1.md` as a possible decision record, but that file is the
  2026-10-28 R1-era policy sign-off (proof apps, QEMU-ready), superseded by
  `PLAN_DECISION_REGISTER.md`. `PLAN_EXECUTION.md` cites the latter correctly;
  no action beyond not treating the old record as current.

## Required user decisions

1. **Second-game asset license (blocks P3/D6).**
   - (a) Keep strict free-license-only: pick FreeDM or a small BSD/CC0 PWAD on
     the existing `DOOM.E32` (FREE_GAMES C1), or a verified libre
     Wolf3D-compatible asset pack (C6) for an actual Wolf3D engine port.
   - (b) Explicitly admit id's shareware WL1 as a user-supplied, never-committed
     exception to `DOOM_PORTING.md` §4.
   - (c) Drop P3 from this plan (no second game).
   - **Recommendation:** (a) with C1 (FreeDM/PWAD on DOOM.E32) as the proven
     path; Wolf3D engine only if a libre asset pack is user-dropped. Never (b)
     silently — it contradicts the written policy.
2. **Does P2 run at all, given the frozen 2026-11-24 GUI-retirement decision?**
   - (a) Confirm console+DOOM as the standing UI; remove P2 from the overnight
     run (keep it as a documented option).
   - (b) Re-authorize P2 (port-bridge pixel milestone on genuine EWSRV, honestly
     labeled) for this plan.
   - **Recommendation:** (a) for the overnight run — the retirement decision is
     the most recent explicit user choice; revisit P2 only on a fresh request.
3. **Port blit opcode inside patched genuine `ws_main.cpp` (only if P2 runs).**
   - (a) Sanction one port-defined opcode in the `patched/` copy as glue,
     mandatory "port-bridge" labeling + sha256 in `BUILD_MANIFEST.json` (G16).
   - (b) Forbid touching genuine EWSRV at all: pixels only via exec 270 fbblit
     outside the server (DOOM-style), EWSRV stays text-only.
   - **Recommendation:** (b) unless the user explicitly wants EWSRV-composited
     pixels; (a) otherwise, with the labeling/hash conditions.
4. **Commit authorizations (D1, D3, D9).** Standing rule: no commit/push unless
   explicitly asked. Overnight work can deliver working-tree changes + green
   gates without any commit.
   - (a) Authorize a named docs-only commit list for P0 hygiene.
   - (b) Authorize per-step deletion commits in P1.
   - (c) No commits at all this run.
   - **Recommendation:** (c) for overnight; the user authorizes commits in the
     morning report.
5. **Disk-image growth for P3 smoke (only if P3 proceeds).** freedoom2.wad
   (28.8 MB) exceeds the 16.8 MB headroom.
   - (a) Grow `FS_MB` in `tools/mkgrubiso.py` (one-number change).
   - (b) Per-test WAD swap (stage Phase 2 instead of Phase 1).
   - **Recommendation:** (b); avoid changing the shipped image layout.

## Corrections to PLAN_EXECUTION.md

1. **P3 / D6:** replace "Wolf3D shareware recommended" with the policy-compliant
   default (FreeDM / BSD-CC0 PWAD on `DOOM.E32`, per `PLAN_SURVEY_FREE_GAMES.md`
   ranking), and state that Wolf3D shareware WL1 is **excluded by
   `DOOM_PORTING.md` §4** unless the user explicitly amends that policy
   (decision 1). Cite `PLAN_SURVEY_GAME_ASSETS.md` §5 as the blocker.
2. **P3 step 1:** note freedoom2.wad does not fit the 16.8 MB headroom —
   requires WAD swap or `FS_MB` grow (decision 5); don't present it as free.
3. **P2 preamble:** delete the unsupported claim "the user wants GUI back".
   State instead that the 2026-11-24 decision retired the GUI "for now" and P2
   is gated on explicit re-authorization (decision 2); default = P2 not in the
   overnight run.
4. **P2 blit opcode:** add an explicit clause that adding an opcode to patched
   `genuine/ewsrv/patched/ws_main.cpp` is authored logic inside genuine code,
   sanctioned only as labeled port-bridge glue with provenance hash (decision
   3), and that gate 5's "genuine EWSRV" claim then refers to the unmodified
   text path only.
5. **Header input list:** add `PLAN_SURVEY_FREE_GAMES.md` and
   `PLAN_SURVEY_GAME_ASSETS.md` to the inputs.
6. **D-register note:** add one line that `r1_decision_record_v1.md` is the old
   R1-era record and `PLAN_DECISION_REGISTER.md` is the live one.

## Can proceed without user input?

**Partially, yes — as unattended working-tree work with green gates, no
commits:** P0 (hygiene moves, `.gitignore`, freeze register, baseline strict
run), P1 deletion steps 1–4/6–7 (D4 defaults to leaving Lua untouched), P4's
CI part (PCI/ECAM + USB mass-storage glue + readiness audit + QEMU USB gates),
and P5's static parts (G16, manifest/metrics, `make isogrub` + smoke on the
artifact). **Blocked without user input:** every commit/push (decision 4), all
of P2 (frozen GUI decision, decisions 2–3), and the asset-gated half of P3
(decisions 1 and 5; P3 can only deliver a SKIP-clean scaffold). `kernelhwsrv/`
stays read-only throughout; no `make clean`; no QEMU was run for this review.

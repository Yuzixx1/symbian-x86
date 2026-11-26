# PLAN MASTER INDEX — every plan doc in `qemu-port/docs/`

Derived live from disk on 2026-11-25: 29 `PLAN_*.md` + `ROADMAP_SUMMARY.md` = 30 docs.

## Start here

- [ROADMAP_SUMMARY.md](ROADMAP_SUMMARY.md) — P0–P5 roadmap on one screen.
- [PLAN_DECISION_SHEET.md](PLAN_DECISION_SHEET.md) — the six decisions that block Planning -> Execution.

## DECISIONS RESOLVED (2026-11-25)

- [PLAN_DECISIONS_RESOLVED.md](PLAN_DECISIONS_RESOLVED.md) — the six defaults, settled.
  - GUI retired: winsrv/GUI desktop is gone; no GUI revival work.
  - FreeDM PWAD: the second game is FreeDM on the existing DOOM.E32.
  - exec-270 only: only the exec-270 path is supported for execution.
  - No overnight commits: autonomous runs make no commits overnight.
  - WAD swap: PWAD selection is a WAD swap, not a new port.
  - Netstack permanent: the netstack stays; no removal.

## Surveys (17)

- [PLAN_SURVEY_BUILD_TOOL.md](PLAN_SURVEY_BUILD_TOOL.md) — `genuine_build.py --name` traversal and constraint headroom.
- [PLAN_SURVEY_ESOCK.md](PLAN_SURVEY_ESOCK.md) — can genuine ESOCK come from the tree?
- [PLAN_SURVEY_FREE_GAMES.md](PLAN_SURVEY_FREE_GAMES.md) — free-licensed candidates for the "second game" proof.
- [PLAN_SURVEY_GAME_ASSETS.md](PLAN_SURVEY_GAME_ASSETS.md) — Wolf3D asset sourcing, disk budget, packaging.
- [PLAN_SURVEY_GATES.md](PLAN_SURVEY_GATES.md) — verification-gate inventory vs. proposed next phases.
- [PLAN_SURVEY_GUI_MILESTONE.md](PLAN_SURVEY_GUI_MILESTONE.md) — smallest credible "GUI desktop boots" milestone on genuine EWSRV+EDISP.
- [PLAN_SURVEY_HW_USB.md](PLAN_SURVEY_HW_USB.md) — real-hardware boot and USB readiness of the x86/PC glue.
- [PLAN_SURVEY_HYGIENE.md](PLAN_SURVEY_HYGIENE.md) — working-tree hygiene after the winsrv retirement.
- [PLAN_SURVEY_MEDIA.md](PLAN_SURVEY_MEDIA.md) — media/codec/audio sources in kernelhwsrv vs qemu-port.
- [PLAN_SURVEY_NEXT_GAME.md](PLAN_SURVEY_NEXT_GAME.md) — cost estimate for a second genuine-style game port.
- [PLAN_SURVEY_PROVENANCE.md](PLAN_SURVEY_PROVENANCE.md) — what "genuine completeness" means, measured.
- [PLAN_SURVEY_REALHW_METHOD.md](PLAN_SURVEY_REALHW_METHOD.md) — real-PC boot test method (CD-R boot of the combo ISO).
- [PLAN_SURVEY_ROLLBACK.md](PLAN_SURVEY_ROLLBACK.md) — reusable rollback protocol for guarded deletions.
- [PLAN_SURVEY_SHIMS.md](PLAN_SURVEY_SHIMS.md) — remaining hand-written shims (planning input).
- [PLAN_SURVEY_SOURCE_IMPORT.md](PLAN_SURVEY_SOURCE_IMPORT.md) — importing MISSING genuine sources (UIKON/EIKON, GDI/FBSRV, ESOCK) from upstream.
- [PLAN_SURVEY_UIKON_GDI.md](PLAN_SURVEY_UIKON_GDI.md) — UIKON / GDI / EWSRV desktop stack feasibility.
- [PLAN_SURVEY_WSRV_NAME.md](PLAN_SURVEY_WSRV_NAME.md) — why `CreateSession("!E32WindowServer")` fails (KErrBadName/KErrNotFound).

## Decisions (4)

- [PLAN_CANONICAL_DECISIONS.md](PLAN_CANONICAL_DECISIONS.md) — the one merged list the user must answer.
- [PLAN_DECISION_REGISTER.md](PLAN_DECISION_REGISTER.md) — conflicts between stated choices and technical reality.
- [PLAN_DECISION_SHEET.md](PLAN_DECISION_SHEET.md) — the six decisions that block Planning -> Execution.
- [PLAN_RECONCILIATION.md](PLAN_RECONCILIATION.md) — contradictions in the execution plan vs. stated choices and surveys.

## Execution (3)

- [PLAN_EXECUTION.md](PLAN_EXECUTION.md) — overnight autonomous work plan (P0..P5).
- [PLAN_FREEDM_MILESTONE.md](PLAN_FREEDM_MILESTONE.md) — P3-lite: second playable workload = FreeDM on the existing DOOM.E32.
- [PLAN_FREEDM_OVERNIGHT.md](PLAN_FREEDM_OVERNIGHT.md) — P3-lite FreeDM staging + gate, unattended-overnight design.

## Commit/hygiene (1)

- [PLAN_P0_COMMIT.md](PLAN_P0_COMMIT.md) — design of the single docs-only P0 hygiene commit.

## Verification (4)

- [PLAN_CONSISTENCY_CHECK.md](PLAN_CONSISTENCY_CHECK.md) — review of the P0 commit plan vs. the P1 deletion plan.
- [PLAN_FREEDM_VERIFY.md](PLAN_FREEDM_VERIFY.md) — review verdict on the FREEDM_SIZE pin and SKIP/baseline claims.
- [PLAN_INDEX_AUDIT.md](PLAN_INDEX_AUDIT.md) — final audit of the plan-docs index.
- [PLAN_OVERNIGHT_FEASIBILITY.md](PLAN_OVERNIGHT_FEASIBILITY.md) — review of the "no-decision overnight work" list.

## Overview

- [ROADMAP_SUMMARY.md](ROADMAP_SUMMARY.md) — qemu-port P0–P5 synthesized on one screen.

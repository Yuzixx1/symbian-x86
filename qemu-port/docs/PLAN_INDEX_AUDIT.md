# Plan-docs index audit — FINAL AUDIT

Scope: `qemu-port/docs/PLAN_*.md` and `qemu-port/docs/ROADMAP_SUMMARY.md` vs.
link targets in `qemu-port/docs/DOCS_INDEX.md`. Read-only audit; DOCS_INDEX.md
was not modified.

## Plan docs found

26 `PLAN_*.md` files plus `ROADMAP_SUMMARY.md` (27 files total):

- PLAN_CANONICAL_DECISIONS.md
- PLAN_CONSISTENCY_CHECK.md
- PLAN_DECISION_REGISTER.md
- PLAN_DECISION_SHEET.md
- PLAN_EXECUTION.md
- PLAN_FREEDM_MILESTONE.md
- PLAN_OVERNIGHT_FEASIBILITY.md
- PLAN_P0_COMMIT.md
- PLAN_RECONCILIATION.md
- PLAN_SURVEY_BUILD_TOOL.md
- PLAN_SURVEY_ESOCK.md
- PLAN_SURVEY_FREE_GAMES.md
- PLAN_SURVEY_GAME_ASSETS.md
- PLAN_SURVEY_GATES.md
- PLAN_SURVEY_GUI_MILESTONE.md
- PLAN_SURVEY_HW_USB.md
- PLAN_SURVEY_HYGIENE.md
- PLAN_SURVEY_MEDIA.md
- PLAN_SURVEY_NEXT_GAME.md
- PLAN_SURVEY_PROVENANCE.md
- PLAN_SURVEY_REALHW_METHOD.md
- PLAN_SURVEY_ROLLBACK.md
- PLAN_SURVEY_SHIMS.md
- PLAN_SURVEY_SOURCE_IMPORT.md
- PLAN_SURVEY_UIKON_GDI.md
- PLAN_SURVEY_WSRV_NAME.md
- ROADMAP_SUMMARY.md

## Missing from index

All 27 files (every `PLAN_*.md` and `ROADMAP_SUMMARY.md`). Each filename stem
was searched in DOCS_INDEX.md case-insensitively and as a Markdown link target
(`](...PLAN...` / `](...ROADMAP...`); matches: 0 for every one. The index does
link other plan-named docs (e.g. EFILE_DUPLICATE_LAUNCH_PLAN.md,
SVCMGR_PLAN_REVIEW.md, WINSRV_MIGRATION_PLAN.md), but no `PLAN_*`-prefixed doc
and no ROADMAP_SUMMARY.md.

## Counts

- `PLAN_*.md` docs on disk: 26
- `ROADMAP_SUMMARY.md`: 1
- In-scope files total: 27
- Indexed in DOCS_INDEX.md: 0
- Missing from index: 27

## Verdict

FAIL — none of the 26 PLAN_* docs or ROADMAP_SUMMARY.md is indexed in
DOCS_INDEX.md (0/27 linked). DOCS_INDEX.md needs a plan-docs section; that
edit is owned by another task, so this audit only reports the gap.

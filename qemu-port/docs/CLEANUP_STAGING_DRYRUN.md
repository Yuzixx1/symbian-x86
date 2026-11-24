# Cleanup Staging Dry-Run Validation

Date: 2026-11-24. HEAD = `d26fade`. READ-ONLY validation: every proposed command
from `docs/CLEANUP_COMMIT_INVENTORY.md` was run with `--dry-run` appended. **No
paths were actually staged** (no `git add`/`git rm` without `--dry-run` was run).
No commit/push. `kernelhwsrv/` untouched (`git status`/`git diff` for the path:
empty).

## 1. Commands run (from repo root)

```sh
git rm --dry-run -- qemu-port/user/portfilesrv.cpp \
                   qemu-port/tools/mke32_portfilesrv.py 'qemu-port/:portfilesrv'
git add --dry-run -- qemu-port/user/user.mk
git add --dry-run -- qemu-port/docs/DOCS_INDEX.md qemu-port/docs/FINAL_PROJECT_SUMMARY.md \
    qemu-port/docs/PRECOMMIT_AUDIT.md qemu-port/docs/PRESERVATION_AUDIT.md \
    qemu-port/docs/ARCHIVE_FINAL_CHECK.md qemu-port/docs/CLEANUP_LOG.md \
    qemu-port/docs/CLEANUP_STEP_STATUS.md qemu-port/docs/FINAL_TREE_VERIFICATION.md \
    qemu-port/docs/POST_REMOVAL_VERIFY.md qemu-port/docs/R1FS_GLUE_FEASIBILITY.md \
    qemu-port/docs/REMAINING_SHIMS_AUDIT.md qemu-port/docs/SVCMGR_PLAN_REVIEW.md \
    qemu-port/docs/SVCMGR_RETIREMENT_PLAN.md qemu-port/docs/WINSRV_EDISP_SCOPE.md \
    qemu-port/docs/CLEANUP_COMMIT_INVENTORY.md
```

(Group C — `qemu-port/Makefile`, `qemu-port/variant/qemu_variant.h`,
`qemu-port/variant/qemu_variant_asic.cpp` — deliberately NOT dry-run/staged: in
flight per inventory §1.C.)

## 2. Would-stage list (19 entries: 3 removals + 16 adds)

```
rm  'qemu-port/:portfilesrv'
rm  'qemu-port/tools/mke32_portfilesrv.py'
rm  'qemu-port/user/portfilesrv.cpp'
add 'qemu-port/user/user.mk'
add 'qemu-port/docs/DOCS_INDEX.md'
add 'qemu-port/docs/FINAL_PROJECT_SUMMARY.md'
add 'qemu-port/docs/PRECOMMIT_AUDIT.md'
add 'qemu-port/docs/PRESERVATION_AUDIT.md'
add 'qemu-port/docs/ARCHIVE_FINAL_CHECK.md'
add 'qemu-port/docs/CLEANUP_COMMIT_INVENTORY.md'
add 'qemu-port/docs/CLEANUP_LOG.md'
add 'qemu-port/docs/CLEANUP_STEP_STATUS.md'
add 'qemu-port/docs/FINAL_TREE_VERIFICATION.md'
add 'qemu-port/docs/POST_REMOVAL_VERIFY.md'
add 'qemu-port/docs/R1FS_GLUE_FEASIBILITY.md'
add 'qemu-port/docs/REMAINING_SHIMS_AUDIT.md'
add 'qemu-port/docs/SVCMGR_PLAN_REVIEW.md'
add 'qemu-port/docs/SVCMGR_RETIREMENT_PLAN.md'
add 'qemu-port/docs/WINSRV_EDISP_SCOPE.md'
```

Matches inventory groups A + B + D + E exactly. All intended deletions (3) and all
intended docs/build-glue (16) present.

## 3. Forbidden-absent confirmation

Grep of the would-stage set for `state.json | qemu-slot.lock | archive-56fbaff |
qemu-port/build/ | kernelhwsrv/ | AGENTS.md` → **0 matches**. None of the
do-not-stage items (inventory §1.G) would be staged; `AGENTS.md` and
`state.json` (group F) also absent.

`git check-ignore -v` on `symbian-x86-archive-56fbaff.tar.gz`,
`qemu-port/.sybian/qemu-slot.lock`, `qemu-port/.sybian/coordinator/state.json`
→ exit 1 (NOT gitignored) — confirms the inventory caveat: the tarball and lock
are untracked-but-not-ignored, so they are excluded only by explicit path
selection, never by `git add -A`. `git status --ignored -- qemu-port/build/`
shows `!! qemu-port/build/` (build artifacts properly gitignored).

## 4. Cross-check vs `git status --short`

27 entries total: 10 `M`, 3 `D`, 14 `??`. The 19 would-stage entries correspond
to all 3 `D` + 1 of the `M` (user.mk) + 15 of the `??` docs. Left unstaged by
design: group C (3 `M`), group F (`AGENTS.md`, `state.json`), `qemu-slot.lock`,
the archive tarball, and one untracked doc NOT in the plan (see caveats).

## 5. Verdict: PLAN VALID

The staging plan stages exactly the intended 19 paths and nothing forbidden.

## 6. Caveats

- One untracked doc, `qemu-port/docs/EUSER_SHIM_SCOPE.md`, is NOT in the plan's
  add list — decide explicitly whether to include it or leave untracked.
- Group C (r1_fs glue: Makefile, qemu_variant.h, qemu_variant_asic.cpp) remains
  excluded until the in-flight editor finishes and the diff is re-reviewed.
- Several docs were recently modified (DOCS_INDEX.md, CLEANUP_LOG.md,
  SVCMGR_*, WINSRV_EDISP_SCOPE.md) — re-check mtimes/content before real staging.
- `.git/index.lock` was briefly held by another git process during validation;
  `git add --dry-run` succeeded on retry. Harmless here, but real staging should
  run when no other git process is active.
- No commit/push performed; no authorization given. kernelhwsrv/ verified clean.

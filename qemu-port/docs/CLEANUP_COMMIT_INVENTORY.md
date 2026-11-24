# Cleanup Commit Inventory — staging plan for a future user-authorized commit (REFRESHED)

Date: 2026-11-24 (refresh). HEAD = `d26fade` (pushed). All commands below are
read-only inspection; **nothing is staged or committed here**. No commit/push
without explicit user authorization. `kernelhwsrv/` verified clean
(`git status --short -- kernelhwsrv/` empty, re-checked at refresh time).

**Refresh reason:** `CLEANUP_STAGING_DRYRUN.md` §6 flagged two stale points, both
now resolved:
1. `qemu-port/docs/EUSER_SHIM_SCOPE.md` was untracked but missing from the add
   list → now included in the docs group.
2. Group C (r1_fs kernel glue) was excluded as in-flight → **now LANDED +
   VERIFIED** (`R1Fs::` symbols 56→0 per `nm`, `FatMount`/`FatUse` present;
   kernfull + isogrub clean; smoke / retire 4/4 / fullchain 14/14 / fat_io
   PASS; see `CLEANUP_LOG.md` step 3 and `docs/R1FS_REMOVAL_STATUS.md`). It is
   now its own commit group.

**New in-flight caveat (refresh-time):** the svcmgr apply agent is editing
`qemu-port/variant/e32load.cpp`, `qemu-port/variant/qemu_variant_asic.cpp`,
`qemu-port/variant/qemu_variant.h`, `qemu-port/user/user.mk` (and Makefile)
RIGHT NOW. Those paths are **IN FLIGHT** — keep them out of any staging run
until the apply agent finishes and the diff is re-reviewed, or split as noted
in §4.

## 1. Inventory (git status --short, grouped)

### A. Deletions (retired portfilesrv artifacts) — 3 files
- `D qemu-port/user/portfilesrv.cpp` (228 lines deleted)
- `D qemu-port/tools/mke32_portfilesrv.py` (176 lines deleted)
- `D qemu-port/:portfilesrv` (840-byte binary stray, deleted)

### B. Build-glue edits for the portfilesrv removal — 1 file
- `M qemu-port/user/user.mk` — drops `PORTFILESRV.E32` from `userland`/`svcmgr`
  targets and the portfilesrv build rules. **Path now also touched by the
  in-flight svcmgr apply agent — stage only after that settles and the diff is
  re-reviewed.**

### C. r1_fs kernel-glue removal — LANDED + VERIFIED, own commit group
Verified landed (agent-reported: `R1Fs::` 56→0, gates green; `CLEANUP_LOG.md`
step 3 LANDED + VERIFIED):
- `M qemu-port/Makefile` — removes `r1_fs_core.o`/`r1_fs_fat.o` kernel objects/rules
- `M qemu-port/variant/qemu_variant.h` — removes `R1FsSession`/`R1FsCore` decls
- `M qemu-port/variant/qemu_variant_asic.cpp` — Asic::Init3 R1 mount block
  replaced with direct `FatMount(4,0)`
**Same in-flight caveat as B:** `qemu_variant_asic.cpp`/`qemu_variant.h`/
`Makefile` are being edited by the svcmgr apply agent now; stage only after it
finishes and the r1_fs-only diff is re-confirmed.

### D. Docs — modified (tracked) — 4 files
- `M qemu-port/docs/DOCS_INDEX.md`
- `M qemu-port/docs/FINAL_PROJECT_SUMMARY.md`
- `M qemu-port/docs/PRECOMMIT_AUDIT.md`
- `M qemu-port/docs/PRESERVATION_AUDIT.md`

### E. Docs — new (untracked) — 15 files (add list now complete)
- `qemu-port/docs/ARCHIVE_FINAL_CHECK.md`
- `qemu-port/docs/CLEANUP_COMMIT_INVENTORY.md` (this file)
- `qemu-port/docs/CLEANUP_LOG.md`
- `qemu-port/docs/CLEANUP_STAGING_DRYRUN.md`
- `qemu-port/docs/CLEANUP_STEP_STATUS.md`
- `qemu-port/docs/EUSER_SHIM_SCOPE.md` ← **added at refresh** (was missing)
- `qemu-port/docs/FINAL_TREE_VERIFICATION.md`
- `qemu-port/docs/POST_REMOVAL_VERIFY.md`
- `qemu-port/docs/R1FS_GLUE_FEASIBILITY.md`
- `qemu-port/docs/R1FS_REMOVAL_STATUS.md` ← **added at refresh** (was missing)
- `qemu-port/docs/REMAINING_SHIMS_AUDIT.md`
- `qemu-port/docs/SVCMGR_PLAN_REVIEW.md`
- `qemu-port/docs/SVCMGR_RETIREMENT_PLAN.md`
- `qemu-port/docs/WINSRV_EDISP_SCOPE.md`

### F. Modified but OUT of cleanup scope (user decision)
- `M AGENTS.md` (root, +40 lines) — project instructions, not cleanup material.
- `M qemu-port/.sybian/coordinator/state.json` — coordinator runtime state.

### G. Do-NOT-stage list (never `git add`)
- `qemu-port/.sybian/coordinator/state.json` (runtime state)
- `qemu-port/.sybian/qemu-slot.lock` (untracked lock; not gitignored)
- `symbian-x86-archive-56fbaff.tar.gz` (preservation tarball; not gitignored)
- `qemu-port/build/` artifacts (gitignored; verify with `git status --ignored`)
- `kernelhwsrv/` — READ-ONLY, never touched by any staging command
- `qemu-port/variant/e32load.cpp` — no diff at refresh, but owned by the
  in-flight svcmgr apply agent; do not stage.
- NEVER `git add -A` (project rule).

## 2. Verification results

- `kernelhwsrv/`: clean, untouched (empty `git status`/`git diff` for the path,
  re-verified at refresh).
- Refresh-time `git diff --stat`: 13 files changed, +253/−510 — deletions (A),
  glue (B, C), and docs (D) only, plus out-of-scope F entries.
- Secrets scan (prior dry-run): only false-positive "token" wording in
  rollback-snapshot labels; no credentials in any staged-candidate file.

## 3. Exact staging commands (run from repo root, ONLY after explicit user authorization)

Suggested split: **TWO commits** — (1) code cleanup, (2) docs.

```sh
# === COMMIT 1: code cleanup (deletions + build glue + landed r1_fs removal) ===
# A. deletions (stage removal explicitly)
git rm -- qemu-port/user/portfilesrv.cpp qemu-port/tools/mke32_portfilesrv.py 'qemu-port/:portfilesrv'
# B+C. glue — ONLY after the svcmgr apply agent finishes and the diff is re-reviewed:
git add -- qemu-port/user/user.mk qemu-port/Makefile \
          qemu-port/variant/qemu_variant.h qemu-port/variant/qemu_variant_asic.cpp

# === COMMIT 2: docs ===
git add -- qemu-port/docs/DOCS_INDEX.md \
          qemu-port/docs/FINAL_PROJECT_SUMMARY.md \
          qemu-port/docs/PRECOMMIT_AUDIT.md \
          qemu-port/docs/PRESERVATION_AUDIT.md \
          qemu-port/docs/ARCHIVE_FINAL_CHECK.md \
          qemu-port/docs/CLEANUP_COMMIT_INVENTORY.md \
          qemu-port/docs/CLEANUP_LOG.md \
          qemu-port/docs/CLEANUP_STAGING_DRYRUN.md \
          qemu-port/docs/CLEANUP_STEP_STATUS.md \
          qemu-port/docs/EUSER_SHIM_SCOPE.md \
          qemu-port/docs/FINAL_TREE_VERIFICATION.md \
          qemu-port/docs/POST_REMOVAL_VERIFY.md \
          qemu-port/docs/R1FS_GLUE_FEASIBILITY.md \
          qemu-port/docs/R1FS_REMOVAL_STATUS.md \
          qemu-port/docs/REMAINING_SHIMS_AUDIT.md \
          qemu-port/docs/SVCMGR_PLAN_REVIEW.md \
          qemu-port/docs/SVCMGR_RETIREMENT_PLAN.md \
          qemu-port/docs/WINSRV_EDISP_SCOPE.md

# sanity: git status --short must show only intended paths; nothing from group F/G
# dry-run any of the above by appending --dry-run
```

## 4. Suggested commit messages

Commit 1 (code cleanup):
```
Cleanup: delete retired portfilesrv artifacts + remove landed r1_fs kernel glue

Deletes portfilesrv.cpp, mke32_portfilesrv.py and the stray :portfilesrv
binary; drops PORTFILESRV.E32 rules from user.mk; removes the landed r1_fs
kernel glue (Makefile objects, R1Fs decls in qemu_variant.h, direct
FatMount(4,0) in qemu_variant_asic.cpp; R1Fs:: symbols 56->0, gates green).
Genuine EFSRV remains the sole file server (d26fade). kernelhwsrv/ untouched.
```

Commit 2 (docs):
```
Docs: cleanup-phase log, staging dry-run, and scope plans (post-d26fade)

Adds CLEANUP_*/R1FS_*/SVCMGR_*/WINSRV_EDISP_SCOPE/EUSER_SHIM_SCOPE/
POST_REMOVAL_VERIFY docs and updates DOCS_INDEX and audit summaries.
```

**Split fallback if the svcmgr apply is still in flight at staging time:**
stage commit 1 as deletions-only now (`git rm` the 3 portfilesrv paths), and
move B+C (user.mk, Makefile, qemu_variant.h, qemu_variant_asic.cpp) into a
follow-up commit once the apply agent finishes and the r1_fs-only diff is
re-reviewed. Never stage B/C files while their diff still contains unreviewed
svcmgr-apply changes.

## 5. Caveats / blockers

- No commit or push without explicit user authorization (none given).
- **IN FLIGHT at refresh:** svcmgr apply agent owns `e32load.cpp`,
  `qemu_variant_asic.cpp`, `qemu_variant.h`, `user.mk` (and `Makefile`) —
  groups B and C must not be staged until it finishes; re-check
  `git diff --stat` for those paths first.
- `qemu-slot.lock` and the archive tarball are untracked but NOT gitignored —
  excluded only by explicit path selection, never `git add -A`.
- `AGENTS.md` and `state.json` modifications are out of scope; leave unstaged.
- No QEMU runs performed; this inventory is read-only. kernelhwsrv/ untouched.

# PLAN_P0_COMMIT — the concrete P0 hygiene commit (design only)

**Date:** 2026-11-25. **Scope:** design of the single docs-only P0 commit that
cleans the working tree at HEAD `ee8490e` on branch `main`. This document is
the only file written by this task. **No commit, no push, no `make clean`, no
QEMU, no edits under `kernelhwsrv/` (READ-ONLY, verified clean).** Per
AGENTS.md ("Do not commit/push unless explicitly asked", "never `git add -A`"),
execution of §1/§3/§4 awaits an explicit user authorization; every step below
uses explicit paths only.

**Live-tree caveat:** sibling agents are still writing `qemu-port/docs/PLAN_*.md`
(19 untracked at snapshot time, count drifted 15→17→19 while this was drafted;
23 untracked — 17 PLAN_SURVEY files — at the 2026-11-25 correction).
The PLAN subset of the stage list is therefore pinned by a regeneration command
(§1, step 2), not by the enumerated snapshot alone; the staged set is verified
exactly in §5 before commit.

## 1. Stage list

Exactly these paths, staged with one explicit `git add` (never `git add -A`,
never `git commit -a`):

**Group A — modified, staged (6):**
- `AGENTS.md`
- `.gitignore` (rules appended per §4)
- `qemu-port/docs/CLEANUP_LOG.md`
- `qemu-port/docs/DOCS_INDEX.md`
- `qemu-port/docs/FINAL_GATE_STATUS.md`
- `qemu-port/docs/GO_NO_GO.md`

**Group B — new closeout, staged (1):**
- `qemu-port/docs/WINSRV_RETIREMENT_FINAL.md` — the authoritative closeout
  (APPLY_RC=0, P1–P3 PASS, commit `94ce9c9`, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` clean, 11/11 strict PASS);
  it explicitly supersedes the `WINSRV_DELETE_WATCH*` reports, which is what
  makes §3 safe.

**Group C — new PLAN docs, staged (all untracked `qemu-port/docs/PLAN_*.md`
at execution time; 19 at snapshot plus this file, 23 untracked at the
2026-11-25 correction — 17 PLAN_SURVEY + 6 other PLAN_* incl. this file):**

**Group D — other new non-PLAN docs, staged (1):**
- `qemu-port/docs/ROADMAP_SUMMARY.md` — appeared after the snapshot; matches
  neither Group A/B nor the `PLAN_` grep, so it is staged explicitly (else
  §5(c)'s empty-others check would fail-closed abort). No other non-PLAN new
  doc exists at correction time (`PLAN_CANONICAL_DECISIONS.md` absent).
- `qemu-port/docs/PLAN_P0_COMMIT.md` (this file)
- `qemu-port/docs/PLAN_DECISION_REGISTER.md`
- `qemu-port/docs/PLAN_EXECUTION.md`
- `qemu-port/docs/PLAN_SURVEY_BUILD_TOOL.md`
- `qemu-port/docs/PLAN_SURVEY_ESOCK.md`
- `qemu-port/docs/PLAN_SURVEY_FREE_GAMES.md`
- `qemu-port/docs/PLAN_SURVEY_GAME_ASSETS.md`
- `qemu-port/docs/PLAN_SURVEY_GATES.md`
- `qemu-port/docs/PLAN_SURVEY_GUI_MILESTONE.md`
- `qemu-port/docs/PLAN_SURVEY_HW_USB.md`
- `qemu-port/docs/PLAN_SURVEY_HYGIENE.md`
- `qemu-port/docs/PLAN_SURVEY_MEDIA.md`
- `qemu-port/docs/PLAN_SURVEY_NEXT_GAME.md`
- `qemu-port/docs/PLAN_SURVEY_PROVENANCE.md`
- `qemu-port/docs/PLAN_SURVEY_REALHW_METHOD.md`
- `qemu-port/docs/PLAN_SURVEY_ROLLBACK.md`
- `qemu-port/docs/PLAN_SURVEY_SHIMS.md`
- `qemu-port/docs/PLAN_SURVEY_SOURCE_IMPORT.md`
- `qemu-port/docs/PLAN_SURVEY_UIKON_GDI.md`
- `qemu-port/docs/PLAN_SURVEY_WSRV_NAME.md`

Execution:

```sh
cd "/home/yuzi/Documents/Sybian REAL"
# 1. fixed set
git add AGENTS.md .gitignore \
  qemu-port/docs/CLEANUP_LOG.md qemu-port/docs/DOCS_INDEX.md \
  qemu-port/docs/FINAL_GATE_STATUS.md qemu-port/docs/GO_NO_GO.md \
  qemu-port/docs/WINSRV_RETIREMENT_FINAL.md \
  qemu-port/docs/ROADMAP_SUMMARY.md
# 2. dynamic PLAN set (explicit paths derived from git status, not add -A)
git status --porcelain | grep '^?? qemu-port/docs/PLAN_' | awk '{print $2}' \
  | tee /tmp/p0_plan_list.txt | xargs git add
# 3. commit only after the §5 staged-set guard passes
git commit -m "P0 hygiene: commit winsrv closeout + PLAN_* surveys + status docs; archive untracked WINSRV_DELETE_WATCH reports; gitignore tarball/lock/stray"
```

Suggested message only — append the standard Mistral Vibe trailer when the
authorized agent actually commits. Do not push.

## 2. Exclusions (never staged, never committed in this change)

- `qemu-port/.sybian/coordinator/state.json` — tracked, modified (timestamp
  bump `2026-10-29` → `2026-11-23` only). Per directive it stays **unstaged**:
  it is tracked, so "untracked" is not achievable without an unauthorized
  `git rm --cached`, and `.gitignore` has no effect on tracked files, so no
  rule is added for it. Leave the working-tree modification in place; do not
  `git restore` it either.
- `symbian-x86-archive-56fbaff.tar.gz` (2,481,838 B, 983 entries) — preservation
  point per AGENTS.md (`archive/handwritten-r25` @ `56fbaff` + tarball). Stays
  on disk, stays untracked, silenced by a new `*.tar.gz` rule.
- `qemu-port/.sybian/qemu-slot.lock` (0 B) — transient flock created by
  `.sybian/bin/coord` / `.sybian/bin/work`. Stays untracked; explicit rule.
  Do **not** ignore all of `qemu-port/.sybian/` — `bin/`, `coordinator/cards/`,
  `coordinator/state.json`, `workers/inbox/` are tracked.
- `qemu-port/9` (0 B stray) — deleted in §3, never staged; defensive
  `/qemu-port/9` rule in `.gitignore`.
- The **128 untracked** `qemu-port/docs/WINSRV_DELETE_WATCH*.md` — archived per
  §3, never staged. The **197 tracked** WATCH files already in git history are
  out of scope: do not delete them in this commit.
- Everything under `kernelhwsrv/` — READ-ONLY; §5 asserts zero staged paths
  there.
- `build/`, `*.o`, `*.E32`, `*.log`, `*.zip`, … — already ignored; untouched.

## 3. Archive method

Order matters: **commit first** (the closeout lands in git history = all
evidence has a permanent home), then archive. No `git rm` is involved — the
WATCH files are untracked, so plain `mv`/`rm` on an explicit list.

```sh
# 0. precondition: P0 commit landed and contains the closeout
git show --stat HEAD | grep WINSRV_RETIREMENT_FINAL
# 1. snapshot the exact untracked WATCH set from git status (never a blind glob)
git status --porcelain | grep '^?? qemu-port/docs/WINSRV_DELETE_WATCH' \
  | awk '{print $2}' > /tmp/p0_watch_list.txt
wc -l < /tmp/p0_watch_list.txt        # must print 128
# 2. archive: move to an ignored dir inside the repo (reversible)
mkdir -p qemu-port/docs/archive/winsrv-delete-watch
while IFS= read -r f; do mv "$f" qemu-port/docs/archive/winsrv-delete-watch/; done < /tmp/p0_watch_list.txt
# 3. delete the 0-byte stray (confirm size first)
test "$(stat -c %s qemu-port/9)" = 0 && rm qemu-port/9
```

Accepted alternative to step 2 if the user prefers zero residual files:
`xargs rm < /tmp/p0_watch_list.txt`. Safe because evidence is preserved three
ways: the committed closeout, the 197 tracked WATCH files still in git
history, and `PLAN_SURVEY_HYGIENE.md` (records the 128 reports md5-unique,
8–30 lines each). Never use `git clean` here.

## 4. .gitignore rules to append (root `.gitignore` is the only tracked one)

```gitignore
# Preservation snapshot of archive/handwritten-r25 @ 56fbaff — never commit
*.tar.gz
# Stray 0-byte runtime artifact at qemu-port root
/qemu-port/9
# Transient coordinator flock (created by .sybian/bin/coord and .sybian/bin/work)
/qemu-port/.sybian/qemu-slot.lock
# Archived superseded WINSRV_DELETE_WATCH monitor reports
/qemu-port/docs/archive/
```

## 5. Verification commands

Run from repo root, in this order; abort on any mismatch.

```sh
# a. staged set is exactly §1 (no more, no less)
git diff --cached --name-only | sort > /tmp/p0_staged.txt
{ git status --porcelain | grep -E '^( M|M |MM)' | awk '{print $2}' | grep -v state.json
  printf '%s\n' .gitignore qemu-port/docs/WINSRV_RETIREMENT_FINAL.md \
    qemu-port/docs/ROADMAP_SUMMARY.md
  cat /tmp/p0_plan_list.txt; } | sort > /tmp/p0_expected.txt
diff /tmp/p0_staged.txt /tmp/p0_expected.txt          # must be empty
# b. hard guards
git diff --cached --name-only | grep -c '^kernelhwsrv/'   # must print 0
git diff --cached --name-only | grep -E 'tar\.gz|qemu-slot\.lock|state\.json|WINSRV_DELETE_WATCH|qemu-port/9$'   # must print nothing
# c. only after a+b pass: commit (message per §1), then:
git status --porcelain    # must show ONLY:  M qemu-port/.sybian/coordinator/state.json
git ls-files --others --exclude-standard                  # must be empty (all else ignored)
git ls-files --others --ignored --exclude-standard | grep -E 'tar\.gz|qemu-slot\.lock'   # both listed
# d. after §3 archive step:
git status --porcelain | grep -c WINSRV_DELETE_WATCH      # must print 0
ls qemu-port/docs/WINSRV_DELETE_WATCH*.md | wc -l         # must print 197 (tracked only)
ls qemu-port/docs/archive/winsrv-delete-watch/ | wc -l    # must print 128
test ! -e qemu-port/9                                     # stray gone
# e. read-only boundary
git -C kernelhwsrv status --porcelain                     # must be empty
# f. no push was performed
git status -sb                                            # may show ahead of origin/main; that is expected — do NOT push
```

## 6. Rollback note

The commit is docs-only, so rollback is trivial and lossless:

- **Before any push:** `git reset --hard HEAD~1` restores the exact prior
  state (the commit touches no untracked file and no code).
- **If §3 already ran:** `mv qemu-port/docs/archive/winsrv-delete-watch/WINSRV_DELETE_WATCH*.md qemu-port/docs/`
  restores all 128 reports; `touch qemu-port/9` recreates the 0-byte stray
  (no content ever existed). `state.json`, the tarball, and `kernelhwsrv/`
  are never touched by any step.
- **If a push ever happens** (not authorized here): `git revert --no-edit HEAD`.
- The preservation tarball `symbian-x86-archive-56fbaff.tar.gz` is never
  staged, moved, or deleted, so it needs no rollback path.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. The commit-readiness condition listing 11/11 strict PASS is now a 15-row strict PASS. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

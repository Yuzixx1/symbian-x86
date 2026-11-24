# PRESERVATION AUDIT — pre-commit verification

Read-only confirmation that the documented preservation points are intact before
any eventual commit. `kernelhwsrv/` is READ-ONLY and untouched; no QEMU was run;
nothing was committed, pushed, or edited except this report.

## 1. Preservation points

Commit `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` (short `56fbaff`) is the
archive point. The annotated tag dereferences to that commit; its tag object is
`4f085970f9b9133859764dfd9624335bd2a321f7`.

| Point | Expected | Actual | OK? |
| --- | --- | --- | --- |
| `origin` branch `archive/handwritten-r25` | points to `56fbaff` | `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` (`refs/heads/archive/handwritten-r25`) | YES |
| `origin` annotated tag `archive/handwritten-r25` | dereferences to `56fbaff` | tag object `4f08597`, `refs/tags/archive/handwritten-r25^{}` = `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` | YES |
| Local branch `archive/handwritten-r25` | points to `56fbaff` | `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` | YES (reported ambiguous with tag; `refs/heads/...` resolves cleanly) |
| Local tag `archive/handwritten-r25` | dereferences to `56fbaff` | `git rev-list -n 1` = `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` | YES |
| `git rev-parse archive/handwritten-r25^{commit}` | `56fbaff` | `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` | YES |
| `symbian-x86-archive-56fbaff.tar.gz` | present, readable, 983 entries | `-rw-r--r--` 2,481,838 bytes; `tar tzf | wc -l` = `983` | YES |
| `origin` remote | GitHub source | `https://github.com/Yuzixx1/symbian-x86.git` (fetch/push) | YES |
| `HEAD` vs archive point | `HEAD` = `d26fade` (authorized commits `7f76900` + `d26fade` pushed to `origin/main`; archive point stays `56fbaff`) | `HEAD` = `origin/main` = `d26fade5db13a140845a30e0faabe4550fb3940d`; chain `d26fade` → `7f76900` → `56fbaff`; 9 dirty paths (M + ??) | YES |
| `docs/SHIM_RETIREMENT_PLAN.md` archive refs | branch/tag `archive/handwritten-r25`, commit `56fbaff`, tarball name | all correct (lines 190, 214–216, 223, 254) | YES |
| `docs/PRECOMMIT_AUDIT.md` archive refs | tarball `symbian-x86-archive-56fbaff.tar.gz` | correct (lines 20, 49) | YES |

No mismatches found. No wrong archive name or commit reference in either doc.

## 2. Rollback recipe

1. Never use `git add -A` — the tarball is untracked and not gitignored. Stage by
   explicit path per `qemu-port/docs/CHANGE_INVENTORY.md` §4.
2. To restore a single shim file from the archive point:
   `git checkout archive/handwritten-r25 -- qemu-port/<path>` (or
   `git show archive/handwritten-r25:qemu-port/<path>` to inspect/compare first).
3. To compare or restore the whole tree, use a worktree at the archive point:
   `git worktree add /tmp/opencode/handwritten-r25 archive/handwritten-r25`.
4. Recover the complete pre-pivot source snapshot from the tarball if git refs
   are unavailable: `tar xzf symbian-x86-archive-56fbaff.tar.gz -C <dest>`.
5. Verify any restoration with `git rev-parse archive/handwritten-r25^{commit}`
   returning `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` and
   `tar tzf symbian-x86-archive-56fbaff.tar.gz | wc -l` returning `983`.

## 3. Caveats

- The tarball is **git source only** (983 entries); it contains no build
  artifacts. Binaries are reproducible from source with `make` — do not expect
  prebuilt images inside it.
- `archive/handwritten-r25` is both a branch and a tag name, so a bare
  `git rev-parse archive/handwritten-r25` warns "refname is ambiguous" and shows
  the *tag object* (`4f08597`), not the commit. Use `^{commit}` /
  `refs/heads/archive/handwritten-r25` / `git rev-list -n 1` for the commit.
- The annotated tag dereferences (not points directly) to `56fbaff`; both are
  correct by design.
- **Correction (2026-11-24):** `HEAD` is now `d26fade5db13a140845a30e0faabe4550fb3940d`,
  identical to `origin/main` (chain `d26fade` → `7f76900` → `56fbaff`). Both
  post-pivot commits (`7f76900` pivot, `d26fade` shim retirement) were authorized
  and pushed; no new commit or push was authorized or performed for this
  correction. The tree has 9 intentional dirty leftovers: `M` `state.json`,
  `M` `DOCS_INDEX.md`, `M` `AGENTS.md`, `??` stray `-D__cdecl` file,
  `??` `qemu-slot.lock`, `??` archive tarball. The archive branch/tag and
  tarball at `56fbaff` above are unchanged and remain the preservation points.
- This report is the only file written; `kernelhwsrv/` was not touched.

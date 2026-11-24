# ARCHIVE FINAL CHECK — post-commit preservation verification

Read-only confirmation that the documented preservation points survived commits
`7f76900` (genuine-userspace pivot) and `d26fade` (shim retirement).
`kernelhwsrv/` is READ-ONLY and untouched; no QEMU was run; nothing was committed
or pushed; this report is the only file written.

## 1. Preservation points

| Point | Expected | Actual | OK? |
| --- | --- | --- | --- |
| `origin` branch `main` | `d26fade` (shim retirement) | `d26fade5db13a140845a30e0faabe4550fb3940d` (`refs/heads/main`) | YES |
| `main` history order | `d26fade` → `7f76900` → `56fbaff` | `git log --oneline -3 main` = `d26fade`, `7f76900`, `56fbaff` | YES |
| `origin` branch `archive/handwritten-r25` | `56fbaff` | `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` (`refs/heads/archive/handwritten-r25`) | YES |
| `origin` annotated tag `archive/handwritten-r25` | dereferences to `56fbaff` | tag object `4f08597`, `refs/tags/archive/handwritten-r25^{}` = `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` | YES |
| Local branch `archive/handwritten-r25` | `56fbaff` | `git rev-list -n 1 refs/heads/archive/handwritten-r25` = `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` | YES |
| Local tag `archive/handwritten-r25` | dereferences to `56fbaff` | `git rev-list -n 1 refs/tags/archive/handwritten-r25` = `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` | YES |
| `git rev-parse archive/handwritten-r25^{commit}` | `56fbaff` | `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` (warns "refname is ambiguous" — expected) | YES |
| `symbian-x86-archive-56fbaff.tar.gz` | present, readable, 983 entries | `tar tzf … \| wc -l` = `983`; 2,481,838 bytes; untracked (`??`) | YES |
| `docs/PRESERVATION_AUDIT.md` archive refs | branch/tag/tarball at `56fbaff` | correct (lines 9–20, 39–41) — all archive name/commit references intact | YES |
| Project `AGENTS.md` archive refs | `archive/handwritten-r25` at `56fbaff`, tarball name | correct (lines 27–28, 90–97); `d26fade` / `7f76900` pushed state also correct | YES |
| `origin` remote | GitHub source | `https://github.com/Yuzixx1/symbian-x86.git` (fetch and push) | YES |

No wrong archive name or commit reference in either doc. No preservation-point
drift: both post-pivot commits are on `main`, while the archive branch/tag and
tarball still pin `56fbaff`.

## 2. Remote refs (`git ls-remote --heads --tags origin`)

```
56fbaffbcf41cc4f97d60a6deeef6912376a0e44  refs/heads/archive/handwritten-r25
d26fade5db13a140845a30e0faabe4550fb3940d  refs/heads/main
4f085970f9b9133859764dfd9624335bd2a321f7  refs/tags/archive/handwritten-r25
56fbaffbcf41cc4f97d60a6deeef6912376a0e44  refs/tags/archive/handwritten-r25^{}
```

## 3. Tarball hash

```
f52798d554b32d7688b6b78fce319776126bb99aa1209fb22acfdc6ab6d0cb50  symbian-x86-archive-56fbaff.tar.gz
```

No pre-pinned sha256 for this tarball exists in the docs (the audit records only
entry count and size), so this value is recorded here as the baseline for future
comparison. Entry count `983` matches the documented expectation exactly.

## 4. Caveats

- `docs/PRESERVATION_AUDIT.md` is explicitly a **pre-commit** snapshot: its row
  "HEAD vs archive point" (line 22) and caveat at lines 54–55 still say `HEAD` =
  `56fbaff` with 56 dirty paths. That is now stale in the literal sense — current
  `HEAD` = `d26fade5db13a140845a30e0faabe4550fb3940d` with 9 dirty paths — but it
  is not a preservation-point error: the archive name, commit and tarball refs in
  that doc are all still correct. Do not edit that pre-commit report.
- `archive/handwritten-r25` is both a branch and a tag, so a bare
  `git rev-parse archive/handwritten-r25` warns "refname is ambiguous" and returns
  the tag object; use `^{commit}` or `refs/heads/...` for the commit.
- The annotated tag dereferences (not points directly) to `56fbaff`; both forms
  are correct by design.
- The tarball is **git source only** (983 entries), no build artifacts; binaries
  are reproducible from source with `make`. It remains untracked — never use
  `git add -A`.
- Rollback remains valid via `git show 7f76900:<path>` / a worktree at
  `archive/handwritten-r25` / extracting the tarball.
- `kernelhwsrv/` was not touched; no QEMU, commits or pushes were performed.

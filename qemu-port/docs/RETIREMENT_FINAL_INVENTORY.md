# RETIREMENT_FINAL_INVENTORY — exact staging plan (read-only, 2026-11-24)

Plan only. No staging, commit, push, QEMU run, `make clean` or `kernelhwsrv/`
change was made by this inventory. Fallback session after Mistral
`API budget exhausted`. Baseline HEAD `7f76900` (branch `main`);
`origin/main` matches local. The only file created here is this report.

## 1. Counts (from repo root `git status --short`)

| Category | Count | Files |
|---|---|---|
| Modified tracked | 4 | `qemu-port/user/svcmgr.cpp`, `qemu-port/variant/e32load.cpp`, `qemu-port/docs/DOCS_INDEX.md`, `qemu-port/.sybian/coordinator/state.json` |
| Untracked | 8 | 5 new docs + this report's 1 predecessor set below, plus 2 runtime/stray and the tarball |
| `kernelhwsrv/` changes | 0 | clean (`git status --short kernelhwsrv/` → empty, `git diff --stat` → empty) |
| `qemu_variant_asic.cpp` diff | 0 | byte-identical to HEAD (svcmgr apply rolled back) |

`git diff --stat`: 4 files, 28 insertions, 3 deletions (2 source lines, +25
`DOCS_INDEX.md`, 1 `state.json` timestamp). `git diff --check`: clean (no
whitespace errors).

Untracked breakdown: `docs/CP4_STRESS_INDEPENDENT_CHECK.md`,
`docs/POST_COMMIT_TREE_REVIEW.md`, `docs/RETIREMENT_COMMIT_REVIEW.md`,
`docs/SHIM_RETIREMENT_RESULT.md`, `docs/STALE_OBJECT_REBUILD.md` (5 new docs),
plus this report = 6 new docs total; `.sybian/qemu-slot.lock` (0-byte runtime
lock), `-D__cdecl=__attribute__((cdecl))` (0-byte stray compiler-flag artifact),
`symbian-x86-archive-56fbaff.tar.gz` (~2.4 MB preservation tarball).

`docs/` currently holds **164** `.md` files; this report makes **165**.
`DOCS_INDEX.md` was verified **append-only** (one hunk added at EOF, no
deletions) but its backfill text still claims 159 — see Caveats.

## 2. Verified retirement edits (exact)

- `qemu-port/variant/e32load.cpp` ~1552 (`GenuineBootLaunch`):
  `SvcMgrBootLaunch();  // fallback: our svcmgr starts \PORTFILESRV.E32`
  → `// RETIRED by r1_shim_retire_apply: SvcMgrBootLaunch();  // fallback ...`
- `qemu-port/user/svcmgr.cpp` ~250 (`kServices[]`):
  `{ "\\PORTFILESRV.E32", 0, "!FileServer", 0 },  /* F32 / R1 file server */`
  → `// RETIRED by r1_shim_retire_apply: { "\\PORTFILESRV.E32", ... }, ...`
- `qemu-port/variant/qemu_variant_asic.cpp`: no diff (rollback complete).

Both lines are commented, not deleted. Genuine chain (`\EFILE`/`\EFSRV`/
`\ESTART`/`\E32STRT`/`\EWSRV`/`\ESHELL`, `GenuineBootSpawn`,
`ShellSpawnFull`) untouched.

## 3. Secrets scan

Grep for API keys, passwords, bearer tokens, `ghp_`/`AKIA` patterns and private
key headers across all report docs: **no credentials found**. Matches for the
word "token" are only shim-tool operation identifiers (`RETIRE-PORTFILESRV`,
`ROLLBACK-PORTFILESRV-20261124_083011`, etc.), not secrets — nothing to redact.

## 4. Exact staging commands (path-by-path; never `git add -A` / `git add .`)

```bash
cd '/home/yuzi/Documents/Sybian REAL'
git add qemu-port/variant/e32load.cpp \
        qemu-port/user/svcmgr.cpp \
        qemu-port/docs/SHIM_RETIREMENT_RESULT.md \
        qemu-port/docs/CP4_STRESS_INDEPENDENT_CHECK.md \
        qemu-port/docs/STALE_OBJECT_REBUILD.md \
        qemu-port/docs/RETIREMENT_COMMIT_REVIEW.md \
        qemu-port/docs/POST_COMMIT_TREE_REVIEW.md \
        qemu-port/docs/RETIREMENT_FINAL_INVENTORY.md \
        qemu-port/docs/DOCS_INDEX.md
git diff --cached --stat            # expect 9 files: 2 source, 6 new docs, 1 index
git status --short                  # confirm staged set before committing
```

Staged set = **9 files**: 2 source edits + 6 new docs (including this report) +
1 append-only index. Do not stage anything else.

## 5. Do NOT stage

- `symbian-x86-archive-56fbaff.tar.gz` — preservation tarball.
- `qemu-port/.sybian/coordinator/state.json` — runtime `updated_at` only.
- `qemu-port/.sybian/qemu-slot.lock` — 0-byte runtime lock.
- `qemu-port/-D__cdecl=__attribute__((cdecl))` — 0-byte stray artifact; delete
  in a separate authorized cleanup, never commit.
- `qemu-port/build/*` — gitignored build outputs (ISO ~75.9 MB, ELF, `.o`);
  rebuilt locally by `touch` + `make`, never staged.
- `kernelhwsrv/` — READ-ONLY, clean.

## 6. Commit message

```
Retire portfilesrv/r1_fs shims: genuine EFSRV is sole file server

Comment out the SvcMgrBootLaunch() fallback in GenuineBootLaunch and the
\PORTFILESRV.E32 service row in svcmgr; both shims retired after 7f76900.
Snapshots: shim_retire_backup_20261124_083011 (portfilesrv) and
_083050 (svcmgr, rolled back, qemu_variant_asic.cpp restored).
Gates (final state, stale build object repaired): final verify 11/11 strict
PASS, retire 4/4, fullchain 14/14 (no fallback, 0 faults), cp4_stress 4/4.
kernelhwsrv/ untouched; no shim retired until its gate passed.
```

## 7. Caveats

- `SHIM_RETIREMENT_RESULT.md` §5 still records the **intermediate** 10 PASS /
  1 FAIL (cp4_stress) state. That failure was proven to be a stale
  `build/qemu_variant_asic.o`, not a retirement regression; after `touch` +
  `make` rebuild the final state is **11/11**. Read §5 with
  `CP4_STRESS_INDEPENDENT_CHECK.md` + `STALE_OBJECT_REBUILD.md`.
- `DOCS_INDEX.md` backfill text says 159 `.md` files; actual count is now 165
  with this report. Its diff is append-only and the linked reports exist; a
  future authorized cleanup may refresh the count, but no further index edit
  is made here.
- `RETIREMENT_COMMIT_REVIEW.md` §4 staged only 4 files (2 code + 2 docs) and an
  older commit message; this inventory supersedes both with the full 9-file
  plan and final 11/11 gate results.
- Snapshots `/tmp/opencode/shim_retire_backup_20261124_083011` and `_083050`
  are sha256-verified against HEAD originals; keep them until the commit is
  confirmed, then they may be discarded (rollback tokens recorded in the docs).
- No commit/push authorization exists here — this is a plan only.

---

## Supersession note — 2026-11-26

**Any 11-row / 11-pass `r1_final_verify.py --strict-skip` requirement or claim in this document is historical.** The current authority is 15 rows: `FINAL27_RC=0` (15 pass, 0 skip, 0 fail), captured in `/tmp/opencode/final27_gate.log`. Read every "11/11", "11 rows", or "PASS (11 pass, 0 skip, 0 fail)" above as a record of the state at its own date, not as a live gate. The body of this file is left byte-identical; this note is the only change.

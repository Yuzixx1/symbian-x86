# RETIREMENT_COMMIT_REVIEW — applied-shim diff audit + future commit staging plan

Read-only review dated 2026-11-24. No edits except this report; no QEMU run, no
commit, no push, no `make clean`. `kernelhwsrv/` verified clean and untouched.
Baseline HEAD: `7f76900` (branch `main`). Fallback session review after Mistral
`API budget exhausted`.

## 1. `git diff` audit — only intended shim lines changed

`git diff -- qemu-port/variant/e32load.cpp qemu-port/user/svcmgr.cpp
qemu-port/variant/qemu_variant_asic.cpp` shows exactly two changed lines, each a
commented-out (not deleted) shim line:

- `qemu-port/variant/e32load.cpp:1552` (`GenuineBootLaunch`):
  `- SvcMgrBootLaunch();  // fallback: our svcmgr starts \PORTFILESRV.E32`
  `+ // RETIRED by r1_shim_retire_apply: SvcMgrBootLaunch();  // fallback ...`
- `qemu-port/user/svcmgr.cpp:250` (`kServices[]`):
  `- { "\\PORTFILESRV.E32", 0, "!FileServer", 0 },  /* F32 / R1 file server */`
  `+ // RETIRED by r1_shim_retire_apply: { "\\PORTFILESRV.E32", ... }, ...`
- `qemu-port/variant/qemu_variant_asic.cpp`: **no diff** — the out-of-scope svcmgr
  edit was applied then rolled back; the file is byte-identical to HEAD.

**Genuine entries intact:** the genuine boot list (`kGenuineBootList`,
`GenuineBootSpawn()`, `ShellSpawnFull()`, the `\EFILE`/`\EFSRV`/`\ESTART`/
`\E32STRT`/`\EWSRV`/`\ESHELL` chain) and all other `kServices[]` rows are
unchanged; the diff touches no genuine component. The EUSER.DLL null-path
prerequisite row and the commented H-02/L-06/future rows remain verbatim.

**Shim-only disk-fallback behaviour:** with both shim launch lines retired, a
disk-only boot (no genuine EFSRV present) no longer falls back to launching
`\PORTFILESRV.E32` via `SvcMgrBootLaunch()`. The fallback call in
`GenuineBootLaunch` is now unreachable-as-retired, so `genuineFs || serverUp`
must be satisfied by the genuine EFSRV/F32 server or no file server comes up.
`r1_genuine_fullchain_test.py` confirmed the fallback is **not used on the
genuine path** (14/14, "fallback svcmgr/portfilesrv not used PASS", genuine file
server up with no fallback, 0 faults). This is the intended effect: shim
retirement removes the legacy disk path, leaving only the genuine chain.

## 2. `git status --short` — tree after retirement

Modified (tracked):
- `qemu-port/user/svcmgr.cpp` — intended retirement edit.
- `qemu-port/variant/e32load.cpp` — intended retirement edit.
- `qemu-port/docs/DOCS_INDEX.md` — append-only backfill (+25 lines, 156→159);
  unrelated to retirement, produced by prior commit-prep work.
- `qemu-port/.sybian/coordinator/state.json` — runtime state (`updated_at`
  timestamp only); do not stage.

Untracked (`??`):
- `qemu-port/docs/POST_COMMIT_TREE_REVIEW.md` — new review doc (not retirement).
- `qemu-port/docs/SHIM_RETIREMENT_RESULT.md` — new doc recording the applied
  guarded retirement, gates, and (see §3) a flawed rollback recipe.
- `qemu-port/-D__cdecl=__attribute__((cdecl))` — 0-byte stray compiler-flag
  artifact; exclude/delete in cleanup, never stage.
- `qemu-port/.sybian/qemu-slot.lock` — 0-byte runtime lock; do not stage.
- `symbian-x86-archive-56fbaff.tar.gz` — ~2.4 MB preservation tarball; do not
  stage.
- `qemu-port/docs/RETIREMENT_COMMIT_REVIEW.md` — **this new report** (created by
  this review; previously absent). `docs/` now holds 161 `.md` files (162 with
  this report). `DOCS_INDEX.md` was NOT updated here — that would be a second
  edit, outside this task's single-file allowance; add an index entry in a
  future authorized cleanup if desired.

## 3. Snapshot verification and rollback

Both snapshots exist and are intact. File hashes match **both** their manifest
`sha256` entries **and** the git HEAD (`7f76900`) originals:

| Snapshot | Files | sha256 vs manifest & HEAD |
|---|---|---|
| `/tmp/opencode/shim_retire_backup_20261124_083011` (portfilesrv, token `ROLLBACK-PORTFILESRV-20261124_083011`) | `variant/e32load.cpp` = `356e7bdd…`, `user/svcmgr.cpp` = `e54b0ac7…` | MATCH |
| `/tmp/opencode/shim_retire_backup_20261124_083050` (svcmgr, token `ROLLBACK-SVCMGR-20261124_083050`, consumed by rollback) | `variant/qemu_variant_asic.cpp` = `ef3878be…` | MATCH |

Working-tree hashes: `e32load.cpp` `7c520f79…` and `svcmgr.cpp` `b03e6615…`
differ from HEAD (retirement applied); `qemu_variant_asic.cpp` `ef3878be…`
equals HEAD (rolled back). No snapshot was restored during this review.

**Corrected rollback recipe** (preferred — tool, then manual fallback):

```bash
cd '/home/yuzi/Documents/Sybian REAL/qemu-port'
# Preferred: tool-managed rollback with token from the manifest
python3 tools/r1_shim_retire_apply.py --rollback \
  /tmp/opencode/shim_retire_backup_20261124_083011 \
  --confirm ROLLBACK-PORTFILESRV-20261124_083011
# Manual fallback: restore exact HEAD originals (snapshot == HEAD, verified above)
git -C '/home/yuzi/Documents/Sybian REAL' checkout HEAD -- \
  qemu-port/variant/e32load.cpp qemu-port/user/svcmgr.cpp
cd '/home/yuzi/Documents/Sybian REAL/qemu-port'
make isogrub && python3 tools/r1_os_selfcheck.py
git -C '/home/yuzi/Documents/Sybian REAL' status --short -- kernelhwsrv/  # must be empty
```

**Caution — existing doc recipe is wrong for `e32load.cpp`:**
`qemu-port/docs/SHIM_RETIREMENT_RESULT.md` §6 suggests
`git checkout archive/handwritten-r25 -- ...`. That ref is ambiguous (branch at
`56fbaff`, tag at `4f08597`); both versions of `e32load.cpp` hash to
`d094d372…`, **not** the pre-retirement HEAD original `356e7bdd…`. Using it
would restore an old handwritten-era file, not undo this retirement. Use the
snapshot or `HEAD` restore above instead. (`svcmgr.cpp` happens to be identical
across these refs, which hides the problem.) Fix that doc line in a future
authorized edit; not changed here.

## 4. Future commit staging plan (NOT executed — no authorization)

Do **not** run `git add -A` / `git add .`. Stage by explicit path only:

```bash
cd '/home/yuzi/Documents/Sybian REAL'
git add qemu-port/variant/e32load.cpp \
        qemu-port/user/svcmgr.cpp \
        qemu-port/docs/SHIM_RETIREMENT_RESULT.md \
        qemu-port/docs/RETIREMENT_COMMIT_REVIEW.md
git status --short            # confirm staged set before committing
git diff --cached --stat      # expect 4 files: 2 code + 2 new docs
```

Optional (only if a separate docs/index commit is authorized):
`git add qemu-port/docs/DOCS_INDEX.md` to index the three backfilled reports and
this review. Exclude / never stage: `symbian-x86-archive-56fbaff.tar.gz`,
`qemu-port/.sybian/coordinator/state.json`,
`qemu-port/.sybian/qemu-slot.lock`, and the 0-byte stray
`qemu-port/-D__cdecl=__attribute__((cdecl))` (delete the stray in a cleanup
commit instead). `kernelhwsrv/` is read-only and clean — never add it.

Suggested commit message (retirement-only scope):

```
Retire PORTFILESRV disk fallback; keep genuine EFSRV boot chain

Comment out the SvcMgrBootLaunch() fallback in GenuineBootLaunch and the
\PORTFILESRV.E32 service row in svcmgr. Genuine boot list unchanged.
Gates: retire 4/4, fullchain 14/14 (fallback unused, 0 faults).
Final verify 10/11; sole failure cp4_stress is pre-existing (A/B confirmed).
```

## 5. Gate summary (from SHIM_RETIREMENT_RESULT.md, re-verified here as facts)

- `r1_genuine_retire_test.py`: PASS 4/4.
- `r1_genuine_fullchain_test.py`: PASS 14/14, no fallback used.
- `r1_final_verify.py --strict-skip`: 10 PASS / 1 FAIL (`selfcheck` →
  `cp4_stress`: `done ids [1,2,3] != survivors [1,3,4]`). A/B on a stashed
  pristine baseline reproduced the identical failure — pre-existing, not a
  retirement regression. No checker/threshold was edited.

No commit, push, `make clean`, QEMU run, or `kernelhwsrv/` change was made by
this review.

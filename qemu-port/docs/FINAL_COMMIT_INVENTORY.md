# FINAL COMMIT INVENTORY — pre-commit staging plan (read-only audit)

Nature: **read-only audit**. No QEMU, no build, no gate re-run, no git index change,
no commit/push. `kernelhwsrv/` is READ-ONLY and untouched. This document is the
**only** file written by this audit. A full PASS does **not** itself authorise a
commit or push — those require an explicit request from the user.

## 1. Repository state at audit time

- `git diff --check` → **clean** (no whitespace errors).
- `git status --short -- kernelhwsrv/` → **empty** (kernelhwsrv/ clean; confirms READ-ONLY boundary held).
- Root `.gitignore` (only ignore file present) excludes build outputs
  (`build/`, `*.o`, `*.elf`, `*.E32`, `*.e32`, `*.iso`, `*.img`, `*.bin`, `*.vdi`,
  `*.log`, `*.zip`, `__pycache__/`, `*.pyc`, `/kernelhwsrv/`). Note `*.tar.gz`
  is **not** ignored — hence the preservation tarball is untracked and must
  never be swept in with `git add -A`.

## 2. File counts / categorization

| Category | Count | Action |
|---|---|---|
| Modified tracked — root | 1 (`README.md`) | STAGE |
| Modified tracked — `qemu-port/docs/` | 1 (`DOCS_INDEX.md`) | STAGE |
| Modified tracked — `qemu-port/fs/` | 2 (`r1_fs_backend.h`, `r1_fs_server.cpp`) | STAGE |
| Modified tracked — `qemu-port/variant/` | 4 (`e32load.cpp`, `qemu_display.cpp`, `qemu_variant.h`, `qemu_variant_asic.cpp`) | STAGE |
| Modified tracked — `qemu-port/tools/` | 2 (`e32read.py`, `r1_doom_multikey_test.py`) | STAGE |
| Modified tracked — runtime state | 1 (`.sybian/coordinator/state.json`) | **DO NOT STAGE** |
| Untracked — root `AGENTS.md` | 1 | STAGE |
| Untracked — `qemu-port/docs/*.md` | 50 (+ this report = 51) | STAGE |
| Untracked — `qemu-port/tools/r1_*.py`, `genuine_build.py` | 13 (+ 2 modified above = 15 total tools) | STAGE |
| Untracked — `qemu-port/genuine/` (non-ignored source + build scripts only) | 110 | STAGE |
| Untracked — preservation tarball | 1 (`symbian-x86-archive-56fbaff.tar.gz`) | **DO NOT STAGE** |
| Untracked — slot lock | 1 (`.sybian/qemu-slot.lock`) | **DO NOT STAGE** |
| Untracked — stray artifact | 1 (`qemu-port/-D__cdecl=__attribute__((cdecl))`, 0 bytes) | **DO NOT STAGE** |

**Total stageable after this report lands: 185 files**
(2 root + 52 docs + 15 tools + 110 genuine + 4 variant + 2 fs).
`git add -n` dry-run over the planned groups confirmed 184 before this report,
and confirmed 110 non-ignored files under `qemu-port/genuine/`.

`qemu-port/genuine/` contents staged are source and build inputs only
(`.cpp/.h/.S/.s/.py/.ld/.def/.def.maps/.sh/.md/.json/.A/.hcf/.hda`). Gitignored
build artifacts under it (`build/`, `obj/`, `obj_build/`, `*.o`, `*.elf`,
`*.E32`, `__pycache__/`, `*.pyc`) are skipped automatically by `git add`.

## 3. Secrets / sensitive-data scan

- Content grep for `sk-`, `mtk_`, `Bearer <token>`, `api_key=`, literal
  `password="..."`, and `BEGIN ... PRIVATE KEY` across **all 184 stageable
  files** → **zero matches**. No secret values to redact.
- Consistent with `qemu-port/docs/SECURITY_AUDIT.md` verdict **SAFE**: no
  hardcoded credentials, no `shell=True`/`eval`/`exec` of untrusted input in
  scope; all subprocess calls use list form.
- Documented false positives (no action): `secret=RGBY` game-test fixture in
  pre-existing `r1_codebreak_test.py` (out of scope); "password notifier" UI
  strings in genuine EPL-sourced `ws_main.cpp`; EPL license URLs `"password"`-
  adjacent text; local `RETIRE-*`/`ROLLBACK-*` "token" confirmation strings in
  `r1_shim_retire_apply.py`; `AF_UNIX` local monitor sockets only, no egress.

## 4. Exact staging commands (never `git add -A`)

```bash
cd "/home/yuzi/Documents/Sybian REAL"

git add AGENTS.md README.md
git add qemu-port/docs/
git add qemu-port/variant/ qemu-port/fs/
git add qemu-port/tools/
git add qemu-port/genuine/

# verify before committing
git diff --cached --stat
git diff --cached --check
git status --short      # do-not-stage items must still be unstaged/untracked
```

Each path is explicit; none of these globs touch the do-not-stage items.
Note `git add qemu-port/docs/` will include this inventory file once written.

## 5. Do-not-stage list

- `qemu-port/.sybian/coordinator/state.json` — tracked runtime coordinator state; keep working-tree-only.
- `qemu-port/.sybian/qemu-slot.lock` — untracked runtime lock.
- `symbian-x86-archive-56fbaff.tar.gz` — untracked preservation tarball; `git add -A` is forbidden precisely because of this.
- `qemu-port/-D__cdecl=__attribute__((cdecl))` — 0-byte stray compiler-flag artifact.
- `kernelhwsrv/` — READ-ONLY, currently clean; never add.
- All gitignored build artifacts (ISOs/ELFs/E32s/objects/logs) — reproducible from source via `make`; auto-skipped by the explicit `git add` calls above.

## 6. Suggested commit message

```
Genuine-userspace pivot: real EKA2 boot chain + verification gates

Land the genuine Symbian userspace pivot on x86/PC: EUSER/EFSRV/EWSRV/ESTART/
ESHELL built from real Symbian source under qemu-port/genuine/, the GRUB ISO
deliverable, and the full verification / shim-retirement tooling.

Verification (2026-11-24):
- r1_final_verify.py --strict-skip: OVERALL PASS, 11/11, 0 skip, 0 fail (exit 0)
- fullchain: PASS 14/14, all four genuine components r=0, no svcmgr fallback
- DOOM/Freedoom main/audio/playtest PASS
- security audit SAFE (no credentials/injection); preservation audit PASS

kernelhwsrv/ untouched (READ-ONLY). Build artifacts gitignored/reproducible
via make. No shims retired; no commit/push automation.
```

## 7. Caveats

- **No commit and no push without an explicit user request.** This document is a
  staging plan only.
- **Never `git add -A`** — it would sweep the tarball, `state.json`, the lock
  file and the stray artifact.
- Gate evidence is from `FINAL_VERIFICATION_RESULT.md` (2026-11-24, 11/11 strict
  PASS), `FULLCHAIN_VERIFICATION.md` (14/14) and `DOOM_INTEGRATION_VERIFICATION.md`
  (PASS). Older status pages (`FINAL_GATE_STATUS.md`, `FINAL_PROJECT_SUMMARY.md`,
  `GO_NO_GO.md`) still record earlier FAIL / NOT-DONE / CONDITIONAL-GO verdicts
  on prior trees and are superseded by the 2026-11-24 strict run; reconcile them
  before treating the docs set as internally consistent.
- A PASS does **not** authorise shim retirement. Retirement stays gated on
  `r1_genuine_retire_test.py` + the guard apply tool per `AGENTS.md`.

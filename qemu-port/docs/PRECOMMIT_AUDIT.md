# PRECOMMIT AUDIT — genuine-userspace pivot (preparation only)

**Date:** 2026-11-24. **Scope:** read-only audit + this checklist. No QEMU, no rebuild,
no commit, no push, no shim retirement. `kernelhwsrv/` is READ-ONLY and untouched.

> **STATUS: NO-GO.** EFSRV/F32 retire gate = **FAIL** — the EFILE entry-trampoline
> (A2) fix is PENDING (source only, unbuilt/unproven); duplicate EFILE owners still
> kill the `DServer` (`SessionPath r=-15`). This document is **prep, not authorization**.
> Do nothing below until the gate reports PASS and the user explicitly asks.

## 1. Ordered commands (run in this order; all from `qemu-port/`)

1. `python3 tools/r1_final_verify.py --dry-run` — plan only, no verdict.
2. `python3 tools/r1_final_verify.py --strict-skip` — authoritative 11-test gate.
3. `python3 tools/r1_efile_acceptance.py --dry-run` — 9-criterion plan, no QEMU.
4. `python3 tools/r1_efile_acceptance.py` — real run; requires `RESULT: PASS (9/9 …)`, exit 0.
5. `git status --short` — confirm tree matches CHANGE_INVENTORY.md; confirm no `kernelhwsrv/` entries.
6. `git diff --check` — clean (no whitespace errors).
7. Secret scan (read-only) — e.g. `git diff | grep -nEi '(api[_-]?key|token|secret|password[[:space:]]*=|BEGIN [A-Z ]*PRIVATE KEY|AKIA[0-9A-Z]{16})'`; expect no hits, only known keyword-only false positives (genuine Symbian thunk/stub names).
8. **Stage BY PATH explicitly** — `git add AGENTS.md README.md qemu-port/variant/ qemu-port/fs/ qemu-port/tools/ qemu-port/docs/ ...` per CHANGE_INVENTORY.md §4. **NEVER `git add -A`** — the tarball `symbian-x86-archive-56fbaff.tar.gz` is untracked and NOT gitignored; a blanket add would stage it.
9. Commit message template (fill brackets, do not commit until user asks):
   ```
   Genuine-userspace pivot: real EUSER/EFSRV/EWSRV/ESTART + DOOM on x86

   Port shim only: PE2 imports 512, ProtServ grant, supervisor boot DThread,
   lazy VBE, virtual FS Backend, genuine component build/toolchain.

   Verified: fullchain 14/14, DOOM PASS, EFSRV gate PASS, EFILE 9/9.
   Shims retired only via guarded tool with gate PASS (if any).
   Refs: CHANGE_INVENTORY.md, GO_NO_GO.md, EFILE_CONVERGENCE.md.
   ```
10. **Push only on explicit user request** (never automatic).

## 2. Must be true before commit

- [ ] EFSRV/F32 retire gate `[PASS]` and `RESULT: PASS` (`r1_genuine_retire_test.py`, full run).
- [ ] Other retire gates PASS: EWSRV, ESTART, EUSER (and HAL/DM 15/15, non-gate).
- [ ] Fullchain boot **14/14** rows, all four genuine components `r=0`, 0 faults (`FULLCHAIN_VERIFICATION.md`).
- [ ] DOOM main/audio/playtest PASS, 0 faults, `EKA2_CRISP_STOP` absent (`DOOM_INTEGRATION_VERIFICATION.md`).
- [ ] EFILE acceptance **9/9** (`r1_efile_acceptance.py` real run, exit 0).
- [ ] Fresh ISO rebuilt and all gates re-run against it (stale-ISO false-PASS path in FINAL_VERIFY_REVIEW.md §3).
- [ ] `kernelhwsrv/` clean and untouched; no `make clean`.
- [ ] No shim retired except via `r1_shim_retire_apply.py` with the gate PASS (preview → `--shim <name> --apply --confirm <TOKEN>`); never invoked while FAIL/SKIP.
- [ ] `git diff --check` clean; secret scan clean; tree matches CHANGE_INVENTORY.md.

## 3. DO NOT COMMIT

- `qemu-port/.sybian/coordinator/state.json` (modified timestamp) and `qemu-port/.sybian/qemu-slot.lock`.
- `symbian-x86-archive-56fbaff.tar.gz` (untracked, not gitignored — the `git add -A` trap).
- Build artifacts: `qemu-port/build/`, `qemu-port/genuine/build/`, `obj*/`, `*.o`, `*.elf`, `*.E32`, `*.FSY`, `*.pyc`, `__pycache__/` (gitignored; 278 files under `genuine/`).
- `/tmp/opencode` logs, screenshots/screendumps, ISOs, disk images and other scratch output.
- Anything under `kernelhwsrv/` (read-only).

## 4. Current verdict

NO-GO. EFSRV gate FAIL pending the A2 entry-trampoline fix (unbuilt/unverified);
per GO_NO_GO.md §2 no shim may be retired and no commit/push may occur. This
checklist is preparation only — passing commands here do not grant authorization.

## 5. Tree-state snapshot (post-commit, 2026-11-24)

This is a **post-commit snapshot** dated 2026-11-24 — no new commit/push is
authorized by this document. The pivot and shim-retirement work is committed
and pushed:

- `HEAD` = `origin/main` = `d26fade5db13a140845a30e0faabe4550fb3940d`
  (chain `d26fade` → `7f76900` → `56fbaff`); both commits (`7f76900`
  genuine-userspace pivot, `d26fade` shim retirement) were user-authorized
  and pushed to `origin/main`.
- Remaining working-tree leftovers, all intentionally unstaged:
  `M qemu-port/.sybian/coordinator/state.json`, `M qemu-port/docs/DOCS_INDEX.md`,
  `M AGENTS.md`, `?? qemu-port/-D__cdecl=__attribute__((cdecl))` (stray),
  `?? qemu-port/.sybian/qemu-slot.lock`, `?? symbian-x86-archive-56fbaff.tar.gz`
  (archive tarball — still NOT gitignored, never `git add -A`).
- `kernelhwsrv/` remains READ-ONLY and untouched. The checklist, ordered
  commands, and do-not-commit list above remain valid.

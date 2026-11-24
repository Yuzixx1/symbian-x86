# POST_REMOVAL_VERIFY — portfilesrv removal impact check

Date: 2026-11-24 ~18:49 EET
Scope: VERIFY ONLY. No source/build edits made by this check. `kernelhwsrv/` untouched
(git status shows no changes under `kernelhwsrv/`).

## Trigger

A smoke run transiently FAILED at 18:47:31 exactly while portfilesrv files were being
deleted (`user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `:portfilesrv` removed;
`user/user.mk` edited 18:47:06; `build/user/PORTFILESRV.E32` gone). Goal: confirm whether
the removal broke anything after the tree settled.

## 1. Tree state at check time

- `.sybian/qemu-slot.lock`: present but EMPTY (0 bytes, mtime 2026-11-23 22:15) — no active holder.
- `user/user.mk` mtime 2026-11-24 18:47:06, stable across a 5 s re-stat — no edit in progress.
- `user/` dir mtime 18:47:09, stable. Deletion settled; no race.
- `user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `:portfilesrv`, `build/user/PORTFILESRV.E32`:
  all absent (git status: `D`).
- `user/user.mk` references to "portfilesrv" are COMMENT-ONLY (lines 56, 66, 262, 426, 619,
  689, 718, 907, 1101); no non-comment rule/target references remain.

## 2. Build evidence

Command: `make userland svcmgr` (no `make clean`)
Log: `/tmp/opencode/post_removal_build.log`
Result: EXIT=0 — `make: Nothing to be done for 'userland'.` / `Nothing to be done for 'svcmgr'.`
Makefile parses cleanly post-edit; all targets up to date.

- `build/user/PORTFILESRV.E32`: NOT present — removal effective, not regenerated.
- `build/user/PORTFILESRV.EXE`: still present but STALE (mtime 2026-11-21 12:56, pre-removal
  leftover; not rebuilt by this `make`).
- `build/user/SVCMGR.E32` + `SVCMGR.EXE`: present, SVCMGR.E32 rebuilt 2026-11-24 18:47:19
  (post-removal). All other user E32s (DOOM.E32, EWSRV-adjacent apps, etc.) present.

## 3. Per-test verdicts (run serially, all after tree settled)

| Test | Result | Log |
|---|---|---|
| `python3 tools/r1_qemu_smoke.py` | **PASS** (all 10 markers PASS, exit 0) | `/tmp/opencode/post_removal_smoke.log` (serial: `/tmp/opencode/r1_qemu_smoke.log`) |
| `python3 tools/r1_genuine_retire_test.py` | **PASS 4/4** (efsrv, ewsrv, estart, euser; exit 0) | `/tmp/opencode/post_removal_retire.log` |
| `python3 tools/r1_genuine_fullchain_test.py --log-dir /home/yuzi/.cache/sybian-r1` | **PASS 14/14** (exit 0), incl. row `fallback: svcmgr/portfilesrv not used — PASS` and `faults: no Exc/PanicKernExec/FAULT (0 lines) — PASS` | `/tmp/opencode/post_removal_fullchain.log`; per-run logs under `/home/yuzi/.cache/sybian-r1/` (e.g. `r1_fullchain.log`, `r1_fullchain.png`) |

No fallback markers in any run; genuine boot chain (EUSER → EFILE → ESTART → EWSRV →
ESHELL) completes with the genuine file server up and no portfilesrv involvement.

## 4. Verdict

**Removal SAFE.** The portfilesrv deletion (source, build script, `:portfilesrv` entry,
user.mk rule) broke nothing: build succeeds without regenerating PORTFILESRV.E32, and
smoke / retire (4/4) / full-chain (14/14) all PASS post-removal. The 18:47:31 smoke failure
coincided with the in-progress deletion (transient tree state), not with any persistent
damage.

## 5. Caveats

- `build/user/PORTFILESRV.EXE` is a stale pre-removal artifact (2026-11-21); harmless, not
  rebuilt. Deleting it is a cleanup choice, not required for correctness.
- Smoke/retire scripts stage a cached `:portfilesrv` fixture from
  `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/` into the test FAT image — a test
  fixture in cache, NOT a build product; source is gone from the tree.
- `user/user.mk` still contains comment-only mentions of portfilesrv; cosmetic.
- Tests were run serially after mtimes confirmed settled; concurrent-agent edits during
  the runs were not observed (lock file empty throughout).

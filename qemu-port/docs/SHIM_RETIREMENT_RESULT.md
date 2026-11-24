# SHIM_RETIREMENT_RESULT — guarded apply (2026-11-24)

User-authorized guarded retirement after commit `7f76900` (pushed to `origin/main`).
Scope per goal: **file-server shims `portfilesrv` + `r1_fs` only**. `kernelhwsrv/`
READ-ONLY and verified clean throughout; no delete / commit / push / `make clean`.

## 1. Preflight (all satisfied)

- Edit targets clean at 7f76900: `git status --porcelain -- qemu-port/variant/e32load.cpp
  qemu-port/user/svcmgr.cpp qemu-port/variant/qemu_variant_asic.cpp` → empty.
- Default preview: all 4 BLOCKED/SKIP by design. `--from-file
  /tmp/opencode/final_verify_genuine_retire.log` (real full run, `dry-run=False`):
  portfilesrv/r1fs READY (EFSRV/F32 PASS), winsrv READY (EWSRV PASS), svcmgr READY
  (ESTART/ESHELL PASS). Per-shim preview (`--shim portfilesrv|r1fs|winsrv|svcmgr`)
  matched; no `--apply` in any dry-run.
- Help tokens verified: `RETIRE-PORTFILESRV`, `RETIRE-R1FS`, `RETIRE-WINSRV`,
  `RETIRE-SVCMGR`.

## 2. Per-shim outcome

| Shim | Outcome | Command / result |
|---|---|---|
| **portfilesrv** | **APPLIED** | `python3 tools/r1_shim_retire_apply.py --shim portfilesrv --apply --confirm RETIRE-PORTFILESRV` → exit 0; pre-gate EFSRV PASS, post-gate EFSRV PASS |
| **r1_fs_\*** | **RETIRED via shared edits** | Identical edit set to portfilesrv. Standalone `--shim r1fs --apply --confirm RETIRE-R1FS` → exit 2 REFUSE (targets already modified / edits already applied). No second edit needed or possible. |
| **svcmgr** | **APPLIED then ROLLED BACK (out of scope)** | Applied exit 0 (pre/post ESTART gate PASS); rolled back because the goal names only the file-server shims. Snapshot restored via tool (exit 0). |
| **winsrv** | NOT APPLIED | No apply path by design (file removal + human review); genuine EWSRV entries kept. |

## 3. Edits actually in the tree (2 lines)

- `qemu-port/variant/e32load.cpp:1552` — `SvcMgrBootLaunch();` fallback commented out
  (`// RETIRED by r1_shim_retire_apply: ...`). Reached only when no genuine file
  server is present; fullchain confirms it is not used.
- `qemu-port/user/svcmgr.cpp:250` — `\PORTFILESRV.E32` service row commented out.

Genuine boot list (`\EFILE`/`\EFSRV`/`\ESTART`/`\E32STRT`/`\EWSRV`/`\ESHELL`) and
`GenuineBootSpawn()`/`ShellSpawnFull()` untouched. `qemu_variant_asic.cpp` fully
restored by the svcmgr rollback (`git diff` empty for that file).

## 4. Snapshots and gate logs

- `/tmp/opencode/shim_retire_backup_20261124_083011` — portfilesrv, 2 files,
  sha256 manifest, token `ROLLBACK-PORTFILESRV-20261124_083011`.
- `/tmp/opencode/shim_retire_backup_20261124_083050` — svcmgr, 1 file, token
  `ROLLBACK-SVCMGR-20261124_083050` (consumed by the rollback).
- Apply gate logs: `/tmp/opencode/shim_retire_gate_{efsrv,estart}_{pre,post}`.

## 5. Verification after final state

- `python3 tools/r1_genuine_retire_test.py --log-dir /tmp/opencode` → **PASS 4/4**
  (efsrv, ewsrv, estart, euser), exit 0.
- `python3 tools/r1_genuine_fullchain_test.py --log-dir /tmp/opencode` → **PASS
  14/14**, exit 0; includes `fallback svcmgr/portfilesrv not used PASS`,
  `genuine file server up (no fallback) PASS`, 0 faults.
- `python3 tools/r1_final_verify.py --strict-skip` → **10 PASS / 1 FAIL** (exit 1).
  All genuine + EFSRV/EWSRV/ESHELL/FAT-IO + DOOM (main/audio/playtest/multikey)
  gates PASS. Sole failure: `selfcheck` → step `cp4_stress`
  (`done ids [1,2,3] != survivors [1,3,4]`).
- `cp4_stress` rebuilds kernfull itself (PASS); `make build/kernfull.elf userland`
  and `make isogrub` in selfcheck both PASS — ISO/ELF are current. No extra
  rebuild requested by the tool.

### cp4_stress failure is PRE-EXISTING, not a retirement regression

A/B on the same tree: `git stash push -- qemu-port/user/svcmgr.cpp
qemu-port/variant/e32load.cpp` (pristine 7f76900 baseline) → `cp4_stress` fails
with the identical `done ids [1,2,3] != survivors [1,3,4]`; `git stash pop`
restored the retirement edits. Repeated runs with and without the edits are
identical. Rolling back the retirement would not fix it, so the authorized
retirement was kept. This pre-existing failure needs a separate owner/test fix;
no checker, threshold or marker was edited here.

## 6. Rollback recipe (if ever required)

```bash
cd qemu-port
# Preferred: tool-managed rollback with token from the manifest
python3 tools/r1_shim_retire_apply.py --rollback \
  /tmp/opencode/shim_retire_backup_20261124_083011 \
  --confirm ROLLBACK-PORTFILESRV-20261124_083011
# Manual fallback (git, no tool): restore exact HEAD (= 7f76900) originals.
# The snapshot above is sha256-verified to equal HEAD; qemu_variant_asic.cpp
# is already byte-identical to HEAD and needs no restore.
git -C '/home/yuzi/Documents/Sybian REAL' checkout HEAD -- \
  qemu-port/variant/e32load.cpp qemu-port/user/svcmgr.cpp
# To inspect an original without checking out:
#   git -C '/home/yuzi/Documents/Sybian REAL' show 7f76900:qemu-port/variant/e32load.cpp
cd qemu-port && make isogrub && python3 tools/r1_os_selfcheck.py
git -C '/home/yuzi/Documents/Sybian REAL' status --short -- kernelhwsrv/  # must be empty
```

**Do NOT use `archive/handwritten-r25` here.** That ref is ambiguous (branch at
`56fbaff`, tag at `4f08597`) and both versions of `e32load.cpp` hash to
`d094d372…`, which is **not** the pre-retirement HEAD original `356e7bdd…`;
restoring from it would put an old handwritten-era file back instead of undoing
this retirement. Use the sha256-verified snapshot or `HEAD` (7f76900) instead.
(`svcmgr.cpp` happens to be identical across those refs, which hides the bug.)

`kernelhwsrv/` was never touched. No commit or push was made.

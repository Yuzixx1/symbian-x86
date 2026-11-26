# CHANGE INVENTORY — uncommitted genuine-userspace pivot

**Date:** 2026-11-24. **Scope:** read-only inventory for a future commit (only when the user asks).
No files were modified other than this report. No commits/pushes. `kernelhwsrv/` is clean and untouched;
`make clean` was not run; no QEMU tests were run for this report.

## 1. Working-tree shape (from repo root)

- `git status --short`: **10 modified tracked files + 21 untracked entries** (one is the collapsed
  `qemu-port/genuine/` directory). `git diff --stat`: 10 files, **+434 / −47** lines.
- `qemu-port/genuine/` holds **387 files total: 109 non-ignored source/config/docs** (would be
  committed) and **278 gitignored artifacts** (`build/`, `obj*/`, `*.o`, `*.elf`, `*.E32`,
  `__pycache__/`, `*.pyc`). `qemu-port/build/` (ISO, ELF, scratch) is gitignored via `.gitignore:2`.
- `symbian-x86-archive-56fbaff.tar.gz` is **untracked and NOT gitignored** — a plain `git add -A`
  would stage it. Must be excluded explicitly (see §5).

## 2. Grouped inventory

| Paths | One-line purpose | Verification (doc / test) | Commit? | In-flight? |
|---|---|---|---|---|
| `AGENTS.md` (root, new) | Project governance: genuine-userspace rule, `kernelhwsrv/` read-only boundary, boot order, verification gates, preservation points. | `GENUINE_PROGRESS.md`, `GENUINE_USERSPACE.md` | YES | No |
| `qemu-port/variant/e32load.cpp` | PE2 imports 16→512; `ECapabilityProtServ` grant in `E32Run`; user-mode LFB map; exec-81 `FsRegisterThread` teardown wrapper; genuine boot list with `svcmgr` fallback. | `r1_genuine_retire_test.py`, `r1_qemu_smoke.py`, `r1_os_selfcheck.py`; `BOOT_CHAIN_VERIFICATION.md` | YES | No — exec-81 teardown fix **LANDED** (0 FAULT) |
| `qemu-port/variant/qemu_variant.h` | `TE32Info::iSlots[512]` / import-count comment for genuine DLLs. | `r1_e32_validate_test.py`, `tools/e32read.py`, retire suite | YES | No |
| `qemu-port/variant/qemu_variant_asic.cpp` | Spawn genuine boot chain on a supervisor DThread from `Asic::Init3` instead of `SvcMgrBootLaunch()` (fixes `KERN 0x62`); shim-only disks still boot as before. | `r1_qemu_smoke.py` (11/11 PASS), `GENUINE_PROGRESS.md` B4/B7 | YES | No — **JUST LANDED** |
| `qemu-port/variant/qemu_display.cpp` | Lazily enable Bochs VBE LFB when genuine EWSRV asks for screen info (boot stays VGA text). | `r1_doom_test.py`, `r1_genuine_ewsrv_test.py`; `DOOM_INTEGRATION_VERIFICATION.md` | YES | No |
| `qemu-port/fs/r1_fs_backend.h` | Make `Backend` methods virtual (virtual dtor + overridable FAT adapter) for genuine `efsrv_min` backend injection. | `r1_genuine_efsrv_test.py`, `r1_genuine_fat_io_test.py` | YES | No — B3 virtual `Backend` + on-disk FAT I/O **LANDED** |
| `qemu-port/fs/r1_fs_server.cpp` | Accept genuine const descriptors (`GetDesMaxLength == -38`), widen 8-bit names, restore descriptor length; root-causes EFSRV IPC blocker. | `r1_genuine_efsrv_test.py` (PASS), `r1_genuine_fat_io_test.py` (PASS) | YES | No — root cause fixed |
| `qemu-port/tools/e32read.py` | PE2 import-count sanity 16 → 512 for genuine EWSRV/EFILE ordinals. | `tools/e32read.py <image>.E32`; `ABI_AUDIT.md` §8 | YES | No |
| `qemu-port/tools/r1_doom_multikey_test.py` | Lag-immune full-log marker parse, CRLF/LF twin dedup and action-based attribution for multi-key holds. | `r1_doom_multikey_test.py`; `DOOM_INTEGRATION_VERIFICATION.md` §5 | YES | **YES** — `diagonal_2` false FAIL is a harness attribution artifact (DOOM verification PASS report written); follow-up pending |
| `qemu-port/docs/DOCS_INDEX.md` | Append-only index entries for all new genuine-pivot reports (105 indexed Markdown docs). | `r1_os_selfcheck.py` docs check (n/a) | YES | No |
| `qemu-port/.sybian/coordinator/state.json` | Coordinator `updated_at` timestamp bump only — transient tooling state. | n/a | **NO** | No |
| `qemu-port/.sybian/qemu-slot.lock` | Empty QEMU slot lock file from test orchestration. | n/a | **NO** | No |
| `qemu-port/docs/` — 10 new files: `ABI_AUDIT.md`, `BOOT_CHAIN_VERIFICATION.md`, `DOOM_INTEGRATION_VERIFICATION.md`, `EFILE_EXIT111_ANALYSIS.md`, `EFILE_START_ANALYSIS.md`, `EWSRV_CLIENT_BLOCKER.md`, `GENUINE_BUILD_REVIEW.md`, `GENUINE_PROGRESS.md`, `GENUINE_USERSPACE.md`, `SHIM_RETIREMENT_PLAN.md` | Architecture, live status, boot-chain/DOOM verification, EFILE root-cause analysis, ABI audit, shim-retirement plan and build review. | `GENUINE_PROGRESS.md` §1–§5; `ABI_AUDIT.md` §8; `DOOM_INTEGRATION_VERIFICATION.md` §6 | YES | Updated: KErrBadName FIXED, exit-111 root cause documented (glue fix in flight), EWSRV client window fix LANDED, eshell PS page fault IN FLIGHT; DOOM PASS report + ABI audit consolidated |
| `qemu-port/tools/genuine_build.py` | Main genuine-component build driver (`-freg-struct-return` + `-fshort-wchar`). | `r1_os_selfcheck.py`, `genuine/BUILD_MANIFEST.json`, `GENUINE_BUILD_REVIEW.md` | YES | No — `-fshort-wchar` **LANDED**; KErrBadName (-28) FIXED (UTF-16 literals, 464 binds, 0 faults) |
| `qemu-port/tools/r1_genuine_retire_test.py` | Per-subsystem retire gate runner (efsrv FAIL exit-111-only / ewsrv, estart, euser PASS). | `GENUINE_PROGRESS.md` §1 verdicts | YES | Gate results LANDED; efsrv markers updated — **FAIL only due to exit 111** |
| `qemu-port/tools/r1_genuine_efsrv_test.py`, `r1_genuine_fat_io_test.py` | Genuine EFSRV IPC round trip and on-disk FAT32 file-I/O proof (`FATIOCLIENT.E32`). | `GENUINE_PROGRESS.md` §2 (B5 DONE) | YES | No — PASS; B3 on-disk FAT I/O **LANDED** |
| `qemu-port/tools/r1_genuine_ewsrv_test.py` | Boot-time EWSRV marker detection, 212 binds, no duplicate launch. | `GENUINE_PROGRESS.md` §2 | YES | No — external client window fix **LANDED** |
| `qemu-port/tools/r1_genuine_eshell_test.py`, `tools/r1_shim_retire_plan.py` | Genuine eshell command-path exercise (TIME/CHKDSK/DRVINFO/RUNEXEC/…); dry-run shim inventory (0 READY / 4 BLOCKED). | `ABI_AUDIT.md` §6; `SHIM_RETIREMENT_PLAN.md` | YES | **YES** — eshell PS page fault fix IN FLIGHT |
| `qemu-port/genuine/` — toolchain/common: `build_efile.py`, `f32_efsrv_build.py`, `pack_multi.py`, `patch_efile_name.py`, `gdb_*.py`, `link_*.ld`, `BUILD_MANIFEST.json`, `README.md`, `*.A` archives (3), `prove_*.py` (5), `*.md` reports (3) | Per-component build/link/probe drivers, genuine static archives, link maps and workstream reports. | `genuine/BUILD_MANIFEST.json`, `GENUINE_BUILD_REVIEW.md`, `r1_os_selfcheck.py` | YES | No — EFILE `-fshort-wchar` flag change **LANDED** in build scripts (with `-freg-struct-return` in `build_fsy_dll.py`) |
| `qemu-port/genuine/hal/` (9 files) | Genuine HAL DLL (`hal_halu.def`, generated config/values, glue, QEMU HCF/HDA). | `genuine/prove_hal_dm.py` PASS (`ECPU=2`, 4096 page, GetAll=121) | YES | No — smoke PASS, not a retire gate |
| `qemu-port/genuine/dm/` (14 files) | Genuine domain manager: `dm_domainsrv`, `dm_domaincli`, `dm_domainpolicy` + def/stub generators. | `genuine/prove_hal_dm.py` PASS (`CheckValues r=0`, bad spec `-260`) | YES | No |
| `qemu-port/genuine/ewsrv/` (14 files: build scripts, glue/client/shims, 5 `patched/` sources, defs/stubs) | Genuine window server from `kernel/eka/ewsrv` with 4 patched copies; `-freg-struct-return` rebuild. | `r1_genuine_ewsrv_test.py` PASS, `genuine/prove_ewsrv_bind.py`, `ABI_AUDIT.md` §3 | YES | No — `!E32WindowServer` external client window fix **LANDED**; pointer-event path UNTESTED |
| `qemu-port/genuine/estart_eshell/` (22 files) | Genuine `estart_`/`etshell_`/dbg images, sret-ABI rebuild, shell command-path test driver. | `r1_genuine_eshell_test.py` PASS, `genuine/estart_eshell/prove_eshell.py` | YES | **YES** — eshell PS page fault fix IN FLIGHT (`ABI_AUDIT.md` §6) |
| `qemu-port/genuine/efsrv_min/` (4), `f32_probe/` (4), `efat32/` (11), `efile/` (5) | Genuine F32/EFSRV/EFAT32/EFILE minimal servers, probes, FAT DLL glue and patched `sf_main.cpp`. | `r1_genuine_efsrv_test.py`, `r1_genuine_fat_io_test.py`, `genuine/prove_efile_genuine.py`, `genuine/prove_efile_qemu.py` | YES | **YES** — KErrBadName (-28) FIXED via UTF-16 literals; exit-111 root cause documented, glue fix (heap vtable + IPC round trip) IN FLIGHT; retire gate efsrv FAIL (exit 111 only) |
| `qemu-port/genuine/hello/` (3), `genuine/F32_EFSRV_REPORT.md`, `HAL_DM_NOTES.md`, `COORD_EWSRV.md` | Export probes and per-workstream notes/reports. | `GENUINE_PROGRESS.md` §5 | YES | No |
| `symbian-x86-archive-56fbaff.tar.gz` | Preservation tarball (git source at `56fbaff`) — keep on disk, not in history. | Branch/tag `archive/handwritten-r25` | **NO** | No |

## 3. In-flight / just-landed summary

- **LANDED (2026-11-24):**
  - `-fshort-wchar` rebuild in `tools/genuine_build.py`, `genuine/build_efile.py`,
    `genuine/f32_efsrv_build.py`, `genuine/efat32/build_fsy_dll.py` (the latter also
    `-freg-struct-return`). `EFILE.E32` **273 032 B / 463 imports**, UTF-16 literals, QEMU **464 binds**,
    File server thread registered=1, port-shim=0, E32Main=0, **0 faults** — **KErrBadName (-28) FIXED**.
  - EWSRV external client window fix (`EWSRV_CLIENT_BLOCKER.md`).
  - B3 virtual `Backend` (`r1_fs_backend.h`) + on-disk FAT I/O.
  - EFSRV retire gate markers updated — efsrv now **FAIL only due to exit 111** (no other blockers).
  - Exit-111 root cause documented (`EFILE_EXIT111_ANALYSIS.md`); DOOM verification PASS report written
    (`DOOM_INTEGRATION_VERIFICATION.md`); ABI audit consolidated (`ABI_AUDIT.md`).
- **STILL IN FLIGHT:**
  1. EFILE heap vtable fix + IPC round trip — the remaining glue fix behind exit 111.
  2. DOOM multi-key harness `diagonal_2` false FAIL (`r1_doom_multikey_test.py` §5 — raw evidence says
     all releases processed; **harness attribution artifact, not a game bug**).
  3. eshell PS page fault fix (`ABI_AUDIT.md` §6).
- **GATES / EARLIER LANDED:** retire gate results ewsrv/estart/euser **PASS**, efsrv **FAIL**
  (exit 111 only); exec-81 teardown fix → 0 FAULT; supervisor-DThread genuine boot chain; B5 FAT I/O.
  No shim retired (0 READY / 4 BLOCKED).

### Verified today

- EFILE build ABI: 273 032 B / 463 imports, UTF-16 `_LIT`s, 464 QEMU binds, thread registered=1,
  port-shim=0, E32Main=0, 0 faults; KErrBadName resolved.
- EWSRV client window path; B3 virtual Backend + on-disk FAT I/O; EFSRV gate markers (efsrv exit-111 only).
- DOOM verification PASS report and consolidated ABI audit on disk.

## 4. Recommended commit order (when asked)

1. `AGENTS.md` + `qemu-port/docs/` (10 new) + `DOCS_INDEX.md` — governance and status docs.
2. `qemu-port/variant/` (4) + `qemu-port/fs/` (2) + `tools/e32read.py` — kernel-side port plumbing.
3. `qemu-port/tools/` (7 new test/build drivers) — verification harnesses.
4. `qemu-port/genuine/` (109 non-ignored source/config files) — genuine components and toolchain, one
   commit per component if preferred (common → HAL/DM → EWSRV → estart/etshell → F32/EFSRV/EFILE/EFAT32).
5. `docs/CHANGE_INVENTORY.md` (this file) last, after the tree matches it.
   Exclude everything in §5 explicitly; stage by path, never `git add -A` at the repo root.

## 5. DO NOT COMMIT

- `qemu-port/.sybian/coordinator/state.json` (modified; transient timestamp),
  `qemu-port/.sybian/qemu-slot.lock` (test lock).
- `symbian-x86-archive-56fbaff.tar.gz` (preservation tarball; untracked but **not** gitignored —
  never `git add -A`; stage by path only).
- Any `qemu-port/build/` or `qemu-port/genuine/build/`, `obj*/`, `*.o`, `*.elf`, `*.E32`, `*.FSY`,
  `*.pyc`/`__pycache__` (already gitignored — 278 files under `genuine/` alone).
- Transient logs, QMP screendumps/stale PNGs (e.g. `/tmp/opencode/doom_verify_*.{log,png}`), ISOs,
  disk images and other scratch outputs. None are currently in git status.
- Nothing under `kernelhwsrv/` (read-only; currently clean).

**Secret check:** no high-confidence secrets (API keys, tokens, private keys, AWS/GitHub/OpenAI
patterns) found in any changed file. Nine files produce keyword-only false positives for
"Password/Passwd" (genuine Symbian export- thunk/stub names, `.A` symbol strings and one genuine
UI text copied into a window-server descriptor); no values are reproduced here.

## 6. Preservation state

- HEAD `56fbaff` on `main`, tracking `origin/main`; branch and tag
  `archive/handwritten-r25` both at `56fbaff`; tarball `symbian-x86-archive-56fbaff.tar.gz` on disk.
- Nothing committed or pushed since the pivot; all work above is uncommitted working-tree state.

## 7. Supersession — 2026-11-26 (current scale)

**This report supersedes the §1 snapshot above, which is HISTORICAL and kept verbatim.** The
`2026-11-24` figures (`10 modified + 21 untracked`, `+434 / −47`) predate the winsrv/GUI retirement
and the genuine-userspace landings; do not quote them as current.

- Current working tree at HEAD `ee8490e`: **85 modified tracked files (16 of them deletions),
  +5577 / −1792 lines, 215 untracked entries** (`git status --short` = 300 entries; nothing staged).
  Substantially larger than the §1 `10 + 21 / +434 / −47` snapshot.
- Authoritative gate: **`FINAL27_RC=0`, 15/15 PASS, 0 skip, 0 fail/not-run**
  (`/tmp/opencode/final27_gate.log`); see `FINAL_GATE_STATUS.md` §EOF and `FINAL_VERIFY_REVIEW.md:18`.
- §3 in-flight items and §5 DO NOT COMMIT rules still stand; `kernelhwsrv/` remains read-only and
  untouched. No commits, no push, no `make clean`, no QEMU, no downloads for this entry — §1–§6 are
  unchanged history, appended to only.

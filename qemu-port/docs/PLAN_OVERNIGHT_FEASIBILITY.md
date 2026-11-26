# PLAN OVERNIGHT FEASIBILITY — review of the 'no-decision overnight work' list

**Date:** 2026-11-25. **Scope:** verification only. Read
[PLAN_DECISION_SHEET.md](PLAN_DECISION_SHEET.md),
[PLAN_EXECUTION.md](PLAN_EXECUTION.md), [PLAN_P0_COMMIT.md](PLAN_P0_COMMIT.md)
against the live tree at HEAD `ee8490e` (branch `main`). No commit, no push, no
`make clean`, no QEMU, `kernelhwsrv/` untouched (verified clean:
`git -C kernelhwsrv status --porcelain` empty). This file is the only file
written.

## Step-by-step feasibility

### P0 — hygiene file moves, .gitignore, KERNEL_HOOK_FREEZE, baseline gates
- **Staging paths exist.** Group A modified files are all present and dirty:
  `AGENTS.md`, `qemu-port/docs/CLEANUP_LOG.md`, `DOCS_INDEX.md`,
  `FINAL_GATE_STATUS.md`, `GO_NO_GO.md` (all ` M` in git status).
  `WINSRV_RETIREMENT_FINAL.md` exists on disk as untracked (Group B).
  Caveat: `.gitignore` is tracked but **not yet modified** — PLAN_P0_COMMIT
  Group A lists it as "modified, staged (6)", which presupposes the §4 rule
  append happens before staging. Not a blocker; the append is itself P0 work.
- **WATCH counts match the plan exactly:** 128 untracked
  `qemu-port/docs/WINSRV_DELETE_WATCH*.md`, 197 tracked. Tarball
  `symbian-x86-archive-56fbaff.tar.gz` present (2,481,838 B, untracked),
  `qemu-port/9` present (0 B), `qemu-port/.sybian/qemu-slot.lock` present.
  `state.json` modification correctly excluded from staging.
- **Archive target dir git-ignorability: conditional, not yet true.**
  `git check-ignore qemu-port/docs/archive/winsrv-delete-watch/x.md` returns
  rc=1 — the current tracked `.gitignore` has no `/qemu-port/docs/archive/`
  rule (nor `*.tar.gz`, `/qemu-port/9`, `qemu-slot.lock`). The dir becomes
  ignorable only **after** P0's own `.gitignore` edit lands. Ordering
  prerequisite within P0: `.gitignore` edit first, archive move second.
  Works fine as a working-tree-only change.
- **KERNEL_HOOK_FREEZE.md** does not exist yet — it is a P0 deliverable to be
  authored, as expected.
- **Baseline gates exist:** `tools/r1_final_verify.py --strict-skip` with the
  11 DEFAULT_TESTS (selfcheck … doom_multikey), `r1_genuine_retire_test.py`,
  `r1_genuine_fullchain_test.py` all present. Note: these boot QEMU.

### P1 — guarded deletion steps 1–4, 6–7 + G12
- All referenced paths exist: `fs/r1_fs_*` (see count caveat below),
  `services/r1_service_host.{cpp,h}`, `services/r1_service_state.h`,
  `variant/r1_fs_channel.{cpp,h}`, `user/ipc.c`, `tests/r1_fs_core/`,
  `tests/r1_fs_channel/` (directories, not .cpp files — plan's
  `tests/r1_fs_{core,channel}` shorthand matches), Makefile opt-in block at
  `Makefile:341-352` (`R1_FS_CHANNEL`), orphaned `user/user.mk` r1_fs rules at
  ~354-362, `tools/r1_shim_retire_plan.py` / `r1_shim_retire_apply.py` /
  `r1_winsrv_delete.py` all present.
- **No-commit guarantee is real:** both guarded executors hard-refuse
  `git commit`, `git push`, `make clean`, `rm -rf` in argv
  (`FORBIDDEN` / `FORBIDDEN_ARGV`), and never touch `kernelhwsrv/`.
  P1 "commit point" text is explicitly gated on D3, which defaults to no.
- **G12 (`tools/r1_shim_residue_sweep.py`) does not exist yet** — it is to be
  authored overnight, as the plan says. See Hidden prerequisites for the Lua
  whitelist requirement.

### P4 (CI part) — PCI bridge/ECAM, USB mass-storage, readiness audit, QEMU USB gates
- `variant/qemu_pci.cpp` exists but is **bus-0 only** (header: "PCI
  configuration-space scanner (bus 0)"); no bridge walk or ECAM. `variant/
  qemu_usb.cpp` (580 ln) has no BOT/CBW/SCSI mass-storage. So the P4 work is
  genuinely additive glue on top of existing files — no prerequisite beyond
  the freeze register (D2 defaults to the freeze list, which P4 respects:
  variant-side only).
- `tools/r1_realhw_readiness_audit.py` does not exist yet — to be authored.
  QEMU USB gates exist: `r1_usb_ehci_test.py`, `r1_usb_ohci_test.py`.
  These steps boot QEMU.

### P5 (static part) — G16, manifest/metrics, make isogrub + smoke, final strict
- `tools/r1_genuine_source_audit.py` (G16) does not exist yet — to be authored.
  `genuine/BUILD_MANIFEST.json` exists. `make isogrub` target exists
  (`Makefile:593`, depends on `build/kernfull.elf` + `build/multiboot.elf`).
  `tools/r1_qemu_smoke.py` exists.
- **Stale plan text, not a blocker:** PLAN_EXECUTION P5 says "fix the `--name`
  path-traversal issue in `genuine_build.py` first" — that fix is already
  enforced in `tools/genuine_build.py` (sanitize-before-create, review issue
  #2, regex `^[A-Za-z0-9][A-Za-z0-9._-]*$`). The file is at `tools/`, not
  `genuine/`; path in the plan is also off by one directory.

## Hidden prerequisites

1. **Ordering:** P0's `.gitignore` append must land before the WATCH archive
   move, or the archive dir shows up as 128 new untracked files.
2. **G12 whitelist:** with D4 defaulting to "leave Lua untouched", G12's
   "no live references" assertion must whitelist `ports/lua/e32_shim.c` and
   the `user.mk:1536-1551` LUA.E32 wiring, or G12 FAILs on an item whose fate
   is an open decision.
3. **QEMU availability:** the overnight list includes baseline strict 11/11,
   P4 QEMU USB gates, and the P5 ISO smoke — all boot QEMU. The decision
   sheet's footer "no QEMU runs for this sheet" reads as scoped to the sheet
   itself, but if it was meant to cover overnight work it contradicts three
   listed items.
4. **Count drift:** `fs/r1_fs_*` is 14 files; PLAN_EXECUTION P1 step 3 says
   "13 files". Cosmetic; the sweep should key on names, not the count.

## Decision dependencies found

1. **WATCH archive step secretly needs D-COMMITS ≠ (a) (or D1).**
   PLAN_P0_COMMIT §3 step 0 states the precondition: "P0 commit landed and
   contains the closeout" before any WATCH file is moved; PLAN_SURVEY_HYGIENE
   steps 1–2 say the same ("commit it first … archive after the closeout is
   committed"). Under the D-COMMITS default (a) — no commits at all — that
   precondition can never be met, yet the decision sheet lists "P0: hygiene
   file moves" as proceeding with NO decisions. This is the one listed step
   with a hidden decision dependency. (Safety note: the 197 tracked WATCH
   files remain in git history regardless, so archiving without a commit is
   not unsafe — but the written procedure forbids it, so the step must be
   re-scoped or excluded.)
2. **G12 vs D4 (Lua):** not a hard dependency if Lua is whitelisted (see
   Hidden prerequisites #2); without the whitelist it becomes one.
3. Everything else checks out as decision-free: no listed step commits or
   pushes (executors FORBIDDEN-list it; all commit points sit behind D1/D3/D6/
   D9, all defaulting to no), nothing touches `kernelhwsrv/`, no `make clean`
   anywhere, P2/P3-asset work is correctly excluded.

## Verdict

**Executable with one exception.** The no-decision overnight list is real and
its paths exist at HEAD `ee8490e`: P0 staging set, P1 deletion targets and
guarded executors, P4 glue files, P5 manifest/ISO target are all present; the
archive dir becomes git-ignorable via P0's own `.gitignore` edit; nothing in
the list commits, pushes, runs `make clean`, or edits `kernelhwsrv/` (clean).
The single step that secretly needs a decision is the **WATCH archive move**:
its documented precondition is a landed P0 commit, which D-COMMITS default
(a) forbids — re-scope it (archive is evidentially safe via the 197 tracked
WATCH files) or drop it from the overnight list. Secondary flags: G12 must
whitelist Lua, the `.gitignore` edit must precede the archive, and three
listed items (baseline 11/11, USB gates, ISO smoke) require QEMU at execution
time despite the sheet's "no QEMU" footer.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. The feasibility notes that price a baseline strict 11/11 run price the 15-row run instead. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

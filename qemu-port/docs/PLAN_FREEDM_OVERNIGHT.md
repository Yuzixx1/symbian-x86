# PLAN_FREEDM_OVERNIGHT — P3-lite FreeDM staging + gate, unattended-overnight design

**Status: DESIGN ONLY.** This document is the only file written by the design
step. No code written, no commits/pushes, no `make clean`, no QEMU runs by the
designer, no downloads. `kernelhwsrv/` is READ-ONLY and untouched. Basis:
`docs/PLAN_FREEDM_MILESTONE.md` (P3-lite milestone) and
`docs/PLAN_SURVEY_FREE_GAMES.md` (§3 rank 1 = C1 FreeDM, §4 SKIP discipline,
§6.3 SKIP-clean-first).

**Asset reality check at design time:** `/tmp/opencode/doom/freedoom-0.13.0/`
contains `freedoom1.wad` and `freedoom2.wad` but **no `freedm.wad`**, and a
full search of `/tmp/opencode` finds no `freedm*.wad` anywhere; `qemu-port/wads/`
does not exist. Therefore today the FreeDM gate is expected to stay
`RESULT: SKIP` (exit 0) — which is the designed zero-decision behavior, not a
failure.

---

## Steps

The overnight run needs **zero user decisions**: every branch is decided by
file-existence probes and exit codes, never by a prompt.

1. **Probe asset presence (decides the whole run).** Check
   `/tmp/opencode/doom/freedoom-0.13.0/freedm.wad` first (the task-mandated
   path), then the milestone's fallback list (`qemu-port/wads/freedm.wad`,
   `qemu-port/wads/FREEDM.WAD`, `qemu-port/build/wad/freedm.wad`,
   `qemu-port/build/wad/FREEDM.WAD`, `/tmp/opencode/doom/freedm.wad`).
   - Present → full path: stage + boot gate.
   - Absent (today's state) → SKIP path: gate prints `[SKIP]` + `RESULT: SKIP`,
     exit 0; no QEMU boot; ISO build still runs to prove the append-only
     `mkgrubiso.py` change is a no-op without the asset.
2. **Static check.** `python3 -m py_compile tools/mkgrubiso.py
   tools/r1_freedm_test.py tools/r1_final_verify.py` — must pass before
   anything else runs.
3. **SKIP-clean-first gate run** (survey §6.3): run
   `python3 tools/r1_freedm_test.py` with the asset absent; require
   `RESULT: SKIP` and exit 0 *before* any QEMU boot with assets.
4. **ISO staging proof.** Run `python3 tools/mkgrubiso.py /tmp/opencode/r1_freedm_stage.iso`
   (direct script invocation — **never `make clean`, never a full `make`**).
   Then `mdir -i build/programs.img@@1048576 ::` and assert: with the asset
   absent, no `FREEDM.WAD` token and the free-bytes line is unchanged from
   baseline; with the asset present, `FREEDM.WAD` token present and free
   space ≥ 0 (the free-space guard warned-and-skipped if not).
5. **FreeDM gate boot (only if asset present).**
   `python3 tools/r1_freedm_test.py` — stages FreeDM bytes under the 8.3
   alias `FDOOM1.WAD` on its own 64 MB disk (`/tmp/opencode/r1_freedm.img`),
   boots, asserts the `FREEDM:` marker family plus the pinned
   `DOOM: WAD C:/FDOOM1.WAD <FREEDM_SIZE>` byte count, best-effort QMP
   screendump to `/tmp/opencode/freedm_final.png`.
6. **Regression gate.** `python3 tools/r1_final_verify.py --strict-skip` —
   must remain **11/11** (see Gate order).

## Files

Exact files the overnight run may touch — nothing else:

| File | Change | Why |
|---|---|---|
| `qemu-port/tools/mkgrubiso.py` | Append-only: add `WAD2_NAME = "FREEDM.WAD"`, `WAD2_CANDIDATES`, `find_wad2()` (clone of `WAD_CANDIDATES`/`find_wad()`, lines 80–93); in `build_programs_image()` (lines 163–213) stage `(wad2, WAD2_NAME)` behind a free-space guard (`mdir` free-bytes parse; warn+skip, never fail, never grow `FS_MB`); extend the mdir verification loop (~line 201) with the normalized token `freedmwad`; extend `iwad_note` in `curated_programs()` (~lines 130–133) to mention `FREEDM.WAD` when staged | Multi-asset staging on `/PROGRAMS.IMG`; geometry `DISK_MB, FS_MB, PART_LBA = 64, 48, 2048` (line 74) untouched |
| `qemu-port/tools/r1_freedm_test.py` | **New file**, clone of `tools/r1_doom_test.py` (450 lines): own paths (`DISK/LOG/PNG/ISO/QMP = /tmp/opencode/r1_freedm.*`), `FREEDM_CANDIDATES` = probe list from Step 1, `E32_CANDIDATES` unchanged, `check_assets()` returns None → `[SKIP]` + `RESULT: SKIP` exit 0 when DOOM.E32 or freedm.wad missing (mirrors `r1_doom_test.py:158–176`); `build_disk()` stages `(freedm, "FDOOM1.WAD")` + `DOOM.E32` + `EUSER.DLL` with mdir check for `doome32` + `fdoom1wad` (`r1_doom_test.py:185–213`); reuse `DOOM:` markers + pin `FREEDM_SIZE` exact byte count via the `DOOM: WAD C:/FDOOM1.WAD <FREEDM_SIZE>` line (clone of `WAD_SIZE = "28795076"`, lines 82, 318–321); own `FREEDM:` PASS/FAIL family; same dual boot mode, 75 s cap, `-vga std` | Dedicated gate with SKIP-on-missing-asset discipline |
| `qemu-port/docs/PLAN_FREEDM_OVERNIGHT.md` | This file | The design artifact |

**Explicitly NOT touched:** `tools/r1_final_verify.py` (`DEFAULT_TESTS` at
lines 121–135 stays exactly 11 entries — the FreeDM gate is deliberately not
wired in yet), `user/doome32.c`, anything under `ports/doom/`, `user/user.mk`,
the root `Makefile` (`FS_MB` stays 48), `tools/r1_doom_test.py` and the other
ten gates, all of `kernelhwsrv/` (read-only).

## Gate order

Strict sequence; each gate must pass before the next starts, and a failure
triggers Rollback immediately (no keep-going overnight):

1. `py_compile` on the three Python files (static, seconds).
2. `tools/r1_freedm_test.py` **standalone, asset absent** → require
   `RESULT: SKIP`, exit 0 (SKIP-clean-first, survey §6.3). This is the gate
   that runs in full today.
3. `mkgrubiso.py` direct-run staging proof + `mdir` verification (no QEMU).
4. `tools/r1_freedm_test.py` full QEMU boot — **only if the freedm.wad probe
   in Step 1 found an asset**; otherwise this gate is the Step-2 SKIP and is
   done.
5. `tools/r1_final_verify.py --strict-skip` → require 11/11 PASS-or-intended,
   i.e. exactly the 11 `DEFAULT_TESTS` entries, verdict unchanged from
   baseline. Because `r1_freedm_test.py` is **not** in `DEFAULT_TESTS`, its
   SKIP can never make the strict-skip run fail — that is why the run remains
   11/11 with the new gate present-but-unwired.

## Rollback

If any gate fails overnight:

1. Stop at the first failure; do not proceed to later gates.
2. `git checkout -- qemu-port/tools/mkgrubiso.py` (restores the pre-change
   staging code; the change is append-only, so this is a clean restore).
3. Delete the untracked `qemu-port/tools/r1_freedm_test.py`
   (`rm` — it was never committed, so nothing else references it).
4. Re-run `python3 tools/r1_final_verify.py --strict-skip` and require 11/11
   again — with both files back at baseline this is guaranteed, since
   `DEFAULT_TESTS` was never modified.
5. Never grow `FS_MB`, never edit `r1_final_verify.py`, never `make clean` as
   a "fix" — the free-space guard's warn-and-skip is the designed degradation,
   not a failure.

## No-commit rationale

- The task scope is **design + staging + gate only**; committing is out of
  scope and would freeze an overnight-unverified gate into history.
- Project policy (survey §4, `docs/DOOM_PORTING.md` §4): WAD assets are never
  committed, and gate wiring into `DEFAULT_TESTS` is a separate, reviewed
  decision — keeping it out is what lets the strict-skip run stay 11/11.
- The overnight run is unattended: leaving the tree dirty-but-known
  (`git status` shows only `mkgrubiso.py` modified + `r1_freedm_test.py`
  untracked + this doc) is safer than any commit, and Rollback above restores
  baseline with two commands.
- Nothing is pushed; `kernelhwsrv/` is read-only and never entered.

## Verdict

**GO for the overnight run, in SKIP mode.** With `freedm.wad` absent from
`/tmp/opencode/doom/freedoom-0.13.0/` (verified at design time), the run
exercises: py_compile → SKIP-clean `r1_freedm_test.py` (exit 0, `RESULT:
SKIP`) → append-only `mkgrubiso.py` no-op ISO build with unchanged `mdir`
free space → `r1_final_verify.py --strict-skip` 11/11. Zero user decisions,
zero commits, zero QEMU boots in the current asset state. If a freedm.wad is
later dropped into the probed path, the same script automatically takes the
full path (stage `FREEDM.WAD` on the ISO, boot FreeDM as `FDOOM1.WAD`, pin
`FREEDM_SIZE`) with no further decisions.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. Each instruction to re-run and require 11/11 refers to the 15-row gate; the FreeDM row remains deliberately unwired. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

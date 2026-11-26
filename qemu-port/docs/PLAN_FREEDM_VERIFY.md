# PLAN_FREEDM_VERIFY — review verdict on the FREEDM_SIZE pin and SKIP/baseline claims

**Scope:** read-only review of `docs/PLAN_FREEDM_OVERNIGHT.md`,
`docs/PLAN_FREEDM_MILESTONE.md`, `docs/PLAN_SURVEY_GAME_ASSETS.md` against
the live filesystem and `qemu-port/tools/`. No commits, no QEMU, no
`make clean`; `kernelhwsrv/` untouched.

## Asset presence

`/tmp/opencode/doom/freedoom-0.13.0/` contains `freedoom1.wad` (28,795,076 B)
and `freedoom2.wad` (28,787,748 B) but **no `freedm.wad`** (directory listing +
`stat` both confirm). A full `find /tmp/opencode -iname 'freedm*'` returns
nothing. All five fallback candidates are absent
(`qemu-port/wads/freedm.wad`, `qemu-port/wads/FREEDM.WAD`,
`qemu-port/build/wad/freedm.wad`, `qemu-port/build/wad/FREEDM.WAD`,
`/tmp/opencode/doom/freedm.wad`), and `qemu-port/wads/` does not exist. The
plans' design-time asset claim is accurate: the FreeDM asset is absent today.

## Size pin

The plans pin `FREEDM_SIZE = "<exact bytes>"` as a **placeholder to be
recorded at drop-off**, not a guessed constant — so there is no wrong number
to be brittle about. The pin mechanism itself is proven by the existing DOOM
gate: `WAD_SIZE = "28795076"` (`tools/r1_doom_test.py:82`) matches the actual
`freedoom1.wad` byte size (28,795,076) exactly, and the assertion runs on the
`DOOM: WAD C:/FDOOM1.WAD <size>` guest line. Because the pin is an exact byte
count, a swapped-in Freedoom IWAD (28,795,076 / 28,787,748 B) cannot satisfy
it. Robust as designed; unpopulated until an asset lands, which is correct.

## SKIP behavior

The SKIP path is real and exit-0 in the template gate:
`tools/r1_doom_test.py:422-425` — `check_assets()` returning None prints
`RESULT: SKIP` and `return 0`; `check_assets()` (`r1_doom_test.py:151-172`)
returns None when the E32 or WAD is missing. `tools/r1_freedm_test.py` does
not exist yet (design-only), but the milestone mandates a clone of this exact
pattern with the freedm candidate added, so the SKIP path is a proven
copy-paste of exit-0/no-op behavior, not a new invention. With the asset
absent, the overnight plan's SKIP gate is a true no-op: no QEMU boot, no
disk, exit 0.

## Baseline safety

- `tools/mkgrubiso.py` currently contains **zero** `freedm`/`WAD2` references
  (grep confirms) — the tree is at baseline; the WAD2 change is not yet
  applied and is specified as append-only.
- With no WAD2 asset, the specified `find_wad2()` returns None, the staging
  block and free-space guard never execute, `files` is unchanged, and the
  mdir verification loop iterates only over the original names — so the
  staged `PROGRAMS.IMG` and its free-bytes line are byte-identical to
  baseline. Geometry `DISK_MB, FS_MB, PART_LBA = 64, 48, 2048` (line 74) is
  untouched by design.
- `tools/r1_final_verify.py:121-133` — `DEFAULT_TESTS` has exactly **11**
  entries and no freedm gate; `--strict-skip` therefore remains 11/11 with
  the new gate present-but-unwired.

## Verdict

**PIN ROBUST — claims verified.** freedm.wad is genuinely absent from every
probed path, so the designed `RESULT: SKIP` / exit-0 path is the correct
current behavior and is backed by a proven exit-0 pattern in
`r1_doom_test.py`. `FREEDM_SIZE` is an exact-byte placeholder (no guessed
value), and the pin mechanism is validated by the matching `WAD_SIZE` pin for
freedoom1.wad. The append-only `mkgrubiso.py` WAD2 staging is a strict no-op
when no WAD2 asset exists (no `files` delta, no geometry change), and the
strict-skip baseline stays exactly 11/11 since `DEFAULT_TESTS` is unmodified.
No discrepancies found between the plans and the live tree.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. The claim that `--strict-skip` stays exactly 11/11 because `DEFAULT_TESTS` is unmodified is obsolete: `DEFAULT_TESTS` is 15 rows. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

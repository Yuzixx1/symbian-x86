# PLAN_SURVEY_GATES — verification-gate inventory vs. proposed next phases

Read-only survey of `qemu-port/` as of HEAD `ee8490e`. No edits outside this
file; `kernelhwsrv/` untouched (read-only); no commit/push; no `make clean`;
no QEMU runs. Purpose: map existing gates to the five proposed phases and name
the NEW gate/harness each phase needs so `r1_final_verify.py --strict-skip`
stays 11/11 green while scope grows.

## Existing gate inventory

**The authoritative 11/11 strict gate** = `tools/r1_final_verify.py --strict-skip`
(`tools/r1_final_verify.py:108-119`, DEFAULT_TESTS; `--strict-skip` makes SKIP
fatal, line 433). The 11 entries, in order:

| # | Gate name | Script | Cost class |
|---|---|---|---|
| 1 | selfcheck | `tools/r1_os_selfcheck.py` (build + isogrub + qemu_smoke + cp4_ca + cp4_stress + selftest) | slowest (~900 s, build + 3 QEMU boots) |
| 2 | qemu_smoke | `tools/r1_qemu_smoke.py` | 1 boot (~150 s) |
| 3 | genuine_retire | `tools/r1_genuine_retire_test.py` (subsystems efsrv/ewsrv/estart/euser; BUILT + static wire-opcode checks + QEMU boot markers) | slow (~600 s, several boots) |
| 4 | genuine_efsrv | `tools/r1_genuine_efsrv_test.py` | 1 boot (~300 s) |
| 5 | genuine_ewsrv | `tools/r1_genuine_ewsrv_test.py` (genuine !E32WindowServer session + EWCLIENT text window + QMP screendump) | 1 boot (~300 s) |
| 6 | genuine_eshell | `tools/r1_genuine_eshell_test.py` | 1 boot (~300 s) |
| 7 | genuine_fat_io | `tools/r1_genuine_fat_io_test.py` | 1 boot (~300 s) |
| 8-11 | doom_main / doom_audio / doom_playtest / doom_multikey | `tools/r1_doom_*.py` (--no-build-iso, staged fresh ISO; SKIP when DOOM assets missing) | 4 boots (~240-360 s each) |

**Supporting gates (not in the 11, but load-bearing):**

- Shim-retirement toolchain: `tools/r1_shim_retire_plan.py` (audit-only planner,
  hard dry-run-only), `tools/r1_shim_retire_apply.py` (guarded executor;
  pre-write gate must report PASS, snapshot+rollback, never deletes),
  `tools/r1_winsrv_delete.py` (guarded P1-P3 deletion executor with static
  DELETE_SET, reuses apply helpers by import; winsrv retirement already landed
  in `94ce9c9`/`ee8490e`).
- In-selfcheck gates: `tools/r1_selftest.py`, `tools/r1_cp4_ca_test.py`,
  `tools/r1_cp4_ca_stress_test.py`.
- GUI/blit pixel-proof gates (all SKIP-graceful when artifacts absent):
  `tools/r1_fbblit_test.py` (exec 270 op 3, QMP pixel check),
  `tools/r1_gcblit_test.py` (exec 251 GC_BLIT, QMP pixel check),
  `tools/r1_window_system_test.py`, `tools/r1_graphical_desktop_test.py`,
  `tools/r1_desktop_test.py`, `tools/r1_guishell_test.py`,
  `tools/r1_gui_apps_test.py`, `tools/r1_gui_regression.py`.
- Hardware/USB QEMU-only gates: `tools/r1_usb_ehci_test.py`,
  `tools/r1_usb_ohci_test.py`, `tests/test_xhci_keyboard.py`,
  `tools/r1_virtio_{blk,net,rng}_test.py`, `tools/r1_qemu_{display,mouse,nic,dns,audio}_test.py`.
- Game-port gates: the `r1_doom_*` family (7 scripts incl. visual/release/stuck)
  plus `tools/r1_lua_test.py`, `tools/r1_zmachine_test.py`, etc. — all
  SKIP-on-missing-assets.

## Coverage by phase

1. **Finish shim deletion** — Covered: gate 3 (`r1_genuine_retire_test.py`,
   per-subsystem PASS is the READY signal) + `r1_shim_retire_plan.py` /
   `r1_shim_retire_apply.py` / `r1_winsrv_delete.py` enforce gate-before-touch,
   kernelhwsrv-clean refusal, rollback. Gap: remaining orphans
   (`fs/r1_fs_*` 13 files, `services/r1_service_host.*`, `variant/r1_fs_channel.*`,
   stale build artifacts per `docs/PLAN_SURVEY_SHIMS.md`) are invisible to the
   11-gate — nothing asserts their absence.
2. **Genuine EWSRV GUI/Wsrv session + BitBlt/font proof** — Partially covered:
   gate 5 proves session connect + titled *text* window (`TText` cells; genuine
   `ws_win.cpp` has no bitmaps/fonts/BitBlt — `docs/PLAN_SURVEY_UIKON_GDI.md:11`).
   `r1_fbblit_test.py`/`r1_gcblit_test.py` prove raw framebuffer blits via exec
   270/251, not EWSRV-composited BitBlt; no font/glyph gate exists.
3. **Second game** — Template covered: gates 8-11 (`r1_doom_*` family, SKIP
   discipline, fresh-ISO staging) + reusable bridges per
   `docs/PLAN_SURVEY_NEXT_GAME.md`. No second-game gate exists yet.
4. **Real-PC/USB readiness** — Only QEMU-side: `r1_usb_ehci/ohci_test.py`,
   `r1_virtio_*`, `tests/test_xhci_keyboard.py` (all q35/QEMU machine types;
   `docs/PLAN_SURVEY_HW_USB.md` lists real-PC gaps: bus-0-only PCI, no ECAM,
   no NVMe, no APIC/ACPI). No real-hardware harness exists.
5. **Imported genuine sources if available** — Covered by gates 3-7: they build
   from read-only `kernelhwsrv/` via `genuine/build_*.py` and SKIP gracefully
   when genuine sources/artifacts are absent (`r1_genuine_ewsrv_test.py`
   docstring). Gap: no gate audits *which* genuine components have port build
   scripts or verifies sources are unmodified vs `kernelhwsrv/`.

## New gates required

- **G12 `tools/r1_shim_residue_sweep.py`** (phase 1): static, no QEMU — assert
  retired shim files absent, no live kernel hook references them, orphan
  `fs/r1_fs_*`/`services/r1_service_host*` either deleted or explicitly
  whitelisted; cheap (<5 s), so it can sit first in DEFAULT_TESTS without
  cost. Keeps deletion progress visible under --strict-skip.
- **G13 `tools/r1_genuine_ewsrv_gdi_test.py`** (phase 2): boot genuine EWSRV
  + pixel-drawing client (edisp/LFB pixel path per
  `docs/PLAN_SURVEY_UIKON_GDI.md` section on wd_generic), QMP screendump
  pixel-checked for BitBlt output + glyph rendering; SKIP cleanly when genuine
  GDI/BitBlt sources are absent so strict-skip stays green.
- **G14 `tools/r1_<game2>_test.py`** (phase 3): clone of `r1_doom_test.py`
  (boot + markers + ord-915 bind + QMP pixel check + input drive), SKIP when
  game assets absent; add to DEFAULT_TESTS only after assets land, else keep
  out-of-band with the same SKIP discipline. Watch disk headroom
  (~16.8 MB free after Freedoom1 — `docs/PLAN_SURVEY_NEXT_GAME.md`).
- **G15 `tools/r1_realhw_boot.py`** (phase 4): off-CI harness — boot the GRUB
  ISO on a physical PC, capture COM1 serial log, assert boot markers + USB
  enumeration (EHCI/xHCI keyboard+mouse) markers; never in DEFAULT_TESTS.
  In-CI complement: static `r1_realhw_readiness_audit.py` (PCI bridge/ECAM,
  NVMe, APIC/ACPI gap checklist) so the 11/11 reflects real-PC gaps without
  hardware.
- **G16 `tools/r1_genuine_source_audit.py`** (phase 5): static matrix of
  `kernelhwsrv` components vs `qemu-port/genuine/build_*.py`, with sha256 of
  patched copies vs pristine sources; no QEMU, <5 s.

## Gate ordering/cost

Static gates (G12, G16, realhw audit) are seconds-cheap and belong FIRST in
DEFAULT_TESTS (fail fast before any QEMU spend). Slow gates stay ordered:
selfcheck (~900 s) and genuine_retire (~600 s) first (they gate everything),
then single-boot genuine gates 2,4-7 (~150-300 s), then game gates (~240-360 s
each). Adding G13/G14 as QEMU gates adds ~300-360 s each to a strict run;
prefer keeping them out-of-band until their assets exist, relying on SKIP
discipline, so the current 11/11 wall time does not regress.

## Risks

- `--strict-skip` makes any SKIP fatal: new gates must SKIP only when a
  prerequisite is genuinely absent, and DOOM's asset-SKIPs mean the 11/11
  strict run already depends on DOOM.E32/FREEDOOM1.WAD/EUSER.DLL being present.
- Phase 2 cannot be fully "genuine": upstream EWSRV is text-only, so BitBlt/font
  proof must ride the port-side edisp/LFB pixel path — label it port-bridge,
  not genuine, or gate 5's "genuine" claim gets diluted.
- Phase 3 asset size vs 64 MB disk image headroom; X: RAMDISK opens fail
  (KErrCorrupt), assets must stage on AHCI C:.
- Phase 4 has no CI hardware; a static audit can drift from real firmware
  behavior — keep QEMU CI primary per `docs/PLAN_SURVEY_HW_USB.md`.
- `kernelhwsrv/` read-only constrains phases 2 and 5: no fixes upstream, only
  `genuine/*/patched/` copies plus port glue.
- Gate sprawl: every new entry in DEFAULT_TESTS multiplies serial QEMU boots;
  prefer consolidated single-boot gates (like gate 5) over per-feature boots.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. In particular, the "authoritative 11/11 strict gate" definition near the top of this survey names 11 rows where the tool now defines 15. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

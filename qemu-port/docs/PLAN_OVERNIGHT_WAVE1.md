# PLAN OVERNIGHT WAVE1 — first-wave overnight briefs (P0, P1, P3-scaffold, P4-CI)

**Date:** 2026-11-25. **Status: PLANNING ONLY** — this file is the only file
written; no QEMU runs, no commits/pushes, no `make clean`, `kernelhwsrv/`
READ-ONLY. Basis: [PLAN_DECISIONS_RESOLVED.md](PLAN_DECISIONS_RESOLVED.md)
(D1–D6 resolved). **P2 OUT OF SCOPE** (D1), **P5 deferred**. P3 = FreeDM
BSD-CC0 PWAD on the existing `DOOM.E32` (D2) per
[PLAN_FREEDM_OVERNIGHT.md](PLAN_FREEDM_OVERNIGHT.md) +
[PLAN_FREEDM_MILESTONE.md](PLAN_FREEDM_MILESTONE.md).

## Standing rules (every brief)

1. Exclusive file ownership as listed per brief — touch nothing else.
2. No commits/pushes (D4): working-tree changes + green gates only.
3. `kernelhwsrv/` READ-ONLY: never written; `git -C kernelhwsrv status
   --porcelain` must stay empty.
4. No `make clean` ever; incremental `make` only, stale artifacts by path.
5. QEMU serialized: flock `qemu-port/.sybian/qemu-slot.lock` around every
   boot; exit-124 FAIL = slot contention — re-run isolated before believing it.
6. Each brief ends at its go/no-go gate; first NO-GO stops the wave.

## Do NOT (all briefs)

- No WATCH archive move — gated on the commits decision (D4 = no commits).
- No Wolf3D/shareware, no `ports/wolf3d/`, no `r1_wolf3d_test.py` (D2).
- No asset download — user-supplied files only.
- No CD-R / physical boot — QEMU CI only.
- No kernel hook edits — exec 81/269/270, boot contract, freeze list NO-GO.

## P0 — Baseline, hygiene, kernel-hook freeze

- **Owns:** `.gitignore` append, deletion of 0-byte `qemu-port/9`,
  `docs/KERNEL_HOOK_FREEZE.md` (new), baseline gate logs to `/tmp/opencode/`,
  `docs/FINAL_GATE_STATUS.md` edit. NOT the WATCH archive move (see Do NOT).
- **Runs:** fresh `r1_final_verify.py --strict-skip` + `r1_genuine_retire_test.py`
  4/4 + `r1_genuine_fullchain_test.py` 14/14 under the qemu-slot lock.
- **Gate:** GO only with strict-skip 11/11 + retire 4/4 + fullchain 14/14 on a
  stable uninterrupted run; NO-GO on any SKIP/FAIL or exit 124 (re-run isolated).

## P1 — Guarded shim deletion (steps 1–4, 6–7 + G12)

- **Owns:** `fs/r1_fs_*`, `services/r1_service_host.{cpp,h}`,
  `services/r1_service_state.h`, `variant/r1_fs_channel.{cpp,h}`,
  `user/user.mk` (orphaned r1_fs rules + opt-in block + cosmetic comments),
  `user/ipc.c`, `tools/r1_genuine_retire_test.py` static-check rewrite,
  `tools/r1_shim_residue_sweep.py` (new G12), stale build artifacts by path.
  Executors `r1_shim_retire_plan/apply.py`, `r1_winsrv_delete.py` refuse
  commit/push/`make clean`/`rm -rf` in argv — keep them that way.
- **Runs:** plan → real pre-gate → apply with token → snapshot → incremental
  `make` → per-phase gate → strict-skip, per step; Lua untouched (D4 default).
- **Gate:** after each step and at phase end — G12 PASS + retire 4/4 +
  fullchain 14/14 + strict-skip 11/11; NO-GO on any FAIL/SKIP, AMBIGUOUS plan,
  or non-empty `kernelhwsrv` porcelain.

## P3-scaffold — FreeDM PWAD staging + SKIP-clean gate (on existing DOOM.E32)

- **Owns:** `tools/r1_freedm_test.py` (new, clone of `r1_doom_test.py`, SKIP
  exit 0 on missing asset, `FREEDM:` marker family), append-only
  `tools/mkgrubiso.py` (`WAD2_NAME="FREEDM.WAD"`, `WAD2_CANDIDATES`,
  `find_wad2()`, free-space-guarded staging, never grow `FS_MB`).
  `tools/r1_final_verify.py` UNTOUCHED (stays 11/11; FreeDM gate not wired in).
- **Runs:** `py_compile` the three Python files → `r1_freedm_test.py` with
  asset absent must print `RESULT: SKIP` exit 0 (SKIP-clean-first) →
  `mkgrubiso.py` direct-run staging proof + `mdir` free-space unchanged →
  full QEMU boot only if a `freedm.wad` probe hits → strict-skip 11/11.
- **Gate:** GO = SKIP-clean exit 0 + ISO proof + strict-skip 11/11 unchanged;
  with asset present, add `r1_freedm_test.py` PASS. NO-GO on any regression.

## P4-CI — Real-PC/USB readiness, CI part only (QEMU-first)

- **Owns:** `variant/qemu_pci.cpp` (bridge walk bus>0 + ECAM), `variant/qemu_usb.cpp`
  + new polled BOT/CBW mass-storage driver, `tools/r1_realhw_readiness_audit.py`
  (new, static, in-CI). G15 real-hw harness and CD-R boot stay human/off-CI.
- **Runs:** readiness audit + `r1_usb_ehci_test.py`, `r1_usb_ohci_test.py`,
  `tests/test_xhci_keyboard.py`, virtio gates, all under the qemu-slot lock,
  then strict-skip.
- **Gate:** GO = audit PASS + all QEMU USB/virtio gates PASS + strict-skip
  11/11, no boot-time regression (HwInit0 idempotency freeze). NO-GO on any
  USB-gate regression.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. Wave-1 GO/NO-GO lines that pair strict-skip 11/11 with retire 4/4 and fullchain 14/14 keep their decision logic and take the 15-row count. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

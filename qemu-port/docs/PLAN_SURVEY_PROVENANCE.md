# PLAN_SURVEY_PROVENANCE — what "genuine completeness" means, measured

**Date:** 2026-11-24. **Scope:** read-only survey. No web, no edits outside this
file, no commit/push, no `make clean`, no QEMU. `kernelhwsrv/` untouched.
Sources: `docs/GENUINE_PROGRESS.md`, `docs/REMAINING_SHIMS_AUDIT.md`,
`docs/GENUINE_BUILD_REVIEW.md`, `genuine/BUILD_MANIFEST.json`, tree line counts.

## 1. Current state / metrics

"Genuine" = byte code compiled from the read-only `kernelhwsrv/` tree (or its
prebuilt DLLs) and actually loaded/bound in the boot chain. Everything else in
`qemu-port/` is port glue (hand-written x86/QEMU code).

- **Genuine components live today** (all retire gates PASS, 2026-11-24):
  EUSER.DLL (prebuilt, resident every boot, 2503 exports), full EFILE.E32
  (273,068 B, 464 EUSER imports bound, on-disk FAT I/O verified), EFSRV/F32
  probes, EWSRV.E32 (56,812 B, 212 binds), ESTART/ESHELL, HAL/DM probes.
- **Genuine source lines compiled:** fileserver F32/EFSRV/EFILE = 93 sources,
  ~83,260 lines (`kernelhwsrv/userlibandfileserver/fileserver/`); EWSRV ~6,275
  lines; EUSER ~56,395 lines (prebuilt DLL). Roughly **~146k kernelhwsrv lines**
  back the running genuine payload out of a 1,230,821-line read-only pool (~12% of
  the pool is in use; the rest is kernel core we do not compile).
- **Port glue still compiled/linked:** `user/ + fs/ + services/ + variant/`
  ≈ 49,979 lines; kernel-side `nk/ variant/ boot/ mmu/ disk/ console/ loader/`
  ≈ 20,970 lines → **~71k lines of glue**, i.e. glue still dominates source-line
  count ~1:2 even though the four server subsystems run genuine code.
- **No shim file is deleted yet.** The EFSRV cut is a boot-path bypass only:
  `portfilesrv.cpp`/`fs/r1_fs_*` objects are still linked (`Makefile:326`); winsrv
  files were deleted in a later commit but `build/user/winsrv.o` artifacts remain.

## 2. Metric proposal — "genuine code ratio"

Track **two complementary ratios**, reported per release by a script (e.g.
extending `tools/r1_shim_retire_plan.py`):

1. **Genuine byte ratio (primary):** `sum(bytes of E32 images in the boot path
   whose code originates from kernelhwsrv) / sum(bytes of all E32 images in the
   boot path)`. Today: genuine payload ≈ EFILE 273k + EWSRV 57k + EUSER.DLL +
   estart/etshell ~111k ≈ **~460–500 KB genuine** vs total staged images
   (adds portfilesrv/svcmgr shims still on disk). Computable mechanically from
   `genuine/BUILD_MANIFEST.json` + build logs; immune to line-count games.
2. **Genuine source-line ratio (secondary):** kernelhwsrv-compiled lines /
   (kernelhwsrv-compiled + qemu-port glue lines), scoped to server subsystems
   (exclude `user/` demo apps and `tests/`). Today ≈ 146k / (146k + ~30k
   server-relevant glue) ≈ **~83% by lines within servers**, but only ~12% of
   the total kernelhwsrv pool is exercised.

Gate rule: a subsystem counts as "genuine" only when its retire gate is PASS
*and* its shim files are deleted (not merely bypassed) — this closes the
"bypass counted as retirement" hole seen in REMAINING_SHIMS_AUDIT §1.

## 3. Largest glue files (current tree)

| Lines | File | Class |
|---|---|---|
| 2830 | `variant/kern_shell.cpp` | B debug console (keep) |
| 2022 | `variant/fat32.cpp` | **A** kernel FAT glue — retire with r1_fs_* |
| 1568 | `variant/e32load.cpp` | B loader/port bridge, but hosts svcmgr + exec 81/268–270 handlers (shrink, keep core) |
| 796 | `user/desktop.c` | app, not server glue |
| 764 | `variant/qemu_audio.cpp` | B PC audio bridge (keep) |
| 742 | `user/ws_e32_client.c` | winsrv client shim remnant |
| 710 | `variant/qemu_variant_int.cpp` | B input/IRQ bridge (keep) |
| ~350–310 | `fs/r1_fs_core.cpp` (346), `fs/r1_fs_fat.cpp` (307), `fs/r1_fs_server.cpp` (201) | **A** retire with EFSRV |
| 293+ | `user/netstack.c` family | B authored exception — never retire (ESOCK absent) |

Load-bearing PC bridges (keep, class B): storage (`qemu_ata/ahci/virtio_blk/usb/xhci`),
display (`qemu_display/qemu_fbblit`), network (`qemu_nic/virtio_net/netbridge`),
audio, input, PCI/ASSP, `e32load.cpp` core, `runtime.c/ipc.c`. Replacement logic
(class A, retire): `variant/fat32.cpp` + `fs/r1_fs_*`, `user/svcmgr.cpp` +
SvcMgr exec handlers in `e32load.cpp`, winsrv remnants (`ws_e32_client.c` et al.),
`user/euser_shim.cpp` (already closed per recent commits; verify).

## 4. Retirement shortlist (dependency order)

1. `fs/r1_fs_*` + `variant/fat32.cpp` + `user/portfilesrv.cpp` — EFSRV gate PASS;
   delete files/objects/`R1Fs::*` construction after a clean strict verify.
2. `user/svcmgr.cpp` + `SvcMgr*Exec`/execs 268–270 in `e32load.cpp` — keep the
   exec-81 `FsRegisterThread` wrapper until EFILE teardown is proven without it.
3. winsrv remnants (`user/ws_e32_client.c`, stale `build/*winsrv*` objects) —
   after `edisp` QEMU port + external-client window gate.
4. `user/euser_shim.cpp`/`e32base.h` static linkage — once `elf2e32.py` emits real
   PE2 imports for all user E32s.
5. Never: `user/netstack.c` family (no ESOCK in this kernelhwsrv snapshot).

## 5. Risks

- Bypasses are not deletions: shims remain compiled; a disk without staged
  genuine images won't boot. Keep the rollback snapshot until full gate green.
- exec-81 wrapper removal reintroduces KERN 4 on EFILE teardown; PTRINJ sink lives
  in exec 270 op 4 — sequence svcmgr removal carefully.
- Gate snapshots conflict across docs (older FAIL vs current PASS); use a fresh
  `r1_final_verify.py --strict-skip` as authority.
- Byte-ratio metric depends on accurate provenance manifest; keep
  `genuine/BUILD_MANIFEST.json` updated per build.
- kernelhwsrv/ is read-only; `--name` path traversal in `genuine_build.py`
  (GENUINE_BUILD_REVIEW issue #2) must be fixed before any further genuine builds.

**2026-11-26 — correction:** §3/§4 are partly stale — S3 has landed (delete `variant/r1_fs_channel.*` + 6 `fs/r1_fs_channel*`/`r1_fs_wire_logic.h` + `tests/r1_fs_channel/`) and G12/W5 are now wired as `r1_final_verify.py` rows 1–2 (`g12_residue`, `missing_statics`), giving `r1_shim_residue_sweep.py` `RESULT: PASS (0 residue, 43 allowed comments)` and a strict `final13` run of **13/13 PASS, 0 skip, 0 fail, `FINAL13_RC=0`** (`/tmp/opencode/final13_gate.log`); provenance itself is unchanged — `r1_source_provenance_audit.py` still `RESULT: PASS (genuine 8/8 sourced, 8 artifacts, shims retired 4/6)`, though the 16 S3-era deletions (15 `r1_fs_channel*`/`r1_service_host*` files + `fs/r1_fs_core.mmp`) are still uncommitted at HEAD `ee8490e`, so none of this is reproducible from HEAD alone.

**2026-11-26 — correction (§1):** the §1 "still compiled/linked" claims on line 23 and line 28 were missed by the 2026-11-26 note above, which named only §3/§4; both are superseded — no `portfilesrv`, `winsrv`, `svcmgr` or `r1_fs_channel` source or object files remain anywhere in the tree, `Makefile` cites none of them (so the `Makefile:326` citation no longer resolves), the surviving `fs/r1_fs_core|fat|server|backend` files are the genuine F32 copy rather than shims, and the strict 13-row gate is **13/13 PASS** (0 skip, 0 fail, `FINAL13_RC=0`).

**2026-11-26 (later — 15-row gate) — correction to both notes above:** where the §3/§4 and §1 corrections each record a 13-row strict gate of 13/13 PASS, the gate is now **15/15 PASS** (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`); the two rows added beyond the earlier 13 are `g12_negative` (`tools/r1_g12_negative_test.py`) and `hal_probe` (`tools/r1_genuine_hal_test.py`), which sit alongside the already-present `g12_residue`/`missing_statics` rows. Everything else in those two notes is unchanged: `r1_shim_residue_sweep.py` PASS (0 residue, 43 allowed comments), `r1_source_provenance_audit.py` PASS (genuine 8/8 sourced, 8 artifacts, shims retired 4/6), and the 16 S3-era deletions remain uncommitted at HEAD `ee8490e`.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. This includes the three 2026-11-26 correction notes above, which stop at a 13-row gate of 13/13; their 15-row successor is this footer. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

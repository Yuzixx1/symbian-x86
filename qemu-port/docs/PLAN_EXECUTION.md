# PLAN EXECUTION — overnight autonomous work plan (P0..P5)

**Date:** 2026-11-25. **Scope:** planning only. This document is the only file
written for this task: no code started, no commit/push, no `make clean`, no
QEMU run, `kernelhwsrv/` untouched (read-only). Inputs: all
`docs/PLAN_SURVEY_*.md` (ESOCK, GATES, GUI_MILESTONE, HW_USB, HYGIENE, MEDIA,
NEXT_GAME, PROVENANCE, ROLLBACK, SHIMS, SOURCE_IMPORT, UIKON_GDI, WSRV_NAME),
`docs/r1_decision_record_v1.md`, `docs/GO_NO_GO.md`, root `AGENTS.md`, HEAD
`ee8490e`.

## Executive summary

Six phases, strictly ordered P0 → P5. P0 freezes the baseline and the risky
kernel hooks; P1 finishes deletion of bypassed shims under the guarded
executors; P2 lands the smallest credible GUI milestone on genuine EWSRV+edisp
(pixel proof via port-defined opcode, honestly labeled port-bridge); P3 ports
a second game (Wolf3D shareware recommended) on the DOOM-proven bridges; P4
makes the ISO real-PC/USB-ready (QEMU-first: static audit in CI, physical boot
off-CI); P5 closes with provenance audit and the bootable GRUB ISO as the
artifact. Every phase is gated on `tools/r1_final_verify.py --strict-skip`
11/11 plus its own new gate; nothing mutates unless its pre-gate is PASS;
`kernelhwsrv/` is never edited; no genuine logic is ever replaced (authored
code is glue only — the single sanctioned exception remains the authored
`!SocketServer` netstack, ESOCK being absent from the tree). Commits happen
only on explicit user request (standing rule); overnight work therefore
delivers working-tree changes + green gates, and commit points are listed per
phase for the user to authorize.

## Standing invariants (apply to every phase)

1. `kernelhwsrv/` READ-ONLY — compile from it only; patched copies live under
   `qemu-port/genuine/*/patched/` only.
2. Never replace genuine logic: port code is x86/PC glue (driver, loader,
   bridge), never a replacement for real Symbian logic (AGENTS.md). Netstack
   stays as the documented authored exception (PLAN_SURVEY_ESOCK).
3. Authoritative gate: `python3 tools/r1_final_verify.py --strict-skip` =
   11/11 PASS (selfcheck, qemu_smoke, genuine_retire, genuine_efsrv,
   genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio,
   doom_playtest, doom_multikey). Retire sub-gate:
   `tools/r1_genuine_retire_test.py` 4/4. Full chain: `r1_genuine_fullchain_test.py` 14/14.
4. Guarded mutation only: `tools/r1_shim_retire_plan.py` (dry-run planner) →
   `tools/r1_shim_retire_apply.py` / `tools/r1_winsrv_delete.py` (snapshot +
   token + per-phase gates + auto-rollback; refuse `git commit`/`push`/
   `make clean`/`rm -rf` in argv). Never invoke an apply while any gate is
   FAIL/SKIP. Snapshots in `/tmp/opencode/` — copy manifests into
   `docs/` or `archive/` before any reboot; git-history fallback is
   `git show <sha>:<path>`.
5. No `make clean` ever; incremental `make` only. Delete stale artifacts by
   path, not by clean.
6. No commit/push unless the user explicitly asks (AGENTS.md). Each phase
   lists its commit point; the overnight run stops at green gates + listed
   files.
7. EFILE stays guard-only — never re-add the trampoline/veneer (KERN 4).
8. QEMU slot discipline: use the `.sybian/qemu-slot.lock` flock; an isolated
   smoke FAIL with exit 124 was slot contention, re-run before believing it.
9. QEMU is the primary CI; real hardware is a secondary smoke path only
   (`docs/hw_worksheet.md` policy: build PC only, never near the reference
   PC's data disk).
10. Bootable ISO artifact = `build/symbian-grub.iso` via `make isogrub`
    (GRUB2 combo BIOS+UEFI, Multiboot1 contract unchanged).

## User-decision register (blocking decisions, by phase)

| # | Decision | Blocks | Recommendation in surveys |
|---|---|---|---|
| D1 | Authorize committing the pending hygiene/closeout docs (WINSRV_RETIREMENT_FINAL.md, 4 PLAN_SURVEY files, 5 modified docs, state.json) | P0 commit point | PLAN_SURVEY_HYGIENE sequence step 1 |
| D2 | Confirm the kernel-hook freeze list (P0 register) as NO-GO without explicit approval | All phases | PLAN_SURVEY_HW_USB risk list + PLAN_SURVEY_SHIMS "never in this pass" |
| D3 | Authorize shim-deletion commits (fs/r1_fs_*, service host, channel, stale artifacts) | P1 commit points | PLAN_SURVEY_SHIMS deletion order 1–6 |
| D4 | LUA.E32: gate it first or remove the app (currently ungated) | P1 step 5 | PLAN_SURVEY_SHIMS |
| D5 | GUI purity axis: (a) port-bridge pixels on genuine EWSRV now, or (b) also import upstream UIKON/GDI sources externally (EPL license review, network fetch) | P2 (and P5 imports) | PLAN_SURVEY_UIKON_GDI step 3; PLAN_SURVEY_SOURCE_IMPORT |
| D6 | Second game choice + supplying/authorizing the shareware IWAD assets; approve disk-image growth or selectable WAD set (16.8 MB free after Freedoom1) | P3 | PLAN_SURVEY_NEXT_GAME: rank 2 Wolf3D shareware |
| D7 | Real-PC scope: CD-R first medium on the reference PC; approve variant-side bus>0 PCI walk + ECAM + USB mass-storage driver as new glue | P4 | PLAN_SURVEY_HW_USB first 3 steps |
| D8 | Approve external upstream import (license verification per directory; `genuine/imported/<area>/` + hash in BUILD_MANIFEST.json) and any loader cap raise (512 KiB image buffer) if a genuine multi-component stack is attempted | P5 | PLAN_SURVEY_SOURCE_IMPORT build steps |
| D9 | Final acceptance: `build/symbian-grub.iso` as the deliverable artifact + final strict 11/11 + commit authorization for the whole overnight delta | P5 close | AGENTS.md primary deliverable |

## P0 — Baseline, hygiene, and the kernel-hook freeze

**Order:** first — everything else is measured against this baseline.

**Deliverables (files/gates):**
- `docs/PLAN_EXECUTION.md` (this file) — plan of record.
- `docs/KERNEL_HOOK_FREEZE.md` — the freeze register (see below); referenced
  from AGENTS.md at next authorized doc commit.
- Hygiene per PLAN_SURVEY_HYGIENE: commit (only if D1 authorized) the
  closeout + survey docs; archive/delete the 128 `WINSRV_DELETE_WATCH*.md`
  into `docs/archive/`; delete 0-byte `qemu-port/9`; extend `.gitignore`
  with `*.tar.gz`, `qemu-port/.sybian/qemu-slot.lock`, stray-file pattern.
  `symbian-x86-archive-56fbaff.tar.gz` stays untracked, never committed.
- Baseline gate evidence: fresh `r1_final_verify.py --strict-skip` 11/11 log
  + `r1_genuine_retire_test.py` 4/4 + fullchain 14/14, stored under
  `/tmp/opencode/` and summarized in `docs/FINAL_GATE_STATUS.md` (doc edit
  only; commit needs authorization).

**Freeze register (NO-GO without explicit user decision — D2):**
- exec 81 `PortFsRegisterThread` wrapper, exec 269 `PortKillExec`, exec 270
  ops 2/3/4 incl. PTRINJ sink (`variant/e32load.cpp`) — live bridges pinned by
  cp4/pointer gates and the EFILE KERN-4 constraint.
- Boot contract: `boot/multiboot.S`, `_mb_entry`, 0x9000 handoff page —
  load-bearing for QEMU `-kernel` and GRUB `multiboot`.
- `Interrupt`/`X86::Init1Interrupts`/`Asic::Init1/Init3`/`NTimerQ::Init1`/
  `ResetEntry` (kernelhwsrv, frozen repo); PIC shadow-mask rule: new IRQ
  bindings use `QemuIrq::Enable` direct RMW, never `Interrupt::Enable`.
- `Asic::Init3` mount order (AHCI dev 4 part 0 = C:, module dev 8 = X:) and
  `partlba=2048` baked into `tools/mkgrubiso.py`/MBR layout.
- `QEMU_RAM_SIZE` 128M heap sizing (`inc/qemu_config.h`); `HwInit0`
  `__init_array` walk must stay idempotent.
- EUSER.DLL prebuilt 2503 ordinals — never rebuilt, never grown; new client
  surface goes through IPC function numbers, not new ordinals.
- EFILE guard-only single-owner model (no trampoline, ever).

**User decisions first:** D1 (commit authorization), D2 (freeze list).

**Unattended overnight:** yes — hygiene file moves, `.gitignore`, baseline
strict run (~2 h wall: selfcheck ~900 s + retire ~600 s + 8 boots).

**Go/no-go gate:** GO to P1 only with strict-skip 11/11 PASS on a stable,
uninterrupted run + retire 4/4 + fullchain 14/14 + `git status` clean except
the intentional preservation tarball. NO-GO: any SKIP/FAIL, or QEMU slot
contention (re-run isolated, do not proceed on 124).

## P1 — Delete the bypassed shims (genuine completeness, real deletion)

**Order:** second — shrinks the tree before new features, and keeps the 11/11
meaningful as "genuine-only boot path".

**Deliverables (files/gates), following PLAN_SURVEY_SHIMS deletion order:**
1. Delete stale artifacts by path (no `make clean`): `qemu-port/PORTFILES.RV.E32`,
   `build/user/PORTFILESRV.EXE`, `build/**/r1_fs_*.o`, stale `build/*winsrv*`.
2. Drop orphaned rules: `user/user.mk:354-362` r1_fs rules; delete
   `services/r1_service_host.{cpp,h}`, `services/r1_service_state.h`.
3. Port the gate check off the shim header first: rewrite the static opcode
   check in `tools/r1_genuine_retire_test.py:46-48,127` to compare
   `user/ipc.c` PortFile ops against genuine `TFsMessage` directly; verify
   `r1_genuine_retire_test.py` still 4/4; then delete `fs/r1_fs_*` (13 files)
   and `tests/r1_fs_{core,channel}`.
4. Remove the Makefile opt-in block (`Makefile:341-351`) and
   `variant/r1_fs_channel.{cpp,h}` together with step 3.
5. Lua: per D4 — either add a gate for `LUA.E32` then decide
   `ports/lua/e32_shim.c`, or remove the app. Default if no decision: leave
   Lua untouched (ungated, harmless).
6. Cosmetic: stale portfilesrv/svcmgr comments in `user/user.mk` (lines 51,
   61, 204, 403, 623, 1297, 1314).
7. New gate **G12** `tools/r1_shim_residue_sweep.py`: static, <5 s, no QEMU —
   assert retired files absent, no live references, orphans deleted or
   whitelisted; insert first in `r1_final_verify.py` DEFAULT_TESTS
   (fail-fast, seconds-cheap).
8. Never touched (freeze): exec 81/269/270 hooks; `genuine/ewsrv/ewsrv_shims.cpp`
  + HAL shim (legitimate x86 glue); netstack (authored exception);
  `ports/doom/e32_shim.c` (4/11 doom gates link it — last, only if DOOM moves
  to genuine EUSER, not planned).
9. Commit point: D3 authorization; one commit per deletion step so each is
   individually revertible (`git show <sha>:<path>` rollback).

**User decisions first:** D3 (deletion commit authorization), D4 (Lua).

**Unattended overnight:** yes — each step runs plan → pre-gate (real, not
dry-run) → apply with token → snapshot → incremental `make` → per-phase gate
→ strict-skip re-run. Any non-PASS auto-rolls back. Estimated several strict
runs; fits an overnight window.

**Go/no-go gate:** after each step and at phase end — G12 PASS + retire 4/4 +
fullchain 14/14 + strict-skip 11/11. NO-GO: any FAIL/SKIP, `kernelhwsrv/`
porcelain non-empty, or a plan reporting `AMBIGUOUS` ops.

## P2 — GUI milestone on genuine EWSRV + edisp (pixel proof)

**Order:** third — the user wants GUI back via genuine EWSRV+UIKON "where
possible"; the surveys fix what "possible" means here: upstream EWSRV in this
snapshot is text-only and no UIKON/GDI sources exist locally, so the honest
milestone is genuine EWSRV desktop + edisp LFB pixels with one port-defined
blit opcode, labeled port-bridge, not "genuine GDI".

**Deliverables (files/gates) per PLAN_SURVEY_GUI_MILESTONE:**
- `tools/r1_ws_e32_draw_test.py` — boots genuine EWSRV.E32 + `WSE32DRAW.E32`,
  requires all `WSDRAW:` markers (Connect/Create/FillRect r=0, text, KEY,
  DONE) with 0 faults + QMP screendump whose colored-pixel count exceeds the
  EWSRV desktop-only baseline (IPC markers authoritative; screendump verified
  separately per EWSRV_CLIENT_BLOCKER caveat).
- One port-defined opcode in `genuine/ewsrv/patched/ws_main.cpp` `ServiceL`
  switch + mirrored client call in `user/ws_e32_client.c` — RGBA rect blit
  into the LFB via the genuine software blit path (LFB already U/S=1 in user
  PDs). `kernelhwsrv/` untouched; patched copies only.
- Point one GUI app (e.g. `guihello.e32`) at the pixel path as the demo.
- If D5 selects import: also **G13** `tools/r1_genuine_ewsrv_gdi_test.py`
  (SKIP-clean when genuine GDI sources absent) — kept out-of-band from
  DEFAULT_TESTS until assets exist so the 11/11 wall time does not regress.
- Remove the hand-written `"!"` retry fallback in `PortSessionCreate`
  (`variant/e32load.cpp:1434-1443`) once UTF-16 name lookup is proven, and
  re-run the ewsrv gate (PLAN_SURVEY_WSRV_NAME step 3).
- Docs: update `docs/PLAN_SURVEY_GUI_MILESTONE.md` status + GENUINE_PROGRESS.

**User decisions first:** D5 (purity axis: port-bridge now vs also external
UIKON/GDI import). The port-bridge path needs no further decision and is the
default for overnight.

**Unattended overnight:** yes — harness authoring + repeated QEMU boots
(~300 s each) + strict-skip. No human input needed for the port-bridge path.

**Go/no-go gate:** `r1_ws_e32_draw_test.py` PASS (markers + pixel diff) +
strict-skip 11/11 unchanged + retire 4/4. NO-GO: any marker FAIL, any fault,
or screendump regression vs baseline; then rollback the opcode, keep text-only
EWSRV gate green.

## P3 — Second game on the DOOM-proven bridges

**Order:** fourth — reuses exec 270 fbblit, con_tryread input, exec 68 time,
exec 240 AC'97, ord-915 markers; no new exec slots (table 0..270 full; 274/275
unsafe), no kernelhwsrv changes.

**Deliverables (files/gates) per PLAN_SURVEY_NEXT_GAME (Wolf3D shareware
recommended, D6):**
- Zero-code smoke first: stage a second WAD (Freedoom Phase 2 / PWAD) via
  `tools/mkgrubiso.py` on the existing DOOM.E32 — proves multi-asset staging.
- `ports/wolf3d/` engine tree + new `e32_platform.c` adapted from
  `ports/doom/e32_platform.c` (fbblit `src_bpp=8`, con_tryread, exec 68),
  reusing `e32_shim.c` / `e32_audio.h` unchanged; small IMF/AdLib synth
  modeled on `i_e32sound.c` MUS.
- Append-only `user/user.mk` block cloned from the DOOM block (own
  printf-slot defsym, own `--iat-off` distinct from 0x40000,
  `elf2e32.py --euser-import 915`).
- Asset staging: extend `tools/mkgrubiso.py` (`--wad2` or selectable WAD set);
  assets on AHCI C: only (X: RAMDISK opens fail KErrCorrupt); grow the disk
  image or make WAD set per-test if >16.8 MB needed (D6).
- New gate **G14** `tools/r1_wolf3d_test.py` cloned from `r1_doom_test.py`
  (SKIP-on-missing-assets, `WOLF:` markers, ord-915 bind, QMP screendump, no
  Exc/PanicKernExec/FAULT) + playtest/audio clones; add to DEFAULT_TESTS only
  once assets land, else keep out-of-band with the same SKIP discipline.
- Commit point: needs explicit authorization (D6 covers assets; commit still
  per standing rule).

**User decisions first:** D6 — game choice, asset supply/authorization,
disk-image growth.

**Unattended overnight:** partially — scaffold + user.mk block + SKIP-clean
G14 + all QEMU gates run unattended; the actual asset-gated boot runs only if
the IWAD was supplied before the night starts; otherwise the phase ends at
SKIP-clean + strict 11/11.

**Go/no-go gate:** with assets: G14 PASS + doom gates still PASS + strict
11/11. Without assets: G14 SKIP-clean (exit 0), strict 11/11 PASS — phase GO is
"pipeline ready", full GO needs D6 assets. NO-GO: any doom gate regression or
new exec-slot/table surgery.

## P4 — Real-PC / USB readiness (QEMU-first)

**Order:** fifth — pure variant-side glue; QEMU CI stays primary; no
overnight hardware exists, so CI-visible work is static + QEMU USB gates,
and the physical boot is a human-run off-CI harness.

**Deliverables (files/gates) per PLAN_SURVEY_HW_USB (D7):**
- `variant/qemu_pci.cpp`: recursive PCI-PCI bridge walk (bus > 0) + PCIe ECAM
  (MMCONFIG) fallback — variant-only, no kernelhwsrv touch.
- `variant/qemu_usb.cpp` + new polled USB mass-storage class driver
  (BOT/CBW + SCSI read10) over the existing UHCI/OHCI/EHCI/xHCI front-ends,
  mountable as FAT — the biggest USB gap. HID-only stays the baseline.
- Static in-CI audit `tools/r1_realhw_readiness_audit.py` (PCI bridge/ECAM,
  NVMe, APIC/ACPI, IRQ>16 gap checklist) — seconds-cheap, added early in
  DEFAULT_TESTS alongside G12.
- Off-CI harness **G15** `tools/r1_realhw_boot.py` (never in DEFAULT_TESTS):
  boot `build/symbian-grub.iso` on the physical PC, capture COM1 log, assert
  boot markers + USB enumeration markers.
- Human step (not overnight): burn `build/symbian-grub.iso` to CD-R per
  `docs/hw_worksheet.md`, boot the Pentium-4-era reference machine, record
  PASS/FAIL/HANG (GRUB menu, multiboot handoff, shell, `pci` list).
- Keep green: `r1_usb_ehci_test.py`, `r1_usb_ohci_test.py`,
  `tests/test_xhci_keyboard.py`, `r1_virtio_*`, `r1_qemu_*_test.py`.
- Docs: update `docs/hw_worksheet.md` with results.

**User decisions first:** D7 (scope: CD-R first medium, approve new variant
glue drivers).

**Unattended overnight:** yes for code + audit + QEMU USB gates; no for the
physical burn/boot (human, next day).

**Go/no-go gate:** in-CI — readiness audit PASS + all QEMU USB/virtio gates
PASS + strict-skip 11/11. Real-HW — G15 PASS on the reference PC is the
phase's full GO; overnight GO is CI-green only. NO-GO: any regression in
existing QEMU USB gates or a change that extends QEMU boot time (freeze:
`HwInit0` idempotency).

## P5 — Provenance audit, optional upstream import, bootable-ISO artifact

**Order:** last — closes the loop: measures genuine completeness, optionally
imports missing genuine sources, and produces the artifact.

**Deliverables (files/gates):**
- **G16** `tools/r1_genuine_source_audit.py`: static matrix of `kernelhwsrv`
  components vs `qemu-port/genuine/build_*.py`, sha256 of patched copies vs
  pristine sources, <5 s, no QEMU, early in DEFAULT_TESTS. Fix the
  `--name` path-traversal issue in `genuine_build.py` first (PROVENANCE risk,
  GENUINE_BUILD_REVIEW #2) — port-side fix, allowed.
- Genuine-ratio metric per PLAN_SURVEY_PROVENANCE §2 (byte ratio primary from
  `genuine/BUILD_MANIFEST.json`; line ratio secondary); keep BUILD_MANIFEST.json
  updated per build; a subsystem counts as genuine only when retire gate PASS
  *and* its shim files are deleted.
- If D8 approved: vendor upstream EPL sources under `genuine/imported/<area>/`
  (pristine + upstream hash in BUILD_MANIFEST.json; patched copies under
  `patched/`; per-directory license verification, skip unverifiable
  proprietary bits). UIKON/GDI is the highest-value target; ESOCK stays the
  authored exception (frozen EUSER ordinals make genuine ESOCK clients
  impossible). Single static PE2 image per loader limits (512 slots, one DLL,
  512 KiB buffer — cap raise is itself a D8 decision). Gate each import with
  `tools/r1_genuine_<area>_test.py` mirroring the ewsrv gate.
- **Artifact:** `make isogrub` → `build/symbian-grub.iso` (plus `iso`/`isofull`
  variants), verified by `r1_qemu_smoke.py` boot of the exact artifact; ISO
  carries the staged genuine chain + DOOM (and game 2 if D6 landed).
- Final evidence pack: fresh strict-skip 11/11 (+ G12/G16 + any added G13/G14)
  on the final tree, logs archived to `docs/`, `docs/FINAL_GATE_STATUS.md` and
  `docs/DOCS_INDEX.md` updated.
- Commit point: D9 — final acceptance and explicit commit authorization for
  the whole overnight delta.

**User decisions first:** D8 (import approval + license review + cap raise),
D9 (artifact acceptance + commit authorization).

**Unattended overnight:** yes for G16, manifest/metrics, ISO build + smoke,
final strict run. No for license review of imported sources (human) and for
final acceptance (human).

**Go/no-go gate:** G16 PASS + strict-skip green incl. all added static gates +
`r1_qemu_smoke.py` PASS on `build/symbian-grub.iso` + `git -C kernelhwsrv
status` clean + `git status` shows only intended files. That is the project
GO; D9 then authorizes commits.

## Phase dependency and overnight runbook

```
P0 baseline+freeze ──► P1 shim deletion ──► P2 EWSRV GUI pixel milestone
                                                  │
                                                  ├──► P3 second game (needs D6 assets)
                                                  │
                                                  └──► P4 real-PC/USB readiness (CI part unattended)
                                                           │
                                                           ▼
                                              P5 audit + import? + symbian-grub.iso artifact
```

Suggested overnight sequence (all unattended steps; human decisions D1–D9
resolved beforehand where blocking):
1. P0: hygiene + baseline strict run → freeze register doc.
2. P1: guarded deletion steps 1–4, 6–7 with gates between each → G12.
3. P2: `r1_ws_e32_draw_test.py` + blit opcode + `"!"` fallback removal → gate.
4. P3: wolf3d scaffold + SKIP-clean G14 (+ full G14 only if assets present).
5. P4: PCI bridge/ECAM + USB mass-storage glue + readiness audit + QEMU USB gates.
6. P5: G16 + metrics + `make isogrub` + smoke on the artifact + final strict run.
7. Morning report: per-phase gate verdicts, files changed, rollback tokens,
   open decisions still needing the user.

Human-only (never overnight): D-licensing review, real-PC CD-R boot (G15),
asset downloads, every commit/push.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. Every phase gate, go/no-go condition and evidence-pack line here that requires strict-skip 11/11 now requires the 15-row PASS. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

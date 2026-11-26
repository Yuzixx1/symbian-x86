# ROADMAP_SUMMARY — qemu-port P0–P5 on one screen

Synthesized 2026-11-25 from all 21 `docs/PLAN_*.md` (17 surveys + `PLAN_EXECUTION.md` + `PLAN_DECISION_REGISTER.md` + `PLAN_RECONCILIATION.md` + `PLAN_P0_COMMIT.md`) at HEAD `ee8490e`. Planning only: no code, no commit/push, no `make clean`, no QEMU; `kernelhwsrv/` read-only. Authority gate everywhere: `python3 tools/r1_final_verify.py --strict-skip` = 11/11 PASS (+ retire 4/4, fullchain 14/14).

## 0. DECISIONS RESOLVED (2026-11-25)

All six open decisions answered with the recommended defaults:

1. GUI retired — P2 is OUT OF SCOPE (not merely blocked); console+DOOM stays the standing UI and P2 is dropped from the run.
2. Game assets — FreeDM / free-license PWAD only (decision 7a); no id shareware, no downloads.
3. Blit path — exec-270 blit only; pixels only via exec 270 outside the server, no opcode inside patched `ws_main.cpp`.
4. Commits — no overnight commits or pushes; working tree only unless explicitly authorized.
5. Disk — per-test WAD swap (default) for the 16.8 MB headroom; no `FS_MB` grow.
6. Netstack — the authored `!SocketServer` netstack stays the permanent socket layer; no EPL import.

P3 is a FreeDM SKIP-clean scaffold only until a user-dropped asset arrives.

## 1. Phases P0–P5 (strictly ordered P0 → P5)

| Ph | Goal (one line) | Deliverables | Gate | Overnight autonomous? |
|---|---|---|---|---|
| P0 | Freeze baseline + kernel hooks; tree hygiene | `PLAN_EXECUTION.md`, `KERNEL_HOOK_FREEZE.md`, `.gitignore` rules, delete stray `qemu-port/9`, baseline evidence in `FINAL_GATE_STATUS.md` (WATCH archive move of 128 `WINSRV_DELETE_WATCH*` excluded — gated on the D-COMMITS decision, a landed commit) | strict 11/11 + retire 4/4 + fullchain 14/14 + clean `git status` | Yes |
| P1 | Real deletion of bypassed shims | delete `fs/r1_fs_*` (13 files), `services/r1_service_host.*`, `variant/r1_fs_channel.*`, stale artifacts; port retire-test opcode check off `fs/r1_fs_core.h` first; new static gate G12 | G12 + retire 4/4 + fullchain 14/14 + strict 11/11 after each guarded step | Yes (working tree only; commits need D3) |
| P2 | Pixel-proof GUI milestone on genuine EWSRV+edisp | `tools/r1_ws_e32_draw_test.py`, one port-defined blit opcode in patched `ws_main.cpp` + `ws_e32_client.c`, guihello demo, drop `"!"` fallback | WSDRAW markers + QMP screendump pixel diff + strict 11/11 | **No** — frozen 2026-11-24 GUI-retirement decision; needs re-authorization (decision 3) |
| P3 | Second game on DOOM-proven bridges (reuse exec 270/240/68/86/107/108, ord 915; no free exec slots) | game port tree (`ports/wolf3d/` or FreeDM/PWAD staging), cloned `user/user.mk` block, G14 SKIP-on-missing-assets, `mkgrubiso --wad2` | G14 PASS with assets / SKIP-clean without + doom gates + strict 11/11 | Partial — SKIP-clean scaffold only unless assets pre-supplied (D6) |
| P4 | Real-PC/USB readiness, QEMU-first, variant-side glue only | recursive PCI bridge walk + PCIe ECAM, polled USB mass-storage driver, in-CI readiness audit, off-CI G15 harness | audit + QEMU USB/virtio gates + strict 11/11; G15 on reference PC = full GO | Yes for CI part; CD-R physical boot is human, next day |
| P5 | Provenance audit + bootable-ISO artifact | G16 source audit, genuine-ratio metrics from `BUILD_MANIFEST.json`, `make isogrub` → `build/symbian-grub.iso` + smoke, evidence pack | G16 + static gates + strict green + ISO smoke + `kernelhwsrv/` clean | Yes (license review + final acceptance are human) |

## 2. DECISIONS-NEEDED (options + recommended default)

1. **D1 P0 docs commit** — (a) authorize the named docs-only list in `PLAN_P0_COMMIT.md`; (b) leave tree dirty. Default: (b) overnight; (a) only on explicit request, never `git add -A`.
2. **D2 kernel-hook freeze** — confirm exec 81/269/270, boot contract, mount order, EUSER 2503 ordinals, EFILE guard-only as NO-GO without approval. Default: confirm (freeze holds regardless).
3. **P2 run at all?** — (a) console+DOOM stays the standing UI, drop P2 from the overnight run; (b) re-authorize the port-bridge milestone. Default: (a) per `PLAN_RECONCILIATION.md` C3 (retirement is the latest explicit decision).
4. **P2 blit opcode inside patched genuine `ws_main.cpp`** (only if 3=b) — (a) sanction one labeled port-bridge opcode + sha256 in manifest; (b) forbid, pixels only via exec 270 outside the server. Default: (b).
5. **D3 P1 deletion commits** — one revertible commit per step after green gates. Default: no commits overnight.
6. **D4 LUA.E32** — gate it first, or remove the app. Default: leave untouched (ungated, harmless).
7. **D6 game + assets** — (a) strict free-license-only: FreeDM / BSD-CC0 PWAD on existing DOOM.E32; (b) admit id shareware WL1 as a never-committed exception; (c) drop P3. Default: (a); Wolf3D engine only with a user-dropped libre pack. Disk: 16.8 MB free — freedoom2 (28.8 MB) needs per-test WAD swap (default) or `FS_MB` grow.
8. **D7 real-PC scope** — (a) CD-R first medium + variant-only PCI/ECAM + USB mass-storage, APIC/ACPI closed; (b) QEMU-only. Default: (a); never unlock `kernelhwsrv/`.
9. **D8 external import** — vendor upstream EPL UIKON/GDI/ESOCK under `genuine/imported/` with per-directory license check + 512 KiB cap raise. Default: defer; ESOCK never as EUSER client — authored netstack stays permanent.
10. **D9 final acceptance** — `build/symbian-grub.iso` as the artifact + strict 11/11 + commit authorization for the whole delta. Default: accept when P5 green; commits only when explicitly asked.

## 3. Three hard truths from the surveys

1. **Genuine ESOCK and UIKON/GDI do not exist in this snapshot** — no socket/commsdb/protocol sources, zero socket ordinals in the frozen 2503-export EUSER.DLL, and no UIKON/EIKON/GDI/FBSRV/graphical winsrv in `kernelhwsrv/`, `sdk/`, or the preservation archive. Only route: external EPL import as one static PE2 image (512 slots / 1 DLL / 512 KiB loader caps); the authored `!SocketServer` netstack is the permanent socket layer.
2. **Asset policy is free-license-only with no download path** — `DOOM_PORTING.md` §4 excludes id/shareware assets (incl. Wolf3D WL1 and minizork); assets are user-supplied into candidate paths only; only Freedoom (BSD-3) exists locally; 16.8 MB disk headroom after Freedoom1; assets must stage on AHCI `C:` (X: RAMDISK opens fail KErrCorrupt).
3. **Live exec hooks are frozen, not deletable** — exec 81 (KERN-4 EFILE guard), exec 269 (cp4 gate), exec 270 ops 2/3/4 incl. PTRINJ (pointer gate + DOOM fbblit) are load-bearing, gate-pinned bridges; `fs/r1_fs_core.h` is itself a retire-gate input, so the opcode check must be ported off it before `fs/` deletion.

## 4. Starts tonight with zero user decisions

- P0: hygiene file moves + `.gitignore` + `KERNEL_HOOK_FREEZE.md` + baseline strict run (11/11, 4/4, 14/14; QEMU-requiring at execution time). The WATCH archive move (128 `WINSRV_DELETE_WATCH*`) is excluded from this zero-decision list — gated on the D-COMMITS decision (a landed commit).
- P1: guarded deletions steps 1–4, 6–7 → G12 (plan → real pre-gate → apply + snapshot → incremental `make` → per-phase gates; auto-rollback on any non-PASS); Lua untouched; working tree only, no commit.
- P3: SKIP-clean scaffold (port tree + `user.mk` block + G14) — no assets needed to be SKIP-clean.
- P4 CI part: PCI bridge/ECAM + USB mass-storage glue + readiness audit + QEMU USB gates (QEMU-requiring at execution time).
- P5 static part: G16 + manifest metrics + `make isogrub` + ISO smoke (QEMU-requiring at execution time) + final strict run.
- Never overnight: any commit/push, P2, asset-gated P3, downloads/license review, CD-R physical boot, `kernelhwsrv/` edits, `make clean`.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. The synthesis header's "Authority gate everywhere" line and the per-phase acceptance columns carry the old count. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

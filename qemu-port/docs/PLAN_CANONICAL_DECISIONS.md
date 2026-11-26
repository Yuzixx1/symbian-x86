# PLAN CANONICAL DECISIONS — the one list the user must answer

Date: 2026-11-25. Merges [ROADMAP_SUMMARY.md](ROADMAP_SUMMARY.md) (10 items),
[PLAN_DECISION_SHEET.md](PLAN_DECISION_SHEET.md) (6 items) and
[PLAN_RECONCILIATION.md](PLAN_RECONCILIATION.md) (5 required decisions).
Consistency check: the two user-facing docs do NOT contradict each other —
"10 vs 6" is scope, not conflict (the sheet lists only the 6 decisions that
block the overnight run; the other 4 are confirmations or human acceptance).
P2, P3 and ESOCK stances agree in both: P2 stays retired unless re-authorized;
P3 is a SKIP-clean scaffold only without asset decisions; the authored
`!SocketServer` netstack is the permanent socket layer. `kernelhwsrv/`
read-only; no commit/push; no `make clean`; no QEMU run for this review.

## 1. Canonical decisions (silence = recommendation applies)

- **D1 Commits overnight?** (roadmap 1/5/10; sheet D-COMMITS) — Rec: none. Working tree + green gates only; authorize commits in the morning report. Never `git add -A`.
- **D2 Does P2 (GUI on genuine EWSRV) run at all?** (roadmap 3; sheet D-GUI) — Rec: no. The 2026-11-24 GUI retirement (`94ce9c9`) stands; console shell + DOOM/Freedoom is the UI.
- **D3 Blit path, only if D2 = yes** (roadmap 4; sheet D-BLIT) — Rec: exec-270 fbblit outside the server only; no port opcode inside patched genuine `ws_main.cpp`.
- **D4 Second game + asset license** (roadmap D6; sheet D-GAME) — Rec: FreeDM / BSD-CC0 PWAD on the existing `DOOM.E32`; Wolf3D WL1 shareware only via an explicit written amendment of `DOOM_PORTING.md` §4; otherwise drop P3.
- **D5 freedoom2.wad (28.8 MB) vs 16.8 MB headroom, only if D4 proceeds** (roadmap D6 disk note; sheet D-DISK) — Rec: per-test WAD swap; do not grow `FS_MB`.
- **D6 ESOCK stance** (roadmap D8; sheet D-ESOCK) — Rec: accept the authored netstack as the permanent documented socket ABI; close "genuine ESOCK" as a goal; never as an EUSER client; never unlock `kernelhwsrv/` for socket ordinals.
- **D7 Kernel-hook freeze** (roadmap D2) — Rec: confirm. Exec 81/269/270, boot contract, EUSER 2503 exports, EFILE guard-only (NO-GO without approval).
- **D8 LUA.E32** (roadmap D4) — Rec: leave untouched (ungated, harmless).
- **D9 Real-PC scope** (roadmap D7) — Rec: CD-R as first medium, QEMU-first; variant-only PCI/ECAM + USB mass-storage; APIC/ACPI stay closed.
- **D10 External EPL import of absent genuine components (UIKON/GDI/ESOCK)** (roadmap D8) — Rec: defer. If ever wanted: one static PE2 image under `genuine/imported/` with per-directory license check + 512 KiB cap raise.
- **D11 Final acceptance artifact** (roadmap D9) — Rec: accept `build/symbian-grub.iso` when P5 is green + strict 11/11; commit the whole delta only on explicit request.

## 2. Starts tonight with zero decisions (working tree only, green gates, no commits)

- P0: hygiene file moves, `.gitignore`, `KERNEL_HOOK_FREEZE.md`, baseline strict 11/11 + retire 4/4 + fullchain 14/14 (strict-skip run is QEMU-requiring at execution time). The WATCH archive move (128 `WINSRV_DELETE_WATCH*`) is NOT in this zero-decision list — it is gated on the D-COMMITS decision (a landed commit).
- P1: guarded deletion steps 1–4 and 6–7 → new static gate G12 (Lua untouched; auto-rollback on any non-PASS).
- P3: SKIP-clean scaffold included (port tree + `user.mk` block + G14 SKIP-on-missing-assets); asset-gated P3 blocked (needs the D4 asset decision).
- P4 CI part: PCI bridge/ECAM walk, USB mass-storage glue, readiness audit, QEMU USB gates (QEMU-requiring at execution time).
- P5 static part: G16 provenance audit, manifest metrics, `make isogrub` + ISO smoke (QEMU-requiring at execution time), final strict run.

## 3. Explicit non-goals

- No commit/push; no P2; no asset-gated P3; no downloads or license review; no physical CD-R boot; no `kernelhwsrv/` edits; no `make clean`.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. The D11/P0 acceptance wording below still names a strict 11/11 as its artifact bar. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

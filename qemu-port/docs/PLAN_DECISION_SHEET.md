# PLAN DECISION SHEET — the six decisions that block Planning -> Execution

**Date:** 2026-11-25. Sources: [PLAN_RECONCILIATION.md](PLAN_RECONCILIATION.md),
[PLAN_DECISION_REGISTER.md](PLAN_DECISION_REGISTER.md), [PLAN_EXECUTION.md](PLAN_EXECUTION.md).
Answer the six D-* below (silence = recommended default applies) and overnight work can start.

## D-GUI — does P2 (GUI on genuine EWSRV) run at all?
- **Question:** the 2026-11-24 decision retired the winsrv/GUI desktop ("console shell + DOOM are the UI for now"). Re-authorize P2, or keep the GUI retired?
- **Options:** (a) keep GUI retired — P2 out of the overnight run, kept as a documented option; (b) re-authorize P2 port-bridge pixel milestone on genuine EWSRV.
- **Default:** (a) — retirement is the most recent explicit user choice.
- **Consequence:** (a) console+DOOM only overnight, no EWSRV pixel work; (b) P2 runs unattended, requires D-BLIT, and dilutes gate 5's "genuine EWSRV" claim unless labeled port-bridge.

## D-GAME — second game: which asset path?
- **Question:** P3 needs a second game; `DOOM_PORTING.md` §4 forbids id shareware WADs (free-licensed only).
- **Options:** (a) FreeDM or a small BSD/CC0 PWAD on the existing DOOM.E32 (policy-compliant, proven); (b) Wolf3D shareware WL1 as a user-supplied, never-committed exception — only with an explicit policy amendment; (c) no second game, drop P3.
- **Default:** (a); Wolf3D engine only with a verified libre asset pack the user drops in.
- **Consequence:** (a) zero-code smoke on multi-WAD staging, then real port work; (b) contradicts the written asset policy unless amended in writing; (c) P3 removed, overnight scope shrinks.

## D-BLIT — how do pixels reach the screen? (only if D-GUI = b)
- **Question:** one port-defined opcode inside patched `genuine/ewsrv/ws_main.cpp` is authored logic in genuine code — sanction it, or stay outside the server?
- **Options:** (a) exec-270-only: pixels via exec 270 fbblit outside the server (DOOM-style), EWSRV stays text-only; (b) one port opcode in the patched copy as labeled port-bridge glue, sha256 in BUILD_MANIFEST.json (G16).
- **Default:** (a) unless EWSRV-composited pixels are explicitly wanted.
- **Consequence:** (a) genuine EWSRV claim stays clean, no patched-server risk; (b) gate 5 "genuine EWSRV" then refers to the unmodified text path only, opcode must be honestly labeled.

## D-COMMITS — any commits overnight?
- **Question:** standing rule is no commit/push unless explicitly asked. Authorize any overnight commits?
- **Options:** (a) no commits at all — working-tree changes + green gates only; (b) one docs-only commit with an explicit named file list; (c) per-step deletion commits in P1, explicit paths only (never `git add -A`; the 2.4 MB tarball stays untracked).
- **Default:** (a) — user authorizes commits in the morning report.
- **Consequence:** (a) tree stays dirty, rollback via `git show <sha>:<path>`; (b)/(c) need explicit path lists and are individually revertible.

## D-DISK — freedoom2.wad (28.8 MB) vs 16.8 MB headroom (only if D-GAME proceeds)
- **Question:** stage the second WAD how?
- **Options:** (a) per-test WAD swap — stage Phase 2 instead of Phase 1; (b) grow `FS_MB` in `tools/mkgrubiso.py` (one-number change).
- **Default:** (a) — avoid changing the shipped image layout.
- **Consequence:** (a) no image-layout change, swap flag in mkgrubiso; (b) bigger image, every boot re-stages, layout change ships to all users.

## D-ESOCK — networking: permanent exception or drop?
- **Question:** genuine ESOCK sources don't exist in the frozen tree and EUSER.DLL's ordinal table can't grow (kernelhwsrv read-only). What happens to sockets?
- **Options:** (a) accept the authored netstack/`!SocketServer` as the permanent documented socket ABI — close "genuine ESOCK" as a goal; (b) drop networking entirely; (c) import upstream ESOCK as a static server behind the same ABI (provenance cost, no ABI change). Never unlock kernelhwsrv for socket ordinals.
- **Default:** (a).
- **Consequence:** (a) every standing rule stays intact, netstack never deleted; (b) removes a working feature; (c) adds import/provenance work for zero ABI gain.

## Proceeds overnight with NO decisions (unattended, working-tree only, green gates, zero commits)
- **P0:** hygiene file moves (excluding the WATCH archive move — blocked, see below), `.gitignore`, KERNEL_HOOK_FREEZE register, baseline strict 11/11 + retire 4/4 + fullchain 14/14.
- **P1:** guarded deletion steps 1–4 and 6–7 + new static gate G12 (Lua left untouched by default; G12's residue sweep must whitelist `ports/lua/e32_shim.c` and the `user.mk` LUA.E32 wiring pending D4).
- **P4 (CI part):** PCI bridge/ECAM walk, USB mass-storage glue, readiness audit, QEMU USB gates.
- **P5 (static part):** G16 provenance audit, manifest/metrics, `make isogrub` + smoke on `build/symbian-grub.iso`, final strict run.
- **Blocked without decisions:** every commit (D-COMMITS), the WATCH archive move (its documented precondition is a landed P0 commit, which D-COMMITS default (a) forbids — not part of the no-decision overnight list), all of P2 (D-GUI + D-BLIT), the asset-gated half of P3 (D-GAME + D-DISK — otherwise P3 delivers a SKIP-clean scaffold only). `kernelhwsrv/` stays read-only; no `make clean`; the QEMU-using gates — baseline strict-skip run, QEMU USB gates, and the ISO smoke — require QEMU at execution time (this sheet itself runs no QEMU).

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. The P0 sheet's baseline line names strict 11/11 alongside retire 4/4 and fullchain 14/14. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.

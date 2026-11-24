# GO / NO-GO decision record — genuine-userspace pivot

**Date:** 2026-11-24. **Basis:** read-only consolidation of the verification
reports listed below; no QEMU run, rebuild, source edit, commit or push was
performed for this record. `kernelhwsrv/` untouched. Reports were written at
different times, so where they disagree the most recent reconciled state is used
(and the conflict is called out in the caveat column).

## 1. Subsystem verdicts

| Subsystem | Evidence | Verdict | Caveats |
|---|---|---|---|
| **EUSER** | [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) §1, [GENUINE_USERSPACE.md](GENUINE_USERSPACE.md) §6 | **PASS** | Prebuilt genuine `EUSER.DLL`, 2503 exports, resident every boot; retire gate PASS. Static `euser_shim.cpp` removal is a separate, unstarted step. |
| **EFSRV / F32** | `tools/r1_genuine_efsrv_test.py` **PASS**; `tools/r1_genuine_retire_test.py` **efsrv PASS** (was FAIL), **estart PASS**; [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) §1–§2 | **PASS** | Former sole blocker now cleared. F32CLIENT uses genuine UTF-16 `!FileServer`; full genuine EFILE with **464 binds**, `File server thread registered: 1`, `EFILE: port-shim: 0`, `F32CLIENT: IPC round trip OK` **=2**, **0 FAULT/Panic**, no exit 111. `r1_genuine_efsrv_test.py` PASS and the EFSRV retire gate **flipped FAIL → PASS** (ESTART PASS, smoke PASS). The earlier `-15`/duplicate-owner analysis in [EFILE_IPC_CONNECT_ANALYSIS.md](EFILE_IPC_CONNECT_ANALYSIS.md) is now historical/superseded. |
| **EWSRV** | [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) §1, [BOOT_CHAIN_VERIFICATION.md](BOOT_CHAIN_VERIFICATION.md) §2, [FINAL_VERIFY_REVIEW.md](FINAL_VERIFY_REVIEW.md) | **PASS** | Boot-time markers + gate PASS, 212 binds, screendump renders; `!E32WindowServer` external client window fix LANDED. Earlier screendump FAIL was a harness duplicate-launch artifact, now fixed. **EWSRV/EUSER marker drift fix LANDED: `RESULT PASS`, 4 passed, smoke PASS** on the rebuilt tree. |
| **ESTART / ESHELL** | [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) §1, [BOOT_CHAIN_VERIFICATION.md](BOOT_CHAIN_VERIFICATION.md) §2, [ABI_AUDIT.md](ABI_AUDIT.md) §3 | **PASS** | Gate PASS, boot markers, `E32Main returned 0`, 0 faults; sret-ABI images rebuilt. Some shell command paths (TOKEN/Mid, SYSINFO/TIME) remain PENDING per ABI audit §6; eshell PS page fault fix still IN FLIGHT (does not block the gate). |
| **HAL / DM** | [HAL_DM_REGRESSION.md](HAL_DM_REGRESSION.md), [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) §1 | **PASS** | `prove_hal_dm.py` 15/15 (`ECPU=2`, 4096 page, `GetAll=121`, DM `r=0`/`-260`), no faults. Not a retire gate; images are `-fshort-wchar`-stale but functional on exercised paths; rebuild is hygiene only. |
| **DOOM / Freedoom** | [DOOM_INTEGRATION_VERIFICATION.md](DOOM_INTEGRATION_VERIFICATION.md) §4–§6 | **PASS** | Main/audio/playtest PASS, 27.5 MiB WAD, frames advance cleanly, 0 faults, `EKA2_CRISP_STOP` absent. `r1_doom_multikey_test.py` prints `FAIL` on `diagonal_2` only — documented harness marker-attribution artifact, not a game bug. |
| **Boot chain** | [FULLCHAIN_VERIFICATION.md](FULLCHAIN_VERIFICATION.md), [BOOT_CHAIN_VERIFICATION.md](BOOT_CHAIN_VERIFICATION.md) | **PASS** | Full in-order chain in **one boot**: 14/14 rows, all four genuine components `r=0`, 0 `Exc`/`PanicKernExec`/`FAULT`, fallback absent. Result is against the staged ISO; rebuild fresh and re-run before treating it as current. |
| **Pointer / client** | [POINTER_BRIDGE_REVIEW.md](POINTER_BRIDGE_REVIEW.md), [ABI_AUDIT.md](ABI_AUDIT.md) §6 | **IN FLIGHT** | Bridge review **SAFE** — layout/CS/race correct, delivery to `RequestEvent` exercised, no kernel change. But `CEvent::RunL()` calling EUSER ord 1868 `TRawEvent::Pos()` is only strongly implied, **not directly asserted**; add a post-`Pos()` marker or injected-coordinate assertion before calling the sret path proven. |

**Retire-gate status: all four gates now PASS simultaneously.** EUSER, EFSRV/F32,
EWSRV and ESTART all read PASS on one reconciled tree: EWSRV/EUSER marker fix
landed (`RESULT PASS`, 4 passed, smoke PASS); EFSRV **464 binds, port-shim=0,
`IPC round trip OK`×2, 0 FAULT/Panic** (one transient EFSRV FAIL occurred during
a concurrent rebuild; an immediate re-run passed and is the reconciled result).
The former NO-GO basis — EFSRV blocker plus EWSRV/EUSER marker drift — is
therefore cleared.

## 2. Recommendation — **CONDITIONAL-GO (retirement gated on the conditions below)**

**All four retire gates PASS; do not retire any shim or declare the pivot
complete until the remaining in-flight items are confirmed on a stable tree.**
EFSRV/F32 is PASS (464 binds, thread registered=1, port-shim=0, `IPC round trip
OK`=2, 0 FAULT/Panic, no exit 111) and the EWSRV/EUSER marker fix has landed, so
the previous NO-GO blockers are cleared. The verdict is CONDITIONAL-GO rather
than GO because the following items remain unverified.

Exact remaining conditions for **full GO**:

1. **Guard-vs-trampoline review outcome (`variant/e32load.cpp`)** — guard-only
   IPC works (`port-shim=0`); the trampoline path reports `port-shim=2` and breaks
   it. Must be resolved so the genuine path stays at port-shim=0.
2. **Full-chain re-verification** — re-run the boot chain on a stable, freshly
   built tree; one complete, uninterrupted run (do NOT SIGTERM) with every
   marker stable and all four components `r=0`.
3. **eshell PS** — the ESHELL PS page-fault fix must be confirmed PASS (gate is
   already PASS; this is the outstanding shell-command path).
4. **Complete `r1_final_verify.py --strict-skip` PASS on a stable tree** — every
   individual gate must read PASS with no skips.

Supporting state is ready: security audit **SAFE**; precommit checklist ready;
disk-guard tooling landed; DOCS_INDEX **135/135**. Even at full GO, **commit /
push only on explicit user request**.
`r1_shim_retire_apply.py` must never be invoked while any gate is FAIL/SKIP.

Note: `r1_final_verify.py` itself has two known false-PASS paths
([FINAL_VERIFY_REVIEW.md](FINAL_VERIFY_REVIEW.md) §3 items 1–2: stale DOOM ISO and
nonzero-rc verdict override). Treat its OVERALL PASS as advisory; use
`--strict-skip` and require every individual gate to read PASS.

## 3. Next commands once the remaining fixes land

```bash
cd qemu-port && python3 tools/r1_final_verify.py --dry-run
python3 tools/r1_final_verify.py --strict-skip
```

- Confirm the remaining in-flight items on a stable, freshly built tree:
  `variant/e32load.cpp` resolves to guard-only IPC (`port-shim=0`); the full boot
  chain re-runs clean; eshell PS is PASS; one complete, uninterrupted
  `--strict-skip` run is stable under load. EWSRV/EUSER markers are already
  confirmed PASS on the rebuilt tree.
- Guarded retirement **only when every gate reports PASS**: run
  `tools/r1_shim_retire_apply.py` per [SHIM_RETIREMENT_PLAN.md](SHIM_RETIREMENT_PLAN.md)
  §4 (preview → `--shim <name> --apply --confirm <TOKEN>`; it re-runs the real
  gate, refuses FAIL, rolls back on any failure).
- **Commit / push only on explicit user request**, staging by path per
  [CHANGE_INVENTORY.md](CHANGE_INVENTORY.md) §4.

## 4. Do not do

- No `kernelhwsrv/` edits (read-only).
- No `git add -A` — the archive tarball `symbian-x86-archive-56fbaff.tar.gz` is
  untracked and **not gitignored**; stage by path only.
- No commit or push without an explicit user request.
- No shim retirement before every gate reports PASS **and** all four
  CONDITIONAL-GO conditions above are cleared (all four retire gates already
  PASS, including EWSRV/EUSER markers on the rebuilt tree).

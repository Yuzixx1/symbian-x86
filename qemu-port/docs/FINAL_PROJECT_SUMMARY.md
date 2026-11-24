# FINAL PROJECT SUMMARY — genuine-userspace pivot (ready-to-fill)

**Snapshot:** 2026-11-24 03:22 EET, reconciled from the existing status docs and log tails.
**Nature:** read-only synthesis. No QEMU, build, gate, `kernelhwsrv/` edit, commit or push was made
for this page. **This is a status snapshot, NOT authorization** to retire shims, commit or push.
Where sources disagree, the newest reconciled evidence wins; the conflict is called out below.

## 1. What we built

- A bootable PC OS around the **genuine EKA2 kernel**, with **genuine userspace compiled from
  `kernelhwsrv/`**: EUSER (prebuilt DLL, 2503 exports), EFSRV/F32/EFILE, EWSRV, estart/etshell,
  HAL, domain manager and EFAT32. Boot order: EUSER → EFILE/EFSRV → ESTART → EWSRV → ESHELL,
  launched on a supervisor DThread from `Asic::Init3` with `ECapabilityProtServ` for `!`-names.
- **GRUB is the one explicit exception** to the genuine-source rule; the deliverable is
  `qemu-port/build/symbian-grub.iso` (`make isogrub`). All other authored code is a port shim for
  x86/PC only (PE2 imports 16→512, ProtServ grant, lazy VBE, virtual FS `Backend`, boot plumbing)
  — never a replacement for real Symbian logic. DOOM/Freedoom is the integration proof;
  `EKA2_CRISP_STOP` stays reverted.

## 2. Preservation

- Archive point `56fbaff` is intact: branch **and** annotated tag `archive/handwritten-r25`
  dereference to it, plus `symbian-x86-archive-56fbaff.tar.gz` (2,481,838 B, 983 entries, git
  source only). `HEAD = 56fbaff`, tree dirty; nothing committed or pushed since the pivot.
- Binaries are **not** archived — they are reproducible with `make`. Never use `git add -A`:
  the tarball is untracked and not gitignored. See `PRESERVATION_AUDIT.md`, `CHANGE_INVENTORY.md`.

## 3. Verification — last known verdicts

- `tools/r1_final_verify.py` is the authoritative 11-test gate; **no authoritative full run yet**
  (runbook ready). Two known false-PASS paths require `--strict-skip` plus every individual gate
  PASS. Fast pre-flight: `r1_os_selfcheck.py`.
- **Full chain** last authoritative run **FAIL 11/14** — failing rows: ESTART launch, genuine file
  server up with no fallback, and no Exc/Panic/FAULT. An older 14/14 PASS exists against the staged
  ISO, but has not been reproduced on the current tree.
- **EFILE acceptance (9 criteria):** honest tool result **FAIL 5/9**; the `-15` marker fix is GREEN
  for honesty only, not EFSRV health. The shim/minimal-server `r=0 → OK`×2 path passes honestly.
- **DOOM/Freedoom** (last verification): **PASS** — main/audio/playtest, 27.5 MiB WAD, clean frames
  past the old cap, AC97 audio, real controls and multi-key holds, clean exit, zero
  Exc/Panic/FAULT/#PF. `r1_doom_multikey_test.py` prints FAIL on `diagonal_2` only — a documented
  marker-attribution harness artifact, not a game bug. No re-run after the 03:06–03:15 rebuilds.
- Security audit **SAFE** (no credentials/injection); preservation **PASS**.

## 4. Current blockers

- **EFILE re-entry → `r=-15`:** 3 starts / 2 `E32Main returned 0` cause the owning `DServer` to
  detach sessions, so full-EFILE `SessionPath` is `r=-15`×2, `OK`×0. The EFSRV/F32 retire gate is
  therefore an honest **FAIL 3/4** (EWSRV/ESTART/EUSER PASS) with strict acceptance **FAIL 5/9**.
  Guard-only IPC still works (`port-shim=0`, no faults); the trampoline must never return
  (`port-shim=2`, `#PF`, no IPC). Shim suppression is IN FLIGHT; no clean prove yet.
- **Caller UID3 re-stamp:** `pack_multi.py --uid3` is validated GREEN, but callers are pending —
  ESHELL still `0x10000111`, ESTART blocked with `-11`. Apply `GUARD_ESTART_BLOCK_ANALYSIS.md` §2,
  then re-run the full chain.
- **Overall snapshot: 10 GREEN / 3 IN FLIGHT / 2 FAIL / 1 NOT DONE** (`FINAL_GATE_STATUS.md`,
  03:22). Overall **NOT DONE**. Older docs (`GENUINE_PROGRESS.md`, `GO_NO_GO.md`,
  `PRECOMMIT_AUDIT.md`) still record EFSRV PASS / CONDITIONAL-GO / NO-GO on an earlier tree —
  they are superseded by the newer honest `-15` verdict and must be reconciled before GO.

## 5. NOT done

- **No shim retired** — 0 READY / 4 BLOCKED; guarded apply only when every gate PASS on a stable
  tree, never with any FAIL/SKIP.
- **No commit, no push**, no gate re-run, no authoritative `--strict-skip` PASS, no uninterrupted
  clean EFILE prove. User authorization is required for retirement, commit and push.

## 6. How to finish

1. Apply the caller UID3 re-stamp (`GUARD_ESTART_BLOCK_ANALYSIS.md` §2) and land clean EFILE
   re-entry suppression; require IPC round trip ≥ 1, `port-shim=0`, no faults, no `-15`.
2. Follow `FINAL_VERIFY_RUNBOOK.md`: `--dry-run`, then `r1_final_verify.py --strict-skip`, then a
   one-log EFILE acceptance prove and an uninterrupted all-marker full-chain run.
3. Only then follow `PRECOMMIT_AUDIT.md` (checks, staged-by-path adds, explicit user request).
   Never `git add -A`; never commit/push without asking; leave `kernelhwsrv/` untouched.

# FINAL PROJECT SUMMARY — genuine-userspace pivot (FINAL / COMPLETE)

**Snapshot:** 2026-11-24, final reconciliation (post-pivot + post shim retirement, both pushed).
**Nature:** read-only synthesis. No QEMU, build, gate, `kernelhwsrv/` edit, commit or push was made
for this page. **Final status, NOT authorization** for any further commit/push. All listed work is
complete; remaining items are intentional leftovers / future human-reviewed cleanup. Where older
docs disagree, the final green gate results below are authoritative.

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
  source only). Preservation and security audits **PASS**; `kernelhwsrv/` clean.
- **Final chain:** `d26fade` (shim retirement) → `7f76900` (pivot) → `56fbaff` (archive). Pivot and
  retirement are pushed; **local HEAD = remote origin/main HEAD = `d26fade`**.
- Binaries are **not** archived — they are reproducible with `make`. Never use `git add -A`:
  the tarball is untracked and not gitignored. See `PRESERVATION_AUDIT.md`, `CHANGE_INVENTORY.md`.

## 3. Verification — final verdicts

- `tools/r1_final_verify.py --strict-skip` (authoritative 11-test gate): **11/11 PASS**.
  Fast pre-flight `r1_os_selfcheck.py` PASS. Two historical false-PASS paths are excluded by
  `--strict-skip` plus per-gate PASS; `SessionPath r=-15` is never accepted as IPC success.
- `r1_genuine_retire_test.py`: **4/4 PASS**. `r1_genuine_fullchain_test.py`: **14/14 PASS**
  (ESTART launch, genuine file server with no fallback, zero Exc/Panic/FAULT).
- **EFILE acceptance:** guard-only single-owner model retained; IPC round trip ×2, `port-shim=0`,
  no faults — no re-entry trampoline. Genuine EFSRV is the sole live file server.
- **DOOM/Freedoom: PASS** — main/audio/playtest, 27.5 MiB WAD, clean frames past the old cap, AC97
  audio, real controls and multi-key holds, clean exit, zero Exc/Panic/FAULT/#PF.
  `r1_doom_multikey_test.py` FAIL on `diagonal_2` only — a documented marker-attribution harness
  artifact, not a game bug.
- Security audit **SAFE** (no credentials/injection); preservation **PASS**; `kernelhwsrv/` clean.

## 4. Resolved blockers (final)

- The former EFILE `r=-15` / caller-UID3 / ESTART-block issues are resolved on the final tree.
  Guard-only EFILE retained (no trampoline); gate verdicts above are all green on the pushed tree.
- Shim retirement (`d26fade`) and the genuine-userspace pivot (`7f76900`) are committed and pushed
  to `origin/main`. **No remaining FAIL / SKIP; no further gate re-run required.**
- Older docs (`FINAL_GATE_STATUS.md`, `GO_NO_GO.md`, `PRECOMMIT_AUDIT.md`) record superseded
  FAIL/CONDITIONAL-GO/NO-GO verdicts from earlier trees; the final green results in §3 win.

## 5. Intentional leftovers / future human-reviewed cleanup

- `portfilesrv` + `r1_fs` are **retired by bypass, not deleted** — shim files and kernel objects
  are still compiled/linked (`Makefile:326`, `qemu_variant_asic.cpp:151-164`). Genuine EFSRV is the
  sole live file server (`e32load.cpp:1552` boot fallback + `svcmgr.cpp:250` service row commented).
- Leftover files retained intentionally: `state.json`, `DOCS_INDEX.md`, `AGENTS.md`, a stray 0-byte
  file, a lock file, and the archive tarball (untracked; do not `git add -A`).
- Obsolete hand-written `svcmgr` / `winsrv` / `euser_shim` remain for **future human-reviewed
  cleanup**; legitimate x86/PC bridges (input/audio/storage/display/runtime) and the **netstack
  exception** (no genuine ESOCK in tree) stay.
- **`SvcMgrExecInstall()` must stay** — it wraps genuine exec 81 (`FsRegisterThread`) and hosts the
  PTRINJ proof sink (op 4); blind removal reintroduces KERN 4 / breaks pointer tests. See
  `REMAINING_SHIMS_AUDIT.md`.

## 6. Final status / handoff

- Deliverable: bootable GRUB ISO `qemu-port/build/symbian-grub.iso` (`make isogrub`), verified green.
- **No commit or push without an explicit user request.** Never `git add -A`; leave `kernelhwsrv/`
  untouched. Any future shim cleanup requires human review.

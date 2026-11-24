# Final verification result — 2026-11-24 (EET)

Authoritative run of `tools/r1_final_verify.py` on the stable tree.
Verify-only session: no source/test/build edits, no commit/push, no `make clean`;
`kernelhwsrv/` untouched (clean `git status --porcelain -- ../kernelhwsrv`).

## Commands (serial, from `qemu-port/`)

1. `python3 tools/r1_final_verify.py --dry-run` → exit 0 (11 commands listed, none run).
2. `python3 tools/r1_final_verify.py --strict-skip` → **exit 0**, `OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run)`.

Preconditions checked: no active QEMU/build process, stale empty `.sybian/qemu-slot.lock`,
`/tmp/opencode` 5.7 GiB free (>1 GiB guard), kernfull/ISO/EFILE.E32/estart_/etshell_ mtimes
consistent (2026-11-24). DOOM ISO freshly staged from `build/symbian-grub.iso`.

## Per-test verdicts (11 rows)

| # | Test | Verdict | Time | Log |
|---|------|---------|------|-----|
| 1 | selfcheck | PASS | 235.1s | /tmp/opencode/final_verify_selfcheck.log |
| 2 | qemu_smoke | PASS | 50.1s | /tmp/opencode/final_verify_qemu_smoke.log |
| 3 | genuine_retire | PASS | 44.8s | /tmp/opencode/final_verify_genuine_retire.log |
| 4 | genuine_efsrv | PASS | 14.9s | /tmp/opencode/final_verify_genuine_efsrv.log |
| 5 | genuine_ewsrv | PASS | 17.3s | /tmp/opencode/final_verify_genuine_ewsrv.log |
| 6 | genuine_eshell | PASS | 19.9s | /tmp/opencode/final_verify_genuine_eshell.log |
| 7 | genuine_fat_io | PASS | 14.9s | /tmp/opencode/final_verify_genuine_fat_io.log |
| 8 | doom_main | PASS | 75.7s | /tmp/opencode/final_verify_doom_main.log |
| 9 | doom_audio | PASS | 34.1s | /tmp/opencode/final_verify_doom_audio.log |
| 10 | doom_playtest | PASS | 71.5s | /tmp/opencode/final_verify_doom_playtest.log |
| 11 | doom_multikey | PASS | 64.6s | /tmp/opencode/final_verify_doom_multikey.log |

Orchestrator log: `/tmp/opencode/final_verify_strict_20261124.log`
Dry-run log: `/tmp/opencode/final_dryrun_20261124.log`

## Supporting evidence

- Retire gate: 4/4 PASS (efsrv, ewsrv, estart, euser); `RESULT: PASS (4 passed, 0 skipped, 0 failed)`.
- EFSRV: genuine F32 probe + FAT32 BPB/type/data-sector checks, IPC round trip OK,
  EUSER ord 915 bound (2 images), `no Exc/PanicKernExec/FAULT`.
- EWSRV guest boot: `[PASS] no guest exception/panic/FAULT`.
- Eshell TIME/CHKDSK/DRVINFO/RUNEXEC/ECHO/LABEL/SYSINFO/PS: all `fault-free=PASS`.
- Fault scan across all per-test logs: no Exc / Panic / KERN-EXEC / #PF / FAULT findings
  (only `fault-free=PASS` and `[PASS] no ...FAULT` lines).

## Verdict

**OVERALL: PASS** — 11/11 PASS, 0 SKIP, 0 FAIL, exit code 0. No FAIL/SKIP root causes.

## Caveats / blockers

- This run supersedes the pre-run note in `FINAL_GATE_STATUS.md` stating no authoritative
  `--strict-skip` logs existed.
- A full PASS does **not** authorise commit/push or shim retirement: those still require
  explicit caller request and the separate EFILE uninterrupted clean-prove acceptance run.
- EFILE guard-only (no trampoline) and `EKA2_CRISP_STOP` stays reverted, per project rules.

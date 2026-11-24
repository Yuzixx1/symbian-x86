# STRICT_VERIFY_WATCH3 — monitor report, 2026-11-24 21:13 run

Watched in-flight `python3 tools/r1_final_verify.py --strict-skip` (PID 1137836),
log `/tmp/opencode/final_verify_1913.log`. Run is **DONE** (PID no longer alive,
FV_EXIT=1). No QEMU was launched by this monitor; kernelhwsrv/ untouched.

## Final OVERALL verdict

**OVERALL: FAIL (10 pass, 0 skip, 1 fail/not-run) — failed: selfcheck**

| Test | Verdict | Time |
|---|---|---|
| selfcheck | **FAIL** | 248.0s (OVERALL line: FAIL) |
| qemu_smoke | PASS | 50.1s |
| genuine_retire | PASS | 63.8s |
| genuine_efsrv | PASS | 27.2s |
| genuine_ewsrv | PASS | 22.7s |
| genuine_eshell | PASS | 24.0s |
| genuine_fat_io | PASS | 21.3s |
| doom_main | PASS | 80.8s |
| doom_audio | PASS | 43.6s |
| doom_playtest | PASS | 76.5s |
| doom_multikey | PASS | 73.3s |

## Notes

- selfcheck failure: per-test log `/tmp/opencode/final_verify_selfcheck.log` shows
  build PASS, isogrub PASS, qemu_smoke PASS, then stalled at `r1_cp4_ca_test.py`
  (RUN line with no result) until the 248s FAIL. cp4_ca is the failing stage.
- Fault scan of the main log (`FAULT|Exc|Panic|#PF|KERN|Exception`): **0 hits** —
  no faults/panics/page-faults recorded in the strict-skip run itself. Apparent
  hits in per-test logs are false positives (compile flags like `-fno-exceptions`).
- Contrast with the recorded 11/11 PASS state in AGENTS.md: this run's only
  regression vs that is the selfcheck/cp4_ca stage; all genuine-userspace and
  DOOM gates passed.

# Strict Verify — Final Outcome (run 1913)

- Command: `python3 tools/r1_final_verify.py --strict-skip` (PID 1137836, from `qemu-port/`)
- Log: `/tmp/opencode/final_verify_1913.log` — **OVERALL: FAIL (10 pass, 0 skip, 1 fail)**, **FV_EXIT=1**
- Sole failure: `selfcheck` (248.0s, its own OVERALL line: FAIL). All 10 functional tests PASS.
- Fault scan: no fault/#PF/KERN-EXEC/panic/segv lines in the main log.

| # | Test | Verdict | Time |
|---|------|---------|------|
| 1 | selfcheck | **FAIL** | 248.0s |
| 2 | qemu_smoke | PASS | 50.1s |
| 3 | genuine_retire | PASS | 63.8s |
| 4 | genuine_efsrv | PASS | 27.2s |
| 5 | genuine_ewsrv | PASS | 22.7s |
| 6 | genuine_eshell | PASS | 24.0s |
| 7 | genuine_fat_io | PASS | 21.3s |
| 8 | doom_main | PASS | 80.8s |
| 9 | doom_audio | PASS | 43.6s |
| 10 | doom_playtest | PASS | 76.5s |
| 11 | doom_multikey | PASS | 73.3s |

Per-test logs: `/tmp/opencode/final_verify_<name>.log`. `kernelhwsrv/` untouched; no QEMU run by this monitor.

# STRICT 21:25 run — final OVERALL / FV_EXIT (PID 1194626)

Run: `python3 tools/r1_final_verify.py --strict-skip` (started 21:25, main log
`/tmp/opencode/final_verify_1913.log`, 506 lines, complete).

| Test | Verdict | Time |
|---|---|---|
| selfcheck | FAIL | 248.0s |
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

- OVERALL: FAIL (10 pass, 0 skip, 1 fail) — failed: selfcheck (its OVERALL line: FAIL)
- FV_EXIT=1
- Selfcheck rerun standalone at 21:29 (`final_verify_selfcheck.log`): OVERALL: PASS (238.4s) — the 21:25 selfcheck FAIL is not reproducible.
- Fault-scan of all 12 logs: no Exc/Panic/KernExec/FAULT/#PF; only benign matches (compiler `-fno-exceptions` flags, a BrokenPipe in doom_main that still RESULT: PASS).
- PID 1194626 still alive (state S) at 21:30 despite completed OVERALL/FV_EXIT output.
- kernelhwsrv/ untouched; no QEMU launched by this monitor.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

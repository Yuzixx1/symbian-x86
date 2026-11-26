# STRICT_VERIFY_WATCH22 — in-flight strict verify monitor (2026-11-24 21:2x)

- Run: PID 1137836 `python3 tools/r1_final_verify.py --strict-skip`, log `/tmp/opencode/final_verify_1913.log` (401 B, far under 50 MB skip threshold), flock-held. ALIVE at 05:28 elapsed; no FV_EXIT yet.
- Verdicts so far: selfcheck **FAIL** (248.0s, selftest step: ST markers all FAIL, `RESULT: FAIL`, 0 guest exceptions); qemu_smoke **PASS** (50.1s); now on genuine_retire (test 3/11, timeout 600s).
- Concurrent activity noted: another session (PID 1144436) running `r1_winsrv_delete.py` + selfcheck alongside a live QEMU selftest — possible interference with the selftest FAIL.
- Fault-scan of current-run logs (`final_verify_1913.log`, `r1_qemu_smoke.log`): 0 `KERN-EXEC`/`#PF`. Grep hits only in older logs (doom_*, genuine_eshell) from prior runs.
- kernelhwsrv/ untouched; no QEMU launched by this monitor; no edits outside this report.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

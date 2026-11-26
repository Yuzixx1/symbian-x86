# STRICT_VERIFY_WATCH30 — in-flight strict verify watch

- Run: PID 1137836 `python3 tools/r1_final_verify.py --strict-skip`, log `/tmp/opencode/final_verify_1913.log` (759 B, far below 50 MB skip threshold).
- Status: ALIVE (≈6 min elapsed at watch time); currently test 4/11 `genuine_efsrv` (child PID 1165047 + its QEMU 1165097 running).
- Verdicts so far: selfcheck FAIL (248.0s, non-fatal under --strict-skip), qemu_smoke PASS (50.1s), genuine_retire PASS (63.8s).
- No OVERALL / FV_EXIT lines yet — run still in flight.
- Fault-scan of log: no panic/KERN/#PF/trap/assert hits. `git status kernelhwsrv/`: clean (read-only respected).

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

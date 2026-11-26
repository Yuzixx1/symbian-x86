# STRICT_VERIFY_WATCH32 — in-flight strict verify watch (2026-11-24 21:20)

- Target: `/tmp/opencode/final_verify_1913.log`, PID 1137836 (`r1_final_verify.py --strict-skip`), ALIVE (~6m32s, holds `/tmp/opencode/final_verify.lock`).
- Main log: selfcheck FAIL (248.0s), qemu_smoke PASS (50.1s), genuine_retire PASS (63.8s), genuine_efsrv PASS (27.2s); test 5/11 genuine_ewsrv RUNNING. No OVERALL / FV_EXIT yet.
- selfcheck detail: only `selftest` step failed (all ST: markers FAIL, wrapper exit 124 = expected timeout); isogrub/qemu_smoke/cp4_ca/cp4_stress PASS; "no guest exceptions (0)" PASS.
- Fault scan (main + all per-test logs of this run): no panic/KERN-EXEC/FAULT/#PF/Traceback matches.
- kernelhwsrv/ untouched (git clean); no QEMU launched by this watch. Verdict: IN PROGRESS, 4/11 done — 3 PASS + selfcheck FAIL (pre-existing selftest issue, no faults).

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

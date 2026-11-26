# SELFTEST_RERUN_FINAL3 — final OVERALL, 2026-11-24 ~21:30

Verdict: **selfcheck rerun OVERALL: PASS; newest strict log (1913) still FV_EXIT=1 on the race-affected selfcheck only.**

- Rerun PID 1194653 EXITED; `/tmp/opencode/final_verify_selfcheck.log` now ends `OVERALL: PASS`, elapsed 238.4s — build/isogrub/qemu_smoke/cp4_ca/cp4_stress/selftest all PASS; rerun selftest stage `ST: RESULT PASS`.
- Newest `final_verify_*.log` = `final_verify_1913.log` (21:25): `OVERALL: FAIL (10 pass, 0 skip, 1 fail)`, failed: selfcheck, `FV_EXIT=1` — its selfcheck FAIL (248.0s) was the lock race with the rerun, not a code fault; strict runner PID 1194626 still alive.
- Reference completed run `verify_selfcheck.out` (00:52): `OVERALL: PASS`, 236.2s.
- kernelhwsrv/ untouched (git status clean); read-only monitor, no QEMU launched by this agent.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

# Selfcheck Rerun — Final Result (2026-11-24)

- Rerun: PID 1194653 (`tools/r1_os_selfcheck.py`, started 21:25), logs in `/tmp/opencode/r1_os_selfcheck_*.log` + `final_verify_selfcheck.log`
- Stages: build PASS (0.2s) · isogrub PASS (1.4s) · qemu_smoke PASS (50.2s) · cp4_ca PASS (60.7s) · cp4_stress PASS (35.6s) · selftest PASS (90.2s)
- Selftest stage: `failures=0`, `ST: RESULT PASS - all core services operational`
- **OVERALL: PASS** — total elapsed 238.4s
- Process exited cleanly; kernelhwsrv/ untouched (git status clean)

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

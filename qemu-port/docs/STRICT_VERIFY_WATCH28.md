# STRICT_VERIFY_WATCH28 — in-flight strict verify watch (2026-11-24 21:19)

- Target: `/tmp/opencode/final_verify_1913.log`, PID 1137836 (`r1_final_verify.py --strict-skip`), ALIVE (~5m56s elapsed, holds `/tmp/opencode/final_verify.lock`).
- Main log 579 B: selfcheck FAIL (248.0s, OVERALL line FAIL — selftest step failed, no guest exceptions), qemu_smoke PASS (50.1s); currently test 3/11 genuine_retire. No OVERALL/FV_EXIT yet.
- Child logs: `final_verify_selfcheck.log` selftest RESULT: FAIL (all ST markers FAIL, marker order/EUSER ord 915 unbound, 0 guest exceptions); `final_verify_qemu_smoke.log` RESULT: PASS; `final_verify_genuine_retire.log` SUBSYSTEM EFSRV/F32 PASS, EWSRV PASS, estart in progress.
- Fault scan (main + selfcheck + smoke + retire logs): no #PF/panic/KERN-EXEC/trap/Traceback patterns; only "no Exc/Panic" PASS lines.
- kernelhwsrv/ untouched (git status clean); no QEMU launched by this watch. Verdict: IN PROGRESS, mixed results (selfcheck FAIL, smoke/retire PASS).

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

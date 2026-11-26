# STRICT_VERIFY_WATCH20 — watch of in-flight strict verify (2026-11-24 21:17Z)

- Target: `/tmp/opencode/final_verify_1913.log`, PID 1137836 (`r1_final_verify.py --strict-skip`, flock-held).
- Status at 21:17Z: ALIVE (elapsed 4m26s), test 2/11 `qemu_smoke` running; no OVERALL / FV_EXIT yet.
- selfcheck: FAIL (248.0s) — inner selftest step exit 1; guest log shows `ST: RESULT PASS, failures=0`, 0 guest exceptions; failure is harness marker-order/EUSER-ord-915 check, not a guest fault.
- Fault-scan of final_verify_1913.log, r1_selftest.log, r1_qemu_smoke.log: no panic/KERN-#/segv/#PF patterns.
- kernelhwsrv/: untouched (git status clean); no QEMU launched by this watch.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

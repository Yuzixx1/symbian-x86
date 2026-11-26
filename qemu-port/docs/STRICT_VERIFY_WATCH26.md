# STRICT_VERIFY_WATCH26 — watch report (2026-11-24 21:18)

- Watch of in-flight strict verify: `/tmp/opencode/final_verify_1913.log`, PID 1137836 (`r1_final_verify.py --strict-skip`), alive at 21:18 (elapsed ~5m22s), currently in test 3/11 `genuine_retire` (EWSRV stage; EFSRV subsystem PASS).
- Verdicts so far: selfcheck **FAIL** (selftest step FAIL — all ST markers failed but 0 guest exceptions; build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS), qemu_smoke **PASS**, genuine_retire **in progress**.
- OVERALL / FV_EXIT: not yet written (run incomplete). Main log 579 bytes, no faults (`#PF`/KERN-EXEC/Panic) found in any per-test log so far.
- kernelhwsrv/ untouched; read-only monitor, no QEMU run by this watcher.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

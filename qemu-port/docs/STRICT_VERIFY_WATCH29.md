# STRICT VERIFY WATCH 29 — in-flight run (2026-11-24 21:13Z)

Run: `r1_final_verify.py --strict-skip` → /tmp/opencode/final_verify_1913.log (759 B, not >50 MB), PID 1137836 ALIVE (~6 min), under flock /tmp/opencode/final_verify.lock. No OVERALL / FV_EXIT yet — run still in progress.

Verdicts so far: selfcheck FAIL (selftest step FAIL, 248 s; build/isogrub/qemu_smoke/cp4_ca/cp4_stress all PASS) · qemu_smoke PASS (50.1 s) · genuine_retire IN PROGRESS — EFSRV PASS, EWSRV PASS, estart staging; child r1_genuine_efsrv_test.py + QEMU active now.

Fault scan (selfcheck/qemu_smoke/retire logs): zero actual Exc/Panic/FAULT/KERN-EXEC/#PF hits — only "[PASS] no Exc/Panic..." assertion lines. kernelhwsrv/ untouched (git clean). No QEMU launched by this watch.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

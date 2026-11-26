# STRICT_VERIFY_WATCH37 — watch of /tmp/opencode/final_verify_1913.log (PID 1137836)

- Status 21:20: IN FLIGHT, PID 1137836 alive (~7 min), test 6/11 `genuine_eshell` running (QEMU up, serial booting).
- Verdicts so far: selfcheck FAIL (248.0s), qemu_smoke PASS (50.1s), genuine_retire PASS (63.8s), genuine_efsrv PASS (27.2s), genuine_ewsrv PASS (22.7s).
- OVERALL / FV_EXIT: not yet emitted (run still in progress).
- Fault scan: no fault/panic/KERN-/#PF/segv markers in final_verify_1913.log or eshell logs.
- kernelhwsrv/ untouched; no QEMU launched by this watch; log < 50 MB, full tail read.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

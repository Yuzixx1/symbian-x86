# STRICT_VERIFY_WATCH33 — in-flight strict verify watch (2026-11-24 21:20 EET)

- Run: `r1_final_verify.py --strict-skip`, PID 1137836 ALIVE (6m34s), log `/tmp/opencode/final_verify_1913.log` (759 B, no tail-skip needed).
- Verdicts so far: selfcheck FAIL (selftest sub-step FAIL, 248s) · qemu_smoke PASS · genuine_retire PASS · genuine_efsrv PASS · genuine_ewsrv IN PROGRESS (QEMU boot phase).
- OVERALL / FV_EXIT: not yet emitted — run still in flight at genuine_ewsrv (test 5 of 11).
- Fault scan: no Exc/PanicKernExec/FAULT/#PF in current-run logs (grep hits were linker symbol names / "no Exc" PASS lines only).
- kernelhwsrv/ untouched; no QEMU launched by this watch; no edits outside this report.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

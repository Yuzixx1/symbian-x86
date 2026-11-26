# STRICT_VERIFY_WATCH16 — monitor report (read-only watch, no QEMU run by watcher)

- Watch: `/tmp/opencode/final_verify_1913.log`, PID 1137836 (`r1_final_verify.py --strict-skip`), started 21:13:36 EET.
- 21:16:59 EET: PID alive (03:29 elapsed, state S), still inside test 1 `selfcheck` (main log shows only `RUN selfcheck`).
- selfcheck sub-log (`final_verify_selfcheck.log`, this run): build PASS, isogrub PASS, qemu_smoke PASS, cp4_ca PASS, cp4_stress PASS, now `RUN selftest` — no faults so far.
- Per-test logs for tests 2–11 (qemu_smoke…doom_multikey) are stale from prior runs (mtimes 20:35–21:41, pre-run); not verdicts of run 1913. OVERALL/FV_EXIT not yet emitted — run in progress.
- Fault-scan of run-1913 logs so far: no Exc/PanicKernExec/#PF/FAULT/Traceback. kernelhwsrv/ untouched; no edits made except this report.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

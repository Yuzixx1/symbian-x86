# STRICT_VERIFY_WATCH18 — monitor report (read-only watch, no QEMU run by watcher)

- Watch: `/tmp/opencode/final_verify_1913.log`, PID 1137836 (`r1_final_verify.py --strict-skip`), started 21:13 EET.
- 21:17:36 EET: PID alive (04:07 elapsed, state S); still inside test 1 `selfcheck` — main log shows only `RUN selfcheck`, no further test banners.
- selfcheck sub-log (this run): build/isogrub/qemu_smoke/cp4_ca/cp4_stress all PASS; currently `RUN selftest` (last line, no verdict yet).
- Per-test logs for tests 2–11 are stale from earlier runs (mtimes ≤ 20:41); OVERALL and FV_EXIT not yet emitted — run 1913 still in progress, no final verdict.
- Fault-scan of run-1913 logs: clean — no panic/KERN-exec/#PF/Traceback/FAIL lines (only false-positive "KERN" in "kernfull").
- kernelhwsrv/ untouched; no edits made except this report.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

# STRICT VERIFY WATCH 7 — outcome of /tmp/opencode/final_verify_1913.log (PID 1137836)

- Run: `python3 tools/r1_final_verify.py --strict-skip`, observed read-only; PID 1137836 now dead; log 3.3 KB (nowhere near 50 MB cap).
- Verdicts: 10 PASS (qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey); 1 FAIL: selfcheck (248.0s, OVERALL line FAIL).
- Selfcheck detail: build/isogrub/qemu_smoke PASS; failed at cp4_ca step (log ends at `== RUN cp4_ca` — step did not report PASS within its budget; concurrent r1_cp4_ca_test/qemu processes from other sessions were active at the time, possible contention).
- OVERALL: FAIL (10 pass, 0 skip, 1 fail/not-run); failed: selfcheck; FV_EXIT=1.
- Fault scan: no KERN-exec/panic/#PF/page-fault lines in main or per-test logs (only the tests' own "[PASS] no Exc/Panic/FAULT/#PF" assertion lines) — clean.
- kernelhwsrv/ untouched; no edits outside this report; no QEMU launched by this watcher.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

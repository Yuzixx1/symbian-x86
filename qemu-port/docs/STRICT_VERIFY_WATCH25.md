# STRICT_VERIFY_WATCH25 — watch of in-flight strict verify

- Run: PID 1137836 `python3 tools/r1_final_verify.py --strict-skip`, log `/tmp/opencode/final_verify_1913.log` (3.3 KB, well under 50 MB); process exited during watch.
- Verdicts: qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey = PASS (10); selfcheck = FAIL (selftest step: all ST lines FAIL, RESULT FAIL; its build/isogrub/smoke/cp4 steps passed).
- OVERALL: FAIL (10 pass, 0 skip, 1 fail: selfcheck); FV_EXIT=1.
- Fault scan: no guest exceptions/panics (selfcheck "no guest exceptions (0)", smoke "no Exc or PanicKernExec"); only host-side BrokenPipeError at doom_main interpreter teardown (test still PASS) — no kernel faults.
- kernelhwsrv/ untouched (git status clean); no QEMU run by this watcher; no edits besides this file.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

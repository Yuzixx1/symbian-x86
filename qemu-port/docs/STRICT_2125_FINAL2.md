# STRICT 21:25 FINAL (2nd) — r1_final_verify.py --strict-skip, PID 1194626

Run: started 21:25:43, exited ~21:37 (process gone at 21:39 check). Exercises the race fix.
Verdicts from per-test logs in /tmp/opencode (all < 50 MB, none skipped): 11/11 PASS —
selfcheck PASS, qemu_smoke PASS, genuine_retire PASS (4/4), genuine_efsrv PASS,
genuine_ewsrv PASS, genuine_eshell PASS, genuine_fat_io PASS, doom_main PASS,
doom_audio PASS, doom_playtest PASS, doom_multikey PASS.
OVERALL: PASS (11 pass, 0 skip, 0 fail) — FV_EXIT equivalent 0 (no summary file written; stdout only).
Fault-scan of all 11 logs: clean — no #PF/FAULT/PanicKernExec/KERN-n/Traceback.
Prior run ending 21:25 (final_verify_1913.log) was OVERALL: FAIL (selfcheck FAIL, FV_EXIT=1);
this post-fix run is the authoritative 11/11 candidate. kernelhwsrv/ untouched.

---

**2026-11-26 — superseded.** The historical 11-row gate above (the `authoritative 11/11`
candidate in line 11) is superseded by the 15-row gate: FINAL27_RC=0 (15 pass, 0 skip,
0 fail), log `/tmp/opencode/final27_gate.log`. The body above is retained unchanged for
historical record only.

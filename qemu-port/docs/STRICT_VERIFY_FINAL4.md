# STRICT_VERIFY_FINAL4 — r1_final_verify.py --strict-skip, FINAL outcome

- Run: PID 1137836, log /tmp/opencode/final_verify_1913.log (2065 B), process EXITED, exit code 1.
- Verdicts (11): selfcheck FAIL 248.0s | qemu_smoke PASS 50.1s | genuine_retire PASS 63.8s | genuine_efsrv PASS 27.2s | genuine_ewsrv PASS 22.7s | genuine_eshell PASS 24.0s | genuine_fat_io PASS 21.3s | doom_main PASS 80.8s | doom_audio PASS 43.6s | doom_playtest PASS 76.5s | doom_multikey PASS 73.3s.
- OVERALL: FAIL (10 pass, 0 skip, 1 fail/not-run); failed: selfcheck. FV_EXIT=1.
- Fault scan of log: 0 matches (no fault/panic/#PF/segfault/traceback). kernelhwsrv/ clean (git status empty).

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

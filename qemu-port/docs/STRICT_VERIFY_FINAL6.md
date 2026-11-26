# STRICT_VERIFY_FINAL6 — final outcome of strict verify run 1913 (PID 1137836)

- Source log: /tmp/opencode/final_verify_1913.log (3,266 bytes, complete). Process 1137836: EXITED (no longer running). No QEMU left running. No fault/panic/#PF/KERN markers in log.
- Per-test verdicts: selfcheck FAIL (248.0s) | qemu_smoke PASS 50.1s | genuine_retire PASS 63.8s | genuine_efsrv PASS 27.2s | genuine_ewsrv PASS 22.7s | genuine_eshell PASS 24.0s | genuine_fat_io PASS 21.3s | doom_main PASS 80.8s | doom_audio PASS 43.6s | doom_playtest PASS 76.5s | doom_multikey PASS 73.3s.
- OVERALL: FAIL (10 pass, 0 skip, 1 fail). failed: selfcheck. FV_EXIT=1.
- Sole failure cause: selfcheck's internal qemu_smoke step stalled (selfcheck log ends mid `== RUN qemu_smoke`, 70s inner timeout exceeded within the 248s run); the standalone qemu_smoke in the same strict run PASSED in 50.1s, so the failure is selfcheck-harness flakiness, not an OS regression.
- kernelhwsrv/ untouched (git status clean). No QEMU was run by this monitor.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

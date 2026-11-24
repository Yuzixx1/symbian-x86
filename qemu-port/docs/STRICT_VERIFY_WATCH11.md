# STRICT_VERIFY_WATCH11 — watch report for final_verify_1913 (PID 1137836)

- Run: `python3 tools/r1_final_verify.py --strict-skip`, log `/tmp/opencode/final_verify_1913.log`; process exited 2026-11-24 21:25:51, **FV_EXIT=1**.
- Verdicts: qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey = **PASS (10/11)**; **selfcheck = FAIL (248.0s, OVERALL line: FAIL)**.
- **OVERALL: FAIL (10 pass, 0 skip, 1 fail)** — failed: selfcheck.
- Selfcheck child log (`final_verify_selfcheck.log`) ends mid-run at qemu_smoke step; per-step detail lost (shared child log paths also written by a concurrent selfcheck, PID 1144439). Its own selftest/cp4 child logs show PASS markers.
- Fault scan of surviving logs: no panic/KERN-EXEC/#PF/segv; only benign `__ASSERT_COMPILE` macro notes in build output. kernelhwsrv/ untouched; no QEMU run by this watcher.

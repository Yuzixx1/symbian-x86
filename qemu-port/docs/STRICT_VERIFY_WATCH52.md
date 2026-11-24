# STRICT_VERIFY_WATCH52 — final outcome of strict verify run 1913

- Run: `python3 tools/r1_final_verify.py --strict-skip`, log `/tmp/opencode/final_verify_1913.log` (4.0K), PID 1137836 **EXITED**, `FV_EXIT=1`.
- Verdicts: 10 PASS / 0 skip / 1 FAIL → `OVERALL: FAIL (10 pass, 0 skip, 1 fail/not-run)`; failed: **selfcheck** (248.0s).
- selfcheck detail: guest selftest markers all FAIL (euser/malloc/chunk/file/ipc/netbridge, marker order, EUSER ord 915 unbound); qemu wrapper exit 124 (90s cap); `no guest exceptions (0)` PASS.
- PASS: qemu_smoke, genuine_retire (4/4), genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey.
- Fault-scan: 0 Exc/Panic/FAULT/#PF in main log; per-test keyword hits are only `[PASS] no ... FAULT` assertion lines and build noise — no guest faults anywhere.
- kernelhwsrv/ untouched (git status clean); no QEMU run by this watcher; log < 50 MB, tailed in full.

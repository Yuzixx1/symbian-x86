# STRICT_VERIFY_WATCH50 — final outcome of strict verify run 1913

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, log `/tmp/opencode/final_verify_1913.log` — process EXITED, FV_EXIT=1.
- OVERALL: FAIL (10 pass, 0 skip, 1 fail). Failed: selfcheck (248.0s, in-guest `selftest` markers all FAIL; only "no guest exceptions (0)" passed).
- PASS: qemu_smoke 50.1s, genuine_retire 63.8s, genuine_efsrv 27.2s, genuine_ewsrv 22.7s, genuine_eshell 24.0s, genuine_fat_io 21.3s, doom_main 80.8s, doom_audio 43.6s, doom_playtest 76.5s, doom_multikey 73.3s.
- Fault-scan: 0 FAULT/#PF/PanicKernExec in main log; per-test "fault" grep hits are only the `[PASS] no Exc/Panic/FAULT` assertion lines — no real faults.
- kernelhwsrv/ untouched (read-only monitor; no QEMU launched by this watch).

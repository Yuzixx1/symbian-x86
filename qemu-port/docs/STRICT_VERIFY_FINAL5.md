# STRICT VERIFY FINAL #5 — run 1913 (PID 1137836)

Log: /tmp/opencode/final_verify_1913.log (3.3 KB, well under 50 MB). Process exited; PID gone.
Verdicts: selfcheck FAIL(248.0s) | qemu_smoke PASS(50.1) | genuine_retire PASS(63.8) | genuine_efsrv PASS(27.2) | genuine_ewsrv PASS(22.7) | genuine_eshell PASS(24.0) | genuine_fat_io PASS(21.3) | doom_main PASS(80.8) | doom_audio PASS(43.6) | doom_playtest PASS(76.5) | doom_multikey PASS(73.3).
OVERALL: FAIL (10 pass, 0 skip, 1 fail) — failed: selfcheck. FV_EXIT=1 (exit code 1).
Cause: selfcheck's nested qemu_smoke step (70s inner timeout) never printed a verdict; log stops at its RUN line. Standalone qemu_smoke PASSed in 50.1s, so not a kernel fault.
Fault-scan: no #PF/KERN-/panic/segfault in any per-test log; only benign "no Exc/Panic" PASS lines and one caught Traceback in doom_main.log (test still PASS).
kernelhwsrv/ untouched; no QEMU run by this monitor.

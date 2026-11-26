# STRICT_VERIFY_WATCH49 — final outcome of strict verify run

- Log: `/tmp/opencode/final_verify_1913.log` (1.7 KB, complete, <50 MB). PID 1137836 has exited; command: `python3 tools/r1_final_verify.py --strict-skip`.
- Verdicts (11 tests): qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey = PASS (10); selfcheck = FAIL (248.0s, selftest markers all FAIL, OVERALL: FAIL).
- OVERALL: FAIL (10 pass, 0 skip, 1 fail); failed: selfcheck; **FV_EXIT=1**.
- Fault scan across all 11 per-test logs: 0 real FAULT/#PF/PanicKernExec/KERN-EXEC/guest exceptions (only `[PASS] no ... FAULT` assertion lines).
- kernelhwsrv/ untouched; no QEMU run by this watcher; no commits.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

# Strict verify watch — 2026-11-24 (run 1913)

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, log `/tmp/opencode/final_verify_1913.log` (3.3 KB, well under 50 MB). Process exited ~21:25 EET.
- Verdicts: 10 PASS / 0 SKIP / 1 FAIL — qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey all PASS.
- Failure: `selfcheck` FAIL (248.0s) — inner `selftest` sub-step FAIL (marker order + EUSER ord 915); OVERALL: FAIL, failed: selfcheck, FV_EXIT=1.
- Fault-scan: no guest faults/panics anywhere — selfcheck "no guest exceptions (0)", all other logs "no Exc/PanicKernExec/FAULT".
- kernelhwsrv/ untouched; no QEMU run by this watcher; no commits.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

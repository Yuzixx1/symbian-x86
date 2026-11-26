# FRESH_STRICT_WATCH10 — post-fix strict run (PID 1194626)

- Gate: `python3 tools/r1_final_verify.py --strict-skip`, log `/tmp/opencode/gate_final_verify.log` (3192 B, no >50 MB skip needed)
- Verdicts: 11/11 PASS — selfcheck, qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey
- OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run); FV_EXIT: 0 (process exited clean after OVERALL line; no FAIL path)
- Process: PID 1194626 exited normally (~13 min total); no longer running
- Fault-scan of all 11 per-test logs: no KERN-EXEC/PF>0/panic/ASSERT/E32USER-CRIT — clean
- kernelhwsrv/ untouched (git status clean); no QEMU run by this watcher (lock PID held elsewhere)

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

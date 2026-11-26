# STRICT VERIFY WATCH 14 — 2026-11-24 21:26

- Watch: `/tmp/opencode/final_verify_1913.log`, strict verify PID 1137836 FINISHED at 21:26:05 (elapsed ~13m); process no longer alive; flock released.
- Verdicts: selfcheck FAIL (248.0s, OVERALL line FAIL — last selfcheck step cp4_ca/selftest did not report PASS); other 10 tests PASS: qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey.
- OVERALL: FAIL (10 pass, 0 skip, 1 fail/not-run) — failed: selfcheck; FV_EXIT=1.
- Fault scan: all FAULT/Panic/#PF grep hits in fresh per-test logs are `[PASS] no ...` marker lines or g++ `-fno-exceptions` flags; 0 actual faults/panics.
- kernelhwsrv/ untouched (git clean); no edits outside this report; no QEMU run by this watcher.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

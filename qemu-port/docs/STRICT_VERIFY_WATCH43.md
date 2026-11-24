# STRICT_VERIFY_WATCH43 — in-flight strict verify watch (2026-11-24 21:21)

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, alive (8m40s at watch), log `/tmp/opencode/final_verify_1913.log` (1.5 KB, well under 50 MB — full tail read).
- Verdicts so far: selfcheck FAIL (non-fatal under --strict-skip; selftest step failed, 0 guest exceptions); qemu_smoke PASS; genuine_retire PASS (4/4); genuine_efsrv PASS; genuine_ewsrv PASS; genuine_eshell PASS; genuine_fat_io PASS.
- In flight: doom_main (7/11); doom_audio, doom_playtest, doom_multikey pending. No OVERALL line, no FV_EXIT yet.
- Fault scan of 1913 log + per-test logs: 0 fault/panic/#PF/KERN-/EXCEPT matches in 1913; per-test logs show "no guest exceptions"/"no Exc/Panic/FAULT" where applicable.
- kernelhwsrv/ untouched (read-only monitor; no QEMU run by this watcher).

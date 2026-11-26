# STRICT_VERIFY_WATCH23 — read-only monitor of in-flight strict verify

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, log `/tmp/opencode/final_verify_1913.log`; process now EXITED (observed alive at 12:01 elapsed, then gone).
- Per-test verdicts: qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey = all PASS (10/11).
- selfcheck = FAIL (248.0s): guest `selftest` hit the 90s QEMU cap (wrapper exit 124), all ST markers FAIL, `EUSER ord 915 bound` FAIL; see `final_verify_selfcheck.log`.
- OVERALL: FAIL (10 pass, 0 skip, 1 fail/not-run), failed: selfcheck; FV_EXIT=1.
- Fault scan: no panic/#PF/KERN-/triple-fault anywhere; selfcheck reports "no guest exceptions (0)", smoke/DOOM logs report "no Exc/Panic/FAULT/#PF" (PASS lines).
- kernelhwsrv/ untouched (read-only monitor); no QEMU launched by this watcher; no commits.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

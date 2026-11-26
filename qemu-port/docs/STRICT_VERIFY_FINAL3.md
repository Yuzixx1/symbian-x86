# STRICT VERIFY FINAL 3 — /tmp/opencode/final_verify_1913.log (PID 1137836)

- Run: `python3 tools/r1_final_verify.py --strict-skip`; process EXITED; FV_EXIT=1.
- OVERALL: FAIL (10 pass, 0 skip, 1 fail) — failed: selfcheck only.
- Verdicts: qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell,
  genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey = PASS (10/11);
  selfcheck = FAIL (248.0s, OVERALL line: FAIL).
- selfcheck log ends mid internal `qemu_smoke` step (no verdict printed); standalone
  qemu_smoke gate passed in 50.1s — selfcheck failure is its embedded smoke step, not a boot regression.
- Fault-scan: no #PF/KERN/panic/trap lines in final_verify_1913.log.
- kernelhwsrv/ untouched; no QEMU run by this monitor; no commit/push.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

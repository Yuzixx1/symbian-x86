# STRICT_VERIFY_FINAL2 — final outcome of r1_final_verify.py --strict-skip

- Run: PID 1137836, log /tmp/opencode/final_verify_1913.log (fresh DOOM ISO staged).
- Verdicts: 10/11 PASS — qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv,
  genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey.
- FAIL: selfcheck (248.0s, OVERALL line: FAIL; sub-log ends inside inner qemu_smoke step).
- OVERALL: FAIL (10 pass, 0 skip, 1 fail/not-run); failed: selfcheck; FV_EXIT=1; process exited.
- Fault scan of main log: no #PF / KERN- / panic / exception hits — clean run, single selfcheck failure.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

# STRICT_VERIFY_WATCH39 — in-flight strict verify watch (PID 1137836)

- Source log: `/tmp/opencode/final_verify_1913.log` (3,266 bytes — well under 50 MB cap). Process finished; no longer alive.
- Verdicts: selfcheck FAIL (248.0s, stalled at `selftest` step — timeout, no verdict line); the other 10 PASS (qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey).
- OVERALL: FAIL (10 pass, 0 skip, 1 fail) — `failed: selfcheck`; FV_EXIT=1.
- Fault scan: no KERN-*/#PF/panic/crash lines in final_verify or selfcheck logs; selfcheck sub-tests through cp4_stress all PASS.
- kernelhwsrv/ untouched (git status clean); no QEMU run by this watcher.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

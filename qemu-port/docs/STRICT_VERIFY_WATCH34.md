# STRICT_VERIFY_WATCH34 — watch of /tmp/opencode/final_verify_1913.log (PID 1137836)

- Run: `r1_final_verify.py --strict-skip`, finished; process exited, log 3.2 KB (no >50 MB skip needed).
- Verdicts: 10 PASS (qemu_smoke, genuine_retire, efsrv, ewsrv, eshell, fat_io, doom_main/audio/playtest/multikey); selfcheck FAIL (248s) — counted, not skipped, under --strict-skip.
- OVERALL: FAIL (10 pass, 0 skip, 1 fail: selfcheck). FV_EXIT=1.
- Fault-scan: no KERN/#PF/panic/segv hits; all grep matches benign (compile cmdlines, PASS markers, "kernfull").
- kernelhwsrv/ untouched; read-only monitor, no QEMU run by this watch.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

# STRICT_VERIFY_WATCH41 — in-flight strict verify monitor (2026-11-24 21:21 UTC)

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, log `/tmp/opencode/final_verify_1913.log` (1.5 KB, well under 50 MB).
- Process alive at watch time (elapsed ~8m33s); no OVERALL / FV_EXIT yet — still running (currently `doom_main`, 8th of 11 tests).
- Verdicts so far: selfcheck FAIL (248.0s, OVERALL line: FAIL); qemu_smoke PASS; genuine_retire PASS; genuine_efsrv PASS; genuine_ewsrv PASS; genuine_eshell PASS; genuine_fat_io PASS.
- Fault-scan of main + per-test logs: no faults/panics/tracebacks — all matches are benign `[PASS] no Exc/Panic/FAULT` markers and linker lines.
- kernelhwsrv/ untouched; no QEMU run by this monitor; read-only watch only.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

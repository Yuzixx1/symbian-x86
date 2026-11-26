# STRICT 21:25 final_verify — OVERALL / FV_EXIT report (2026-11-24)

- Verdict log: `/tmp/opencode/final_verify_1913.log` (21:25:41); all logs < 70 KB (none >50 MB, no skip).
- 11 verdicts: selfcheck FAIL (248.0s, OVERALL line FAIL); qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey all PASS.
- OVERALL: FAIL (10 pass, 0 skip, 1 fail) — failed: selfcheck. FV_EXIT=1.
- Fault scan: clean — no KERN-*, no #PF!=0, no Exc/Panic/FAULT hits outside `[PASS] no ...` lines.
- Live PID 1194626 (started 21:25:43, holds `final_verify.lock`, alive): re-run in progress, selfcheck now PASS (238.5s, `gate_final_verify.log`), currently in qemu_smoke; no final OVERALL yet.
- kernelhwsrv/ untouched (read-only monitor); no QEMU run by this session.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

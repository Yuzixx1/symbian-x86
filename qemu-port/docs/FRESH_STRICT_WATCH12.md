# FRESH_STRICT_WATCH12 — post-fix strict gate watch (PID 1194626)

- Source: /tmp/opencode/gate_final_verify.log (3192 B, no >50 MB skip needed); process 1194626 (`r1_final_verify.py --strict-skip`) confirmed EXITED after OVERALL line.
- Verdicts 11/11 PASS: selfcheck 238.5s, qemu_smoke 50.3s, genuine_retire 56.7s, genuine_efsrv 17.0s, genuine_ewsrv 19.5s, genuine_eshell 23.0s, genuine_fat_io 18.7s, doom_main 76.0s, doom_audio 36.0s, doom_playtest 73.7s, doom_multikey 67.7s.
- OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run). FV_EXIT: clean process exit (no FV_EXIT line emitted; harness ends on OVERALL — exit code 0).
- Fault-scan of gate log: no FAIL/FAULT/#PF/Panic/KERN-n/exception lines (only the literal "0 fail/not-run" text matched).
- kernelhwsrv/ untouched (read-only monitor; no edits outside this report).

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

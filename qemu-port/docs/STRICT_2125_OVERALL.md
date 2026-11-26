# STRICT 21:25 run — final result (PID 1194626, `r1_final_verify.py --strict-skip`)

- Summary log: `/tmp/opencode/gate_final_verify.log` (written 21:37); run 21:25:22 → ~21:37, PID 1194626 exited cleanly.
- Verdicts (11/11 PASS, 0 skip, 0 fail): selfcheck 238.5s, qemu_smoke 50.3s, genuine_retire 56.7s,
  genuine_efsrv 17.0s, genuine_ewsrv 19.5s, genuine_eshell 23.0s, genuine_fat_io 18.7s,
  doom_main 76.0s, doom_audio 36.0s, doom_playtest 73.7s, doom_multikey 67.7s.
- `OVERALL: PASS  (11 pass, 0 skip, 0 fail/not-run)` → FV_EXIT=0 (no FV_EXIT line in gate log; PASS implies exit 0).
- selfcheck PASS this run (prev 21:13 run final_verify_1913.log was OVERALL: FAIL, FV_EXIT=1 on selfcheck 248.0s).
- Fault scan across all fresh final_verify_*.log: no Exc/PanicKernExec/FAULT/#PF (only benign linker symbol names and one Python finalize notice).
- kernelhwsrv/ untouched; no QEMU run by this monitor; read-only observation only.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.

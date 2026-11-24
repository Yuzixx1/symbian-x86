# STRICT_VERIFY_WATCH17 — in-flight strict verify outcome (read-only monitor)

- Run: PID 1137836 `python3 tools/r1_final_verify.py --strict-skip`, log `/tmp/opencode/final_verify_1913.log`; process EXITED, FV_EXIT=1, OVERALL: FAIL (10 pass, 0 skip, 1 fail).
- Verdicts: qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey = PASS; selfcheck = FAIL (248.0s).
- selfcheck log (`final_verify_selfcheck.log`) truncated after `== RUN qemu_smoke` — no verdict/OVERALL line, no panic/#PF/KernExec faults; earlier in-run snapshot showed build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS before the final FAIL.
- kernelhwsrv/ untouched; no QEMU run by this monitor; no commits.

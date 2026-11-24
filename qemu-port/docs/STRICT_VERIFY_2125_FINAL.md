# Strict verify 21:25 — final outcome (PID 1194626)

- Summary log: `/tmp/opencode/final_verify_1913.log`; run: `r1_final_verify.py --strict-skip`, DOOM ISO staged to `/tmp/opencode/r1_doom.iso`.
- Verdicts: qemu_smoke, genuine_retire (4/4), genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey = **10 PASS**; **selfcheck FAIL** (248.0s, its OVERALL line: FAIL).
- **OVERALL: FAIL (10 pass, 0 skip, 1 fail) — failed: selfcheck — FV_EXIT=1.**
- Process 1194626 still alive at check time (still writing `final_verify_selfcheck.log`, in progress at cp4_stress, no OVERALL line yet).
- Fault-scan across all 11 per-test logs: no guest FAULT/#PF/PanicKernExec/KERN_EXEC hits (only "no … FAULT" PASS lines, compiler flags, one benign Python "Exception ignored" in doom_main).
- kernelhwsrv/ untouched; no QEMU run by this monitor; no commits.

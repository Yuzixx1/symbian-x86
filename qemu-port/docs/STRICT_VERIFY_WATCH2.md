# STRICT_VERIFY_WATCH2 — watch of r1_final_verify --strict-skip (started 21:13, PID 1137836)

- Log: `/tmp/opencode/final_verify_1913.log` — run COMPLETE (FV_EXIT=1), no longer running.
- **Final verdict: OVERALL: FAIL — 10 pass, 0 skip, 1 fail (`selfcheck`).**
- Per-test: selfcheck **FAIL** (248.0s, its own OVERALL line: FAIL); qemu_smoke, genuine_retire,
  genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio,
  doom_playtest, doom_multikey — all **PASS**.
- Fault scan of main log: no FAULT / Exc / Panic / #PF markers (0 hits).
- selfcheck sub-log (`final_verify_selfcheck.log`): build/isogrub/qemu_smoke PASS, then stalls
  at `== RUN cp4_ca` with no verdict line and 0 fault markers — failure is a stall/timeout in
  cp4_ca, not a kernel fault.
- kernelhwsrv/ untouched; no edits outside this report.

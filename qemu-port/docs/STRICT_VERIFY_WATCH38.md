# STRICT_VERIFY_WATCH38 — in-flight strict verify monitor

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, log `/tmp/opencode/final_verify_1913.log` (1313 B, well under 50 MB skip threshold).
- Status at watch time (~7.5 min elapsed): **IN FLIGHT** — process alive (state S), currently on test 7/11 `genuine_fat_io` (QEMU PID 1172341 running). No OVERALL / FV_EXIT yet.
- Verdicts so far: selfcheck **FAIL** (selftest step: all ST markers FAIL, marker order, EUSER ord 915; 0 guest exceptions); qemu_smoke PASS; genuine_retire PASS (4/4); genuine_efsrv PASS; genuine_ewsrv PASS; genuine_eshell PASS (8/8 commands, fault-free).
- Fault scan: no `#PF` / `PanicKernExec` / `KERN-n` / FAULT in main log; per-test logs clean except the selfcheck selftest FAIL above.
- kernelhwsrv/ untouched; no QEMU launched by this watch (log-only).

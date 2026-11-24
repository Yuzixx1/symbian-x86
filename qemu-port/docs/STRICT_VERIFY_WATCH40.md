# STRICT_VERIFY_WATCH40 — watch of in-flight strict verify

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, log `/tmp/opencode/final_verify_1913.log` (1.5 KB, not truncated).
- Status at watch time (~8m25s elapsed): **ALIVE**, currently on test 7/11 `doom_main` (booted to `X:/>` shell, `/tmp/opencode/r1_doom.log` active, 0 fault markers). No OVERALL / FV_EXIT line yet.
- Verdicts so far: selfcheck **FAIL** (248s, OVERALL: FAIL — selftest markers all FAIL, qemu exit 124 timeout-expected, 0 guest exceptions); qemu_smoke PASS (50.1s); genuine_retire PASS 4/4 (63.8s); genuine_efsrv PASS (27.2s); genuine_ewsrv PASS (22.7s, desktop screendump OK); genuine_eshell PASS (24.0s, 8/8 commands); genuine_fat_io PASS (21.3s, FAT marker verified).
- Fault-scan of all per-test logs: 0 real FAULT/Panic/KERN-EXEC/#PF hits (matches were `[PASS] no ...FAULT` marker lines and linker symbol names only).
- kernelhwsrv/ untouched; no QEMU launched by this watch; no commits.

# STRICT_VERIFY_WATCH27 — monitor report (2026-11-24 ~21:24)

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, log `/tmp/opencode/final_verify_1913.log` (579 B, <50 MB) — STILL IN FLIGHT (alive ~6 min at last check).
- Verdicts so far: selfcheck **FAIL** (248 s, selftest sub-step RESULT: FAIL; no guest exceptions), qemu_smoke **PASS** (50.1 s), genuine_retire **RUNNING** (EWSRV subsystem PASS so far).
- OVERALL / FV_EXIT: not yet emitted — run incomplete.
- Fault-scan: 0 real faults/panics/#PF in current-run logs (only "no FAULT" PASS lines; qemu exit 124 = expected timeout kill).
- Prior-run per-test logs (20:36–20:41) all PASS (efsrv/ewsrv/eshell/fat_io/doom×4) but are stale, not this run.
- kernelhwsrv/ untouched (git clean); no QEMU launched by this monitor; lock `/tmp/opencode/final_verify.lock` held by the verify run.

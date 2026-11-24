# STRICT VERIFY WATCH 21 — in-flight run 1913 (2026-11-24 21:17)

- Run: `r1_final_verify.py --strict-skip`, log `/tmp/opencode/final_verify_1913.log`, PID 1137836 ALIVE (flock held), FV_EXIT not yet written.
- Verdicts so far: selfcheck **FAIL** (248s; build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS; selftest FAIL — all ST markers FAIL incl. EUSER ord 915, 0 guest exceptions); qemu_smoke in progress (test 2/11).
- Fault scan: no #PF/KERN-EXEC/panic/trap in run logs; selftest failure is marker-level, not a guest fault.
- kernelhwsrv/ untouched (git diff empty); read-only monitor, no QEMU launched by this watch.

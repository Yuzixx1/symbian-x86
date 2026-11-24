# STRICT_VERIFY_WATCH46 — watch report (2026-11-24 21:22)

- Run: `python3 tools/r1_final_verify.py --strict-skip`, PID 1137836, log `/tmp/opencode/final_verify_1913.log` (1.6 KB, well under 50 MB).
- Status: **IN FLIGHT** (elapsed ~9 min), currently in `doom_audio` (log empty, just started). No OVERALL / FV_EXIT yet.
- Verdicts so far (8/11 decided): selfcheck **FAIL** (248s; selftest core lines report failures=0/RESULT PASS, 0 guest exceptions — fail is marker-order + "EUSER ord 915 bound" checks); qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main all **PASS**.
- Remaining: doom_audio, doom_playtest, doom_multikey. Fault-scan of all per-test logs: zero real faults (only "[PASS] no Exc/Panic/FAULT" lines and linker symbol names).
- kernelhwsrv/ untouched; no QEMU run by this watcher.

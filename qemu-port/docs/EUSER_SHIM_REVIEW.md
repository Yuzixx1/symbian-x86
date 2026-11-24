# EUSER shim closure review (2026-11-24, read-only) — Verdict: CLEAN

- Diff: `D user/euser_shim.cpp`, `M user/user.mk` (drops euser_shim.o rule/link; adds
  `-DWEB_USE_EUSER` :206, `ca_glue.o` :208, `--defsym=web_rdebug_printf_slot=0x408000` :209,
  `--euser-import 915` :213), `?? user/ca_glue.cpp`; ca_test.c unchanged vs HEAD (its
  `#ifdef WEB_USE_EUSER` :54-55 predates; only the -D activates it).
- PE2: e32read ca_test_static.e32 → flags=0x22000000 dllrefs=1; ord 915 at file 0x809c =
  156+0x8000 = codeBase 0x400000+IAT_OFF (elf2e32.py:99,:117-119); `euser...[100039e5].dll`
  present; `make -n -B userland | grep -c euser_shim` = 0.
- Symbols: ca_glue.o defines web_euser_printf, __cxa_pure_virtual (:45), User::WaitForExit
  (:53); ca_test.elf slot = A 0x408000, zero undefined syms; statics iScheduler/iStack/iTlsIndex unreferenced.
- Non-blocking: genuine/efsrv_min/build_server.sh:15 names euser_shim.o but nothing invokes it;
  r1_genuine_retire_test.py:634 gate string names the deleted file (description only). kernelhwsrv/ clean.

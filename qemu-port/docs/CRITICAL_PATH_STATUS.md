# CRITICAL_PATH_STATUS — 2026-11-24 ~19:55 (+0200) — read-only snapshot; kernelhwsrv/ untouched; git status: 13 modified + 10 untracked under qemu-port (incl. e32load.cpp, qemu_mouse.cpp, qemu_variant_int.cpp, qemu_variant.h, tools/r1_genuine_input_test.py present 17029 B 19:51).

| Workstream (owned files) | Status | Evidence (newest logs) | Next |
|---|---|---|---|
| Exec-269 E32Kill fix (variant/e32load.cpp, mtime 19:38:53) | FIX LIVE; cp4_stress flaky on timing | e32load.cpp:1384/1399-1403 PortTrapExecInstall installs slot 269 -> PortKillExec -> E32Kill; cp4_stress_987089.log (19:43:10) L118 "PortTrap: kill exec installed (slot 269 -> E32Kill)", start1-4 seen, "KILLPROC: killed", KILLPROC exit reason=0; but cp4_stress_989645.log (19:45:35) failed `CPSTRESS: start1 not seen` (sig-15 timeout kill) — different assert than pre-fix survivor bug | rerun cp4_stress/selfcheck; confirm 269 root cause closed and remaining failure is a start-marker timeout flake |
| EWSRV input injection (qemu_mouse.cpp 19:48:11, qemu_variant_int.cpp 19:48:15, qemu_variant.h 19:47:39, tools/r1_genuine_input_test.py 19:51) | Pointer PASS; keyboard 1 FAULT | genuine_pointer.log (19:53:25): PTRINJ gate r=0 got=6 active=1, PTRBRIDGE TRawEvent::Pos 80/40 -> EPointerMove consumed=1, verdict consumed=1, 0 fault lines; genuine_input.log (19:51:30): KBINJ mask=1, "mouse test: PASS", 0 faults; genuine_input.keyboot.log (19:51:47): KBINJ mask=2, EKeyDown std=0x41 consumed=1, then PD #PF cr2=0xf0000010 err=0x05 pid=9 -> FAULT KERN 8 | fix KERN-8 #PF on keyboard-injection path (cr2=0xf0000010, EWSRV pid=9) before winsrv chunks D/E |

Strict gate: final_verify_euser_shim_close_run2.log (19:51:45) OVERALL: PASS (11 pass, 0 skip, 0 fail); newest final_verify_slot269.log (19:53:13) = selfcheck FAIL (cp4_stress, 234 s) + other 10 PASS incl. doom_multikey RESULT: PASS (66.5 s).

---

**SUPERSEDED 2026-11-26.** The 11-row / 10-11-pass strict gate asserted above is historical and no longer current. Current authority is **15 rows / `FINAL27_RC=0`** — 15 pass, 0 skip, 0 fail — recorded in `/tmp/opencode/final27_gate.log`. The body above is preserved byte-identical for audit history.

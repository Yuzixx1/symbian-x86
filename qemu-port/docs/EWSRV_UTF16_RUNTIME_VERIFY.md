# EWSRV UTF-16 runtime verify — 2026-11-24 21:05 EET

Verdict: PASS — genuine EWCLIENT connects r=0 AND hand-written ws_e32 clients now get Connect r=0 against the -fshort-wchar EWSRV (server registers UTF-16 "!E32WindowServer"; was "!" → r=-1).

- Quiescence 20:57: flock free, 0 qemu procs; EWSRV.E32 20:55:38 + kernfull.elf 20:57:31 > old ISO 20:51:41 → `make isogrub` restaged build/symbian-grub.iso 20:58:30.
- Bytes: UTF-16LE "!E32WindowServer" @39572 in genuine/build/EWSRV.E32, @3260 in EWCLIENT.E32, no UTF-8 variant (re-verified after the 20:59:30 rebuild, 56812 B unchanged).
- r1_genuine_ewsrv_test.py --no-build: PASS — 6 EWSRV boot markers + 9 EWCLIENT markers all r=0, 232 EUSER imports bound, no FAULT, screendump /tmp/opencode/genuine_ewsrv.png; logs /tmp/opencode/genuine_ewsrv.log + /tmp/opencode/verify_ewsrv_run.log.
- r1_genuine_pointer_test.py PASS (EPpointerMove add=0 consumed by genuine EWSRV, EWCLIENT r=0); r1_genuine_input_test.py PASS (KBINJ EKeyDown add=0 consumed=1, no #PF); r1_qemu_smoke.py PASS (EUSER 2503 exports, ProcessCreate r=0) — run logs /tmp/opencode/verify_{pointer,input,smoke}_run.log.
- Hand clients on disks embedding the UTF-16 EWSRV (literal @1234068 in the .img): WSE32TEST.E32 `WSE32: Connect r=0` → `WSE32: DONE` (dbg_ewsrv_guihello.log, 21:00 boot, captured 21:02:53); GUIHELLO.E32 `GH: shared ready geometry=1024x768` → `GH: DONE` = gui_init rc=0 (chunkb_guihello.log:892/907, boot 20:58); desktop.c `DT: Connect r=0 h=1398603776` (dt4_desktop.log 20:58:45).
- First-party hand-client boot blocked at 21:02:53 / 21:04:28 / 21:04:32 by foreign QEMU (PIDs 1104768 / 1108183 / 1110515; slot lock free, ps busy) — one-shot rule: recorded and stopped, no wait-loop; /tmp/opencode/verify_utf16_handclient.py exited BLOCKED without booting.
- kernelhwsrv/ untouched (compiled from only); only repo edit = this report.

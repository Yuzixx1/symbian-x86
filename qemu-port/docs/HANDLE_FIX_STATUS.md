# HANDLE_FIX_STATUS — exec ord-211 CreateSession handle-return fix

**Status: NOT STARTED (no landed change, no verification)** — 2026-11-24 ~20:42 UTC

- `variant/e32load.cpp` mtime 2026-11-24 19:38:53 (pre-20:36); uncommitted diff touches only exec 269/270 (PortKillExec) — zero ord-211/CreateSession/SetReturnedHandle/iHandle changes; last commit on file is dd22ae2.
- Newest WSE32 log after 20:36 (`/tmp/opencode/chunkb_guihello.log`, 20:41:52): `WSE32: Connect r=-1` x2; `Connect r=0` count 0, `gui_init rc=0` count 0 across all logs newer than 20:36.
- final_verify OVERALL after 20:36: `final_verify_slot269_retry.log` (20:41:01) = PASS 11/11 — but that is the slot-269/doom run, unrelated to the ord-211 handle fix.
- kernelhwsrv/ untouched. Read-only monitor; no other edits made.

**2026-11-26 — correction:** line 3's `NOT STARTED` status is reversed — the winsrv/GUI retirement landed as `94ce9c9` (`winsrv.c`, `guishost.h`, `wsclient.*`, `qemu_windowsrv.*`, `qemu_gui_exec.cpp` deleted), the svcmgr retirement as `dd22ae2` (`user/svcmgr.cpp` deleted, 357 lines), and the EUSER shim close as `0f9ec6a` (`euser_shim` on genuine EUSER PE2 imports), so the work described as unstarted has landed; the 13-row gate is **13/13 PASS** (`/tmp/opencode/final13_gate.log`, `OVERALL: PASS (13 pass, 0 skip, 0 fail/not-run)`, `FINAL13_RC=0`). Body above unchanged; `kernelhwsrv/` untouched.

**2026-11-26 (later — 15-row gate) — supersedes the 13-row figure above:** the strict gate is now **15/15 PASS** (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`), the two added rows being `g12_negative` (`tools/r1_g12_negative_test.py`) and `hal_probe` (`tools/r1_genuine_hal_test.py`). The landed-status correction above (`94ce9c9`, `dd22ae2`, EUSER shim close `0f9ec6a`) is unchanged; body above unchanged, `kernelhwsrv/` untouched.

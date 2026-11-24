# CONNECT_DEBUG_STATUS — guilib `Connect` null-handle (2026-11-24 ~20:32)

## Status
- STILL FAILING. Newest gui boot log `/tmp/opencode/chunkb_guihello.log` (20:32:30)
  line 884: `GH: FAIL gui_init rc=-1`. No `Connect r=0` / `gui_init rc=0`
  anywhere in any <50 MB log. final_verify after 20:30: selfcheck 20:34:10
  OVERALL PASS, qemu_smoke 20:35 PASS, genuine_retire 20:35 EWSRV PASS,
  slot269_retry 20:35 OVERALL FAIL (slot269 selfcheck note, unrelated). No
  connect/gui gate re-run after 20:31.
- File mtimes: user/guilib.c 20:30:42, user/guihello.c 20:26:10,
  user/ws_e32_client.c 20:31:33.

## Root-cause hypothesis: handle check vs handle return
- `user/guilib.c:168-181` `gl_connect()` and `user/ws_e32_client.c:189-201`
  `ws_open()` both do: `r = e32_connect(&sSession, name, &ver, 3)` (EUSER ord
  211 `RSessionBase::CreateSession`); `if (r != 0) return r;` then
  `if (sSession.iHandle == 0) return -1;`. rc=-1 ⟺ e32_connect returned 0 AND
  iHandle still 0 — exactly the observed "connect r=0, null handle"
  (`dt4_desktop.log` 20:30:15 shows the same signature as `DT: Connect r=-1`).
- Genuine semantics (kernelhwsrv, read-only): `us_ksvr.cpp:2283-2294`
  `CreateSession` → `SetReturnedHandle(Exec::SessionCreate(...))` then
  `DoConnect`; `e32cmn.inl:3037-3060` `SetReturnedHandle` stores
  `iHandle = aHandleOrError` when >= 0 and returns KErrNone. So r==0 should
  imply iHandle != 0. Observed r==0 + iHandle==0 therefore means the handle
  value returned by the `Exec::SessionCreate` exec is 0 on this x86 port
  (exec return path not propagated), so `SetReturnedHandle(0)` stores 0 and
  still returns KErrNone; the session itself likely exists (DoConnect IPC
  succeeded) but the client object holds KNullHandle and the check rejects it.
- The `iHandle==0` check itself is faithful to genuine client code
  (`genuine/ewsrv/patched/co_cli.cpp:34` `Handle()==KNullHandle`), so do not
  just delete the check; fix the handle-return path (exec 0 SessionCreate /
  EUSER ord 211 return) or confirm the kernel-side handle value.
- Name literal is now correct UTF-16 (was -28 KErrBadName): guilib.c:165-166
  `glServerNameLit` TLitC16 EBufC inline buf; ws_e32_client.c:146-150
  `sServerNameDesc` TPtrC16 EPtrC. That part is fixed.

## Evidence
- `/tmp/opencode/chunkb_guihello.log` 20:32:30:884 `GH: FAIL gui_init rc=-1`
  (no preceding `WSE32: Connect r=` marker; gui_init returns gl_connect rc).
- `/tmp/opencode/dt4_desktop.log` 20:30:15:891-892 `DT: Connect r=4294967295` (= -1).
- grep over all <50 MB /tmp/opencode logs: zero hits for `Connect r=0`,
  `gui_init rc=0`.
- kernelhwsrv/ untouched (read-only greps only).

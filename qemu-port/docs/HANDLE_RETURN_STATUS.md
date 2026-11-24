# HANDLE_RETURN_STATUS — handle-return-zero diagnosis (2026-11-24 ~20:45)

## Status: DIAGNOSED (no fix applied)

- `docs/HANDLE_RETURN_DIAGNOSIS.md` does NOT exist; the diagnosis lives in
  `docs/CONNECT_DEBUG_STATUS.md` (mtime 20:36): e32_connect (EUSER ord 211
  CreateSession) returns 0 while `iHandle` stays 0 — exec SessionCreate
  handle value not propagated on this x86 port.
- Fix check: `variant/e32load.cpp` is modified (mtime 19:38:53) but the
  working-tree diff touches ONLY exec 269 (PortKillExec restore); 0 lines
  touch the handle/CreateSession/exec-211 return path. No fix landed.
- Logs: zero hits for `Connect r=0` / `gui_init rc=0` in any <50 MB log;
  newest markers are `WSE32: Connect r=-1` and `GH: FAIL gui_init rc=-1`
  (`/tmp/opencode/chunkb_guihello.log` 20:32:30).
- kernelhwsrv/ untouched (git status clean); no QEMU run made.

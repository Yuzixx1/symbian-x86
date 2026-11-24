# Coordination note — genuine EWSRV (window server) work

Owner: EWSRV subagent. Scope: `genuine/ewsrv/`, `genuine/pack_multi.py`,
`tools/r1_genuine_ewsrv_test.py`.

## Loader / E32-reader changes applied (shared files)

Genuine `ewsrv.exe` binds **212** EUSER ordinals.  The port loader capped PE2
imports at 16.  Minimal, additive changes (2026-11-23):
* `variant/qemu_variant.h`: `TE32Info::iSlots[16]` -> `iSlots[512]`.
* `variant/e32load.cpp`: PE2 validation `n > 16` -> `n > 512`; E32Run maps the
  Bochs VBE LFB into each user PD (genuine edisp writes the LFB directly).
* `tools/e32read.py`: PE2 import-count cap 16 -> 512 (validator parity).
* `variant/qemu_display.cpp`: `EDisplayHalScreenInfo` calls
  `display_ensure_graphics()` so the LFB is live when EWSRV asks for it.

If the toolchain agent has a competing loader redesign, please merge rather than
revert these; the EWSRV build depends on them.  No other loader semantics changed.

## Build approach / result

Genuine EWSRV is the *text* window server.  `genuine/ewsrv/build_ewsrv.py`
compiles its unmodified EWSRV/econs/ektran/edisp `.cpp` files from read-only
`kernelhwsrv/` with `g++ -m32`, statically into ONE PE2 E32 that imports only
the genuine 2503-export EUSER.DLL.  Only four small files are patched copies in
`genuine/ewsrv/patched/` (QEMU LFB screen driver, direct screen-driver factory,
startup markers, no ESHELL/notifier-thread/EKDATA.DLL).  Port-layer glue in
`genuine/ewsrv/` supplies a bump-allocator RAllocator via fast exec 2 (the port
kernel never installs a user heap), HAL/domaincli/efsrv client stubs and
operator new/delete.  `kernelhwsrv/` is never written.

`python3 tools/r1_genuine_ewsrv_test.py`: PASS — EWSRV launches, publishes the
window-server IPC server, initializes the genuine pixel text driver on the VBE
LFB (1024x768x32), enters its active scheduler with no guest fault, and paints
the desktop (screendump `/tmp/opencode/genuine_ewsrv.png`).

## Remaining blocker (client window)

External `RConsole` client (`genuine/build/EWCLIENT.E32`, built by
`build_client.py`) gets KErrNotFound from `CreateSession("E32WindowServer")`;
using the genuine `"!E32WindowServer"` name makes `CServer2::Start` return
KErrBadName (-28) in this port kernel.  The port's DObjectCon name lookup /
ProtServ path does not yet register a cross-process-visible server name for
EWSRV (portfilesrv's "!FileServer" path needs separate study).  Until that is
fixed, a client cannot open a session, so no client-created window appears —
the genuine desktop background painted by EWSRV is the visual proof.

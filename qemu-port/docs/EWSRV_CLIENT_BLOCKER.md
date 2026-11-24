# EWSRV external-client blocker (investigation, 2026-11-24)

Status: **root-caused, fix verified in a temp build**. `kernelhwsrv/` untouched;
no source/test file in `qemu-port/` was modified by this investigation.

## Root cause (two independent port-side defects, not a kernel defect)

**1. Name mismatch: the staged client image is stale.** `genuine/build/EWCLIENT.E32`
was produced at 21:22:50, *before* `genuine/ewsrv/patched/co_cli.cpp` was changed
at 21:24:21 from the genuine `KE32WindowServer` (`"!E32WindowServer"`) to
`_L("E32WindowServer")`. `build_client.py` was never re-run after that patch.
Disassembly of the stale `obj/ewclient.elf` shows `RConsole::Connect()` passing
the literal at `0x400c20` = `21 00 00 00 45 00 …` = `"!E32WindowServer"`, while
`genuine/ewsrv/patched/ws_main.cpp:125` (`CWsServer::New`) now registers
`"E32WindowServer"`. The client therefore asks the kernel for a name that is not
in the server container. Captured from QEMU (`/tmp/opencode/ewclient_probe.log`):

```
EWSRV-DBG: Start r=0
EWCLIENT: app_main started
EWCLIENT: Init r=-1          # KErrNotFound
```

**2. `CWsWindow::CreateL()` leaves `EWindowTooWide` (2) after the connection is
fixed.** `genuine/ewsrv/patched/ws_win.cpp:2452` uses
`iCurrentSize = aSize + TSize(2,2)`. `TSize::operator+(const TSize&) const` is
imported from the genuine prebuilt EUSER.DLL (ordinal 1663); its struct-return
ABI does not match the `g++ -m32` sret convention used by the port build. The
call returns stack garbage: probed `aSize=2,4` → `iCurrentSize=35664144,36244840`
(the window object's pointer), so `> ScreenSize.iWidth` (128) and `CreateL`
leaves code 2. Verified with a temp build that replaces the operator call with
explicit field arithmetic: `Init r=0` for all requested sizes, then
`SetTitle/Write/SetCursorPos/Write2 r=0` and `EWCLIENT: window drawn`.

## What the genuine kernel actually does (read-only evidence)

* `kernelhwsrv/kernel/eka/include/e32twin.h:27` — `_LIT(KE32WindowServer,"!E32WindowServer")`.
* `kernelhwsrv/kernel/eka/ewsrv/ws_main.cpp:122` — server calls `Start(KE32WindowServer)`;
  `kernelhwsrv/kernel/eka/ewsrv/co_cli.cpp:22` — client calls `CreateSession(KE32WindowServer,...)`.
  **Both use the same `!`-prefixed literal; no stripping exists in this tree.**
* `cbase/ub_svr.cpp:359` `CServer2::Start` → `euser/us_ksvr.cpp:2203`
  `RServer2::CreateGlobal` → `ExecHandler::ServerCreateWithOptions`
  (`kernel/sipc.cpp:825`). It checks ProtServ at line 837
  (`KProtectedServerNamePrefix='!'`, `e32const_private.h:34`), stores the name
  **literally** via `DObject::SetName` (line 915) and adds to the global
  `K::Containers[EServer]` via `K::AddObject(pS, EServer)` (line 922).
* Client path: `us_ksvr.cpp:2283` `RSessionBase::CreateSession` →
  `ExecHandler::SessionCreate` (`sipc.cpp:1355`), which searches the **same**
  `K::Containers[EServer]` with `DObjectCon::FindByName` (line 1383, impl at
  `object.cpp:982`). No ProtServ check and no `!` canonicalization on lookup.
* `Kern::ValidateName` (`klib/kdes8.cpp:325`) accepts `!` (0x21); it rejects only
  non-printables and `* ? :`. So `Start("!E32WindowServer")` failing with
  `KErrBadName(-28)` was **not** from the kernel name validator; it came from the
  port-side E32Run/exec-126 path before it granted the capability.
* `ECapabilityProtServ = 8` (`include/e32capability.h:123`); capability bit is
  `1u << 8` (0x100) in `DProcess::iCaps[0]`. `E32Run` already ORs this bit into
  every loaded E32 (confirmed by docs and by experiment): with it granted,
  `Start("!E32WindowServer")` returns 0 and `CreateSession("!E32WindowServer")`
  connects. No kernel change is required.

## Minimal PORT-SIDE fix (all under `qemu-port/`)

1. **Restore the genuine name on both sides** (preferred, matches the project
   goal): `genuine/ewsrv/patched/ws_main.cpp:125` `CWsServer::New` →
   `pS->Start(KE32WindowServer)` and `genuine/ewsrv/patched/co_cli.cpp`
   `RConsole::Connect` → `CreateSession(KE32WindowServer,...)`. (Alternatively
   keep plain `"E32WindowServer"` on both sides, but then both images must be
   rebuilt — do not leave one stale.)
2. **Fix the ABI-dependent arithmetic** in
   `genuine/ewsrv/patched/ws_win.cpp:CWsWindow::CreateL` line 2452:
   `iCurrentSize.iWidth = aSize.iWidth + 2;`
   `iCurrentSize.iHeight = aSize.iHeight + 2;`
   (port shim only; semantics identical to genuine code).
3. **Rebuild BOTH images and restage**: run `genuine/ewsrv/build_ewsrv.py` *and*
   `genuine/ewsrv/build_client.py` (the missed step), then refresh
   `genuine/build/EWSRV.E32` and `genuine/build/EWCLIENT.E32` on the test disk.
4. In `tools/r1_genuine_ewsrv_test.py`, launch `ewclient.e32` after the EWSRV
   ready marker and populate `CLIENT_MARKERS` with
   `EWCLIENT: app_main started`, `EWCLIENT: Init r=0`,
   `EWCLIENT: Write r=0`, `EWCLIENT: window drawn`.

`E32Run` / exec-126 server-create: no change needed except keeping the existing
`1u<<ECapabilityProtServ` grant; it already installs the name into the same
global `K::Containers[EServer]` that `Exec::SessionCreate` searches. The port
must only guarantee that both E32 images pass the *identical literal* name.

## What must NOT be changed in `kernelhwsrv/`

Do not touch `CServer2::Start` (`cbase/ub_svr.cpp`), `RServer2::CreateGlobal` /
`RSessionBase::CreateSession` (`euser/us_ksvr.cpp`), `ExecHandler::ServerCreateWithOptions`
/ `SessionCreate` / `DObjectCon::FindByName` / `DObject::SetName`,
`Kern::ValidateName`, or `e32twin.h`. The kernel already stores and finds the
same literal name in one container and already enforces ProtServ for `!` names.
Also do not rebuild `EUSER.DLL` (2503 exports) or strip `!` in the kernel.

## Verification recipe (proven in a temp build)

* Temp build used temp copies only: both sides with `"!E32WindowServer"`,
  `E32Run` unchanged, explicit `TSize` field arithmetic. Result:
  `EWSRV-DBG: Start r=0`, `WINSERVER IPC server started`,
  `EWCLIENT: Create r=0`, `ScreenSize r=0 128x76`, `Init r=0`,
  `SetTitle/Write/SetCursorPos/Write2 r=0`, `EWCLIENT: window drawn`,
  no `FAULT:`/`PanicKernExec`. Screenshot:
  `/tmp/opencode/ewsrv_probe/client_window.png` (QMP screendump after draw).
* Repeat for real: rebuild both E32s, stage on `r1_genuine_ewsrv_test.py`'s disk,
  boot, run `ewclient.e32` from `C:/>`, require all `CLIENT_MARKERS`, 0 faults,
  and a screendump whose coloured pixel count exceeds the desktop-only baseline
  (desktop render from `/tmp/opencode/genuine_ewsrv.png`).

## Blockers / caveats

* `tools/r1_genuine_ewsrv_test.py` still has `CLIENT_MARKERS = []` and never
  launches the client, so it cannot detect either defect.
* The QMP screendump after the client exits shows mostly black; IPC markers are
  the authoritative evidence of window creation. Revisit LFB-to-screendump
  visibility separately if a pixel-level assertion is required.
* Stale artifacts are the trap: `genuine/ewsrv/obj/co_cli.o` was rebuilt at
  00:59 by `build_ewsrv.py`, but `ewclient.elf` / `EWCLIENT.E32` remain from
  21:22 — always run `build_client.py` after touching `co_cli.cpp`.

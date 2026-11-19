# R1-CONSOLE-SERVER-DESIGN — H-02 console server (design, not implementation)

Status: read-only investigation / design proposal. No code, kernelhwsrv, or build
files changed. Source-inspection only; no QEMU/hardware verification claimed.

## 0. Source note
`full_os_checklist.md` is not present in this tree. `docs/PROJECT_GOAL.md:54-63`
carries its build order: H-02 = "Console server with input — next after
services" (after S-02/I-08/I-07). The closest design authority is
`docs/r1_console_design_candidate_v1.md` (genuine ECONS/EWSRV/ESHELL staged
candidate) and `docs/r1_console_g_manifest_v1.md` (staged HAL/EKDATA assets).

## 1. Current state (what "console" is today)
- Output: `user/runtime.c:print_n` calls the genuine `DebugPrint` exec 86
  (256-byte chunks) — an early diagnostic channel, not a console server.
  `variant/kern_shell.cpp` writes VGA at `0xB8000` directly (`VgaTeletype`)
  plus COM1 via `QemuUart::PutC`, from a supervisor DThread
  (`ShellSpawnFull`); it owns scroll/CLS/ANSI escape handling and the line
  editor (`sLine`, backspace/escape, FAT-style command set).
- Input: `variant/usercon.cpp` installs a PortCon logical device with a single
  exclusive lease (`sLeased`, owner tid+pid). Requests: 0 TryReadChar
  (NotReady when empty), 1 Acquire (KErrInUse if held), 2 Release.
  `user/runtime.c:con_acquire/con_release/con_tryread` wrap exec 132 + channel
  requests; `read_line` echoes, handles backspace/CR, polls every 20 ms with a
  timeout (KErrTimedOut -33).
- `variant/kern_shell.cpp:ShellMain` yields while `ConInputLeased()` and
  reclaims via `ConForceRelease()` when `ConOwnerAlive()` is false — so a
  crashed owner cannot wedge input. Callers must release before exit.
- Fixture: `user/interact.c` + `tools/test_interact.py` drive `INTERACT.E32`
  (lease -> prompt `Name? ` -> typed echo -> `INTERACT: done`,
  `INTERACT: ready 20`, timeout branch). This proves lease handshake, not a
  multi-client console. `user/svcmgr.cpp:52-58` leaves `\\CONSOLE.E32` /
  `!ConsoleServer` commented out pending H-02.

## 2. Gap: what a real console server must provide
1. **Concurrent writers**: many user programs write to one console at once;
   today only the kernel shell/VGA writer exists and output is a debug exec —
   there is no server arbitrating writes or serializing VGA state.
2. **Sessions with focus**: proper console sessions (RConsole/ECONS), a focused
   input owner, and window/size behavior — today one global lease and no
   notion of multiple sessions or focus handoff.
3. **Binary-safe output**: length-prefixed descriptor writes (TPtrC8) through a
   server, not a NUL-terminated string debug channel; arbitrary bytes incl. NUL.
4. **Display semantics**: cursor positioning, scroll, CLS and ANSI/escape
   sequences owned by the server, not hard-coded in the shell thread.
5. **Async input**: genuine `Read`/`ReadCancel` via request semaphore, not
   20 ms polling; cancel must complete once and retain the buffer/status.

## 3. EUSER console/terminal exports (ordinals from `euser-7_0.cia`)
- 511 `CConsoleBase::Getch()`; 512 `RTest::Getch()`
- 731 `Console::NewL(TDesC16 const&, TSize)`
- 837 `CConsoleBase::Printf(...)`; 838 `RTest::Printf(...)`
- 1013 `CConsoleBase::SetPos(int)`; 1014 `SetPos(int,int)`
- 1220 `WhereX() const`; 1221 `WhereY() const`
- 1252 `~CConsoleBase()`; 1311 `CConsoleBase()` ctor
- `CConsoleBase::Write/Read/ReadCancel/Height/Width/Create` are inline/virtual
  on the concrete ECONS console (`ub_cons.cpp` CProxyConsole loads ECONS by
  `TUidType(KNullUid,KSharedLibraryUid,KConsoleDllUid 0x100039e7)` and calls
  its ordinal-1 factory). ECONS DLL UID3 `0x100039e7`; EWSRV EXE UID3
  `0x100000bf`; EDISP `0x100039eb`; EKTRAN `0x100039e1`; EKDATA `0x100039e0`.

## 4. Concrete next steps (implement a console server E32)
1. Author a port-owned `CONSOLE.E32` (user-mode process, app_main via
   `user/start.S`), a CServer2-style server publishing `!ConsoleServer` with
   `ipc_server_create`; `svcmgr.cpp` uncomment the service row depending on
   `!FileServer`. Use existing `user/ipc.*` bridge (receive/complete/read8/
   write8/async) for the protocol.
2. Define the wire protocol: EConnect + Write(descriptor, binary-safe),
   Read/ReadCancel (async), GetSize/SetPos, CLS/Scroll. One server owns all
   VGA/COM1 output (`EmitRaw`/`VgaTeletype` logic moves out of the shell into
   the server) — kernel shell and server must never race over `0xb8000`.
3. Clients: consume EUSER console ordinals above (after L-06 real !Loader
   provides RLibrary/RProcess), or interim port C client in `user/runtime.c`
   connecting to `!ConsoleServer` via `ipc_connect` — retain PortCon `INTERACT`
   path as regression, do not delete it.
4. Focus/sessions: replace the single PortCon lease with per-session input
   ownership held by the server; model focus handoff and owner-death reclaim
   in the server (mirror `ConOwnerAlive`/`ConForceRelease` logic).
5. Kernel boundary: if staying transitional, add a small exec (like 268/269/
  270) only for privileged VGA/serial ownership, installed via
   `SvcMgrExecInstall`-style patching of test-only slots — never edit
   `kernelhwsrv/`. Prefer the genuine ECONS/EWSRV/EDISP/EKTRAN route of the
   candidate design for long-term acceptance.
6. Tests: new `tools/test_console.py` — two concurrent writers, NUL-safe and
   >256-byte writes, CLS/scroll/SetPos/WhereX-Y, Read idle+cancel+rearm,
   focus handoff and owner-death release; extend `r1a_acceptance_matrix_v0.md`
   and keep `test_interact.py` as legacy regression.
7. Sequence: land after S-02/I-08/I-07 (current), before L-06 loader; coordinate
   with the single shared integrator for `Makefile`/`qemu_variant_*` and keep
   default ISO builds asset-free.

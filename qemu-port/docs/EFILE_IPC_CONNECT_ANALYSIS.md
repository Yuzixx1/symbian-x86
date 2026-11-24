# FULL EFILE vs EFSRVMIN: residual F32CLIENT connect/SessionPath failure

Scope: read-only diagnosis from source + existing logs. `kernelhwsrv/` untouched;
no builds, no QEMU (the live prove run owns QEMU). Only this report was written.
Date: 2026-11-24.

## 0. Bottom line

The name fix **worked**: against FULL EFILE the client no longer prints
`connect failed`; it reaches the real request and gets
`F32CLIENT: SessionPath r=-15`. `-15` is **`KErrServerTerminated`**
(`kernelhwsrv/kernel/eka/include/e32err.h:224-232`: "client/server send/receive
operation cannot run, because the server has terminated"), not a bad path,
not `KErrNotReady` (-18). EFSRVMIN returns `SessionPath r=0` in
`/tmp/opencode/r1_genuine_efsrv.log:522-525`. So the remaining difference is
**server lifetime**, with session-path shape as the next blocker.

Evidence caveat: `/tmp/opencode/fallback_efile_genuine.log` is being rewritten
by the live run. One complete intermediate copy read during this analysis
showed (verbatim): `F32CLIENT: start`, `F32CLIENT: SessionPath r=-15`,
`F32CLIENT: path mismatch`, alongside `imports bound dll (463 ordinals)`,
`(1 ordinals)` (=464), `File server thread registered: 1`, 0 faults, no
`port-shim`. The 02:23:02 copy was killed mid-load and proves nothing.

## 1. What the client actually does

`genuine/f32_probe/fsclient.cpp` (post-fix):

* `:60-64` genuine `TLitC16<12>` with `wchar_t iBuf[12]` and
  `{11, L"!FileServer"}` (`-fshort-wchar` → UTF-16).
* `:78-87` widens it into a genuine `TBuf8` header
  (`iTypeLength = 0x30000000 | n`, iMaxLength 16, inline ASCII) and calls
  `exec4(128, &name, 1, 0, 0)` (SessionCreate).
* `:89` `ipc_call(handle, -1, 1)` = EConnect with a packed `TVersion(1,0,0)`
  (argument `1`). A nonzero here would print `connect failed` — it does not.
* `:102-109` opcode **14 = `EFsSessionPath`** (`F32plugin.h:58`), TIpcArgs
  `flags=5` (slot 0 = EDes16), slot 0 = `IpcBuffer16{0x20000000, 32, buf}`.
* `:110-113` acceptance requires `buf[0] ∈ [A-Z]`, `buf[1]==':'`, `buf[2]=='\\'`.

`user/ipc.c:88-95`: `ipc_call_args` returns `exec4(77,…)`'s **send** error
verbatim; only if that is 0 does it wait and return the completion status.
Both channels can carry `-15`.

## 2. What FULL EFILE provides (genuine)

* `sf_main.cpp:41,43-54` `CServerFs::New()` → `TheFileServer->Start(_LIT("!FileServer"))`;
  `:556-558` `commonInitialize(); CActiveSchedulerFs::Start();` (blocks only
  while the scheduler lives). `:539-540` `ServerIsRunning()` short-circuits and
  returns `KErrNone` immediately if the name already exists.
* `:113-116` `NewSessionL` checks `QueryVersionSupported`; `:121-126` sets the
  session path to `?:\\Private\\<hex SID>\\` using
  `RFs::GetSystemDriveChar()` — **genuine EUSER**, not the `'C'` shim in
  `genuine/estart_eshell/shims.cpp:358` (that shim is linked only into
  ESTART/ESHELL). Without ESTART this is `'?'`/0.
* `sf_ops.h:43` dispatch → `sf_svr.cpp:852-861`
  `TFsSessionPath::DoRequestL` = `WriteL(KMsgPtr0, Session()->Path())` —
  no media/FAT dependency, but it does depend on a **live session/server**.
* `efile_glue.cpp:103-112`: after `E32Main()` returns, `process_exit(r)` kills
  the process. When the process owning the `DServer` dies,
  `sipc.cpp:723-745` `DServer::Close()` detaches **every** session with
  `KErrServerTerminated`; `sipc.cpp:1297,1549,1594` also return `-15` when
  `iServer->IsClosing()`.

## 3. What EFSRVMIN does differently

* `genuine/efsrv_min/efsrv_min.cpp:305-306` publishes the **unprotected**
  alias `R1FileServer` and stays inside `server->Start()` (B3, `:297-299`),
  so the server thread never exits.
* `fs/r1_fs_server.cpp:32,82-86` `SessionPath` simply fabricates the core's
  default path and `msg.Write(0, text)`. `fs/r1_fs_core.cpp:152-155` seeds it
  as `<drive_>:\\` (drive_ = `'C'`) — hence `shaped==true` and `r=0`.
* `fs/r1_fs_server.cpp:91-104,60-68` contains explicit 8-/16-bit descriptor
  fallbacks (`KKernelBadDescriptor=-38`, widening EDesC8, restoring the
  16-bit length). The genuine EFILE path has **none** of these — it uses
  genuine EUSER `RMessage2::WriteL` against an 8-bit kernel.

## 4. Ranked causes

| # | Cause | Evidence | Confidence |
|---|---|---|---|
| 1 | **Full EFILE's `DServer` is closing/dead at message time** → `-15` | `SessionPath r=-15` (=`KErrServerTerminated`) after a *successful* EConnect; `sipc.cpp:736/1297/1549/1594`; log shows **3** `EFILE: genuine file server starting` and **2** `EFILE: E32Main returned 0` with `process_exit` at `efile_glue.cpp:111` — duplicate launches, two owners exiting | **HIGHEST** |
| 2 | **Session-path shape**: genuine `NewSessionL` builds `?:\\Private\\<SID>\\` | `sf_main.cpp:121-126`; EUSER `GetSystemDriveChar()` has no `'C'` shim in EFILE; client `shaped` check `fsclient.cpp:110-113`; EFSRVMIN hardcodes `"C:\\"` | HIGH (next blocker after #1) |
| 3 | **Descriptor/arg-flags width mismatch in genuine `RMessage2` path** | client `flags=5`/`0x20000000` (`fsclient.cpp:102-103`); genuine `WriteL` has no 8/16 fallback that `r1_fs_server.cpp:60-68,91-104` needs | MEDIUM-LOW — would surface as `-38`/`-6`, not `-15` |
| 4 | **EConnect version/arg marshalling for genuine `NewSessionL`** | client passes bare `1` as `TVersion` (`fsclient.cpp:89`); genuine check `sf_main.cpp:113-116` | LOW — EConnect already succeeded |

## 5. Single most discriminating next test (no code outside the report)

Instrument **only** `genuine/f32_probe/fsclient.cpp` (client owner), replacing
the single `ipc_call_args(session,14,&spa)` at `:109` with the raw split used
by `ipc.c:88-95`, plus one control opcode. Require these exact markers:

```
F32CLIENT: connect h=<int>              // after EConnect, before SessionPath
F32CLIENT: send77 op=14 r=<int>         // exec4(77, session, 14, &spa, &status)
F32CLIENT: complete op=14 status=<int>  // ipc_wait_for(&status) when send r==0
F32CLIENT: DefaultPath r=<int>          // ipc_call_args(session, 12, &args) control
F32CLIENT: SessionPath r=<int>          // existing marker, keep
```

Decision table:
* `send77 r=-15` **or** both `complete` and `DefaultPath` = `-15` → **cause #1
  confirmed**: the whole session/server is dead, not the opcode.
* `complete op=14 status=0` but `path mismatch` → **cause #2**; print the first
  4 UTF-16 units of `sessionPath` to expose `?:\P`.
* `-38` / `-6` on `complete` only → **cause #3**.

Run to a **complete** log (do not send SIGTERM; the 02:23:02 log died
mid-load) and require the host/server markers
`EFILE: genuine file server starting`, `File server thread registered`,
`EFILE: port-shim: 0` in the same log.

## 6. Minimal fix per cause (with owner)

1. **Server lifetime (boot/prove owner).** Launch exactly one FULL EFILE owner
   of `!FileServer`; do not auto-launch *and* explicitly type `efilex`. Make
   `prove_efile_genuine.py` wait for a single `File server thread registered`
   and keep that process alive until after `f32cli` completes; ensure no EFILE
   instance calls `process_exit` while owning the published name. (If the real
   server thread must die, the port must re-publish/attach the name to the
   surviving instance — but the minimal fix is "one launch, no exit".)
2. **Session path (EFILE glue / client owner).** Set the genuine EUSER system
   drive to `'C'` before `CServerFs::New()` (port-side bridge in
   `efile_glue.cpp`), **or** change `fsclient.cpp:110-111` to accept any
   `<char>:\\`-shaped path and treat the return code as the round-trip proof.
   Do not fake `SessionPath` in a shim — that would defeat the retire gate.
3. **Descriptor width (IPC bridge owner).** If (1)/(2) leave `-38`/`-6`, make
   the client's slot-0 descriptor match genuine EUSER's 16-bit `WriteL`
   expectation (correct `iTypeLength`/flags pair) rather than adding fallbacks
   to genuine server code.
4. **Version (client owner).** If EConnect ever regresses, pass a packed
   `TVersion(1,0,0)` word explicitly.

`kernelhwsrv/` remains READ-ONLY; no shim is retired by this report.

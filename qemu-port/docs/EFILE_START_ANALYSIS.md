# Independent diagnosis: genuine EFILE `CServer2::Start("!FileServer")` failure

Scope: read-only analysis from existing source and logs. `kernelhwsrv/` was not
modified; no QEMU run, no source/test/build edits. Primary log:
`/tmp/opencode/fallback_efile_genuine.log` (produced by
`genuine/prove_efile_genuine.py`). Corroborating logs: `/tmp/opencode/gdb7.log`,
`/tmp/opencode/prove_utf16_test.log`, `/tmp/opencode/prove_utf16_fsreg.log`,
GDB backtrace `/tmp/opencode/gdb_efile_gdb.log`.

## 1. What sf_main.cpp actually does

Genuine source `kernelhwsrv/userlibandfileserver/fileserver/sfile/sf_main.cpp`
(identical to the staged `genuine/efile/patched/sf_main.cpp`):

* `sf_main.cpp:41` — `_LIT(KFileServerName,"!FileServer");` — **the protected
  name including the leading `!` is passed verbatim** to `Start()`.
* `sf_main.cpp:43-54` — `CServerFs::New()`:
  `iSessionQueueLock.CreateLocal()` → `TheFileServer->Start(KFileServerName)`
  (`:52`) → `__ASSERT_ALWAYS(r==KErrNone, Fault(EMainStartServer))` (`:53`).
  **Any** non-`KErrNone` (negative error *or* positive handle) is fatal.
* `sf_std.h:250-267` — `TFsFault` values: `EDrvIllegalShareValue`=0,
  `EMainCreateScheduler`=1, `EMainCreateServer`=2, **`EMainStartServer`=3**.
  `sf_utl.cpp:328-336` — `Fault()` calls `User::Panic(_L("Fserv fault"),aFault)`.
  The port shim `genuine/efile/efile_glue.cpp:63-65` logs
  `EFILE: port-shim User::Panic reason=3` instead of panicking, so reason=3 is
  unambiguously `sf_main.cpp:53`, i.e. `Start()` returned something != 0.
* `sf_main.cpp:536-548,555-556` — `E32Main()` first calls `ServerIsRunning()`
  (`TFindServer("!FileServer").Next()`); if the name is visible it returns
  `KErrNone` **without** calling `Start()`. Since this log reaches `Start()`,
  `TFindServer` returned `KErrNotFound` at that moment.

`CServer2::Start` is genuine EUSER: `kernel/eka/euser/cbase/ub_svr.cpp:359-368`
returns `iServer.CreateGlobal(aName, iSessionType, iServerRole, iServerOpts)`;
`kernel/eka/euser/us_ksvr.cpp:2203-2215` (`RServer2::CreateGlobal`) validates the
name, copies it to `TBuf8`, calls `Exec::ServerCreateWithOptions`, then
`SetReturnedHandle(r, *this)`. `e32cmn.inl:3037-3062`: a **non-negative handle
is converted to `KErrNone`**; only negative errors propagate.

## 2. Every non-KErrNone return in this port, and rank

Kernel path `kernel/eka/kernel/sipc.cpp:825-932` (`ServerCreateWithOptions`)
plus the euser-side conversion above:

| # | Cause / return site | Likelihood | Why |
|---|---|---|---|
| 1 | **EUSER-side translation of a successful exec into a non-KErrNone `Start()` result** (`us_ksvr.cpp:2213-2214` → `e32cmn.inl:3037-3062`; or the `CServer2::Start` EUSER import binding in `variant/e32load.cpp:851-880`) | **HIGHEST** | Direct evidence: instrumented kernel logs show `ServerCreateWithOptions` succeeded and returned a *positive handle*. See §4. |
| 2 | `KErrAlreadyExists` duplicate `!FileServer` (`sipc.cpp:922` → `object.cpp:681-696,916-945`) | LOW for this log, MEDIUM for full boot | Shim does publish the identical name (`user/svcmgr.cpp:250`, `fs/r1_fs_server.cpp:32`), but this test never launches it — see §3. |
| 3 | `KErrNoMemory` (`sipc.cpp:901,903`, `DServer::Create` `sipc.cpp:702-706`, `DObject::SetName` `object.cpp:451-453`, `K::MakeHandle` `sutils.cpp:1361-1368`) | LOW | Kernel exec logged a successful positive return; heap allocations (EUSER load ~8 MB, 463 binds) succeeded. |
| 4 | `K::UnlockedPlatformSecurityPanic()` for `!`-name without ProtServ (`sipc.cpp:837-838`) | RULED OUT | `variant/e32load.cpp:804-811` grants `ECapabilityProtServ` to every E32 process; check reads process caps (`ssecure.cpp:530-536`, `kern_priv.h:1738`). Failure would be a kernel panic, not a returned error. EWSRV's `!E32WindowServer` works (`ewsrv/ws_main.cpp:122-123`). |
| 5 | `KErrBadName` from `User::ValidateName` (`us_ksvr.cpp:2208`, `ub_obj.cpp:1028-1040`) or `Kern::ValidateName` (`object.cpp:1237-1244`) | RULED OUT | `!` = 0x21 is in `[0x20,0x7e]` and is not `*:?`; EWSRV uses the same prefix. |
| 6 | `KErrArgument` for bad mode/role/opts (`sipc.cpp:841-899`) | RULED OUT | `CServerFs` passes `EGlobalSharableSessions` (= `EIpcSession_GlobalSharable` = 2, `e32const.h:3085-3087`, `e32base.h:2203`); logged `mode=2 role=0 opts=0`. |

## 3. Is a shim file server launched first? — No, in this log

* Shim identity is exact: `user/svcmgr.cpp:247-255` launches `\PORTFILESRV.E32`
  and waits for name **`"!FileServer"`**; the host publishes with
  `fs/r1_fs_server.cpp:32` `TInt FileServer::Publish() { _LIT(KName,"!FileServer"); return Start(KName); }`.
  Service-host phases B0-B4 defer that publication (`services/r1_service_host.cpp:92,100-102,129-143`).
* But `genuine/prove_efile_genuine.py:34-49` stages **only** `EFILEX.E32`,
  `F32CLI.E32`, `EUSER.DLL` — no `SVCMGR.E32`, no `PORTFILESRV.E32`.
* `fallback_efile_genuine.log:140-149`:
  `GenuineBoot: probing boot volume for the genuine startup chain` →
  `SvcMgr: SVCMGR.E32 not found r=-1` → shell prompt → `C:/> efilex`.
  `variant/e32load.cpp:1250-1262,1320-1337`: `SvcMgrBootLaunch()` returned
  early because `SVCMGR.E32` was absent, so `PORTFILESRV.E32` was never
  launched. No shim process is running.
* `fallback_efile_genuine.log:1230-1236`:
  `EFILE: genuine file server starting` → `File server thread registered`
  (genuine `sexec.cpp:2223`, i.e. `FsRegisterThread` succeeded) →
  `EFILE: port-shim User::Panic reason=3` → `FAULT: KERN 0x00000004 (4)`.
* Therefore `KErrAlreadyExists` from a shim is **not supported by this log**.
  It remains plausible only for a full boot disk where both `\EFILE.E32` and
  `\SVCMGR.E32` are staged (`GenuineBootLaunch` would normally skip the
  fallback once a genuine file server launches — `e32load.cpp:1327-1337`).

Secondary fault: `/tmp/opencode/gdb_efile_gdb.log:10-20` shows the KERN 4 is
`K::Fault` ← `DThread::Exit` ← `NThread::Exit`, i.e. a **system-permanent
thread-exit** fault (`e32load.cpp:1200-1207`), not the Start error itself. The
current source already contains `PortFsRegisterThread` (`e32load.cpp:1208-1223`)
to clear those flags, but the log's kernel build predates it (no
`FsRegister: cleared ...` line), so rebuilding with current source is expected
to remove the *secondary* KERN 4 once Start is fixed.

## 4. Decisive existing evidence: the kernel call succeeded

`/tmp/opencode/gdb7.log:1230-1235` (same for `prove_utf16_test.log:1230-1235`,
`prove_utf16_fsreg.log:1232-1238`):

```
1230: EFILE: genuine file server starting
1232: File server thread registered
1233: ServerCreate127: mode=2 role=0 opts=0 r=2060124160
1235: FAULT: KERN 0x00000004 (4)
```

`2060124160 = 0x7ACB0000` — a **positive handle**, not a negative error.
Slot 127 is the instrumented `Exec::ServerCreateWithOptions` (genuine slow-exec
entry, `kernel/eka/kernel/execs.txt:1425-1432`; the wrapper install line is
`SvcMgr: ServerCreate logging wrappers installed (126/127)` at `gdb7.log:122`).
`EUSER`'s `RServer2::CreateGlobal` turns a positive handle into `KErrNone`
(`e32cmn.inl:3055-3058`). Yet `sf_main.cpp:53` still fired. So either:

1. the value EFILE sees at the `Start()` call site is not the value the kernel
   exec returned (EUSER-side return/import-binding problem, or a stale EUSER
   image), **or**
2. the logged exec call is not the one whose result `CServerFs::New()` tests.

Either way the fault is **above** `sipc.cpp:825-932`; it is not a kernel object
creation failure. (EWSRV calls the same `CServer2::Start` export with the same
pattern and passes, `ewsrv/ws_main.cpp:122-123`, so the generic path works.)

## 5. Single most discriminating test (for the in-flight agent)

**Print the actual signed return of `CServer2::Start` at the EFILE call site,
in the same run as the existing exec-127 log.** Concretely: wrap the PE2 IAT
slot for EUSER's `_ZN8CServer25StartERK7TDesC16` in the port loader
(`variant/e32load.cpp:851-880`, already owned by that agent) with a shim that
calls the real export and `KPrintf("EFILE Start r=%d (0x%x)\r\n", r, r)`; keep
the exec-127 wrapper enabled. No `kernelhwsrv/` change needed.

Interpretation:

* `r` positive (e.g. `0x7ACB0000`) → cause #1 confirmed: EUSER (or the port's
  import binding) leaks the kernel handle through `Start()` instead of
  `KErrNone`; fix on the port side.
* `r == -15` (`KErrAlreadyExists`) → cause #2; a shim/duplicate did publish
  `!FileServer`; fix boot staging / retire the shim name.
* `r == -38` (`KErrBadName`) / `r == -4` (`KErrNoMemory`) → cause #5/#3.

A cheaper variant: patch only EFILE's port glue
(`genuine/efile/efile_glue.cpp:80-89`) to print the value returned by
`E32Main()`/`CServerFs::New()`; but the IAT wrapper is preferred because it
captures the exact `Start()` result without touching genuine source.

## 6. Minimal port-side correction per cause

* **Cause #1 (most likely):** in `variant/e32load.cpp` EUSER import binding,
  verify the EFILE IAT slot for `CServer2::Start` resolves to the genuine EUSER
  export used successfully by EWSRV; if the prebuilt EUSER returns a raw handle
  for this call, add a port-side thunk that maps `r >= 0` to `KErrNone` before
  EFILE's `__ASSERT_ALWAYS` (do **not** edit `sf_main.cpp`). Alternatively
  rebuild against the EUSER image whose `euseru.def` matches the headers.
* **Cause #2:** ensure only one publisher of `!FileServer`: stage EFILE as
  `\EFILE.E32` so `GenuineBootLaunch` sets `genuineFs` and skips
  `SvcMgrBootLaunch()` (`e32load.cpp:1329-1337`), and remove/disable the
  `\PORTFILESRV.E32` row (`user/svcmgr.cpp:250`) per the retire gate — never
  retire before `r1_genuine_retire_test.py` passes.
* **Causes #3/#5/#6:** no code change indicated by current evidence; keep as
  assertions to check if the discriminating test returns a negative `r`.
* **Regardless of cause:** rebuild with current `e32load.cpp` so
  `PortFsRegisterThread` suppresses the secondary `K::ESystemThreadPanic` on
  EFILE exit (`e32load.cpp:1208-1223`); otherwise KERN 4 will still appear
  after the Start fix.

**Bottom line:** ranked (1) EUSER/return-path translation of a *successful*
kernel server creation, (2) shim `!FileServer` collision, (3) memory, (4-6)
ruled out. Most likely is (1), evidenced by
`gdb7.log:1233 ServerCreate127: ... r=2060124160` (positive handle) followed
immediately by `reason=3`. The shim-collision theory is contradicted for this
specific log because `fallback_efile_genuine.log:142` shows `SVCMGR.E32` absent
and hence `PORTFILESRV.E32` never launched.

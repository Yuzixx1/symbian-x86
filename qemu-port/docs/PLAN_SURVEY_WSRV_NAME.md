# PLAN SURVEY — why CreateSession("!E32WindowServer") fails (KErrBadName/KErrNotFound)

Survey of the server-name registration path in qemu-port vs the read-only
genuine kernel in kernelhwsrv/. No code changed; kernelhwsrv/ untouched.

## Current boot/registration path

- `QemuVariantAsic::Init3` (variant/qemu_variant_asic.cpp:121) mounts FAT (C:),
  then calls `PortTrapExecInstall()` + `GenuineBootSpawn()`; svcmgr is retired
  (dd22ae2), so there is no SvcMgrExecInstall anymore.
- `PortTrapExecInstall()` (variant/e32load.cpp:1453) wraps genuine slow-exec
  slots 127 (ServerCreateWithOptions) and 128 (SessionCreate) with trace-only
  wrappers that call the genuine handlers unchanged. Slot 128 additionally has
  a leftover fallback: on KErrNotFound for a `!`-name it retries once with the
  literal name `"!"` (e32load.cpp:1434-1443) — a hand-written workaround, not a
  genuine mechanism.
- `GenuineBootLaunch()` (e32load.cpp:1541) probes the boot volume for the
  genuine chain `\EFILE.E32 -> \ESTART.E32 -> \EWSRV.E32 -> \ESHELL.E32` and
  launches each via `E32Run` — so genuine EWSRV *is* the daemon that registers
  the name; there is no separate launcher.
- `E32Run` (e32load.cpp:924) ORs `1u<<ECapabilityProtServ` into every loaded
  E32 process, so the genuine ProtServ check in
  `ExecHandler::ServerCreateWithOptions` (kernelhwsrv kernel/eka/kernel/sipc.cpp:837)
  passes and the name is stored literally via `DObject::SetName` + added to
  `K::Containers[EServer]` (sipc.cpp:915-922).
- Genuine EWSRV registers with `pS->Start(_L("!E32WindowServer"))`
  (genuine/ewsrv/patched/ws_main.cpp:129); clients look the same literal up via
  `RSessionBase::CreateSession` -> exec 128 -> `DObjectCon::FindByName` on the
  same container (sipc.cpp:1383). Kernel name validator accepts `!`
  (klib/kdes8.cpp:325), so KErrBadName never comes from the kernel.

## Root cause of KErrBadName/KErrNotFound

Not a kernel defect and not a missing daemon — it is a port-side ABI/name
literal defect plus stale artifacts:

1. g++ -m32 emits `_L`/`_LIT` literals as 32-bit TText; the prebuilt genuine
   EUSER.DLL reads 16-bit TText, so `TPtrC16` name8.Copy truncated
   "!E32WindowServer" to "!" (length 1). Server registered as "!", hand-built
   clients asking for the full 16-char name missed with KErrNotFound
   (docs/HANDLE_RETURN_DIAGNOSIS.md, docs/EWSRV_CLIENT_BLOCKER.md). The -28 seen
   earlier came from descriptor type bits in hand-rolled clients, not the
   kernel validator.
2. Stale image trap: EWCLIENT.E32 was once built before co_cli.cpp was
   re-patched, so client and server disagreed on the literal.

Current state: both patched sources now use `_L("!E32WindowServer")` and all
build scripts (genuine/ewsrv/build_ewsrv.py:56, build_client.py:48,
tools/genuine_build.py:92) pass `-fshort-wchar`, so literals are proper UTF-16;
EWSRV.E32/EWCLIENT.E32 (2026-11-25 16:23) are newer than the patched sources
(2026-11-24 20:55). The `ws_win.cpp` TSize sret-ABI garbage (EWindowTooWide)
was also fixed with explicit field arithmetic (ws_win.cpp:2452-2454).

## Genuine fix options

1. (Preferred, already landed in source) Keep both sides on the genuine
   `KE32WindowServer` literal `"!E32WindowServer"`, built with `-fshort-wchar`,
   both images rebuilt together. Registration/lookup then use only genuine
   mechanisms: exec 127 SetName + K::AddObject, exec 128 FindByName.
2. Remove the hand-written `"!"` retry fallback in `PortSessionCreate`
   (e32load.cpp:1434-1443) once UTF-16 literals are proven — it masks name
   mismatches and is not a genuine mechanism.
3. No kernel change is ever required: ProtServ is granted port-side by E32Run,
   and the kernel already stores/finds the literal name in one container.

## Constraints (frozen kernel hooks, no replacements)

- kernelhwsrv/ is READ-ONLY (separate repo): do not touch sipc.cpp, us_ksvr.cpp,
  object.cpp, kdes8.cpp, e32twin.h, or the genuine ewsrv sources there.
- Keep the prebuilt EUSER.DLL (2503 exports); do not rebuild it.
- Do not hand-write a window server or a name-stripping/shim layer in the port;
  only genuine EWSRV.E32 may publish the name.
- svcmgr launch path stays retired; GenuineBootLaunch is the sole boot path.
- No commits/pushes, no `make clean`, no QEMU runs for this survey.

## Risk

- Stale-artifact regressions: touching co_cli.cpp/ws_main.cpp without rerunning
  BOTH build_ewsrv.py and build_client.py reintroduces the name mismatch.
- The leftover `"!"` fallback in exec 128 can mask future name regressions and
  let a bogus "!" server satisfy clients; it should be removed, not relied on.
- Residual client-side issue seen in docs/CONNECT_NEG28_STATUS.md (Connect r=0
  but handle==0) was diagnosed as the same name-truncation defect
  (HANDLE_RETURN_DIAGNOSIS.md); with UTF-16 rebuilds the handle-return path
  itself is proven intact, but runtime re-verification is still pending.

## First 3 concrete steps

1. Rebuild both images from current sources — `genuine/ewsrv/build_ewsrv.py`
   then `genuine/ewsrv/build_client.py` — and restage EWSRV.E32/EWCLIENT.E32 on
   the r1_genuine_ewsrv_test.py disk so both carry UTF-16 "!E32WindowServer".
2. Run `tools/r1_genuine_ewsrv_test.py` (boot EWSRV via GenuineBoot, launch
   ewclient.e32) and require all CLIENT_MARKERS (`EWCLIENT: Create r=0`,
   `Init r=0`, `window drawn`) with 0 faults — proving exec 127 registered and
   exec 128 found the genuine name.
3. Once green, delete the `"!"` retry fallback in `PortSessionCreate`
   (variant/e32load.cpp:1434-1443) and re-run the same harness to confirm
   genuine-only name lookup still passes.

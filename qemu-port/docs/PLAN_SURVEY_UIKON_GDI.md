# PLAN SURVEY — UIKON / GDI / EWSRV desktop stack feasibility

Read-only survey of `kernelhwsrv/` (untouched), `qemu-port/genuine/`, and
`qemu-port/Makefile`. No builds, no QEMU runs, no writes outside this file.

## Present sources

- **Genuine text window server (EWSRV)**: `kernelhwsrv/kernel/eka/ewsrv/`
  (`ws_main/ws_win/ws_dat/ws_utl/co_cli/co_twin/co_utl/ky_capt/ky_tran.cpp`,
  `bld.inf` pulls `ektran`, `econs`, `ewsrv`, `../drivers/edisp/*`). Text-only:
  `ws_win.cpp` works in `TText` cells, no bitmaps/fonts/BitBlt.
- **Genuine display driver (edisp)**: `kernelhwsrv/kernel/eka/drivers/edisp/`
  (`epoc/generic/wd_generic.cpp` pixel driver, `emul/win32/wd_wins.cpp`).
  `d32otgdi.h` headers are USB-host OTGDI only, unrelated to screen GDI.
- **Genuine EUSER**: sources in `kernelhwsrv/kernel/eka/euser/`; the prebuilt
  genuine `qemu-port/EUSER.DLL` (412,104 B, UIDs 10000079/1000008d/100039e5,
  **2503 ordinals** from `kernel/eka/bx86gcc/euseru.def`). Graphics-relevant
  ordinals exist only at HAL/input level (`UserSvr::ScreenInfo`@1211,
  `HalFunction`@1213/@2005, `RequestEvent`@1216, `WsRegisterThread`@1225,
  `AddEvent`@1235); only allocator `CBitMapAllocator` (~@435-457). No
  `CFbs*`/`CWsScreen*`/`RWindow` classes.
- **Working build pipeline**: `qemu-port/genuine/ewsrv/build_ewsrv.py` compiles
  unmodified EWSRV into ONE PE2 E32 importing only EUSER.DLL; 4 patched copies
  in `genuine/ewsrv/patched/` (LFB screen driver, factory, markers);
  `build_edisp.py` builds `EDISP.DLL` (exports `CScreenDriver::New()` ord 1
  per `bx86/edisp.def`); `build_client.py` builds `EWCLIENT.E32`. Prebuilt
  artifacts in `qemu-port/genuine/build/` (`EWSRV.E32`, `EDISP.DLL`,
  `EWCLIENT.E32`). `tools/r1_genuine_ewsrv_test.py` PASS: EWSRV runs on the
  VBE LFB (1024x768x32) and paints the desktop.
- **Loader support already widened**: PE2 import slots raised 16 -> **512**
  (`variant/qemu_variant.h:397`, `variant/e32load.cpp:533`,
  `tools/e32read.py`); VBE LFB mapped into user PDs; `qemu_display.cpp`
  ensures graphics before EWSRV asks.
- **Port-layer (authored, non-genuine) GUI**: `qemu-port/user/desktop.c`,
  `guilib.c`, `wsdl.c` — glyph/text rendering over the (now retired) shim
  winsrv; GUI apps were just migrated onto genuine EWSRV (recent commits).

## Missing pieces

- **No UIKON / EIKON / app-framework source anywhere** — not in
  `kernelhwsrv/`, not in `sdk/`, not in `symbian-x86-archive-56fbaff.tar.gz`
  (0 hits). Only incidental mentions in release notes / eshell.mmp.
- **No GDI / FBSRV source** — no `CFbsScreenDevice`, font/bitmap server,
  `CFbsBitGc`, `.mbm` loader, or glyph data anywhere in the workspace.
- **No genuine graphical window server** — `kernelhwsrv/` has only the
  *text* EWSRV; upstream winsrv is absent (the port's authored winsrv shim
  was deleted in retirement).
- **Client-session blocker**: `EWCLIENT.E32` gets `KErrNotFound` from
  `CreateSession("E32WindowServer")`; with `"!E32WindowServer"` the genuine
  `CServer2::Start` returns `KErrBadName` (-28). The port's DObjectCon /
  ProtServ name lookup does not yet publish a cross-process-visible server
  name, so no client can open a session to EWSRV.
- `kerneltest/e32test/window/` tests exist upstream but are not ported.

## Build/loader constraints

- **512 import slots** per PE2 image (was 16); genuine EWSRV binds ~212-219
  EUSER ordinals, so headroom exists, but each imported ordinal consumes a
  slot and `codeOff > 512` is rejected (`e32load.cpp:414`).
- **One DLL dependency per image**: the loader binds only EUSER.DLL;
  `ELoadLibrary` is `KErrNotSupported`. A genuine UIKON/GDI stack (which
  would need FBSRV.DLL + UIKON.DLL + EUSER.DLL) can only work as one
  statically-compiled image, not as dynamically linked DLLs.
- **Static buffer caps**: `sSvcImg[512*1024]` for E32 images and
  `sDllFile[512*1024]` for DLLs (`e32load.cpp:660,1265`); EUSER.DLL (~412 KB)
  and DOOM (~338 KB) fit, but a full desktop stack would exceed 512 KB.
- **ABI flags mandatory** for genuine interop with prebuilt EUSER.DLL:
  `-m32 -freg-struct-return` (small-struct returns in EDX:EAX, not sret),
  `-fshort-wchar` (16-bit `TText`), `-mpreferred-stack-boundary=2`, plus the
  tested `__GCC32__/__X86__/__EPOC32__/_UNICODE` define set.
- `kernelhwsrv/` is read-only; genuine sources may only be compiled, with
  patched copies confined to `qemu-port/genuine/ewsrv/patched/`.

## Feasibility verdict

**LOW** for a genuine UIKON/GDI desktop stack compiled from existing source —
the UIKON, GDI/FBSRV, font/bitmap, and graphical winsrv sources simply do
not exist in this snapshot (kernelhwsrv, sdk, and the archive all lack
them). **MEDIUM** for a graphical desktop built on the already-working
genuine EWSRV + EDISP + VBE LFB core, with authored port-side pixels/fonts
behind a Symbian-shaped ABI (per `docs/GENUINE_USERSPACE.md` policy), once
the client-session name-registration blocker is fixed.

## First 3 concrete steps

1. Fix cross-process server-name registration in the port kernel
   (DObjectCon/ProtServ path so `"!E32WindowServer"` resolves instead of
   `KErrBadName`/`KErrNotFound`), then prove `EWCLIENT.E32` opens a session
   and creates a window on genuine EWSRV.
2. Add a pixel/BitBlt path to the edisp/wd_generic LFB driver plus
   authored font/bitmap blit glue in `qemu-port` (no genuine glyph data
   exists locally), verified by a client-drawn-pixels screendump.
3. Decide the purity axis: either pull UIKON/FBSRV/winsrv sources from the
   upstream Symbian open-source release externally, or formally document an
   authored GDI/UIKON replacement behind stable Symbian-shaped execs in
   `docs/GENUINE_USERSPACE.md` before writing any client code against it.

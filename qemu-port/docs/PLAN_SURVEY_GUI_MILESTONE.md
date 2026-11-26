# PLAN SURVEY — smallest credible "GUI desktop boots" milestone on genuine EWSRV+EDISP

Read-only survey of `qemu-port/` (this file is the only write). No QEMU, no
builds, no commit/push, `kernelhwsrv/` untouched. Sources:
`genuine/COORD_EWSRV.md`, `genuine/ewsrv/edisp_glue.cpp`,
`genuine/ewsrv/patched/{ws_main,ws_win,wd_generic}.cpp`,
`variant/qemu_display.cpp`, `variant/qemu_fbblit.{cpp,h}`,
`variant/e32load.cpp:932-936`, `build/export/exec_enum.h:333`,
`user/ws_e32_client.c`, `user/ws_e32_draw_test.c`, `user/user.mk`,
`docs/EWSRV_CLIENT_BLOCKER.md`, `docs/WINSRV_EDISP_SCOPE.md`,
`docs/PLAN_SURVEY_UIKON_GDI.md`, `kernelhwsrv/kernel/eka/include/ws_std.h`.

## What already works

- **Genuine EWSRV boots and paints**: unmodified `kernelhwsrv` EWSRV/econs/
  ektran/edisp sources compiled into ONE PE2 E32 importing only the genuine
  2503-export EUSER.DLL; `tools/r1_genuine_ewsrv_test.py` PASS including the
  full `CLIENT_MARKERS` set (`EWCLIENT: ... window drawn`, 0 faults). The two
  port-side client blockers (identical `"!E32WindowServer"` literal on both
  sides, `TSize` field arithmetic instead of the sret-broken `operator+`)
  are fixed and gated.
- **Genuine edisp pixel driver in-image**: patched `wd_generic.cpp` runs its
  software blit path (`iHwAcc==0`), built-in 8x10 bitmap `Font[256][10]` and
  `Palette256`, writing directly into the VBE LFB (1024x768x32, mode 0x118)
  obtained via `UserSvr::ScreenInfo` -> `Exec::HalFunction(EDisplayHalScreenInfo)`
  answered by `variant/qemu_display.cpp:493` (with lazy
  `display_ensure_graphics()`). Standalone `EDISP.DLL` (ordinal-1
  `CScreenDriver::New`) also buildable via `build_edisp.py`.
- **VBE LFB plumbing**: Bochs DISPI programming in `qemu_display.cpp`, LFB
  base from VGA PCI BAR (fallback 0xE0000000), and `e32load.cpp:932-936`
  maps the LFB into every user PD (`PdMapUser`, U/S=1) — genuine edisp and
  any E32 process can write VRAM directly.
- **Exec 270 ops 2/3 fbblit**: ring-0 bulk blit of a user frame into an LFB
  rect (`fbblit_info`/`fbblit_blit`, 8bpp->32bpp expand), used by DOOM and
  `fbblittest.c`. (The old exec-251 display-list + winsrv shim are deleted;
  only the fbblit exec survives.)
- **Genuine client transport proven**: `user/ws_e32_client.c` opens a real
  `RSessionBase` to `!E32WindowServer` via 4 PE2 IAT slots (EUSER ords
  211/1547/120/915) and drives the genuine `CWsSession` console opcodes
  (0/1/4/6/10/11/13/14/15/17/21). `ws_draw_fill_rect` = space-cell fill via
  ops 21/6/10 (server `WriteCharacter` -> `CScreenDriver::Blit` into LFB);
  `ws_text` = real UTF-16 glyphs; `ws_get_event` = blocking `EConsoleRead`
  (op 11) fed by the KBINJ PS/2 injection bridge. `ws_e32_draw_test.c`
  (WSDRAW: two filled rects + text + button + key echo + screendump spin)
  exists but has no `tools/` harness yet. GUI apps (guishell, guilib, ...)
  already link this client.

## Minimal milestone definition

"GUI desktop boots" = boot the genuine EWSRV.E32 on the VBE LFB, launch one
client E32 that opens a window session on it, and pixels demonstrably land in
the LFB (IPC markers + QMP screendump pixel count above the desktop-only
baseline). Concretely: `r1_genuine_ewsrv_test.py`-style harness launching
`WSE32DRAW.E32` with all `WSDRAW:` markers green and a screendump showing the
client's rects/text over the EWSRV desktop. No UIKON/GDI, no new window
classes — the genuine EWSRV console-rasterizer IS the pixel engine.

## Required glue (PC-only, authored port-side — allowed)

- Client-side: `user/ws_e32_client.c` transport (done) plus, if pixel-true
  (non-cell) windows are wanted, one **port-defined opcode** added to
  `genuine/ewsrv/patched/ws_main.cpp`'s `ServiceL` switch and mirrored in a
  port client header (e.g. blit RGBA rect into LFB via the genuine
  software blit path or a direct LFB write — the LFB is already U/S=1 in
  user PDs). Never edit `kernelhwsrv/`; patched copies only.
- Fonts/pixels: genuine 8x10 font suffices for the milestone; any richer
  authored font is blitted through the same LFB path (exec 270 op 3 or
  direct user-mode LFB write). Palette256 nearest-index mapping already in
  `ws_e32_client.c`.
- Harness: new `tools/r1_ws_e32_draw_test.py` asserting WSDRAW markers +
  screendump pixel diff (no WSDRAW harness exists today).

## Required genuine components

- Genuine EWSRV.E32 (text window server) — present, PASS.
- Genuine edisp `wd_generic.cpp` software driver (Font/Palette256/Blit) —
  present in-image; optional standalone EDISP.DLL.
- Genuine prebuilt EUSER.DLL (2503 ordinals) as the sole DLL dependency.
- Port display-HAL bridge `EDisplayHalScreenInfo` in `qemu_display.cpp`
  answering genuine `UserSvr::ScreenInfo`, plus LFB mapping in `e32load.cpp`.

## Blockers

- `kernelhwsrv/.../ws_std.h` `CWsSession` is **console-only** (EConsole* ops;
  no RWindow/GC/CFbs/bitmap ops). A genuine graphical window API does not
  exist in this snapshot; any pixel window must be a port-added opcode in
  `patched/` copies or cell-rasterization (works today).
- No UIKON/EIKON/GDI/FBSRV sources exist anywhere in the workspace
  (PLAN_SURVEY_UIKON_GDI verdict: LOW for a genuine graphics stack).
- Loader limits: one DLL dependency per image, 512 import slots, 512 KiB
  image cap — a multi-DLL genuine UIKON/GDI stack cannot load.
- `EConsoleSetSize` answers KErrNotSupported in this tree.
- LFB-to-screendump pixel assertions have a known caveat
  (EWSRV_CLIENT_BLOCKER: IPC markers are authoritative; verify screendump
  visibility separately).
- Input is blocking-only (`EConsoleRead`); KBINJ bridge works but no
  non-blocking/polling genuine path.

## First 3 concrete steps

1. Write `tools/r1_ws_e32_draw_test.py`: boot, launch EWSRV.E32 then
   WSE32DRAW.E32, require all `WSDRAW:` markers (Connect/Create/FillRect
   r=0, text drawn, KEY, DONE) with 0 faults, plus QMP screendump whose
   coloured-pixel count exceeds the EWSRV desktop-only baseline.
2. Add one port-defined blit opcode (patched `ws_main.cpp` + client side of
   `ws_e32_client.c`) so a client can push an RGBA pixel rect straight into
   the LFB through genuine EWSRV, and prove it with a marker + screendump.
3. Point one GUI app (e.g. `guihello.e32`) at the pixel path so the
   milestone demo is: genuine EWSRV desktop + one client window with real
   pixels/text — gated by the new harness plus `r1_final_verify.py
   --strict-skip`.

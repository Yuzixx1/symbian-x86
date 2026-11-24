# WINSRV_EDISP_SCOPE — cleanup step 5 scoping (winsrv retirement via genuine edisp + EWSRV client gate)

**Scope:** read-only scoping report, 2026-11-24. No edits except this file. No QEMU,
no commit/push, `kernelhwsrv/` untouched. Sources: `docs/REMAINING_SHIMS_AUDIT.md` §2–3,
`docs/EWSRV_CLIENT_BLOCKER.md`, `docs/CLEANUP_LOG.md`, `genuine/COORD_EWSRV.md`,
`tools/r1_shim_retire_plan.py` (winsrv entry), `tools/r1_genuine_ewsrv_test.py`,
`variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp`, `user/winsrv.c`,
`user/guishost.h`, `genuine/ewsrv/build_ewsrv.py`.

## 1. Current winsrv architecture (what exists today)

- **Kernel shared region + execs 271/272/273** (`variant/qemu_windowsrv.{cpp,h}`):
  static 4,100,096-byte `ws_shared` at fixed phys `0x00800000` (header ≤4 KiB +
  16 windows × 256 KiB 320x200x32 content buffers). Exec 271 = base, 272 = size,
  273 = multiplexed `WSO_*` control (REGISTER_SERVER / CREATE_WINDOW /
  DESTROY_WINDOW / SET_DIRTY / PUSH_EVENT / POP_EVENT / SET_FOCUS / FRAME_TICK).
  64-entry event ring (`WSE_MOVE/DOWN/UP/KEY`) fed by PS/2 ISRs via
  `wsrv_publish_mouse` (`qemu_mouse.cpp:169`) / `wsrv_publish_key`
  (`qemu_variant_int.cpp:326`); installed by `windowsrv_init()` (constructor).
  Slots 271–273 live in the verified 28-byte pad after `EpocSlowExecTable`
  (count bumped 271→274).
- **FB/display-list execs 249/250/251** (`variant/qemu_gui_exec.cpp` +
  `user/guishost.h`): 249 = LFB physical base, 250 = LFB size, 251 = execute a
  display list (`GC_CLEAR/RECT/PUTC/BLIT`, `GUI_MAX_CMDS=640`, bulk RGBA blit via
  `blit_src_phys/size` tail fields) into the VBE LFB + mouse crosshair/buttons.
- **Ring-3 server** `user/winsrv.c` (PE2 compositor over execs 271–273 + 251);
  **client lib** `user/wsclient.{c,h}` + demo `user/wsdl.c`.
- **GUI users (~12 + DOOM fallback):** `user/{desktop,guishell,guihello,guimenu,
  guilib,breakout,saver,shot,dtcalc,dtedit,gcblittest,fbblittest}.c` include
  `guishost.h`/`guilib.h`; `ports/doom/e32_platform.c:31,188-209` uses the
  `guishost.h` `GC_RECT` fallback list through exec 251 when no window server.
- Build wiring: `Makefile:258,261` (`qemu_gui_exec.o`, `qemu_windowsrv.o`),
  `variant/qemu_variant.h:106,246-251` (gui_init / windowsrv_init /
  wsrv_publish_* declarations), `user/user.mk` GUI targets.

## 2. What "edisp" is in this kernelhwsrv snapshot (LDD/PSL correction)

- Path is `kernelhwsrv/kernel/eka/drivers/edisp/` (not `ewsrv/drivers/...`):
  `edisp.mmp` builds **edisp.dll**, a single DLL exporting
  `CScreenDriver::New()` at **ordinal 1** — there is **no separate LDD/PSL pair**
  in this EKA2 tree. `epoc/generic/wd_generic.cpp` = generic pixel text driver
  (font/blit/scroll logic, mode set, palette); `epoc/vga/vgatext.cpp` =
  VGA-text backend (X86 only); `epoc/generic/wd_vt100.cpp` + `edisp_vt100.mmp` =
  serial-terminal variant; `emul/win32/wd_wins.cpp` = WINS emulator.
- EWSRV loads it at runtime: `ewsrv/ws_win.cpp:113-124` —
  `RLibrary edisp; edisp.Load(_L("EDISP.DLL")); f = edisp.Lookup(1);
  ScreenDriver = f();` then `Init(ScreenSize,FontSize)`.
- What `wd_generic.cpp` needs: `UserSvr::ScreenInfo` (→ `Exec::HalFunction(
  EHalGroupDisplay, EDisplayHalScreenInfo)`), optionally `UserSvr::HalFunction(
  EDisplayHalBlockFill/BlockCopy)` for hw accel (KHwAccBlockFill/Copy consts).
- **Bridges that already exist:** `variant/qemu_display.cpp:493-514` answers
  `EDisplayHalScreenInfo` with a full `TScreenInfoV01` (VBE LFB, live via
  `display_ensure_graphics()`), and `e32load.cpp` E32Run maps the VBE LFB into
  each user PD. `genuine/ewsrv/build_ewsrv.py` already compiles a **patched**
  `genuine/ewsrv/patched/wd_generic.cpp` into the monolithic EWSRV.E32: genuine
  font/blit logic, geometry from `UserSvr::ScreenInfo`, 32bpp VBE LFB, and a
  direct `CScreenDriver::New()` factory (no VGA-text, no EDISP.DLL load).

## 3. edisp port plan (what remains)

1. Treat the patched `wd_generic.cpp` already inside `EWSRV.E32` as the edisp
   port — optionally factor it out as a real `EDISP.DLL` PE2 (edisp.mmp shape,
   ordinal-1 export) so `ws_win.cpp`'s genuine `RLibrary::Load("EDISP.DLL")`
   path works unpatched; either way the port-side contract is
   `EDisplayHalScreenInfo` in `qemu_display.cpp` (already present).
2. ~~Fix the two port-side client blockers from `docs/EWSRV_CLIENT_BLOCKER.md`~~
   **ALREADY FIXED and verified 2026-11-24 — not remaining work:**
   (a) identical server name on both sides — genuine `"!E32WindowServer"` is
   live in `patched/ws_main.cpp:128` (`pS->Start(_L("!E32WindowServer"))`) and
   `patched/co_cli.cpp:24` (`CreateSession(_L("!E32WindowServer"),...)`);
   both `build_ewsrv.py` and `build_client.py` were re-run and both E32s
   restaged;
   (b) `patched/ws_win.cpp:2454-2455` now uses explicit field arithmetic
   (`iCurrentSize.iWidth=aSize.iWidth+2;` / `...iHeight=aSize.iHeight+2;` —
   no `TSize::operator+` sret call).
   Evidence: `tools/r1_genuine_ewsrv_test.py` PASS with `CLIENT_MARKERS`
   (lines 56-66) fully populated — `Create/ScreenSize/Init/SetTitle/Write/
   SetCursorPos/Write2 r=0` and `EWCLIENT: window drawn`, 0 faults.
3. Markers: `EWSRV-DBG: Start r=0`, `WINSERVER IPC server started`,
   `EWSRV-EDISP: LFB=… bpp=32` from the patched driver.

## 4. External-client window gate (EWCLIENT markers)

`tools/r1_genuine_ewsrv_test.py` `CLIENT_MARKERS` (lines 56-66), all required with
0 faults: `EWCLIENT: app_main started`, `Create r=0`, `ScreenSize r=0 128x76`,
`Init r=0`, `SetTitle r=0`, `Write r=0`, `SetCursorPos r=0`, `Write2 r=0`,
`window drawn`; launch via `ewclient.e32` after the EWSRV ready marker.
Gate = full `r1_genuine_ewsrv_test.py` PASS including the client block plus a
fresh `r1_final_verify.py --strict-skip`.

## 5. Migration list (winsrv → genuine EWSRV client API)

~12 GUI users + DOOM fallback must move off execs 249/250/251 + 271/272/273 onto
the genuine `RWsSession`/`RWindowGroup`/`RConsole` (co_cli.cpp) client API:
`user/{desktop,guishell,guihello,guimenu,guilib,breakout,saver,shot,dtcalc,
dtedit,gcblittest,fbblittest}.c`, `user/wsclient.{c,h}`, `user/wsdl.c`, and
`ports/doom/e32_platform.c` (`guishost.h` GC_RECT fallback). DOOM graphical
output must stay byte-for-byte unchanged (plan extra-condition).

## 6. Deletion set (after gate passes — human review, no apply path)

- `user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`, `user/wsclient.h`,
  `user/wsdl.c`; `variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`,
  `variant/qemu_gui_exec.cpp`.
- Kernel hooks: `windowsrv_init()`/`wsrv_publish_mouse`/`wsrv_publish_key` decls
  (`qemu_variant.h:246-251`), call sites `qemu_mouse.cpp:169`,
  `qemu_variant_int.cpp:326`, `gui_init` (`qemu_variant.h:106`); restore exec
  table count 274→271 and free slots 249/250/251 + 271/272/273 (pad returns to
  zero); `Makefile:258,261` object rows; `user/user.mk` GUI targets.

## 7. Risks / blockers

- Widest blast radius of all shims (GUI apps + DOOM) — last among servers.
- The two EWSRV client blockers (§3.2) are **ALREADY FIXED (2026-11-24)** and
  gated by `r1_genuine_ewsrv_test.py` CLIENT_MARKERS; if `co_cli.cpp` is ever
  touched again, the stale-image trap still applies — always re-run
  `build_client.py`.
- Never accept `SessionPath r=-15` as IPC success; `!`-prefixed server names
  need `ECapabilityProtServ` (already granted by E32Run).
- edisp in this tree is one DLL (ordinal 1), not LDD/PSL — plan against
  `CScreenDriver`, not a PSL/LDD split.
- `kernelhwsrv/` read-only throughout; no commit/push; no `make clean`.

## 8. Estimated steps

1. ~~Fix the two client blockers + rebuild/restage both E32s~~ DONE 2026-11-24
   (see §3.2 — EWCLIENT gate already green in `r1_genuine_ewsrv_test.py`).
2. Factor/confirm edisp as EDISP.DLL or keep in-image patched driver (~1-2).
3. Re-run full verify (`r1_final_verify.py --strict-skip`) as regression
   check after edisp work (~1).
4. Migrate ~12 GUI users + DOOM fallback to RWsSession/RConsole (~4-6, largest).
5. Delete winsrv files/hooks/execs, rebuild, re-run full gate (~2).

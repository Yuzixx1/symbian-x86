# WINSRV_MIGRATION_PLAN — moving GUI users + DOOM off winsrv execs onto genuine EWSRV

**Status:** READ-ONLY planning report, 2026-11-24. No edits except this file. No QEMU
runs, no commit/push, `kernelhwsrv/` untouched. Sources: `docs/WINSRV_EDISP_SCOPE.md`,
`docs/REMAINING_SHIMS_AUDIT.md`, `tools/r1_shim_retire_plan.py` (winsrv entry),
`user/user.mk`, `user/guishost.h`, `user/guilib.h`, `user/wsclient.{c,h}`,
`variant/qemu_gui_exec.cpp`, `variant/qemu_windowsrv.{cpp,h}`,
`variant/qemu_fbblit.h`, `ports/doom/e32_platform.c`, `genuine/ewsrv/build_client.py`,
`genuine/ewsrv/ewsrv_client.cpp`, `Makefile:258-265,318`, `variant/qemu_variant.h`.

edisp is landed (`genuine/ewsrv/edisp_glue.cpp`, `build_edisp.py`); genuine EWSRV
renders through it and the EWCLIENT client gate PASSES
(`tools/r1_genuine_ewsrv_test.py` CLIENT_MARKERS all r=0, 0 faults). Remaining work
is exactly: migrate every consumer of execs 249/250/251 (display list) and
271/272/273 (window/event ring) onto the genuine EWSRV client API, then delete the
winsrv shim files.

## 1. Consumer inventory (who calls which execs)

Exec map: **249** = LFB phys base, **250** = LFB size, **251** = multiplexed GUI
display list (`arg1==0` → `struct gui_shared*`, `arg1>0` → run N commands)
(`variant/qemu_gui_exec.cpp`, ABI in `user/guishost.h`). **271/272/273** =
`WS_BASE`/`WS_SIZE`/`WS_CTL` over the 4,100,096-byte `ws_shared` at phys
0x00800000 with the 64-entry event ring (`variant/qemu_windowsrv.{cpp,h}`).
**Exec 270 ops 2/3** (`FBO_FBINFO`/`FBO_FBBLIT`) are SvcMgr-ctl capabilities
(`variant/qemu_fbblit.*` + `e32load.cpp:1266-1275`) — **NOT part of the winsrv
deletion set** (they retire with svcmgr, a separate step).

| # | user.mk target | source(s) | execs used | transport |
|---|----------------|-----------|-----------|-----------|
| 1 | GUISHELL.E32 | user/guishell.c | 249/250/251 | raw guishost.h display list |
| 2 | SAVER.E32 | user/saver.c | 249/250/251 | guishost.h |
| 3 | SHOT.E32 | user/shot.c | 249/250/251 | guishost.h (+ PortFile save) |
| 4 | GUIHELLO.E32 | user/guihello.c + guilib.o | 249/250 (raw literals) + 251 via guilib | guilib.h/guishost.h |
| 5 | BREAKOUT.E32 | user/breakout.c | 249/250/251 | guishost.h |
| 6 | GUIMENU.E32 | user/guimenu.c | 249/250/251 | guishost.h (+ exec 268 launch) |
| 7 | DESKTOP.E32 | user/desktop.c | 249/250/251 + gui_shared buttons | guishost.h (+ exec 268) |
| 8 | DTEDIT.E32 | user/dtedit.c + guilib.o | 251 | guilib.h + guishost.h |
| 9 | DTCALC.E32 | user/dtcalc.c + guilib.o | 251 | guilib.h + guishost.h |
| 10 | GCBLITTEST.E32 | user/gcblittest.c | 251 (GC_BLIT) | guishost.h + malloc |
| 11 | WINSRV.E32 | user/winsrv.c | 251 (compositor) + 271/272/273 (`WSO_REGISTER_SERVER/FRAME_TICK/POP_EVENT`) | server itself — delete, not migrate |
| 12 | WSDL.E32 | user/wsdl.c + wsclient.o | 271/272/273 (`WS_BASE/WS_SIZE/WSO_CREATE_WINDOW/DESTROY/SET_DIRTY/POP_EVENT`) | wsclient.h client lib |
| 13 | FBBLITTEST.E32 | user/fbblittest.c | **exec 270 ops 2/3 only** | qemu_fbblit.h — outside winsrv deletion set |
| 14 | DOOM.E32 | ports/doom/e32_platform.c (+doome32.c) | **exec 270 op 3 primary** (`FBO_FBBLIT` in `DG_DrawFrame`); **exec 251 GC_RECT fallback** (`blit_via_gcrect`, e32_platform.c:181-211, only if fbblit fails) | guishost.h fallback only |

Totals: **14 binaries** — 11 on 249/250/251 (incl. the winsrv server itself), 2 on
271/272/273 (winsrv + wsdl/wsclient), 1 on exec 270 only (fbblittest), DOOM split
270-primary / 251-fallback. Input for the interactive apps (desktop, guimenu,
breakout, dtedit, dtcalc, wsdl) is not only keyboard via PortCon but also the
PS/2 mouse published straight into `gui_shared.buttons/button_seq` /
`ws_shared` ring by the kernel ISRs (`qemu_mouse.cpp:169`,
`qemu_variant_int.cpp:326`).

**PE2/EUSER linkage today:** every one of these targets already builds as a
genuine PE2 image with `--euser-import 915` (RDebug::Printf) at the single IAT
slot 0x408000 (`user/user.mk`). None import the ws-client ordinals
(`RWsSession::Connect`, `RWindowGroup`, `RWindow`, `RWsScreenDevice`, GC) yet.
The multi-import pattern already exists and is proven twice: `netstack`
(`user/link_netstack.ld` + `netstack_iat.S`, several ordinals) and **EWCLIENT**
(`genuine/ewsrv/build_client.py`: scans `kernelhwsrv/bx86gcc/euseru.def` for the
needed EUSER symbols, generates one thunk per import, links with per-symbol
`*_slot` defsyms, packs via `genuine/pack_multi.py --ordinals …`; PE2 import slots
already raised to 512).

## 2. Per-consumer classification and effort

Classes: **(a)** already PE2/EWSRV-capable, just switch launch; **(b)** needs a ws
client-API port (wsclient.c → RWsSession calls) — i.e. swap the transport backend,
keep the app logic; **(c)** DOOM special.

- **None are (a) today** — no GUI target calls `RWsSession` yet. But all are PE2
  images with a working EUSER import slot, so every migration is class **(b)**:
  add the ws ordinals via the EWCLIENT/netstack pack pattern and swap the
  guishost/guilib/wsclient backend for a small shared "ews client lib"
  (`RWsSession` + `RWindowGroup` + `RWindow` + blit into the window backing store;
  text apps can use `RConsole` exactly like `ewsrv_client.cpp`). Estimated effort:
  **1 shared lib (~2-3 steps) + ~0.5-1 step per app** front-end.
- Class (b) consumers, least risky first:
  1. **FBBLITTEST** — no winsrv exec at all (exec 270 only); no migration needed,
     retire target with svcmgr step. Effort ~0 for this plan.
  2. **GCBLITTEST** — blit proof; re-point at `RWindow` backing-store blit (or
     drop once fbblit covers it). ~0.5.
  3. **SHOT** — needs framebuffer readback: `RWsScreenDevice`/`ScreenInfo` +
     capture from the window/screen buffer instead of 249/250 raw LFB. ~1.
  4. **SAVER** — animation, no input; pure `RWindow` repaint. ~0.5.
  5. **guilib.c + GUIHELLO / DTEDIT / DTCALC** — port the guilib backend once
     (display-list → `RWindow`/GC calls), then the three apps follow nearly
     free. ~2 for the lib, ~0.5 each app.
  6. **GUISHELL, GUIMENU, DESKTOP, BREAKOUT** — interactive; mouse comes from the
     kernel-published ring today, must come from `RWsSession::GetEvent`/`TWsEvent`
     after migration. ~1 each (input is the shared hard part, see blockers).
  7. **WSDL + wsclient.c** — the direct ancestor of the new ews client lib; port
     `ws_open/ws_window_create/ws_present/ws_get_event` to RWsSession semantics.
     ~1-2 (do this early as the lib pilot, before the apps).
  8. **WINSRV.E32** — the server itself: **delete, do not migrate** (genuine
     EWSRV.E32 is the server). ~0.
- Class (c) **DOOM**: primary path is exec 270 `FBO_FBBLIT`, which survives winsrv
  deletion (retires with svcmgr) — **DOOM's pixels do not depend on 249-251**.
  Only `blit_via_gcrect()` (exec 251) is winsrv-bound. Options: (i) delete the
  fallback once fbblit is gated (doom tests already exercise fbblit; fallback is
  dead code in practice), or (ii) reroute the fallback through an `RWindow`
  blit. Gate requires the DOOM graphical frame **byte-for-byte unchanged**
  (retire-plan extra condition) — so prefer (i), keep `DG_DrawFrame` on fbblit.
  Effort ~0.5-1, **last** in order (playtest-gated).

## 3. Migration order (least risky first)

1. Build the shared **ews client lib** by porting `wsclient.c` → RWsSession
   (EWCLIENT pattern: `build_client.py` thunk generation + `pack_multi.py`
   ordinals; reuse for user.mk targets via a generalized multi-import pack).
2. WSDL (pilot client on a real window + events).
3. GCBLITTEST → FBBLITTEST (proof pair; fbblittest unchanged).
4. SHOT, SAVER (no-input / readback).
5. guilib backend → GUIHELLO, DTEDIT, DTCALC.
6. GUISHELL, GUIMENU, DESKTOP, BREAKOUT (event-driven input via `TWsEvent`).
7. DOOM fallback removal (keep fbblit primary; frame bytes unchanged).
8. Delete WINSRV.E32 + wsclient/wsdl/guishost/guilib old backends.

## 4. Winsrv deletion gate (all must PASS before any file is touched)

- Every consumer above migrated off execs 249/250/251 and 271/272/273
  (grep-clean: no `QE_EXEC_GUI_*`/`QE_EXEC_WS_*`/`guishost.h`/`wsclient.h`/
  `qemu_windowsrv.h` references outside the deletion set).
- `tools/r1_genuine_ewsrv_test.py` PASS **including the EWCLIENT client block**
  (CLIENT_MARKERS all r=0, 0 faults).
- DOOM: `r1_doom_test.py`, `r1_doom_audio_test.py`, `r1_doom_playtest.py`,
  `r1_doom_multikey_test.py` all PASS (graphical frame unchanged).
- `tools/r1_genuine_fullchain_test.py` 14/14 PASS.
- `tools/r1_final_verify.py --strict-skip` 11/11 PASS.
- GUI-app QEMU tests that launch migrated binaries (`r1_guishell_test.py`,
  `r1_saver_test.py`, `r1_shot_test.py`, desktop/winsrv/window tests) re-pointed
  at the EWSRV-based versions or retired at the same commit.
- Human review, no apply path (per `r1_shim_retire_plan.py` winsrv entry).

**Deletion set at gate time:** `user/winsrv.c`, `user/guishost.h`,
`user/wsclient.c`, `user/wsclient.h`, `user/wsdl.c`,
`variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`,
`variant/qemu_gui_exec.cpp`; kernel hooks `qemu_variant.h:106,246-251`
(`gui_init`/`windowsrv_init`/`wsrv_publish_*`), call sites `qemu_mouse.cpp:169`,
`qemu_variant_int.cpp:326`; exec table count 274→271, freeing slots 249/250/251
and 271/272/273 (pad returns to zero); `Makefile:258,261` + `KERNFULL_OBJS`
(318); `user/user.mk` GUI target rows. **Kept:** `variant/qemu_fbblit.*` and exec
270 ops 2/3 (svcmgr-owned, separate retirement), all `r1_*` harnesses.

## 5. Blockers / risks

- **Input injection into genuine EWSRV is the biggest open item.** Today PS/2
  ISRs publish into the ws ring (`qemu_mouse.cpp:169`, `qemu_variant_int.cpp:326`);
  after deletion, interactive apps must receive keys/pointer through genuine
  `RWsSession::GetEvent`. EWCLIENT proved window *output* only — no input path
  to EWSRV is verified yet. Need a port-owned bridge feeding genuine EWSRV's
  event queue (HAL/keyboard channel), analogous to `wsrv_publish_*` but
  targeting EWSRV, or EWSRV-side injection via an exec. Until this lands, desktop/
  guimenu/breakout/dtedit/dtcalc migrations are blocked at the input step.
- EFILE guard-only model: no trampoline; IPC round trip must stay working
  (port-shim `0`, `#PF=0`); never accept `SessionPath r=-15` as success;
  `!E32WindowServer` needs `ECapabilityProtServ` (already granted by E32Run).
- Stale-image trap: if `patched/co_cli.cpp` is touched, always re-run
  `build_client.py` and `build_ewsrv.py` and restage both E32s.
- PE2 import budget: 512 slots is ample for RWsSession/RWindow/RWindowGroup/GC
  ordinals; keep using `pack_multi.py` (never hand-roll IATs).
- edisp in this tree is a single DLL (ordinal-1 `CScreenDriver::New`), already
  landed via `edisp_glue.cpp`; do not plan against an LDD/PSL split.
- DOOM frame must stay byte-for-byte identical — delete the GC_RECT fallback
  rather than rerouting it; fbblit (exec 270 op 3) is the verified primary.
- Widest blast radius of all shims — winsrv retires **last**, after svcmgr
  (exec 270 lives there) only if desired; winsrv deletion itself does not touch
  exec 270.
- `kernelhwsrv/` read-only throughout; no commit/push; no `make clean`.

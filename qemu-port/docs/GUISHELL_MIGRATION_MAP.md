# GUISHELL_MIGRATION_MAP — what the interactive GUI apps use from the winsrv/exec transport, and the ws_e32_client target

Prep for migration chunk D (GUISHELL/GUIMENU/DESKTOP/BREAKOUT). Scope: `user/guishell.c`,
`user/guimenu.c`, `user/desktop.c`, `user/breakout.c` vs `user/ws_e32_client.c` (chunks A/B
landed) and `user/guishost.h` / `variant/qemu_windowsrv.h` ABIs. Read-only survey; no code
changed. kernelhwsrv/ untouched.

## 0. Transport facts

- Execs **249/250/251** (`user/guishost.h:51-53`): 249 = LFB phys base, 250 = LFB byte
  size, 251 multiplexed — `arg1==0` returns `struct gui_shared*` (low RAM, holds
  `fb_w/fb_h`, `mouse_x/y`, `buttons`, `button_seq`, `cmds[640]`), `arg1>0` executes N
  display-list cmds (`GC_CLEAR/GC_RECT/GC_PUTC/GC_BLIT`). Kernel paints the LFB ring-0.
- Execs **271/272/273** (`variant/qemu_windowsrv.h:36-38`): `WS_BASE/WS_SIZE/WS_CTL` over
  `ws_shared`; exec 273 ops `WSO_CREATE_WINDOW/DESTROY/SET_DIRTY/PUSH_EVENT/POP_EVENT/
  SET_FOCUS/FRAME_TICK`; event ring `WSE_MOVE/DOWN/UP/KEY` (`struct ws_event`).
  **None of the four target apps call 271-273 directly** — only `winsrv.c`/`wsdl.c` do.
  These four get input from (a) the PortCon lease `con_acquire/con_tryread/con_release`
  (`user/runtime.h:73-75`) for keys and (b) kernel-ISR-published `gui_shared.mouse_x/y/
  buttons/button_seq` for the pointer.
- Exec **268** (`SvcMgrLaunch`, two TDesC8) used by guimenu/desktop to launch apps —
  svcmgr-owned, **not** in the winsrv deletion set; on the E32 path it becomes a genuine
  process create (RProcess/E32Run), separate from this map.
- All four already import genuine EUSER.DLL ord 915 (`RDebug::Printf`) via the PE2 IAT
  slot — the `marker()` pattern needs no change.

## 1. Per-app winsrv/exec call table

### GUISHELL.E32 (`user/guishell.c`) — static desktop paint + mouse readback

| file:line | call | what it does | ws_e32_client equivalent |
|---|---|---|---|
| guishell.c:143 | `exec2(249,0,0)` | LFB base (log only) | `ws_screen_size()` (op 17, landed, ws_e32_client.c:248) — pixels; base not needed |
| guishell.c:145 | `exec2(250,0,0)` | LFB byte size (log only) | `ws_screen_size()` w*h |
| guishell.c:149 | `exec2(251,0,0)` | get `gui_shared*` | `ws_open()` session handle replaces it (ws_e32_client.c:159) |
| guishell.c:156 | `gSh->magic/version` check | validate shared blk | `ws_server_ready()` (ws_e32_client.c:409) |
| guishell.c:97-98,162,174 | read `gSh->fb_w/fb_h` | framebuffer geometry | `ws_screen_size(&w,&h)` |
| guishell.c:122,134 | `exec2(251,gN,0)` | run display list (GC_CLEAR/GC_RECT/GC_PUTC) | `ws_draw_fill_rect()` (landed, ws_e32_client.c:344) for rects; **GC_PUTC text → `ws_text()` STUB** (ws_e32_client.c:445) |
| guishell.c:167-169 | read `gSh->mouse_x/y` | log pointer pos | **missing**: `ws_get_event()` STUB (ws_e32_client.c:460) → needs RWsSession::GetEvent |

### GUIMENU.E32 (`user/guimenu.c`) — keyboard-nav launcher

| file:line | call | what it does | ws_e32_client equivalent |
|---|---|---|---|
| guimenu.c:337,343 | `exec2(249/250)` | fb base/size (log) | `ws_screen_size()` |
| guimenu.c:347 | `exec2(251,0,0)` | shared blk | `ws_open()` |
| guimenu.c:354-359 | magic + `fb_w/fb_h` | validate + geometry | `ws_server_ready()` + `ws_screen_size()` |
| guimenu.c:62 (flush), :207-221, :398-402 | `exec2(251,gN,0)` | auto-flushing display list | fill_rect (landed) + **ws_text stub**; note GUI_MAX_CMDS auto-flush disappears (EWSRV redraws per write) |
| guimenu.c:365, :407 | `con_acquire()/con_release()` | exclusive input lease | no lease on E32 path — session implies it; drop calls |
| guimenu.c:300-323 `read_key()` | `con_tryread()` + ESC-seq decode | poll keys (W/S/A/D/Enter/Esc/R) | **missing**: `ws_get_event()` WSE_KEY; arrow ESC sequences → keymap fix in flight |
| guimenu.c:238 | `exec2(268,...)` | SvcMgrLaunch selected app | out of winsrv scope; genuine RProcess create (svcmgr retirement, separate) |
| guimenu.c:306,312,386 | `sleep_ms()` | poll pacing | keep (runtime.h) or event-wait |

### DESKTOP.E32 (`user/desktop.c`) — window manager: icons, taskbar, drag, click

| file:line | call | what it does | ws_e32_client equivalent |
|---|---|---|---|
| desktop.c:691-692 | `exec2(249/250)` | fb base/size | `ws_screen_size()` |
| desktop.c:700 | `exec2(251,0,0)` | shared blk | `ws_open()` |
| desktop.c:707-712 | magic + geometry | validate | `ws_server_ready()` + `ws_screen_size()` |
| desktop.c:74 (flush), :354-385 | `exec2(251,gN,0)` | full-frame display list | fill_rect + **ws_text stub**; single exec per frame not needed |
| desktop.c:737-738, :753-762 | read `gSh->buttons/button_seq` | click edge detection | **missing**: `ws_get_event()` WSE_DOWN/UP + `button_seq` equivalent |
| desktop.c:762, :788-800 | read `gSh->mouse_x/y` | pointer pos, drag, cursor sprite | **missing**: WSE_MOVE events |
| desktop.c:718, :467, :823 | `con_acquire/con_release` | input lease (+handoff to child) | drop; child handoff = process create |
| desktop.c:515-538 `read_key()` | `con_tryread()` | keys incl. drag/click kbd | **missing**: WSE_KEY via ws_get_event |
| desktop.c:402 | `exec2(268,...)` | launch icon app | RProcess create (out of winsrv scope) |
| desktop.c:287,335,814 | `time_now()` | taskbar clock | keep (runtime.h) |
| desktop.c:469 | `process_exit(0)` | handoff to launched child | keep |
| desktop.c:143-150 `gWins[8]` | up to 8 windows | WM stacking | **missing**: ws_e32_client is single-window (`sWin==0`); genuine EWSRV console = one window/session → needs multi-session or RWindowGroup decision |

### BREAKOUT.E32 (`user/breakout.c`) — real-time game loop

| file:line | call | what it does | ws_e32_client equivalent |
|---|---|---|---|
| breakout.c:335,340 | `exec2(249/250)` | fb base/size (log) | `ws_screen_size()` |
| breakout.c:345 | `exec2(251,0,0)` | shared blk | `ws_open()` |
| breakout.c:349, :351-352 | magic + `fb_w/fb_h` | geometry → playfield | `ws_server_ready()` + `ws_screen_size()` |
| breakout.c:323 | `exec2(251,gN,0)` per frame (~30 fps) | display list: clear/rects/text HUD | fill_rect + **ws_text stub**; per-frame full clear = fill_rect over window |
| breakout.c:373, :415 | `con_acquire/con_release` | input lease | drop |
| breakout.c:161-177 `handle_input()` | `con_tryread()` drain | A/D paddle, Enter, Esc | **missing**: WSE_KEY via ws_get_event (drain loop → non-blocking event poll) |
| breakout.c:412 | `sleep_ms(20)` | frame pacing | keep |

## 2. Missing-API list (what chunk D still needs in the ws_e32_client layer)

1. **`ws_text()` real glyph path** — stub at ws_e32_client.c:445. Same ops as the
   landed fill_rect (EConsoleSetTextColors 21 + EConsoleSetCursorPosAbs 6 +
   EConsoleWrite 10) but writing real UTF-16 chars instead of 0x20 spaces. Cheap, no
   input dependency. **Unblocks all four apps' paint.**
2. **`ws_get_event()` / input injection** — stub at ws_e32_client.c:460. Needs
   `RWsSession::GetEvent(TWsEvent)` import (new EUSER ordinal via the EWCLIENT
   pack_multi pattern) **plus** a port-owned PS/2→EWSRV event bridge (the
   WINSRV_MIGRATION_PLAN.md §5 blocker) and the keymap fix in flight. Maps PortCon
   `con_tryread` → WSE_KEY and `gui_shared.mouse_*`/`buttons` → WSE_MOVE/DOWN/UP.
   **This is the chunk-D hard gate.**
3. **`ws_button()` / `ws_present()`** — stubs (:450, :455). On the E32 path present is
   a no-op (EWSRV blits on write); button = fill_rect + ws_text composition. Cheap.
4. **`GC_BLIT` bulk blit** — no console-op equivalent in genuine EWSRV. Keep exec 270
   `FBO_FBBLIT` (survives winsrv deletion; retires with svcmgr) for full-frame blits
   (DOOM-style), or defer to a future RWindow/GC path. None of the four apps use it.
5. **Multi-window** — desktop.c manages 8 windows; ws_e32_client has one console
   window (`sWin`). Genuine EWSRV console model is one window per session → either
   one EWSRV session per window or a later RWindowGroup. Decision needed before D5.
6. **`ws_window_geom()`** returns zeros (ws_e32_client.c:428) — track locally or derive
   from `ws_screen_size`; cosmetic.
7. **`ws_set_size()`** returns KErrNotSupported (-18) from this tree's EWSRV
   (ws_e32_client.c:235-238) — IPC real but unsupported; don't rely on it for layout.

## 3. Suggested chunk split for D

- **D0** (no input): `ws_text()` via ops 21/6/10 + `ws_button`/`ws_present` composition
  → GUISHELL paint migrates (screen_size + fill_rect + text, drop 249/250/251).
- **D1** (gate): PS/2→EWSRV input bridge + keymap + `ws_get_event()` over
  RWsSession::GetEvent — enables WSE_KEY/MOVE/DOWN/UP for all interactive apps.
- **D2**: GUIMENU — keyboard nav loop swaps con_tryread→ws_get_event; exec 268 launch
  swapped to RProcess at the svcmgr step (flagged, not blocking paint/nav).
- **D3**: BREAKOUT — same input swap, per-frame fill_rect+text; no mouse.
- **D4**: DESKTOP — last; needs the multi-window decision (#5) plus click/drag from
  WSE_DOWN/MOVE/UP and the buttons/seq edge logic re-expressed on events.

**2026-11-26 — correction (retirement):** every live/pending winsrv-or-GUI premise on this page is superseded — winsrv/GUI desktop was RETIRED in `94ce9c9` (7 files deleted: `user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`, `user/wsclient.h`, `variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`, `variant/qemu_gui_exec.cpp`); execs 249/250/251 and the WINSRV launch path are gone (0 remaining `exec2(249/250/251)` call sites). Console shell + DOOM/Freedoom are the UI; genuine EWSRV is the target when the GUI returns, per AGENTS.md. the authoritative gate is the 15-row roster, 15/15 PASS (`OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, `/tmp/opencode/final27_gate.log`) at HEAD `ee8490e` with no commits. Sections 0–3 below (including the D0–D4 split) stay as a historical, non-actionable survey of the retired transport.

**2026-11-26 — supersession (FINAL27, `FINAL27_RC=0`, 15/15):** the "keymap fix in flight" clause in §2 item 2 (line 92) is **CLOSED** and `ws_get_event()` is no longer a stub. It is **implemented** at `user/ws_e32_client.c:633`: the synchronous `RConsole::Read` shape — `SendReceive(CWsSession::EConsoleRead, …)` on ord 1547, blocking until the server drains a queued read request — with no new EUSER ordinals. The keymap fix is LANDED and VERIFIED per `docs/KEYMAP_FIX_STATUS.md` (built-in `CKeyTranslatorX` table in `genuine/ewsrv/patched/ky_tran.cpp`, no EKDATA.DLL, 0 occurrences of `KERN 8`/#PF). `user/guishost.h` is deleted (`94ce9c9`), so `ws_get_event()` cannot target guishost — it targets genuine EWSRV. Gate of record: `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, `/tmp/opencode/final27_gate.log`, HEAD `ee8490e`, no commits. This note supersedes §2 item 2's status clause only.

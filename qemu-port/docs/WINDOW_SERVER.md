# The window server on the R1 QEMU port

This document describes the **new window system** being built concurrently by
other agents: a real *window server* E32 (`WINSRV`) that owns the framebuffer and
one or more *client* E32s (the first is the windowed calculator `WSDL`) that draw
into their own private window buffers and let the server composite them. It is
the successor to the cooperative "one app paints the whole screen" model
documented in [`GUI_PROGRAMMING.md`](GUI_PROGRAMMING.md),
[`GUI_APP_DEVELOPMENT.md`](GUI_APP_DEVELOPMENT.md) and
[`GRAPHICAL_DESKTOP.md`](GRAPHICAL_DESKTOP.md).

**Status:** the server, its kernel-facing header `variant/qemu_windowsrv.h` and
the client header `user/wsclient.h` are still being written. The *architecture*
below is the contract those pieces implement; where a numeric constant or exec
number is not yet frozen, this document says so and points at the header that
will be authoritative. The underlying video path (Bochs VBE DISPI, mode 0x118,
1024x768x32 LFB, pitch 4096) and the low-level display-list execs 249/250/251
are **already stable** and are not changed by the window system.
`kernelhwsrv/` is not involved and stays untouched.

## 1. Why a window server?

The old model is simple but cannot coexist:

- Each graphical E32 (`GUISHELL`, `GUIHELLO`, `BREAKOUT`, `DTCALC`, `DTEDIT`,
  `DESKTOP`, ...) calls exec 251 with a whole-screen display list and thereby
  **paints the entire 1024x768 framebuffer** on every frame.
- Input is a single PortCon lease (`con_acquire()` / `con_tryread()`): exactly
  one process owns the keyboard at a time.
- "Launching" an app from `DESKTOP` is a **handoff**: DESKTOP prints
  `DT: launching ...` / `DT: handing off`, calls `con_release()` and exits; the
  launched app takes over the screen and the input lease. When it exits you type
  `desktop` again to get back. There is no z-order, no clipping, no focus
  manager and no event delivery — a "window" is just a decorated rectangle.
- The kernel teletype mirrors `KPrintf` into the same LFB with no arbitration,
  so shell output can scroll over a GUI frame between submits.

That is fine for one full-screen program at a time, but not for a desktop where
several applications are visible and interactive simultaneously. The new model
keeps the proven kernel display-list executor but moves frame ownership into a
single long-lived server process.

## 2. The new model in one picture

```
   WSDL (client)          other clients ...        WINSRV (server)
   private window buffer  private window buffer    composited front buffer
   + event polling        + event polling               |
            \                    /                       | exec: commit/composite
             \                  /                        v
        user/wsclient.h  (register / present / events)  kernel
                          \        |        /      variant/qemu_windowsrv.h
                           \       |       /       low, identity-mapped RAM
                            v      v      v        (slots + input ring)
                          slow execs (249/250/251 family + window-srv execs)
                                     |
                                     v
                 Bochs VBE mode 0x118 LFB (~0xFD000000, pitch 4096)
                     PS/2 IRQ12 mouse + PortCon keyboard -> input ring
```

- **The server owns the framebuffer.** Only `WINSRV` submits full-screen
  display lists to exec 251. It draws the desktop background / panel and then
  blits every registered client window into its back buffer, publishing one
  complete frame per exec (no half-composed frames are ever visible).
- **A client owns a window, not the screen.** `WSDL` allocates a *private*
  pixel buffer of fixed maximum geometry, paints its calculator into that
  buffer with simple rect/glyph primitives, and marks the window dirty. It never
  dereferences the LFB (the high-half VRAM is supervisor-only; a ring-3 write
  would fault with `MMU #PF`).
- **The kernel is the transport.** New slow execs (in the same invalid
  64-bit-atomic slot family used by the GUI protocol — 247/248 netbridge,
  249/250/251 GUI) let a client register a window slot, publish its buffer,
  fetch input events and ask the server to recompose. The exact numbers are
  defined in `variant/qemu_windowsrv.h` once it lands; treat that header plus
  `user/wsclient.h` as the ABI authority.

## 3. Kernel execs and shared memory layout

The window system reuses the low-RAM, identity-mapped shared-memory trick of
`struct gui_shared` (`user/guishost.h`), scaled up to a table of windows:

| Facility | Role | Contract |
|---|---|---|
| FB base / size execs (249 / 250) | discovery only | server logs LFB base/size; clients must not write VRAM |
| DRAW exec (251) | kernel executes `GC_CLEAR` / `GC_RECT` / `GC_PUTC` display list, then repaints the mouse | the server submits one composited frame through this exec |
| shared-window exec | query address of the low-RAM window-server control block | returns a `struct ws_shared *` analogue (magic/version, slot count, dirty/valid bits) |
| register / release exec | reserve and free one of the fixed window slots | server or client calls; a slot carries owner id, x/y/w/h, buffer offset/stride and z-order |
| commit / event exec | mark a window buffer dirty; drain a keyboard/mouse event ring | cooperative: no interrupts into userland, only polling |

The control block follows the established pattern: a `magic` (`"WSRV"`-style
fourcc), a `version`, a slot count, per-slot geometry/state words and a bounded
input-event ring with a monotonically increasing sequence counter (the same
edge-detection idea as `gui_shared.button_seq`). Both new headers preserve field
offsets when adding words, so old GUI programs keep working. All fields are
`volatile` because the kernel and the other process write them concurrently;
always verify the magic and version before use, and never index past the
advertised slot/ring capacities.

The per-window pixel buffer is sized once at registration (a fixed maximum,
e.g. a calculator-sized window). There is no dynamic resize: a client that wants
a different shape releases its slot and registers a new one. Colour words remain
`0x00RRGGBB`; text is still the kernel 5x7-in-8x8 font scaled 2x into 16x16
glyph cells with an 18-pixel row pitch (64x42 cells at 1024x768).

## 4. Server lifecycle

1. **Boot.** `make userland && make isogrub` bundles `WINSRV.E32`, `WSDL.E32`
   and `EUSER.DLL` into `/PROGRAMS.IMG` on the GRUB ISO; boot with `-vga std`
   so VBE mode 0x118 is active (look for `DISPLAY: VBE mode 0x118 set ...` and
   `GUI: execs 249/250/251 installed ...` on COM1).
2. **Start the server.** At the `X:/>` prompt type `winsrv`. It claims the
   window-server control block, acquires the PortCon lease, paints the wallpaper
   and panel into its back buffer and emits `WSV: ...` markers (startup, first
   composited frame, client registered, events routed, shutdown).
3. **Serve clients.** The server runs a cooperative loop: poll the input ring,
   route events to the focused window, merge dirty client buffers into the back
   buffer, submit one composited frame with exec 251, then `sleep_ms()` to yield.
   Clients may be started and stopped while it runs.
4. **Client exit.** A client deregisters its slot and releases any lease; the
   server repaints the now-empty area and keeps running.
5. **Server exit.** `WINSRV` releases the control block and input lease and
   returns to the shell (`X:/>`). Any remaining clients lose their display
   service and should exit too.

## 5. Client API (`user/wsclient.h`)

A client is an ordinary PE2 E32 ring-3 program linked with `start.o`,
`runtime.o` and `EUSER.DLL` ordinal 915 markers (see
`GUI_APP_DEVELOPMENT.md` section 5). The client-side header wraps the raw execs
in a small API, conceptually:

| Call | Purpose |
|---|---|
| `ws_connect()` | find/validate the shared control block; fail cleanly if `WINSRV` is not running |
| `ws_create_window(x, y, w, h, title)` | reserve a slot, obtain the private buffer base and stride |
| `ws_rect()` / `ws_text()` | draw into the *private* buffer (never the LFB) |
| `ws_present()` | mark the window dirty so the server recomposites on its next frame |
| `ws_poll_event()` | non-blocking next input event (key, click, focus) for this window |
| `ws_close()` | release the slot and any input lease before returning from `app_main()` |

`WSDL`, the windowed calculator client, follows this pattern: it paints a
title bar, a result field and a keypad into its window buffer, handles digit /
operator keys and clicks from `ws_poll_event()`, and prints `WDL: ...` markers
at start, first frame, each evaluated expression and clean exit. Its markers and
the server's `WSV:` markers are what the host test asserts.

## 6. Input event flow

Input is still **cooperative and polled**, not pushed into a preemptive handler:

1. The PS/2 driver (`variant/qemu_mouse.cpp`) decodes AUX packets on IRQ12 and
   maintains the software cursor; the PortCon device delivers ASCII keys.
2. The kernel writes key presses, cursor moves and click edges into the shared
   input ring and bumps a sequence counter (press+release between two polls is
   still detectable, as with `button_seq` today). PS/2 `E0` arrow scancodes are
   still dropped; ANSI `ESC [ A/B/C/D` over serial and WASD-style keys are the
   reliable inputs.
3. `WINSRV` drains the ring, decides which window has focus (hit-test clicks
   against window rectangles, track the focused window on key presses), and
   queues events for that client.
4. The client calls `ws_poll_event()` in its frame loop and repaints only when
   state changed (the "dirty flag" pattern from `GUI_APP_DEVELOPMENT.md`), then
   calls `ws_present()` and sleeps a short `sleep_ms()` to yield.

If the server or a client stops polling, events back up or are dropped and the
UI appears frozen — the same limitation as the older toolkit, now shared between
processes rather than confined to one.

## 7. Running it and verifying it

```
make userland && make isogrub
qemu-system-i386 -M q35 -m 128 -vga std -display none -serial stdio \
  -cdrom build/symbian-grub.iso -boot d -no-reboot
```

At the `X:/>` prompt:

```
winsrv          # start the server; look for WSV: READY / first frame
wsdl            # start the windowed calculator client; look for WDL: markers
```

Drive the calculator with a few digits, `+`, `=` and Enter/Esc; both windows
remain painted because the server keeps compositing while the client runs. A
PNG screendump of the composed desktop can be taken over QMP (`screendump`)
under `/tmp/opencode/window_system.png`.

`tools/r1_window_system_test.py` is the consolidated host test: it probes
`build/user/WINSRV.E32` and `build/user/WSDL.E32`, skips gracefully if either is
not built yet, stages what is present together with `EUSER.DLL` on the GRUB ISO,
boots once with `-vga std`, feeds `winsrv` then `wsdl`, asserts both marker
families and a clean log (no `Exc` / `PanicKernExec` / `FAULT:`), and grabs a
best-effort QMP screendump. Syntax check with
`python3 -m py_compile tools/r1_window_system_test.py`, then run
`python3 tools/r1_window_system_test.py`.

## 8. Limitations (honest scope)

- **Cooperative.** The server and clients take turns via `sleep_ms()`; there is
  no preemptive GUI scheduling and a blocking call stalls the whole desktop.
- **Fixed resources.** A fixed number of window slots and a fixed maximum
  private-buffer size per window. No dynamic allocation of new windows beyond
  the slot table.
- **No resize.** Window geometry is chosen at registration; move/resize
  gadgets, maximize/minimize and snapping are not implemented.
- **No compositor effects.** Blitting is opaque rect/glyph copying: no
  z-order animation, alpha blending, shadows, sprites, image decode or true GPU
  double buffering. The "back buffer" is software staging.
- **Primitive drawing only.** The kernel still understands only `GC_CLEAR`,
  `GC_RECT` and `GC_PUTC`; display lists remain bounded by `GUI_MAX_CMDS = 640`.
- **Shared LFB.** The kernel teletype still mirrors `KPrintf` into the same
  framebuffer; without arbitration, shell output can briefly overlay a
  composited frame until the next server submit.
- **Input caveats.** PS/2 E0 arrows are dropped; clicks are detected from a
  sequence counter, not delivered as a rich event stream; the PortCon lease is
  still exclusive, so text-mode programs and the GUI do not share the keyboard.

## 9. How this differs from the old DESKTOP

| | Old DESKTOP / windowed-app model | New window-server model |
|---|---|---|
| Framebuffer owner | whichever E32 is currently running | long-lived `WINSRV` |
| Windows | decorated rectangles in one whole-screen display list | registered slots with private pixel buffers |
| Coexistence | one graphical app at a time; launch = handoff + exit | server + several clients visible at once |
| Compositing | none (last repaint wins) | server blits client buffers into one frame |
| Input | one PortCon lease, polled by the active app | kernel event ring; server routes to focused window |
| Launching | select icon -> handoff -> `desktop` again to return | start `winsrv`, then `wsdl` (and more clients) from the shell |
| Failure domain | a crash blanks the single app's screen | a crashing client only loses its own slot |

The two models coexist: all the existing full-screen GUI programs keep working
exactly as before, and `DESKTOP` remains the shipped one-app-at-a-time shell.
The window server is an additional mode, not a replacement that breaks the
existing display-list ABI.

## 10. References

- [`GUI_PROGRAMMING.md`](GUI_PROGRAMMING.md) — stable display-list protocol,
  execs 249/250/251, `struct gui_shared`, primitives and font geometry.
- [`GUI_APP_DEVELOPMENT.md`](GUI_APP_DEVELOPMENT.md) — window anatomy, dirty-flag
  repaint, EUSER ordinal 915 markers, build rules and QEMU test pattern.
- [`GRAPHICAL_DESKTOP.md`](GRAPHICAL_DESKTOP.md) — user-facing old desktop,
  handoff semantics and honest gaps.
- `user/guishost.h`, `user/guilib.h`, `user/guilib.c` — existing ABI headers/toolkit.
- `variant/qemu_windowsrv.h`, `user/wsclient.h` — **authoritative** new headers
  (when present); `user/winsrv.c` and `user/wsdl.c` — reference implementations.
- `tools/r1_window_system_test.py`, `tools/r1_gui_apps_test.py`,
  `tools/r1_graphical_desktop_test.py` — host-side proofs.

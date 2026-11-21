# The graphical desktop on the R1 QEMU port

This is a reader-facing guide to the *graphical* side of the port: what is
actually on the framebuffer, which programs paint it, how input reaches them,
and how to run and verify them. It is derived from the running code —
`variant/qemu_display.cpp`, `variant/qemu_gui_exec.cpp`, `variant/qemu_mouse.cpp`,
`variant/kern_shell.cpp`, `user/guishost.h`, `user/guilib.h`, `user/guilib.c`,
`user/guishell.c`, `user/guimenu.c`, `user/guihello.c`, `user/breakout.c`,
`user/saver.c`, `tools/mkgrubiso.py` — plus `docs/GUI_PROGRAMMING.md` and
`docs/GUI_AUDIO.md`. `kernelhwsrv/` is not involved and stays untouched.

Read `docs/GUI_PROGRAMMING.md` if you want to *write* a GUI program; this
document explains the desktop *as a user* and ties all the pieces together.

## 1. Honest scope: what "desktop" means here

The port has a real 1024x768x32 graphical framebuffer and real full-screen
graphical programs. It does **not** have a preemptive, multi-window compositor.
There is no window server, no z-order, no clipping, no focus manager and no
event delivery. A "window" is a filled rectangle with a title strip; each E32
program runs **one at a time**, repaints the whole framebuffer, and yields the
screen when it exits. The kernel's text console draws into that same LFB with no
arbitration, so shell output (including marker prints) can scroll over a GUI
frame until the app repaints it. Think "cooperative single-app desktop", not
Windows/X11/Wayland.

Consequently the console is *already graphical*: it is text rendered into the
same VBE linear framebuffer. See section 3.

## 2. Video path: VBE 2.0 mode 0x118

`variant/qemu_display.cpp` programs the Bochs VBE DISPI register pair at I/O
**0x1CE (index) / 0x1CF (data)** directly from protected mode — no real-mode
`int 10h` thunk is needed because the flat 4 GiB identity map is already active.

| Property | Value |
|---|---|
| VBE mode | **0x118**, 1024 x 768 x 32 bpp, linear framebuffer (LFB) |
| LFB physical base | read from the VGA PCI BAR (class 03.00); QEMU/q35 normally **0xFD000000** (do not hard-code) |
| Pitch / stride | **4096 bytes** (1024 * 4) |
| Pixel word | `0x00RRGGBB`; the low byte (X) is unused |
| Fallback | non-Bochs VGA: legacy mode 13h, 320x200x8 at 0xA0000 (full Sequencer/CRTC/GC/AC tables) |

Boot banner to look for on COM1 (and on screen):

```
DISPLAY: VBE mode 0x118 set (Bochs DISPI id=0xb0c?, fallback=0)
DISPLAY: LFB base=0xFD000000 1024x768x32 pitch=4096
GUI: execs 249/250/251 installed fb=0x... size=... geometry=1024x768x32 shared=0x...
```

The first DRAW exec also does a VRAM readback and prints
`GUI: pixel readback bg=0x000080 title=0x0080c0 OK|MISMATCH`. If you do not see
these lines you are probably not on QEMU's standard VGA — always boot with
`-vga std` (see section 8).

## 3. Why the console looks text-based

`Asic::DebugOutput()` sends every `KPrintf` both to COM1 and to
`VgaTeletype(c)` in `variant/kern_shell.cpp`. The teletype keeps the classic
80x25 text buffer at `0xB8000` **and** mirrors the same stream into the VBE LFB
via `display_putc()` using the scaled font. The font is a 5x7 stroke table
rendered into 8x8 cells and blown up 2x:

- glyph cell **16x16 px**, row advance **18 px**, giving a **64x42** text grid
  at 1024x768;
- a `GC_PUTC` paints one 16x16 glyph cell; advance `x` by 1 (16 px) per char.

Because `KPrintf` appends `\r\n`, most lines arrive doubled; the LFB mirror
collapses the duplicate newline so no blank rows appear. The `EKA2>` / `X:/>`
prompt is therefore visible on-screen as well as on the serial line. So when
you see "text", you are looking at glyphs painted onto the same framebuffer the
GUI uses — there is no separate text mode. Type a graphical program's name to
take over the whole LFB with real graphics.

## 4. How user programs paint: the display-list protocol

The LFB lives above 0x80000000, which the flat PDE map marks U/S=0
(supervisor-only); a ring-3 E32 that touched VRAM directly would take an MMU
#PF. Instead, programs build a small display list in a low, identity-mapped
RAM buffer and ask the kernel to execute it in ring 0. This is the same trick
netbridge uses, exposed as slow execs in `user/guishost.h`:

| Exec | Macro | Call | Result |
|---|---|---|---|
| 249 | `QE_EXEC_GUI_FBBASE` | `exec2(249, 0, 0)` | framebuffer physical base (u32, log only) |
| 250 | `QE_EXEC_GUI_FBSIZE` | `exec2(250, 0, 0)` | framebuffer byte size (u32) |
| 251 | `QE_EXEC_GUI_DRAW` | `exec2(251, 0, 0)` | address of low-RAM `struct gui_shared` |
| 251 | `QE_EXEC_GUI_DRAW` | `exec2(251, count, 0)` | execute `count` commands, refresh mouse |

`struct gui_shared` carries `GUI_MAGIC` (`"GUI1"`), `GUI_VERSION`, the kernel's
`count`/`draw_calls` counters, `fb_w`/`fb_h`, `mouse_x`/`mouse_y`, and up to
`GUI_MAX_CMDS = 160` commands (`type,a,b,c,d,e`). The three opcodes are:

- `GC_CLEAR (0)` — clear whole framebuffer to `0x00RRGGBB` (first command of
  every frame);
- `GC_RECT (1)` — `x,y,w,h,color`, a clipped filled rectangle;
- `GC_PUTC (2)` — `x,y,char,fg,bg`, one glyph cell.

There is no line, circle, blit, alpha or sprite opcode; everything is composed
from rectangles and glyphs. Frames are whole-screen replacements, so a program
redraws everything it wants visible on each submit. The 160-command budget
fits a 64-cell text line plus a handful of rectangles comfortably.

`user/guilib.h` / `user/guilib.c` is a thin toolkit layered on this protocol:
`gui_clear()` starts a private back list, `gui_rect()`/`gui_text()`/
`gui_window()`/`gui_button()` append to it, and `gui_present()` copies it into
the kernel's front list and submits it in one exec. That is a *software* back
buffer (no half-drawn frame is ever published), not a GPU double-buffer.

## 5. The graphical programs

All are genuine PE2 E32 ring-3 programs that import EUSER.DLL ordinal 915
(`RDebug::Printf`) for markers, with the port's `DebugPrint` exec as fallback.
Markers go to COM1 and are mirrored into the LFB.

- **GUISHELL** (`build/user/GUISHELL.E32`, markers `GUI: ...`) — the original
  desktop shell: dark-blue desktop, `EKA2 PC OS DESKTOP` title bar, three
  labelled windows (`FILES`, `SYSTEM`, `ABOUT`) and an `EKA2 DESKTOP READY`
  taskbar. Draws once, reports mouse x/y, then paints a green `LIVE` animation
  frame after 250 ms. Uses the low-level protocol directly.
- **GUIMENU** (`GUIMENU.E32`, markers `GM: ...`) — a full-screen graphical
  launcher: two columns of bundled applications, a highlighted selection bar,
  a live description panel and key hints. Enter calls trusted slow exec 268
  (`SvcMgrLaunch`) to start the chosen E32; if the exec is unavailable it prints
  `GM: fallback ...` and you launch from the shell instead.
- **GUIHELLO** (`GUIHELLO.E32`, markers `GH: ...`) — the worked `guilib`
  example: a titled window, three focusable buttons (`OPEN`/`EDIT`/`QUIT`), a
  live status panel and mouse-position readout. Demonstrates toolkit drawing,
  ANSI arrow decoding and clean Esc exit.
- **BREAKOUT** (`BREAKOUT.E32`, markers `BRK: ...`) — a real-time graphical
  game: bouncing ball, paddle, brick rows, score and win/game-over states. Uses
  `con_tryread()` in the frame loop with a short `sleep_ms()` between frames.
- **SAVER** (`SAVER.E32`, markers `SV: ...`) — a six-frame animation demo:
  bouncing box, shifting starfield (LCG random) and a 48-bar colour-gradient
  sweep. Useful as a pure rendering smoke test with no interaction required.
- **DESKTOP** (intended markers `DT: ...`) — the consolidated desktop program.
  It is **not present in `build/user/` yet** (another agent is building it);
  the regression test skips it gracefully until `DESKTOP.E32` appears. GUISHELL
  is today's shipped equivalent.
- Related: **SHOT** (`SHOT.E32`, `SH: ...`) renders an RGBA patch and saves the
  raw framebuffer bytes as `SCREEN.RAW` through PortFile IPC — a userland
  "screenshot" without ever dereferencing VRAM.

## 6. Mouse cursor and clicks

`variant/qemu_mouse.cpp` enables the 8042 aux port and AUX IRQ, resets and
streams the PS/2 mouse, and binds IRQ12 with `Interrupt::Bind`. The ISR decodes
button/delta bytes into a clamped 1024x768 software cursor. During each DRAW
exec the kernel paints a **yellow crosshair** (`mouse_render_cursor()`),
executes the user's display list, then overlays its own white box cursor so it
survives desktop repaints. The cursor position is published in
`gui_shared.mouse_x/mouse_y` and readable from programs via `gui_mouse()`.

**Clicks are not wired into applications.** `gui_shared` publishes the cursor
position but no button word, and `guilib`'s `gui_mouse()` returns `buttons = 0`.
The shell verbs `mouse` (x/y/buttons/packets/id/pktsize), `mouse poll` (polled
fallback) and `mouse test` (synthetic 3-byte AUX packet via command 0xD3) prove
the IRQ path, but userland selection is keyboard-driven. Attach a mouse with
`-usbdevice mouse`, `-device usb-mouse`, or QEMU's default PS/2 mouse.

## 7. Keyboard controls

Input is **not** part of the GUI exec protocol. It comes through the PortCon
logical device: `con_acquire()` takes the exclusive input lease (the serial
shell stops consuming keys), `con_tryread()` is non-blocking, and
`con_release()` must be called before `app_main()` returns. Programs get ASCII
bytes, not raw scancodes.

| Program | Keys |
|---|---|
| GUIMENU | W/S (or k/j) up/down, A/D (or h/l) jump columns, **Enter** launch, **Esc** exit |
| GUIHELLO | Arrow keys / WASD move focus, **Enter** press button, **Esc** quit |
| BREAKOUT | A/D move paddle, **Enter** start/restart, **Esc** exit |
| Console / shell | type a program name + Enter; Esc returned by most GUI apps |

**PS/2 E0 (extended/arrow) scancodes are currently dropped**, so WASD are the
primary bindings. ANSI escape sequences arriving over the *serial* stream
(`ESC [ A/B/C/D`) are decoded into arrow keys by `guilib` and by GUIMENU's
`read_key()`; a future kernel that decodes PS/2 arrows will just work with the
existing programs. GUIHELLO and GUIMENU also have bounded idle timeouts so a
headless test cannot hang forever.

## 8. Launching and quitting; boot-to-desktop

1. Build the userland and ISO: `make userland && make isogrub` (see
   `docs/BUILD_GUIDE.md`). `mkgrubiso.py` bundles every `build/user/*.E32`
   (above its size floor) plus `EUSER.DLL` into `/PROGRAMS.IMG` on the ISO,
   loaded by GRUB as a Multiboot module and mounted at `X:` at boot.
2. Boot with standard VGA so the Bochs VBE path is active:

   ```
   qemu-system-i386 -M q35 -m 128 -vga std \
     -cdrom build/symbian-grub.iso -boot d \
     -serial stdio -display none -no-reboot
   ```

   (`tools/boot_grub.sh` wraps this.) At `X:/>` type the program name, e.g.
   `desktop`, `guimenu`, `breakout`, `guihello`, `saver` (or
   `python3 tools/r1_qemu_display_test.py` to check `display test` first).
3. Quit with **Esc** where supported, or let the idle timeout / frame cap end
   the program; you return to the serial shell. GUISHELL exits on its own after
   the animation frame.

**Boot-to-desktop option:** `tools/mkgrubiso.py` now ships a third GRUB menu
entry — "Symbian EKA2 Desktop (graphical)" — alongside the default full kernel
and nanokernel entries. Select it (arrow keys during the ~10 s GRUB countdown)
and GRUB passes `desktop=1` on the Multiboot cmdline (MBI flag bit 2). The shell
latches that token at startup and auto-runs `DESKTOP.E32` via `RunImage()`,
so the graphical desktop appears without typing a command (log:
`Boot desktop: starting DESKTOP.E32`, then `DT: DESKTOP READY`). The default
entry still boots the normal `X:/>` shell, so all tests are unaffected. If you
do not want the menu, simply type `desktop` at the shell prompt instead.

## 9. Screenshots

- **Host side:** with `-vga std` and a real framebuffer, QEMU can dump the
  screen. `-monitor unix:/tmp/opencode/mon.sock,server,nowait` plus HMP
  `screendump /tmp/opencode/shot.ppm` (convert to PNG with Pillow), or QMP
  `{"execute":"screendump","arguments":{"filename":"...","format":"png"}}`.
  `tools/r1_guihello_test.py` and `tools/r1_guimenu_test.py` show both patterns;
  screendumps are best-effort and may be unavailable on headless QEMU.
- **Guest side:** `SHOT.E32` (`shot`) writes a raw RGBA patch to `SCREEN.RAW`
  via PortFile IPC. The kernel itself never exposes VRAM to ring 3.

## 10. Regression test

`tools/r1_graphical_desktop_test.py` is the single consolidated host test. It
probes `build/user/*.E32` for DESKTOP, GUIMENU, BREAKOUT, GUIHELLO, GUISHELL
and SAVER, skips any that are missing, stages each present program plus
`EUSER.DLL` on a FAT32 disk, boots the GRUB ISO with `-vga std`, feeds the
program name, a few keys and Esc, and asserts the right markers (`DT:`, `GM:`,
`BRK:`, `GH:`, `GUI:`, `SV:`) with no `Exc`/`PanicKernExec`, taking a screendump
when possible. Run `python3 -m py_compile tools/r1_graphical_desktop_test.py`
for a syntax check, then `python3 tools/r1_graphical_desktop_test.py`.

## 11. Known gaps (recap)

- Cooperative single-app-at-a-time desktop; no compositor, window server,
  multi-windowing, z-order, clipping, focus, event delivery or preemptive
  scheduling of GUI apps.
- Mouse cursor position is published, but clicks are not delivered to programs.
- PS/2 E0 arrow scancodes are dropped; ANSI arrow sequences work over serial.
- Console teletype and GUI share the LFB without arbitration.
- Only clear/rectangle/glyph primitives; no alpha, sprites, image decode or
  true double buffering; `guilib`'s back list is a software staging buffer.
- Audio is AC'97 PCM-OUT tone generation only (exec 240); see `GUI_AUDIO.md`.

## 12. Mouse input — requirements and verification

The guest uses the **PS/2 mouse on i8042 IRQ12** (`variant/qemu_mouse.cpp`),
**not** a USB tablet. QEMU must expose a PS/2 mouse: the default `-M q35` does.
Do **not** pass `-usbdevice tablet` or `-device usb-mouse` — the guest PS/2
driver will not receive absolute HID events from those devices.

You also need a display that actually delivers pointer events — e.g. `-display
gtk` with a real window, or VNC. `-display none` delivers no pointer events at
all (fine for serial-only smoke tests, useless for mouse testing).

**Verified behaviour:** relative PS/2 motion moves the drawn cursor (confirm
with the `DT: mouse x=... y=...` markers and screendump pixel diffs), and
`mouse info` reports packet/ISR counts. Kernel diagnostics exist: `mouse`
(x/y/buttons/packets/id/pktsize), `mouse test` (synthetic 3-byte AUX packet via
command 0xD3) and `mouse press`.

**If the pointer seems frozen:** check the QEMU flags (no USB tablet/mouse, a
real display), make sure the GTK window has focus, and type `mouse info` at the
shell *before* launching the desktop to confirm packets arrive; `DT: mouse
x=... y=...` markers appear on motion. Remember clicks are not wired into
applications (section 6/11) — cursor motion working is the expected result.

# Building windowed GUI applications on the R1 QEMU port

This is a builder's guide for writing a **windowed** PE2 E32 application — the
kind of program represented by the windowed calculator (`DTCALC`) and text
editor (`DTEDIT`). It is derived from the running code: `user/guishost.h`,
`user/guilib.h`, `user/guilib.c`, `user/guihello.c`, `user/desktop.c`,
`docs/GUI_PROGRAMMING.md` and `docs/GRAPHICAL_DESKTOP.md`. `kernelhwsrv/` is not
involved and stays untouched.

Read `docs/GUI_PROGRAMMING.md` for the raw display-list protocol and
`docs/GRAPHICAL_DESKTOP.md` for the user's-eye view of the desktop. This
document is the layer above: how to assemble those pieces into an interactive
windowed app, and how to prove it works.

## 1. The GUI stack, bottom to top

```
  user's windowed E32  (dtcalc.c / dtedit.c / your_app.c)
        |  guilib widgets: gui_window(), gui_button(), gui_text()
        v
  user/guilib.c        private BACK display list; gui_present() publishes it
        |  exec2(QE_EXEC_GUI_DRAW, n, 0)
        v
  user/guishost.h      struct gui_shared (low, identity-mapped RAM)
        |  slow execs 249 / 250 / 251
        v
  kernel (variant/qemu_gui_exec.cpp)   executes GC_CLEAR/GC_RECT/GC_PUTC
        |  Bochs VBE DISPI, mode 0x118
        v
  QEMU standard VGA   1024x768x32 LFB (~0xFD000000, pitch 4096)
```

1. `variant/qemu_display.cpp` selects VBE mode `0x118`. The LFB sits above
   `0x80000000`, which the flat page map marks supervisor-only, so a ring-3
   program that writes VRAM directly takes an MMU #PF. **Never dereference the
   LFB from user code.**
2. Slow execs (declared in `user/guishost.h`):
   - **249** `QE_EXEC_GUI_FBBASE` — framebuffer physical base (u32, log only);
   - **250** `QE_EXEC_GUI_FBSIZE` — framebuffer byte size (u32);
   - **251** `QE_EXEC_GUI_DRAW` with `arg1==0` — address of `struct gui_shared`;
   - **251** with `arg1>0` — execute `arg1` commands, then refresh the PS/2
     mouse crosshair.
3. `struct gui_shared` carries `GUI_MAGIC` (`"GUI1"`), `GUI_VERSION`, kernel
   counters, `fb_w/fb_h`, `mouse_x/mouse_y`, the PS/2 `buttons` mask and a
   `button_seq` edge counter, plus `cmds[GUI_MAX_CMDS]`.
4. Three opcodes only: `GC_CLEAR` (whole frame), `GC_RECT` (filled, clipped
   rectangle), `GC_PUTC` (one 16x16 glyph cell). Everything visual is composed
   from those. The kernel repaints its crosshair after every submit.
5. `guilib.c` is the toolkit layered on top. Draw calls append to a private
   back list; `gui_present()` copies it into the kernel's front list in one
   atomic exec. That is a *software* staging buffer — no half-drawn frame is
   ever published. It is not GPU double buffering.

Older records (`GUI_PROGRAMMING.md`, `GRAPHICAL_DESKTOP.md`) quote a 160-command
budget; the current `GUI_MAX_CMDS` / `GL_MAX_CMDS` is **640**. Keep the two
definitions in sync if you change the ABI, and never append past the cap.

## 2. Window anatomy

A guilib window is not an OS object — it is a set of rectangles painted into the
current frame. `gui_window(x, y, w, h, title, border, fill)` draws, in order:

- **client fill** — a solid `GUI_BLUE` rectangle when `fill` is set;
- **border** — a 2 px `GUI_LIGHTGRAY` frame on all four sides when `border` is
  set;
- **title bar** — a `GUI_TITLEBAR` strip `GUI_ROW_PITCH` (18 px) high just inside
  the top border, with the title drawn in `GUI_WHITE` at `x+5, y+4`;
- **client area** — everything below the title bar. Your app owns it: draw
  calculator keys, editor text, status strips with `gui_text()` / `gui_rect()`.

`gui_button(win, x, y, w, h, label, focused)` draws a beveled 3D button inside a
window. `focused` selects the highlighted palette (yellow top edge and titlebar
fill) versus the normal gray. The `win` argument is a validity token from
`gui_window()`; geometry remains explicit in screen pixels.

Coordinates are pixels. The font cell is 16x16 px with an 18 px row pitch
(`GUI_CELL_W`, `GUI_CELL_H`, `GUI_ROW_PITCH`), so a 64x42 character grid fits the
screen. `gui_text()` advances one full cell per character.

## 3. Event model: a cooperative poll loop

There is no event dispatcher, no window server and no preemptive GUI
scheduling. Each app owns the framebuffer and the input lease while it runs and
must poll for everything:

```c
for (;;) {
    int key = gui_key();
    int mx, my, buttons;
    gui_mouse(&mx, &my, &buttons);
    ... handle key, handle click edges ...
    if (dirty) { repaint(); gui_present(); dirty = 0; }
    sleep_ms(30);                 /* yield; ~33 Hz input/paint cadence */
}
```

- `gui_key()` drains the PortCon lease acquired by `gui_init()`. It never
  blocks; `GUI_KEY_NONE` (0) means nothing yet. Printable bytes pass through,
  CR/LF map to `GUI_KEY_ENTER`, 8/127 to `GUI_KEY_BACKSPACE`, and ANSI
  `ESC [ A/B/C/D` sequences are decoded into `GUI_KEY_UP/DOWN/LEFT/RIGHT`.
  PS/2 `E0` arrow scancodes are currently dropped, so WASD-style keys or ANSI
  escapes over serial are the reliable inputs.
- `gui_mouse()` returns the kernel crosshair position and the live PS/2 button
  mask (bit 0 = left, 1 = right, 2 = middle).

**Click edges.** The PS/2 ISR writes `buttons` and increments `button_seq` in
`struct gui_shared` on every button-state change. Poll the sequence number to
detect a click even if press and release happen between two polls:

```c
struct gui_shared* sh = (struct gui_shared*)exec2(QE_EXEC_GUI_DRAW, 0, 0);
unsigned last_seq = sh ? sh->button_seq : 0;
...
if (sh && sh->button_seq != last_seq) {
    last_seq = sh->button_seq;
    if (sh->buttons & 1u) try_click(mx, my);   /* left press */
}
```

Re-check `magic == GUI_MAGIC` and `version == GUI_VERSION` before trusting the
struct. An app may also simply use `gui_mouse()`'s `buttons` output and compare
it with the previous mask; `button_seq` is what catches sub-poll click pairs.
All input is cooperative: if you stop polling, keys and clicks queue up or are
lost and the UI appears frozen. Keep `sleep_ms()` short (20–40 ms).

## 4. Dirty-region repaint

Frames are **whole-screen replacements**: `gui_clear()` resets the back list and
emits one `GC_CLEAR`. You cannot patch a single widget — anything you want
visible must be redrawn before the next `gui_present()`.

The efficient pattern for an interactive app is a **dirty flag**:

```c
static int dirty = 1;             /* paint once at startup */
...
if (key == GUI_KEY_ENTER) { do_action(); dirty = 1; }
if (sh && sh->button_seq != last_seq) { ...; dirty = 1; }
if (dirty) {
    paint_window();               /* gui_clear + window + widgets + status */
    gui_present();                /* one atomic exec 251 submit */
    dirty = 0;
}
sleep_ms(30);
```

This is "dirty-region" in the cooperative sense: the *whole frame* is the
region, and the flag suppresses redundant repaints when nothing changed. Do not
let a frame go unpainted after a state change — the kernel teletype shares the
LFB and shell output can scroll over your window between submits. For a live
readout (clock, cursor position, editor status line), either mark dirty when the
value changes or repaint on a bounded timer tick.

## 5. Markers: PE2 EUSER ordinal 915

Every bundled E32 prints serial markers so host tests can assert progress. Use a
genuine import of EUSER.DLL ordinal 915 (`RDebug::Printf`), with the port's
`DebugPrint` exec as fallback. Declare an IAT slot at `0x408000` with the
`--defsym=<app>_printf_slot=0x408000` linker flag and the matching
`--euser-import 915` conversion flag:

```c
extern unsigned int myapp_printf_slot;
typedef void (*NbPrintf)(const char*, ...);
static void marker(const char* s) {
    unsigned int fn = *(volatile unsigned int*)&myapp_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(s);
    else print(s);
}
```

Emit a short, stable prefix — e.g. `DTC: ...` for the calculator and
`DTE: ...` for the editor — at start, after `gui_init()`, after the first frame,
on every handled key/click, and at exit. The host test greps for these strings.

## 6. Build rule pattern

Add a three-stage rule to `user/user.mk` (the `.o` compiles, the `.elf` links
with a printf slot, the `.E32` converts with the EUSER import):

```make
userland: build/user/MYAPP.E32
build/user/myapp.o: user/myapp.c user/runtime.h user/guilib.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/myapp.elf: build/user/start.o build/user/runtime.o build/user/guilib.o build/user/myapp.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=myapp_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/MYAPP.E32: build/user/myapp.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
```

Link `build/user/guilib.o` into any app that uses the toolkit (as `guihello`
does). Build with `make userland && make isogrub`; `tools/mkgrubiso.py` bundles
every `build/user/*.E32` above its size floor, with `EUSER.DLL`, into
`/PROGRAMS.IMG` on the GRUB ISO.

## 7. QEMU test pattern

Always boot with **standard VGA** so the Bochs VBE path is active:

```
qemu-system-i386 -M q35 -m 128 -vga std -display none -serial stdio \
  -cdrom build/symbian-grub.iso -boot d -no-reboot
```

Look for `DISPLAY: VBE mode 0x118 set ...`, `GUI: execs 249/250/251 installed
...` and `GUI: pixel readback ... OK` on COM1. A host test stages the E32 plus
`EUSER.DLL`, waits for the `X:/>` prompt, types the program name, feeds a few
keys, asserts the app's markers, and rejects any `Exc`/`PanicKernExec` line.
Screendumps are best-effort on headless QEMU:

- HMP: `-monitor unix:/tmp/opencode/mon.sock,server,nowait`, then
  `screendump /tmp/opencode/guiapps_<name>.png` (PPM; convert with Pillow);
- QMP: `{"execute":"screendump","arguments":{"filename":"...","format":"png"}}`.

See `tools/r1_guihello_test.py` (single app), `tools/r1_desktop_test.py` (QMP)
and `tools/r1_gui_apps_test.py` (consolidated DTCALC/DTEDIT driver).

## 8. Launching an app

**From the shell:** at the `X:/>` prompt type the E32's base name (`dtcalc`,
`dtedit`, `guihello`) and press Enter. The kernel loads the image, binds
EUSER.DLL's ordinal 915 (`e32run: bind slot ... ord 915` in the log), and calls
`app_main()`. `gui_init()` takes the PortCon lease so the shell stops consuming
keys; call `gui_shutdown()` (or `con_release()`) before returning to the shell.

**From the desktop (once handoff is wired):** selecting an icon and pressing
Enter — or clicking it in builds that wire clicks — makes DESKTOP print its
launch/handoff markers, release the input lease and exit. The launched app then
owns the whole framebuffer. Type `desktop` again to return. The GRUB
"graphical" menu entry can auto-run DESKTOP via the `desktop=1` Multiboot token.

## 9. Limitations

- **No compositor / window server.** A window is rectangles, not a clipped,
  z-ordered surface. No focus manager, no event delivery, no multi-window app.
- **One windowed app at a time.** Each E32 repaints the whole framebuffer and
  owns the input lease; programs run cooperatively and sequentially.
- **Polling only.** `sleep_ms(30)` yields; blocking calls stall input. Clicks are
  edge-detected from `button_seq`, since press and release can occur between
  polls.
- **640-command display lists.** A full window plus a long text line can
  approach the cap; budget `GC_PUTC` calls (one per glyph) carefully and never
  write past `GUI_MAX_CMDS`.
- **Primitive drawing.** No alpha, sprites, blit, image decode or true double
  buffering; only clear, rect and glyph cells.
- **Shared LFB.** The kernel teletype and GUI programs share one framebuffer
  with no arbitration; shell output may briefly overlay your window.

## 10. Worked skeleton: decorated window + button + click

```c
#include "runtime.h"
#include "guilib.h"
#include "guishost.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int myapp_printf_slot;
typedef void (*NbPrintf)(const char*, ...);
static void marker(const char* s) {
    unsigned int fn = *(volatile unsigned int*)&myapp_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(s); else print(s);
}

#define BTN_X 250
#define BTN_Y 300
#define BTN_W 200
#define BTN_H 40

static int inside(int x, int y, int bx, int by, int bw, int bh) {
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

int app_main(void) {
    struct gui_shared* sh;
    unsigned last_seq;
    int mx = 0, my = 0, mb = 0, presses = 0, dirty = 1;

    marker("MY: START");
    if (gui_init() != 0) { marker("MY: FAIL gui_init"); return 1; }
    sh = (struct gui_shared*)exec2(QE_EXEC_GUI_DRAW, 0, 0);
    if (!sh || sh->magic != GUI_MAGIC || sh->version != GUI_VERSION) {
        marker("MY: FAIL shared magic"); gui_shutdown(); return 1;
    }
    last_seq = sh->button_seq;
    marker("MY: window ready");

    for (;;) {
        int key = gui_key();

        gui_mouse(&mx, &my, &mb);
        if (sh->button_seq != last_seq) {
            last_seq = sh->button_seq;
            if ((sh->buttons & 1u) && inside(mx, my, BTN_X, BTN_Y, BTN_W, BTN_H)) {
                ++presses;                       /* click handler */
                marker("MY: button clicked");
                dirty = 1;
            }
        }
        if (key == GUI_KEY_ENTER) { ++presses; dirty = 1; }
        if (key == GUI_KEY_ESC)   { marker("MY: ESC"); break; }

        if (dirty) {
            char status[80];
            unsigned n = 0;
            const char* p = "MY: press ENTER or click OK   presses=";
            while (*p) status[n++] = *p++;
            /* small unsigned formatter (no libc) */
            { char t[12]; int z = 0;
              if (!presses) t[z++] = '0';
              while (presses) { t[z++] = (char)('0' + presses % 10); presses /= 10; }
              while (z) status[n++] = t[--z]; }
            status[n] = 0;

            gui_clear(GUI_NAVY);
            gui_window(120, 100, 640, 420, "MY APP", 1, 1);
            gui_button(0, BTN_X, BTN_Y, BTN_W, BTN_H,
                       "OK", key == GUI_KEY_ENTER);
            gui_text(150, 380, status, GUI_WHITE, GUI_BLUE);
            gui_present();
            dirty = 0;
        }
        sleep_ms(30);
    }
    gui_shutdown();
    marker("MY: DONE");
    return 0;
}
```

Add the `user/user.mk` rule from section 6, build the ISO, stage the E32, and
drive it with the QEMU pattern from section 7. That is the full path from source
to a verified, windowed, clickable GUI application.

**2026-11-26 — correction:** this page's live/pending winsrv-or-GUI premise is superseded: the winsrv/GUI desktop was **retired in `94ce9c9`** (7 files deleted — `winsrv.c`, `guishost.h`, `wsclient.*`, `qemu_windowsrv.*`, `qemu_gui_exec.cpp`), and execs **249/250/251** plus the **WINSRV launch path are gone**, so sections 1-8 describe a stack that no longer exists; the **console shell plus DOOM/Freedoom are the UI**, and **genuine EWSRV** is the target when GUI returns, per `AGENTS.md`. the authoritative gate is the **15-row** roster at **15/15 PASS** (`OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, `/tmp/opencode/final27_gate.log`) at HEAD `ee8490e`, with no commits.

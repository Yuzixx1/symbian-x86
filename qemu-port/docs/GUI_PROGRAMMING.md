# Writing GUI programs on the R1 QEMU port

This is a practical guide to painting pixels and reading input from a ring-3
PE2 E32 program. It is derived from the running code:
`user/guishost.h`, `user/guishell.c`, `user/saver.c`, `user/shot.c`,
`variant/qemu_display.cpp`, `variant/qemu_gui_exec.cpp` and `docs/GUI_AUDIO.md`.
`kernelhwsrv/` is not involved and stays untouched.

**Status of the two API layers**

- **Stable, low-level API — use this today.** The display-list protocol in
  `user/guishost.h` and slow execs **249/250/251**. It has shipped in
  `GUISHELL`, `SAVER` and `SHOT` and is what every GUI program below uses.
- **Higher-level GUI toolkit — in progress.** A `user/guilib.h` style helper
  (windows, widgets, event loop) is being built by another agent. When it lands
  it will sit *on top of* the same display-list protocol; it will not change the
  exec numbers or `struct gui_shared` layout described here. Until then, treat
  the protocol below as the contract.

## 1. The framebuffer

`variant/qemu_display.cpp` programs the Bochs VBE DISPI registers (I/O 0x1CE
index / 0x1CF data) directly from protected mode and selects VBE mode **0x118**:

| Property | Value |
|---|---|
| Resolution / depth | **1024 x 768 x 32 bpp** (RGBX) |
| LFB physical base | ~**0xFD000000** (read from the VGA PCI BAR; do not hard-code) |
| Pitch / stride | **4096 bytes** (1024 * 4) |
| Pixel format | 32-bit word `0x00RRGGBB`; lower byte is the unused X byte |
| Fallback | 320x200x8 mode 13h at 0xA0000 (non-Bochs VGA) |

The LFB sits above 0x80000000, which the flat PDE map marks **U/S=0**
(supervisor-only). A ring-3 E32 process that writes VRAM directly faults with
an MMU #PF. **Never dereference the LFB from user code.** Discover it with exec
249/250 for logging only, and draw by submitting a display list through exec
251. The kernel executes the list in ring 0, then overlays the mouse crosshair.

Boot banner to look for: `DISPLAY: LFB base=0x... 1024x768x32 pitch=4096` and
`GUI: execs 249/250/251 installed fb=0x... size=... geometry=1024x768x32`.

## 2. Slow execs 249 / 250 / 251

Declared in `user/guishost.h`, called with `exec2(unsigned number, unsigned
arg1, unsigned arg2)` from `runtime.c`:

| Exec | Macro | Call | Result |
|---|---|---|---|
| 249 | `QE_EXEC_GUI_FBBASE` | `exec2(249, 0, 0)` | framebuffer physical base (u32) |
| 250 | `QE_EXEC_GUI_FBSIZE` | `exec2(250, 0, 0)` | framebuffer byte size (u32) |
| 251 | `QE_EXEC_GUI_DRAW` | `exec2(251, 0, 0)` | address of low-RAM `struct gui_shared` |
| 251 | `QE_EXEC_GUI_DRAW` | `exec2(251, count, 0)` | execute `count` commands, refresh mouse |

`arg1 == 0` on exec 251 is the *query* form; `arg1 > 0` is the *submit* form.
These slots are patched into the invalid 64-bit-atomic family of
`EpocSlowExecTable` (247/248 already belong to netbridge). Exec **240** is the
audio tone exec, a separate facility.

## 3. Shared buffer layout (`struct gui_shared`)

```c
#define GUI_MAGIC       0x47554931u   /* "GUI1" */
#define GUI_VERSION     1u
#define GUI_MAX_CMDS    160u

struct gui_cmd {
    volatile unsigned type;
    volatile unsigned a, b, c, d, e;
};

struct gui_shared {
    volatile unsigned magic;      /* GUI_MAGIC after boot init */
    volatile unsigned version;    /* GUI_VERSION */
    volatile unsigned count;      /* kernel: commands run last call */
    volatile unsigned draw_calls; /* kernel: number of DRAW execs served */
    volatile int      fb_w, fb_h; /* framebuffer geometry (1024, 768) */
    volatile int      mouse_x, mouse_y; /* kernel PS/2 cursor position */
    volatile unsigned reserved;
    struct gui_cmd    cmds[GUI_MAX_CMDS];
};
```

All fields are `volatile`: the kernel writes `count`, `draw_calls` and the mouse
coordinates while your process is running. Always verify `magic == GUI_MAGIC`
and `version == GUI_VERSION` before using the buffer, and cap your command
index at `GUI_MAX_CMDS` (extra `cmd()` calls are silently dropped).

## 4. Drawing primitives

Three opcodes are defined; each is filled into one `struct gui_cmd`:

| Opcode | Fields used | Meaning |
|---|---|---|
| `GC_CLEAR` (0) | `a` | clear whole framebuffer to `0x00RRGGBB` colour |
| `GC_RECT` (1) | `a=x b=y c=w d=h e=colour` | axis-aligned filled rectangle, clipped by the kernel |
| `GC_PUTC` (2) | `a=x b=y c=char d=fg e=bg` | draw one glyph cell, foreground/background colours |

Colours are plain `0x00RRGGBB` (e.g. dark blue `0x00000080`, white
`0x00FFFFFF`). `GC_CLEAR` is normally the first command of every frame;
`GC_RECT` builds windows, borders and game sprites; `GC_PUTC` writes text.
There is no line, circle, blit or alpha opcode — compose everything from
rectangles and glyphs. A 160-command budget per submit means a full-screen
text line (64 cells), a handful of rectangles, and a clear fit comfortably.

## 5. Font and text geometry

The kernel font is a 5x7 stroke table rendered into 8x8 cells and scaled 2x
(`QEMU_DISPLAY_FONT_SCALE = 2`, `QEMU_DISPLAY_CELL_W = 8*2`):

- glyph cell: **16 x 16 pixels**;
- row advance: **18 pixels** (16 + a 2 px gap);
- at 1024x768 this gives a **64 x 42 character grid**;
- a `GC_PUTC` at `(x, y)` paints a 16x16 cell; advance `x` by 1 per character
  (i.e. 16 px), `y` by 1 per row (i.e. 18 px). The `text()` helper in
  `guishell.c` emits one `GC_PUTC` per character with `++x`.

Lowercase letters are accepted (the kernel folds a-z onto A-Z); unsupported
characters render as a blank cell.

## 6. The build/submit cycle

1. `int shb = exec2(QE_EXEC_GUI_DRAW, 0, 0);` — get the shared buffer base.
2. Cast to `struct gui_shared*` and verify magic/version.
3. Reset your local command counter `gN = 0`.
4. Append commands with a helper that guards `gN < GUI_MAX_CMDS`.
5. Submit with `exec2(QE_EXEC_GUI_DRAW, gN, 0);`.
6. To animate, sleep a few ms (`sleep_ms`) and repeat from step 3.

Frames are whole-screen replacements: each submit starts with `GC_CLEAR`, so
you redraw everything you want visible. The kernel repaints the mouse after
every submit, so a crosshair survives your repaint; read `mouse_x/mouse_y` from
the shared struct if your UI needs the cursor position.

## 7. Keyboard input for games

Input is **not** part of the GUI exec protocol. It goes through the PortCon
logical device and the helpers in `user/runtime.h`:

- `int con_acquire(void)` — take the exclusive input lease (the serial shell
  stops consuming keys while held); returns negative on failure.
- `int con_tryread(void)` — non-blocking: a byte 0..255, or negative
  (`-18` = `KErrNotReady`, meaning "no key yet").
- `int read_line(char* buf, unsigned cap, int timeout_ms)` — line-oriented,
  echoes, handles backspace, folds CR to LF; returns length or negative.
- `void con_release(void)` — give the lease back **before** `app_main` returns.

The cooked path delivers **ASCII bytes**, not raw keyboard scancodes. The
kernel driver owns the 8042/USB scancode-to-ASCII translation; there is no
public userland API that exposes make-codes, so an arrow key is not delivered
as a universal scancode value. Escape sequences from a terminal arrive as their
literal bytes (e.g. ESC `[` `A`) and must be decoded by your program if you
want them. The proven game convention is simpler: steer with **w/a/s/d** (as
in `user/snake.c`, `user/pacman.c`, `user/tetris.c`) read via `read_line()` or
`con_tryread()`, optionally lower-casing A-Z. A real-time graphical game should
use `con_tryread()` in its frame loop and poll `sleep_ms(10..30)` between
frames; always `con_release()` on exit.

## 8. Worked minimal example (window + key)

Save as `user/hello_win.c`:

```c
#include "runtime.h"
#include "guishost.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int hello_printf_slot;      /* IAT slot at 0x408000 */
typedef void (*NbPrintf)(const char*, ...);
static void marker(const char* s) {
    unsigned int fn = *(volatile unsigned int*)&hello_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(s); else print(s);
}

static struct gui_shared* gSh;
static unsigned gN;
static void cmd(unsigned t, unsigned a, unsigned b, unsigned c,
                unsigned d, unsigned e) {
    if (gN < GUI_MAX_CMDS) {
        struct gui_cmd* q = &gSh->cmds[gN++];
        q->type=t; q->a=a; q->b=b; q->c=c; q->d=d; q->e=e;
    }
}
static void rect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned col)
    { cmd(GC_RECT, x, y, w, h, col); }
static void text(unsigned x, unsigned y, const char* s, unsigned fg,
                 unsigned bg) {
    while (*s) { cmd(GC_PUTC, x, y, (unsigned char)*s++, fg, bg); ++x; }
}

static void frame(char key) {
    gN = 0;
    cmd(GC_CLEAR, 0x00000080u, 0, 0, 0, 0);          /* blue desktop */
    rect(200, 120, 620, 300, 0x000060A0u);          /* window body */
    rect(200, 120, 620, 2,   0x00C0C0C0u);          /* border */
    rect(200, 418, 620, 2,   0x00C0C0C0u);
    text(220, 150, "HELLO GUI - PRESS q TO EXIT",
         0x00FFFFFFu, 0x000060A0u);
    text(220, 180, key ? (const char[]){key, 0} : "KEY: -",
         0x00FFFFFFu, 0x000060A0u);
    exec2(QE_EXEC_GUI_DRAW, gN, 0);
}

int app_main(void) {
    int shb, key = 0;
    marker("HG: START");
    shb = exec2(QE_EXEC_GUI_DRAW, 0, 0);
    if (shb <= 0) { marker("HG: FAIL no shared buffer"); return 1; }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->version != GUI_VERSION) {
        marker("HG: FAIL shared magic"); return 1;
    }
    if (con_acquire() < 0) { marker("HG: console busy"); return 1; }
    frame(0);
    marker("HG: window drawn");
    for (;;) {
        int c = con_tryread();
        if (c >= 0) {
            if (c >= 'A' && c <= 'Z') c += 32;
            key = c;
            frame(key);
            if (key == 'q') break;
        }
        sleep_ms(20);
    }
    con_release();
    marker("HG: DONE");
    return 0;
}
```

Add a build rule in `user/user.mk` mirroring `guishell`:

```make
build/user/hello_win.o: user/hello_win.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/hello_win.elf: build/user/start.o build/user/runtime.o build/user/hello_win.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=hello_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/HELLOWIN.E32: build/user/hello_win.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
```

Stage `HELLOWIN.E32` + `EUSER.DLL` on a FAT disk or bundle it in
`PROGRAMS.IMG`, boot with `qemu-system-i386 -M q35 -m 128 -vga std ...` and type
`HELLOWIN` at the `EKA2>` prompt. Markers (`HG: ...`) use genuine EUSER.DLL
ordinal 915 with the `DebugPrint` exec as fallback.

## 9. Known gaps

- No compositor, window server, widgets, z-order, clipping or event delivery:
  a "window" is just rectangles; each E32 runs sequentially and repaints the
  whole framebuffer. This is what the in-progress toolkit aims to improve.
- No alpha blending, sprites, image decode or double buffering — only clear,
  rect and glyph cells.
- The framebuffer console (`VgaTeletype`) and GUI programs share the LFB with
  no arbitration; launching a GUI program simply repaints it.

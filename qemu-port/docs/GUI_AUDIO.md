# Display, GUI and sound in the QEMU port

This document covers the graphics pipeline (`variant/qemu_display.cpp`,
`variant/qemu_gui_exec.cpp`, `variant/qemu_mouse.cpp`, `variant/kern_shell.cpp`)
and the audio pipeline (`variant/qemu_audio.cpp`, `variant/qemu_audio_exec.cpp`,
`user/*.c`). Both are Variant-owned; `kernelhwsrv/` is untouched. Public APIs
are declared in `variant/qemu_variant.h`; the kernel/user transport struct is
`user/guishost.h`. All objects are listed in `KERNFULL_OBJS` (Makefile line 312).

## 1. VBE 2.0 framebuffer (`qemu_display.cpp`)

* **Mode** — 1024x768x32bpp, VBE mode **0x118**, linear framebuffer. The driver
  programs the *Bochs VBE DISPI* register pair at I/O **0x1CE (index) / 0x1CF
  (data)** directly from protected mode; no real-mode `int 10h` thunk is needed
  because the flat 4 GiB identity map is already active. It writes an ID check
  first (`0xB0C0..0xB0CF`), disables the screen, programs XRes=1024, YRes=768,
  Bpp=32, Bank=0, VirtWidth=1024, YOffset=0, then enables with
  `ENABLE|LFB|NO_CLEAR` (bits 0, 6, 5) and reads the geometry back.
* **LFB base** — taken from the VGA PCI BAR (class 03.00) with a base in the
  high half; QEMU/SeaBIOS on q35 normally assigns **0xFD000000**, while the
  driver's last-resort default is 0xE0000000. **Pitch = 1024*4 = 4096 bytes.**
  Fallback for non-Bochs VGA is legacy mode 13h (320x200x8 at 0xA0000) using
  full Sequencer/CRTC/GC/AC register tables.
* **Public API** — `display_init()` (idempotent, boot constructor),
  `display_info()`, `display_clear(color)`, `display_rect(x,y,w,h,color)`,
  `display_putc(x,y,ch,fg,bg)`, `display_test()`,
  `display_geometry(w,h,bpp)`, `display_lfb()`, `display_fb_size()`,
  `display_getpixel(x,y)`. Colors are `0x00RRGGBB`; `PutPixel` writes 32/24/16/8
  bpp according to the active mode.
* **Font/grid** — a 5x7 stroke-font table is rendered into 8x8 cells, with each
  font pixel blown up into a `QEMU_DISPLAY_FONT_SCALE = 2` block: glyph cells
  are **16x16** (`QEMU_DISPLAY_CELL_W = 8*2`) and the row advance is **18 px**
  (`CELL_H = 8*2 + 2`, a 2 px gap). At 1024x768 this yields a **64x42 text
  grid**. These compile-time constants are shared with the shell so glyph and
  cursor advance always agree.
* **`display test`** — clears dark blue, draws a green 4 px border, paints
  `EKA2 GUI READY` in white, then reads back pixel (0,0) and (10,10) and prints
  `DISPLAY TEST: pixel readback bg=... border=... OK|MISMATCH` plus
  `EKA2-GUI-READY framebuffer OK`. Boot banner: `DISPLAY: VBE mode 0x118 set
  (Bochs DISPI id=0xb0c?, fallback=0)` and `DISPLAY: LFB base=0x... 1024x768x32
  pitch=4096`.

## 2. Console mirroring (`VgaTeletype` in `kern_shell.cpp`)

`Asic::DebugOutput()` sends every `KPrintf` character both to COM1 and to
`VgaTeletype(c)`. The teletype maintains the classic 80x25 text buffer at
`0xB8000` (with CRTC cursor updates) **and** mirrors the same stream into the
VBE LFB via `display_putc()` in the scaled font. Because genuine `KPrintf`
appends `\r\n` to each line, most lines arrive as `\r\n\r\n`; the LFB mirror
collapses the duplicate newline (`gFxLineFresh`) so no blank rows appear —
COM1 and the 0xB8000 buffer remain byte-for-byte unchanged. The framebuffer
grid scrolls by copying whole 32-bit rows. The `EKA2>` prompt is therefore
visible on-screen as well as on the serial console, and `VgaInit()` clears the
LFB before the shell starts. The shell runs as a genuine `NThread` spawned
from `Asic::Init3`.

## 3. Display HAL

`QemuDisplayRegisterHal()` is called from `QemuVariantAsic::Init3()` (after
`K::Init3` allocated `K::HalEntryArray`) and calls
`Kern::AddHalEntry(EHalGroupDisplay, DisplayHalFunction, 0)`, printing
`DISPLAY: EHalGroupDisplay(4) registered r=N`. `DisplayHalFunction` answers
`EDisplayHalScreenInfo` with a 24-byte `TScreenInfoV01`-compatible struct
(address valid, LFB pointer, width, height) via `Kern::InfoCopy`, and accepts
`EDisplayHalWsRegisterSwitchOnScreenHandling` as a no-op; everything else
returns `KErrNotSupported`. This is the path `UserSvr::ScreenInfo` uses.

## 4. Kernel↔user GUI transport (`qemu_gui_exec.cpp`, `user/guishost.h`)

VRAM is above 0x80000000, which the flat PDE map marks U/S=0: a ring-3 E32
process that touches it directly would fault. The bridge follows the
netbridge pattern — a low, identity-mapped shared buffer plus patched slow
execs in `EpocSlowExecTable` (the invalid 64-bit-atomic family 247–251;
247/248 already belong to netbridge):

* **exec 249** — `QE_EXEC_GUI_FBBASE`: returns the framebuffer physical base.
* **exec 250** — `QE_EXEC_GUI_FBSIZE`: returns the framebuffer byte size.
* **exec 251** — `QE_EXEC_GUI_DRAW`: `arg1 == 0` returns the address of
  `struct gui_shared`; `arg1 > 0` executes that many display-list commands in
  ring 0 and refreshes/publishes the mouse position.

`struct gui_shared` carries `GUI_MAGIC` ("GUI1"), `GUI_VERSION`, `count`,
`draw_calls`, `fb_w/fb_h`, `mouse_x/mouse_y`, and up to `GUI_MAX_CMDS = 160`
`struct gui_cmd` entries (`type,a,b,c,d,e`). Opcodes: `GC_CLEAR` (color),
`GC_RECT` (x,y,w,h,color), `GC_PUTC` (x,y,char,fg,bg). `gui_init()` is a boot
constructor that calls `display_init()` and `mouse_init()`, zeroes and
publishes the shared struct, and installs the three exec handlers, printing
`GUI: execs 249/250/251 installed fb=0x... size=... geometry=1024x768x32
shared=0x...`.

During a DRAW exec the kernel calls `mouse_render_cursor()`, scans VRAM for the
mouse's **yellow** crosshair arms (`ScanCursor`) to recover x/y, executes the
user's list, then overlays its own white box cursor so it survives desktop
repaints. The first draw also does a VRAM pixel readback at (0,0) and (100,20)
and prints `GUI: pixel readback bg=... title=... OK|MISMATCH`.

## 5. Ring-3 GUI programs

* **`user/guishell.c` → GUISHELL.E32** — discovers fb base/size (249/250),
  obtains the shared buffer (251, arg1=0), builds a display list with a dark
  blue desktop, a title bar (`EKA2 PC OS DESKTOP`), three labelled windows
  (`FILES`, `SYSTEM`, `ABOUT`) and a taskbar (`EKA2 DESKTOP READY`), submits it
  (251, arg1>0), reports the kernel mouse x/y, then after 250 ms draws a green
  `LIVE` animation frame. Markers via EUSER.DLL ordinal 915 `RDebug::Printf`.
* **`user/saver.c` → SAVER.E32** — six animation frames: bouncing box,
  shifting starfield (LCG random), and a 48-bar colour-gradient sweep. Each
  frame begins with `GC_CLEAR` + title strip and is published through exec 251.
* **`user/shot.c` → SHOT.E32** — renders a 64x64 RGBA patch at (16,96) with a
  white crosshair into a private buffer, mirrors it into the LFB via a one-row
  per-scanline display list, and saves the raw 16384 bytes as `SCREEN.RAW`
  through PortFile IPC (`file_save`). Ring-3 never dereferences the LFB.
* All three use `exec2()` and the `guishost.h` layout; test drivers are
  `tools/r1_guishell_test.py`, `r1_saver_test.py`, `r1_shot_test.py` (feed the
  shell verbs `guishell`, `saver`, `shot` after staging the E32 and EUSER.DLL on
  a FAT32 disk).

## 6. Audio (`qemu_audio.cpp`, `qemu_audio_exec.cpp`)

The AC'97 controller is PCI class **04.01** (`8384:7600` on QEMU), with BAR0 =
NAM mixer and BAR1 = NAB bus-master, both I/O-space on QEMU (typically
`0xC000`/`0xC400`). `audio_init()` is a boot constructor: enable I/O+MEM+bus
master, reset PCM-OUT and codec, poll the vendor-ID ready condition, set the
front DAC to 48 kHz and unmute master/PCM-out.

The data path uses a 2-entry BDL (address + ctl_len, IOC bit 31) and a static
PCM buffer. `audio_play_test()` duplicates a 48-entry integer 1 kHz sine table
to both stereo channels for 0.1 s (4800 frames). `audio_tone(freq, ms)` accepts
20 Hz–20 kHz and 1–500 ms, phase-accumulates the same table in **16.16 fixed
point** (no FPU/libm), and fills up to 24000 frames. Both program BDBAR, LVI=0,
clear PoSR and set RPBM, then poll `PoSR.DCH` with `PoPICB == 0`. Shell verbs:
`audio` (BARs/vendor ID/GLOB_STA/PO_CIV/PO_PICB) and `audio play`.
Boot banner: `audio: AC97 00:xx.x BAR0=...(IO) BAR1=...(IO)`, `audio: vendor ID
8384:7600 codec ready`.

`qemu_audio_exec.cpp` exposes `audio_tone()` to ring 3 as slow exec **240**
(`QE_EXEC_AUDIO_TONE`, `a1`=Hz, `a2`=ms), patching the sole still-invalid slot;
it prints `audio exec: tone installed at slot 240`. Callers:
* **`user/soundplr.c`** — plays C4–E4–G4–C5 (262/330/392/523 Hz, 200 ms each).
* **`user/morse.c`** — `encode <text>` / `play <text>` / `selftest` at 600 Hz
  (dot 100 ms, dash 300 ms), graceful degradation if the exec is missing.
* **`user/music.c`** — `scale <root>` / `play <root>` / `selftest` using an
  equal-temperament C4–B4 table (A4=440 Hz) and 0/2/4/5/7/9/11 major steps,
  150 ms per note.
Tests: `tools/r1_qemu_audio_test.py`, `r1_soundplr_test.py`, `r1_morse_test.py`,
`r1_music_test.py`; QEMU needs `-audiodev none,id=a0 -device AC97,audiodev=a0`.

## 7. PS/2 mouse (`qemu_mouse.cpp`)

The 8042 aux port (I/O 0x64/0x60) is enabled with AUX IRQ (config bit 1), reset
to defaults, read for its device ID (3-byte or 4-byte wheel packets), and put
into streaming mode. IRQ12 (slave PIC IR4) is bound with
`Interrupt::Bind(12, MouseIsr, 0)`; the shell-time re-arm explicitly opens the
master cascade IR2 (`0x21 &= ~4`) and uses `QemuIrq::Enable(12)` rather than
`Interrupt::Enable(12)` to avoid clobbering the shadow masks of live IRQs. The
ISR decodes button/delta bytes into a clamped 1024x768 software cursor and
`mouse_render_cursor()` paints a **yellow crosshair** on the LFB with
`display_rect()` (erasing with the GUI background blue). The shell verb `mouse`
calls `mouse_info()` to report x/y/buttons/packets/id/pktsize; `mouse poll` is a
polled fallback; `mouse test`
injects a synthetic 3-byte AUX packet via command 0xD3 to prove the IRQ path.
QEMU attaches one with `-usbdevice mouse` / `-device usb-mouse` or the default
PS/2 mouse.

## 8. Current gaps

* There is **no compositor, window server or multi-app GUI**: `GUISHELL` draws
  a static desktop (windows are rectangles, not live widgets) and one animated
  `LIVE` indicator; each E32 runs sequentially. No clipping, z-order, event
  delivery or input focus.
* No real sound-card mixer: only AC97 PCM-OUT tone generation; no PCM capture,
  no volume UI, no sample playback, and audio completion is **polled** (no IRQ).
* The framebuffer console and the GUI both write the same LFB with no
  arbitration; switching from the shell to a GUI program simply repaints it.

**2026-11-26 — correction (supersedes the live/pending winsrv-or-GUI premise of this page):** the winsrv/GUI desktop was **RETIRED** in commit `94ce9c9`, which deleted 7 files — `winsrv.c`, `guishost.h`, `wsclient.*`, `qemu_windowsrv.*`, `qemu_gui_exec.cpp`; the GUI execs **249/250/251** and the `WINSRV` launch path are gone, so sections 4–5 describing them are historical only. The **console shell + DOOM/Freedoom** are the UI, and genuine **EWSRV** is the target when GUI returns, per `AGENTS.md`; the 15-row gate is **15/15 PASS** (`FINAL27_RC=0`) at HEAD `ee8490e` with no commits.

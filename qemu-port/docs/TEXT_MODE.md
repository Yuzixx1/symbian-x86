# Booting in real VGA text mode, switching to VBE for graphics

This guide explains the console display architecture of the R1 QEMU port: the
machine starts in **genuine VGA text mode 3** and moves to a **VBE 2.0 linear
framebuffer** only when a graphical E32 program needs pixels. It is derived from
`variant/qemu_display.cpp`, `variant/kern_shell.cpp`, `variant/qemu_variant.h`,
`variant/qemu_gui_exec.cpp`, `variant/qemu_mouse.cpp`, `docs/GRAPHICAL_DESKTOP.md`
and `docs/GUI_AUDIO.md`. `kernelhwsrv/` is not involved and stays untouched.

If you only want to *run* programs, `docs/GRAPHICAL_DESKTOP.md` and
`docs/r1_boot_and_selftest.md` are the usage guides. This document is about
*what the video hardware is doing* and how to prove it from a host test.

> **Status, honestly:** this architecture is landing in parallel with this
> document. The pieces that already exist in the tree are the mode-3 teletype
> at `0xB8000`, the VBE 0x118 driver and display-list protocol, `display_set_mode()`
> plus the `FbMirrorReset()` / `GuiGeometryReset()` / `MouseGeometryReset()`
> switch hooks, and the `QemuFbMirrorSet()` mute. The acceptance gate for the
> full behaviour described here is `tools/r1_textmode_test.py`. If the boot
> banner still says `DISPLAY: VBE mode 0x118 set` at the prompt, that build has
> not landed the lazy-switch half yet; trust the source and the test result.

## 1. The two video modes

| | Boot / shell | Graphical app |
|---|---|---|
| VGA/VBE mode | mode **3** (colour text) | VBE **0x118**, LFB |
| Geometry | **80 x 25** cells | **1024 x 768** pixels |
| Host screendump size | **720x400** or **640x400** | **1024x768** |
| Memory | text buffer at **0xB8000** | LFB from VGA PCI BAR (~0xFD000000) |
| Cell / pitch | 8x16 or 9x16 character cell | 4096 bytes (1024 * 4) |
| Palette | **16** colours, attribute byte `0x07` | 0x00RRGGBB pixels |
| Who writes it | `VgaTeletype()` / `EmitRaw()` in `kern_shell.cpp` | GUI display lists via exec 251, DOOM's fbblit |

The text buffer is the classic two-byte-per-cell layout: low byte = character,
high byte = attribute (blink/background/foreground). `sCol`, `sRow` and the CRTC
cursor registers at I/O `0x3D4/0x3D5` keep the hardware cursor in sync; scrolling
copies whole 80-cell rows. Every `KPrintf` reaches `Asic::DebugOutput()`, which
sends the bytes to COM1 (`QemuUart::PutC`) and to the teletype for the screen.
`VgaInit()` clears the 2000 cells at startup, so the shell prompt is produced by
the same hardware text path a real PC BIOS would use.

## 2. Why boot in text mode at all

Earlier builds called `display_init()` from a static constructor, so the Bochs
VBE DISPI registers were programmed to 1024x768x32 *before* the shell printed
anything. The 0xB8000 cells were then no longer scanned out, and the kernel had
to emulate a terminal by painting a 5x7 stroke font into the LFB at 2x scale
(`display_putc()`, 16x16 glyph cells, 18 px row advance, a 64x42 grid). That
worked, but it had real costs:

* text was a bitmap: blurry, chunky and expensive (every character repainted
  256 pixels, every scroll copied ~3 MB of framebuffer);
* console output could scroll over a GUI frame unless the mirror was muted,
  which is exactly the console-bleed/flicker the DOOM visual test guards against;
* real operating systems do not do this — they boot in the firmware's 80x25
  text mode and leave VBE mode setting until a graphical app actually asks for
  it. The BIOS character generator gives crisp glyphs, hardware scrolling and
  a natural 16-colour palette at no CPU cost.

Booting in mode 3 restores that behaviour. The serial console and the visible
screen now agree byte-for-byte (both come from the same teletype stream), and
COM1-only marker output stays cheap.

## 3. The lazy switch to VBE 0x118

Nothing switches modes while you type at `X:/>`. The switch is initiated when
the shell launches a known full-screen program. `IsGraphicsImage()` in
`variant/kern_shell.cpp` matches the base name (case-insensitively, ignoring
`X:` prefixes and `.EXE`/`.E32`) against DESKTOP, GUISHELL, GUIMENU, GUIHELLO,
BREAKOUT, SAVER, DTCALC, DTEDIT, WINSRV, WSDL and DOOM. Before `E32Run()` that
launch path calls `QemuFbMirrorSet(1)` so every launch-time `KPrintf` and the
child's own `RDebug::Printf` markers go to COM1 only — they must not paint over
the pixels the app is about to produce.

The actual mode change is `display_set_mode(0x118)` in `variant/qemu_display.cpp`:

1. `display_init()` discovers the Bochs VBE extension by reading DISPI ID
   register index 0 at I/O `0x1CE`/`0x1CF` (expect `0xB0C0..0xB0CF`).
2. `DispiProgram(1024, 768, 32)` disables the screen, writes XRes/YRes/Bpp/Bank/
   VirtWidth/YOffset, then re-enables with `ENABLE | LFB | NO_CLEAR`.
3. The geometry is read back and committed to `gLfb`, `gWidth`, `gHeight`,
   `gBpp`, `gPitch` and `gMode`. The LFB base comes from the VGA PCI BAR
   (class 03.00, high-half prefetchable); QEMU/q35 normally reports
   `0xFD000000`. Never hard-code it.
4. `display_clear(0)` blanks the new framebuffer, then the three switch hooks
   run: `FbMirrorReset()` drops the cached text grid/cursor,
   `GuiGeometryReset()` refreshes `fb_w/fb_h` in `struct gui_shared`, and
   `MouseGeometryReset()` re-clamps the software crosshair to 1024x768.
5. A serial marker records the event: `VBE: SWITCH OK mode=0x118 1024x768x32
   LFB=0x... pitch=4096`. A failed programming attempt restores the old
   geometry and prints `VBE: SWITCH FAIL mode=... (kept ...)`, leaving the
   previous mode intact.

Programs then paint as before: ring-3 builds a display list in low identity-
mapped RAM and asks slow exec 251 to run it in ring 0 (`GC_CLEAR`, `GC_RECT`,
`GC_PUTC` — see `docs/GRAPHICAL_DESKTOP.md` §4). DOOM uses the fbblit extension
of the same path. Nothing about that protocol changes; only *when* the
framebuffer is created moves from boot to first graphical launch.

## 4. What happens when a graphical app exits

Be explicit about this: the guest does **not** currently issue a real-mode
`int 10h` to switch the VGA hardware back to mode 3 on application exit. After
DOOM or GUISHELL quits the card is still in VBE 0x118.

What is restored is the *console stream*, not the video mode: the mirror owner
bookkeeping in `kern_shell.cpp` reaps dead GUI pids and calls
`QemuFbMirrorSet(0)` plus `VgaInit()` when the last graphics owner dies
(`SH: mirror resumed for child` on COM1 for a non-graphical child launched by
DESKTOP). `FbEnsure()` re-reads `display_geometry()`, so subsequent text is
scaled-font output on the LFB (the old 64x42 mirror) while `0xB8000` keeps
receiving its 80x25 copy in parallel. The screen therefore returns to a
terminal-looking view, but a host screendump still reports 1024x768. A true
int-10 switch-back to a 720x400 mode-3 console is a known gap, not a feature to
rely on. The `vbe` shell command can be used to inspect modes (`vbe`) or force
one (`vbe 118`); switching to a text mode through that path is not supported.

## 5. Effect on existing programs

* **Text E32 apps** (CALC, FILEMGR, EDITOR, the bundled games, shell verbs such
  as `version` and `help`) are unchanged. They only ever see the console
  stream, which still goes to COM1 and to the mode-3 text buffer. They also get
  the scaled-font LFB mirror when the display is in graphics mode.
* **Graphical E32 apps** (GUISHELL, GUIMENU, GUIHELLO, BREAKOUT, SAVER,
  DESKTOP, DOOM) now *cause* the switch instead of assuming an LFB exists at
  boot. Their fb base/size queries (execs 249/250) and display lists keep
  working because `display_set_mode()` publishes the new geometry first.
* **Multi-app desktop**: `sGuiMirrorPids[]` tracks every live graphics owner, so
  the mirror resumes only after the last one exits — a windowed client does not
  steal the screen back from the compositor merely by being launched.
* **Mouse and GUI HAL** consumers re-clamp through `MouseGeometryReset()`; the
  old 1024x768-only assumptions remain valid because 0x118 is still the target.
* On a non-Bochs VGA card the fallback stays mode 13h (320x200x8 at `0xA0000`);
  VBE switching is then unavailable and the test's graphics assertion cannot
  pass. Always boot with `-vga std`.

## 6. QEMU flags and shell verbs

```
qemu-system-i386 -M q35 -m 128 -vga std -display none \
  -serial stdio -cdrom build/symbian-grub.iso -boot d -no-reboot
```

`-vga std` is mandatory: it is the Bochs VBE DISPI implementation the driver
programs. `-display none` is fine for serial tests; QEMU keeps a virtual
console that QMP `screendump` can still capture. Do not use a USB tablet for
mouse work (the guest wants the default PS/2 device), see
`docs/GRAPHICAL_DESKTOP.md` §12. Useful shell verbs:

* `version`, `help` — cheap text output that proves the text console works;
* `display` / `display test` — framebuffer info and the blue-screen banner;
* `vbe` — list modes; `vbe 118` — force the graphics mode by hand;
* program names (`doom`, `guishell`, ...) — trigger the lazy switch.

## 7. Testing

`tools/r1_textmode_test.py` is the acceptance test. It reuses the asset staging
and boot helpers from `tools/r1_doom_test.py` and the held-open QMP session from
`tools/r1_doom_audio_test.py`, then:

1. boots the GRUB ISO with `-vga std -serial stdio` and waits for the shell
   prompt (green `X:/> ` in the log);
2. takes a screendump and asserts a real 80x25 text screen: size 720x400 or
   640x400, at most 32 distinct colours (VGA's 16-colour palette), at least
   three banded glyph rows with frequent on/off transitions in the top 120
   lines, and explicitly *not* 1024x768;
3. feeds `version` and `help`, asserts their text on COM1 and diffs the two
   text screendumps to prove the visible text region changed;
4. launches `doom` (default) or `guishell` (`--app guishell`) and takes a third
   screendump that must be 1024x768; the serial VBE-switch marker is reported;
5. fails on any `Exc`, `PanicKernExec`, `FAULT:` or `MMU #PF` line.

```
python3 -m py_compile tools/r1_textmode_test.py
python3 tools/r1_textmode_test.py                 # DOOM path
python3 tools/r1_textmode_test.py --app guishell  # lighter graphical app
```

Missing DOOM.E32/IWAD (or the GUISHELL E32 and ISO) produces `RESULT: SKIP`
with exit 0 — never a false pass. Screendumps are best-effort: QMP handshake or
PNG write failures are printed as `[INFO] ... NOT VERIFIED` and do not crash the
test, but *when a PNG exists its resolution/palette assertions are fatal*. PNGs
land in `/tmp/opencode/textmode_{prompt,after_cmds,gfx}.png` for manual review.
Complementary checks: `tools/r1_doom_visual_test.py` (no console bleed during
gameplay), `tools/r1_qemu_display_test.py` (LFB and display HAL), and
`tools/r1_graphical_desktop_test.py` (GUI program markers).

## 8. Known gaps

* No int-10 switch back to mode 3 after a graphical app exits (§4).
* The LFB text mirror and GUI still share VRAM without arbitration; the mirror
  mute is a convention keyed on the launched program's name, not a capability.
* `display_set_mode()` only knows the VBE graphics modes 0x115/0x117/0x118;
  there is no shell command to request a text mode from VBE.
* QEMU screendump geometry is host-side evidence of the guest mode; a QEMU
  build without `-vga std` or a headless QMP failure leaves the visual checks
  unverified (reported honestly, not silently passed).

**2026-11-26 — correction (supersedes this file's live/pending winsrv-or-GUI premise):** winsrv/GUI desktop was RETIRED in commit `94ce9c9` (7 files deleted: `winsrv.c`, `guishost.h`, `wsclient.c`, `wsclient.h`, `qemu_windowsrv.cpp`, `qemu_windowsrv.h`, `qemu_gui_exec.cpp`); the winsrv slow execs 249/250/251 and the WINSRV launch path are gone, so every reference above to `variant/qemu_gui_exec.cpp`, exec 251 display lists, execs 249/250 geometry probes, the WINSRV launch path and `WSDL`/`DESKTOP` graphics owners is historical only. The console shell plus DOOM/Freedoom are the UI; genuine EWSRV is the target whenever a GUI returns, per `AGENTS.md`. the authoritative gate is the 15-row roster, 15/15 PASS (`OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, `/tmp/opencode/final27_gate.log`) at HEAD `ee8490e` with no new commits.

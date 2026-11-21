# End-User Application Catalog (R1 QEMU port)

This catalog lists the ~70 end-user E32 programs built for the port. Each is a
freestanding 32-bit PE2-style E32 image that runs as a ring-3 user process under
the genuine EKA2 kernel, loaded either by the shell's `RunImage()` or by the
boot-time service manager.

## How the programs are built, shipped and launched

- **Source:** `user/<name>.c` (a few services use `user/<name>.cpp`, plus the
  shared `user/runtime.c`, `user/malloc.c`, `user/ipc.c`). Each program exposes
  `int app_main(void)` and gets its command line via `command_line()`.
- **Build rules:** defined in `user/user.mk` (included by the top-level
  `Makefile`) and aggregated under the `userland` target. The common recipe
  (abbreviated **Std+915** below) is:
  `build/user/NAME.E32: build/user/name.elf tools/elf2e32.py tools/mke32.py`
  → `python3 tools/elf2e32.py $< $@ --euser-import 915` →
  `python3 tools/e32read.py $@ --quiet`. The ELF is linked with
  `ld -m elf_i386 --emit-relocs -T user/link.ld` from `start.o + runtime.o +
  <name>.o (+ malloc.o/ipc.o as needed)`. **+915** means the image carries a
  genuine PE2 import of `EUSER.DLL` ordinal 915 (`RDebug::Printf`) so its serial
  markers use the real EUSER import mechanism. Programs without that import are
  marked **Std (no import)**.
- **Markers:** the short prefix the host test scripts look for on the serial
  console (e.g. `CALC:`, `ST:`). Some programs emit a second prefix for a
  sub-feature (noted where relevant).
- **Shipping:** every `build/user/*.E32` is packed into `build/PROGRAMS.IMG`,
  which is attached to the GRUB ISO as a Multiboot module. At boot the module is
  snapshotted into high RAM as a read-only block device and mounted as **`X:`**
  (`X: mounted (programs from GRUB module)`), so all programs below are
  launchable from `X:` — e.g. `CALC`, `X:SNAKE`, `X:\PING.E32 5`. FAT is already
  mounted at boot; do **not** type `MOUNT` (see `SHELL_REFERENCE.md`).
- **QEMU tests:** host-side scripts in `tools/`, run as
  `python3 tools/r1_<name>_test.py`. They stage the E32 on a FAT32 disk, boot
  `build/kernfull.elf`, launch the program from the serial shell, and assert the
  markers and a clean exit (0 only on full pass). The test column gives the
  script name (without the `tools/` prefix).

## System utilities

| E32 | Description | Markers | Build | QEMU test |
|-----|-------------|---------|-------|-----------|
| **CALC** | Integer expression calculator (`+ - * / ()`); command line or self-test | `CALC:` | Std+915 | `r1_calc_test.py` |
| **SYSINFO** | Reports CPU/kernel/port identity and feature summary | `SYS:` | Std+915 | `r1_sysinfo_test.py` |
| **CLOCK** | Live clock/time display from the RTC exec | `CLK:` | Std+915 | `r1_clock_test.py` |
| **BENCH** | Dhrystone-style integer CPU throughput benchmark | `BENCH:` | Std+915 | `r1_bench_test.py` |
| **MEMTEST** | User-heap malloc/free stress and corruption check | `MEM:` | Std+915 | `r1_memtest_test.py` |
| **WC** | Word/line/character counter for a text file | `WC:` | Std+915 | `r1_wc_test.py` |
| **HEXDUMP** | Hex + ASCII dump of a file | `HD:` | Std+915 | `r1_hexdump_test.py` |
| **FGREP** | Search a file for a literal string (grep) | `FG:` | Std+915 | `r1_fgrep_test.py` |
| **FCOPY** | File copy / file utility over PortFile IPC | `FC:` | Std+915 | `r1_fcopy_test.py` |
| **FILEMGR** | Text-mode file manager (dir/cat/help/quit) | `FM:` | Std+915 | `r1_filemgr_test.py` |
| **TEXTEDIT** | Line-oriented text editor with file open/save | `TEXTEDIT:` | Std+915 | — (editor fixture) |
| **NOTEPAD** | Advanced multi-file notepad with search/replace | `NP:` | Std+915 | `r1_notepad_test.py` |
| **ADDRBOOK** | Address book (add/list/find contacts) | `AB:` | Std+915 | `r1_addrbook_test.py` |
| **PWVAULT** | Password manager / credential vault (`unlock 1234`) | `PV:` | Std+915 | `r1_pwvault_test.py` |
| **RECIPES** | Recipe book browser | `RB:` | Std+915 | `r1_recipes_test.py` |
| **PS** | Userland process census viewer (mirrors shell `PS`) | `PS:` | Std+915 | `r1_ps_test.py` |
| **TASKMGR** | Interactive task manager; list/kill processes by id | `TM:` | Std+915 | `r1_taskmgr_test.py` |
| **IFCONFIG** | Report e1000/SLIRP interface config (classic layout) | `NET:` | Std+915 | `r1_ifconfig_test.py` |
| **MONITOR** | Repeating system-monitor panel from userland | `MON:` | Std+915 | `r1_monitor_test.py` |
| **WORLDTIME** | World clock across time zones | `WT:` | Std+915 | `r1_worldtime_test.py` |

Additional utilities (built the same way, in `PROGRAMS.IMG` on `X:`):

| E32 | Description | Markers | QEMU test |
|-----|-------------|---------|-----------|
| **CAL** | Calendar utility (`CAL:` via ordinal 915) | `CAL:` | `r1_cal_test.py` |
| **STOPWATCH** | Stopwatch/timer | `SW:` (`HH:`) | `r1_stopwatch_test.py` |
| **PRIMES** | Prime-number generator/checker | `PR:` | `r1_primes_test.py` |
| **ASCIIART** | ASCII-art banner generator (`banner <word>`) | `AA:` | `r1_asciiart_test.py` |
| **FLIGHT** | Flight simulator / altitude tracker | `FL:` | `r1_flight_test.py` |
| **APPLAUNCH** | Application launcher / boot menu (lists and starts apps) | `APP:` | `r1_applaunch_test.py` |
| **SOUNDPLR** | AC97 sound player (melody via audio exec 240) | `SND:` | `r1_soundplr_test.py` |

## Network applications

All real-network apps use the kernel↔userland e1000 frame bridge
(`user/netbridge.h`, slow execs 247/248) and SLIRP gateway `10.0.2.2`; the local
service variants use the `!SocketServer`/IPC bridge (`user/ipc.h`).

| E32 | Description | Markers | Build | QEMU test |
|-----|-------------|---------|-------|-----------|
| **PING** | Real ICMP echo request/reply with checksum verification | `PING:` | Std+915 (`ping.o`) | `r1_ping_test.py` |
| **DHCPC** | DHCP DORA lease over the frame bridge | `DHCP:` | Std+915 (`dhcpc.o`) | `r1_dhcpc_test.py` |
| **DNSRES** | RFC1035 UDP/53 A-record resolver + IPC service | `DNSRES:` | Std+915 (`dnsresolv.o`) | `r1_dnsres_test.py` |
| **DNSNET** | Real DNS A query (ARP + UDP/53) over the frame bridge | `DNSNET:` | Std+915 (`dnsnet.o`) | `r1_dnsnet_test.py` |
| **HTTPGET** | Minimal TCP SYN handshake + HTTP/1.0 GET client | `HTTP:` | Std+915 (`httpget.o`) | `r1_httpget_test.py` |
| **WBROWSER** | Text-mode web browser (DNS + HTTP + HTML tag strip) | `WEB:` | Std+915 (`wbrowser.o`) | `r1_wbrowser_test.py` |
| **WGET** | HTTP file downloader | `WGET:` | Std+915 | `r1_wget_test.py` |
| **TELNET** | Interactive TCP telnet-style client | `TNET:` | Std+915 | `r1_telnet_test.py` |
| **NTPC** | Real NTP v4 time-sync client (T1-T4 offset in ms) | `NTP:` | Std+915 (`ntpc.o`) | `r1_ntpc_test.py` |
| **NETBRIDGE** | ARP request/reply proof through the frame bridge | `NB:` | Std+915 (`netbridge_test.o`) | `r1_netbridge_test.py` |

Other network building blocks shipped on `X:`: **NETSTACK** (`NETSTACK:`,
compact IPv4/IPv6 stack exposed as an IPC "NetStack" server) and the
**ca_test** web-browser fixture (`WEB:`, built by `tools/mke32_webbrowser.py`,
exercised by `r1_web_browser_test.py` / `r1_cp4_ca_test.py`).

## GUI programs

| E32 | Description | Markers | Build | QEMU test |
|-----|-------------|---------|-------|-----------|
| **GUISHELL** | GUI desktop shell; paints a display list on the VBE framebuffer | `GUI:` | Std+915 (`guishell.o`) | `r1_guishell_test.py` |
| **SAVER** | Animated screensaver drawn into the VBE framebuffer | `SV:` | Std+915 | `r1_saver_test.py` |
| **SHOT** | Screenshot tool; grabs the `gui_shared` framebuffer | `SHOT:` | Std+915 | `r1_shot_test.py` |

All three discover the LFB via slow execs 249 (physical base) and 250 (size).
Related host graphics checks: `r1_qemu_display_test.py`, `r1_qemu_mouse_test.py`.

## Games

| E32 | Description | Markers | QEMU test |
|-----|-------------|---------|-----------|
| **SNAKE** | Classic Snake | `SNAKE:` | `r1_snake_test.py` |
| **SNAKE2** | Faster, scored Snake 2.0 | `S2:` | `r1_snake2_test.py` |
| **MINES** | Minesweeper | `MS:` | `r1_mines_test.py` |
| **TICTACTOE** | Tic-tac-toe vs computer | `TT:` | `r1_tictactoe_test.py` |
| **HANGMAN** | Word-guessing Hangman | `HG:` | `r1_hangman_test.py` |
| **CHESS** | Chess opening-moves viewer | `CH:` | `r1_chess_test.py` |
| **SUDOKU** | 9×9 sudoku game/validator | `SU:` | `r1_sudoku_test.py` |
| **HANOI** | Tower of Hanoi puzzle | `HN:` | `r1_hanoi_test.py` |
| **MAZE** | Deterministic 9×9 maze generator/solver | `MZ:` | `r1_maze_test.py` |
| **BATTLE** | Battleship | `BT:` | `r1_battle_test.py` |
| **CONNECT4** | Connect-Four | `C4:` | `r1_connect4_test.py` |
| **CODEBREAK** | Mastermind-style code breaker | `CB:` | `r1_codebreak_test.py` |
| **BLACKJACK** | Blackjack card game | `BJ:` | `r1_blackjack_test.py` |
| **QUIZ** | Trivia quiz | `QZ:` | `r1_quiz_test.py` |
| **PACMAN** | Maze-muncher game | `PC:` | `r1_pacman_test.py` |
| **TETRIS** | Falling-block game | `TT2:` | `r1_tetris_test.py` |
| **WORDSEARCH** | Word-search puzzle | `WS:` | `r1_wordsearch_test.py` |
| **DICE** | Dice roller / random tool | `DC:` | `r1_dice_test.py` |
| **FORTUNE** | Fortune-cookie messages | `FORT:` | `r1_fortune_test.py` |
| **MORSE** | Text ↔ International Morse (with optional audio tones) | `MRS:` | `r1_morse_test.py` |
| **MUSIC** | Music scales / equal-temperament table (optionally played) | `MU:` | `r1_music_test.py` |

All games use the **Std+915** build rule and carry a 915 `RDebug::Printf`
import for their markers.

## Educational / reference

| E32 | Description | Markers | QEMU test |
|-----|-------------|---------|-----------|
| **SOLAR** | Solar-system fact viewer | `SO:` | `r1_solar_test.py` |
| **ASTRO** | Astronomy / deep-sky fact viewer | `AS:` | `r1_astro_test.py` |
| **PERIODIC** | Periodic table viewer | `PT:` | `r1_periodic_test.py` |
| **OCEAN** | Ocean / geography fact viewer | `OC:` | `r1_ocean_test.py` |
| **MOVIES** | Movie database browser | `MV:` (`2001:`) | `r1_movies_test.py` |
| **ZODIAC** | Zodiac sign reference | `ZD:` | `r1_zodiac_test.py` |
| **UNITS** | Unit conversion tool | `UN:` | `r1_units_test.py` |
| **CURRENCY** | Fixed-point currency converter | `CUR:` | `r1_currency_test.py` |

All use **Std+915**. Additional reference tools: **PRIMES**, **ASCIIART**,
**FLIGHT** (listed under utilities above).

## Infrastructure & test fixtures

These are not "apps" in the recreational sense but are built as E32 images and
bundled on `X:`; they provide the runtime services and end-to-end proofs that
the other programs depend on.

| E32 | Description | Markers | Build | QEMU test |
|-----|-------------|---------|-------|-----------|
| **SVCMGR** | Service manager; starts/supervises F32 (portfilesrv) at boot | `SVCMGR:` | Std (no import), launched by `SvcMgrBootLaunch()` | exercised via `r1_selftest.py` |
| **PORTFILESRV** | F32 file service behind PortFile IPC (`!FileServer`) | `PORTFILESRV:` | Std (no import) | exercised via `r1_selftest.py` |
| **WAITFOR** | User-mode blocking wait via ProcessLogon executive (123) | `WAITFOR:` | Std (no import) | used by process tests |
| **KILLPROC** | Peer killer: kills another user process by container id | `KILLPROC:` | Std (no import) | `r1_cp4_ca_test.py`, `r1_cp4_ca_stress_test.py` |
| **CPSTRESS** | Long-lived IPC client for async request re-signal stress | `CPSTRESS:` | Std (no import) | `r1_cp4_ca_stress_test.py` |
| **SELFTEST** | One comprehensive OS self-test → single PASS/FAIL verdict | `ST:` | Std+915 (`selftest.elf`, `--euser-import 915`) | `r1_selftest.py` |

`SELFTEST` exercises EUSER binding, the user heap, `RChunk`, PortFile save/read,
the IPC server/client round trip and e1000 netbridge discovery in one program;
run it from `X:` with `SELFTEST` (host: `python3 tools/r1_selftest.py`).

Lower-level fixtures also built as E32 (used by the driver/runtime regression
tests rather than by end users): `ECHO`, `TYPE`, `CP`, `FILECHK`, `CWD`, `CRASH`,
`MALLOC`, `INTERACT`, `SLEEP`, `CHUNK`, `CHKOPEN`, `IPCSRV`, `IPCCLI`, plus the
host-only `r1_e32_validate_test.py` (`r1_e32_validate*` docs in `docs/`).

## Host test inventory

Hardware/driver QEMU checks that accompany the app catalog:
`r1_qemu_smoke.py`, `r1_qemu_nic_test.py`, `r1_qemu_dns_test.py`,
`r1_qemu_audio_test.py`, `r1_qemu_display_test.py`, `r1_qemu_mouse_test.py`,
`r1_usb_ohci_test.py`, `r1_usb_ehci_test.py`, `r1_virtio_blk_test.py`,
`r1_virtio_net_test.py`, `r1_virtio_rng_test.py`.

All paths above are relative to `qemu-port/`; build artifacts live in
`build/user/` and test logs/images under `/tmp/opencode/`.

## New GUI toolkit / BASIC programs (append-only)

These are being built concurrently by other agents. An entry here lists the
intended image, marker prefix and harness; a program is only "shipped" once its
`build/user/<NAME>.E32` exists and is packed into `PROGRAMS.IMG`.

| E32 | Description | Markers | Build | QEMU test |
|-----|-------------|---------|-------|-----------|
| **BREAKOUT** | Real-time graphical Breakout on the VBE LFB (exec 251 display lists + `con_tryread()` input) | `BO:` | Std+915 | `r1_gui_regression.py` |
| **GUIMENU** | Graphical launcher / menu painted through the display-list protocol | `GM:` | Std+915 | `r1_gui_regression.py` |
| **GUIHELLO** | Minimal GUI window + key demo (worked example in `GUI_PROGRAMMING.md`) | `HG:` | Std+915 | `r1_gui_regression.py` |
| **BASIC** | Freestanding BASIC line interpreter (integer/fixed-point only, no libc) | `BASIC:` | Std+915 | `r1_gui_regression.py` |

`tools/r1_gui_regression.py` is a single test that probes `build/user/*.E32` for
all four programs, skips any that are not built yet, boots with `-vga std`,
stages each program plus `EUSER.DLL`, feeds its name at the `EKA2>` prompt, and
asserts a known marker and no `Exc`/`PanicKernExec`. See
[`GUI_PROGRAMMING.md`](GUI_PROGRAMMING.md) for the display-list API and
[`COMPLEX_PROGRAMS.md`](COMPLEX_PROGRAMS.md) for the porting constraints.

## Graphical desktop entry (append-only)

| E32 | Description | Markers | Build | QEMU test |
|-----|-------------|---------|-------|-----------|
| **DESKTOP** | The consolidated full-screen graphical desktop: title bar, windows and taskbar painted on the VBE LFB through exec 251 display lists; W/S/A/D navigation, Enter to activate, Esc to return to the shell | `DT:` | Std+915 | `r1_graphical_desktop_test.py` |

`DESKTOP.E32` is still being built by another agent; until it exists in
`build/user/`, GUISHELL (`GUI:`) is the shipped desktop-equivalent program. The
consolidated test `tools/r1_graphical_desktop_test.py` probes for DESKTOP and
the existing graphical apps (GUIMENU `GM:`, BREAKOUT `BRK:`, GUIHELLO `GH:`,
GUISHELL `GUI:`, SAVER `SV:`), stages the present ones on the GRUB ISO, boots
with `-vga std`, feeds each name plus a few keys and Esc, and asserts the right
marker with no `Exc`/`PanicKernExec` (plus best-effort screendumps). Missing
programs are skipped, not failed. See [`GRAPHICAL_DESKTOP.md`](GRAPHICAL_DESKTOP.md)
for the full desktop architecture and usage guide.

## Windowed GUI apps (append-only)

These are the first **windowed** E32 applications: each paints its own decorated
window, buttons and mouse hit-testing on the VBE LFB through exec 251 display
lists (exec 268 launches them from the desktop). Both are staged in
`/PROGRAMS.IMG` and appear as clickable desktop icons (the icon count went from
8 to 10; `DT: icons=10`). The porting pattern is written up in
[`GUI_APP_DEVELOPMENT.md`](GUI_APP_DEVELOPMENT.md).

| E32 | Description | Markers | Build | QEMU test |
|-----|-------------|---------|-------|-----------|
| **DTCALC** | Windowed calculator: a real window with a clickable keypad (`DCALC: buttons=` / `DCALC: click ...`), keyboard entry, expression evaluation and a live result display. Mouse clicks are hit-tested against the button rectangles; Enter/= evaluates, Esc exits. | `DCALC:` | Std+915 | `r1_dtcalc_test.py` |
| **DTEDIT** | Windowed text editor: a text canvas plus a toolbar (`DTE: click TEXT/CLEAR/SAVE/CLOSE/QUIT`), click-to-focus and typing with newline/^S handling. **Saving (`SAVE`, ^S) needs the PortFile server running** (`file_save` IPC, exactly as in the older `TEXTEDIT`); without it the edit session still works but the save reports `DTE: save failed`. | `DTE:` | Std+915 | `r1_dtedit_test.py` |

Both are graphical foreground programs: when launched via exec 268 from
`DESKTOP` they take over the display and the desktop hands input over to them
(`DT: handing off`, `DT: DONE`). Run them directly from the `X:/>` shell with
`DTCALC` / `DTEDIT`, or click the new `DTCALC` / `DTEDIT` icons on the desktop.

## Window system (append-only)

The window server is a new mode alongside the one-app-at-a-time desktop: a
long-lived server owns the framebuffer and composites private client window
buffers, so several clients can stay visible at once. Both images are still
being built by other agents; an entry here lists the intended image, marker
prefix and harness, and is only "shipped" once its `build/user/<NAME>.E32`
exists and is packed into `PROGRAMS.IMG`. See
[`WINDOW_SERVER.md`](WINDOW_SERVER.md) for the full architecture.

| E32 | Description | Markers | Build | QEMU test |
|-----|-------------|---------|-------|-----------|
| **WINSRV** | Window server: claims the shared window control block, owns the VBE LFB, paints the desktop, routes polled input events to the focused window and composites registered client buffers into one frame per exec 251 submit. Start it first with `winsrv`; Esc/clean exit returns to the shell. | `WSV:` | Std+915 | `r1_window_system_test.py` |
| **WSDL** | Windowed calculator client: registers a fixed-size window with `WINSRV`, draws a keypad/result field into its private buffer, handles digit/operator keys and clicks through the client event API, and presents dirty frames for the server to composite. Start with `wsdl` while `winsrv` is running. | `WDL:` | Std+915 | `r1_window_system_test.py` |

`tools/r1_window_system_test.py` probes for both E32s, skips gracefully while
either is missing, stages the present ones with `EUSER.DLL`, boots with
`-vga std`, feeds `winsrv` then `wsdl` plus a few keys, asserts the `WSV:` and
`WDL:` markers with no `Exc`/`PanicKernExec`/`FAULT`, and takes a best-effort
screendump at `/tmp/opencode/window_system.png`. Unlike the old DESKTOP handoff
(`DT: handing off` -> app takes over -> `desktop` again), the server stays
resident while clients come and go.

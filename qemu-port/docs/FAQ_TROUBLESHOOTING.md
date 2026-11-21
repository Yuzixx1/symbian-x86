# FAQ & Troubleshooting — EKA2 x86 QEMU port

Practical answers to the questions that actually come up when building and
running the `qemu-port/` tree. This is a *current-behaviour* document: where a
symptom was fixed in a past revision (R4/R5/R7), the fix is described so you can
tell a stale guide or an old binary from a real problem. The canonical usage
recipe is [`r1_boot_and_selftest.md`](r1_boot_and_selftest.md); command details
are in [`SHELL_REFERENCE.md`](SHELL_REFERENCE.md).

All paths and commands below are relative to `qemu-port/` unless stated
otherwise. `kernelhwsrv/` is READ-ONLY and must never be edited — see the last
question.

## Display & console

### Q: Why is the console serial-only? I only see output on COM1.

**A: It isn't any more.** The shell is mirrored to the VBE linear framebuffer
by `VgaTeletype()` in `variant/kern_shell.cpp`: every `KPrintf` character goes
both to COM1 and into the LFB through `display_putc()` in the scaled 5x7 font,
so the `EKA2>` prompt and all shell output are visible in the QEMU window. The
classic 80x25 text buffer at `0xB8000` is still updated too, but under VBE it is
no longer what the screen scans out — the LFB mirror is.

Use `-vga std` on the QEMU command line so the Bochs VBE DISPI registers are
available; the driver sets 1024x768x32 (VBE mode `0x118`) and reads back the
LFB base (normally `0xFD000000` on q35). The on-screen mirror has been
confirmed by screenshot in a real QEMU run. If you boot with `-display none`
you explicitly disable that window — see the blank-screen question below.

### Q: Why is the text so small / thin / sparse on screen?

**A: Fixed in R4.** The console font is blown up by
`QEMU_DISPLAY_FONT_SCALE = 2`: glyph cells are 16x16 (8x8 stroke cell doubled)
and the row advance is **18 px** (16 + a 2 px gap), giving a 64x42 grid at
1024x768. Earlier builds used an 8 px glyph with a 16 px row step, which looked
thin and left large gaps. If text still looks small on your display, check that
the driver actually entered **VBE mode 0x118** (1024x768x32): the boot banner
prints `DISPLAY: VBE mode 0x118 set ...` and `DISPLAY: LFB base=0x...`. A
fallback to legacy mode 13h (320x200x8) means `-vga std` is missing or the VBE
DISPI probe failed.

### Q: I get a blank/black QEMU window — but `-serial stdio` works. Why?

**A: You are almost certainly using `-display none` (or omitted `-vga std`).**
`-display none` disables the graphical window entirely; `-vga std` is what
exposes Bochs VBE so the framebuffer console can appear. Use the primary
command from [`r1_boot_and_selftest.md`](r1_boot_and_selftest.md):

```
qemu-system-i386 -M q35 -m 128 -vga std -serial stdio -no-reboot \
  -cdrom build/symbian-grub.iso -boot d \
  -netdev user,id=n0 -device e1000,netdev=n0
```

If the window appears but stays black, confirm the kernel reached the shell on
COM1 first; no `EKA2>` on serial means a boot problem, not a display problem.

### Q: The QEMU window keyboard does nothing — typing only works in `-serial stdio`. Why?

**A: Fixed in R7.** The PS/2 IRQ1 line needs re-arming after boot:
`Asic::Init1` re-masks the whole 8259 after boot constructors run, and a key
pressed early can strand a scancode in the 8042 output buffer, holding IRQ1
high so the next low→high edge is never seen. `ShellMain()` now calls
`QemuKbdArm()` before the input loop (idempotent): it drains stale scancodes,
re-enables the 8042 keyboard, rebinds/clears IRQ1 and unmasks it via the live
PIC IMR, so the window and COM1 are both live from the first `EKA2>` prompt.
Check from the shell with `kbd` — it prints PS/2 IRQ1 ISR/scancode/key counts
plus the PIC IMR and 8042 status; `kbd arm` re-runs the re-arm sequence if a
key seems stuck. If `kbd` counts stay at zero while COM1 works, make sure you
are typing into the QEMU window (not the serial terminal) and try `kbd arm`.

### Q: Backspace moves the cursor but doesn't erase the character. Why?

**A: Fixed in R7.** A lone `0x08` only steps the cursor back on a serial
terminal — it leaves the glyph on screen. `EmitBackspace()` in
`variant/kern_shell.cpp` now emits `\b` + space + `\b`, blanking the cell on
COM1 *and* the VGA/LFB mirrors (the same three-byte sequence is used in
`console/shell.cpp` and `variant/genuine_console.cpp`). Both `0x08` (BS) and
`0x7f` (DEL) are handled, and ESC clears the whole line the same way. A guide
that shows a single `\b` predates R7.

## Booting, storage & programs

### Q: I booted the plain GRUB ISO and nothing is mounted / `DIR` shows no programs. Why?

**A: Fixed in R5.** GRUB now loads `PROGRAMS.IMG` as a Multiboot module and the
kernel snapshots it into high RAM as a read-only block device (**dev 8**), then
auto-mounts it as **`X:`** with `FatMountAlias(8,0,'X')`. On a diskless ISO boot
`X:` becomes the active drive, so `DIR` and program launches work immediately.
At the prompt try:

```
VOL                         REM list mounted volumes (C: and X:)
X:                          REM switch to the programs drive
DIR                         REM list PROGRAMS.IMG contents
CALC                        REM or X:CALC, HELLO, SNAKE
```

Make sure the ISO was built with `make isogrub` and that `build/user/*.E32`
exist; if `mtools` is absent, `PROGRAMS.IMG` is skipped and the `module
/programs.img` line is omitted from `grub.cfg`, leaving `X:` empty. Also note
the ~10 s GRUB countdown before autoboot.

### Q: Why does typing `MOUNT` hang the shell?

**A: Don't type it — FAT is already mounted at boot.** The boot path calls the
FAT backend mount (dev 4, part 0) and then aliases `C:`/`X:` before the shell
starts. The no-argument `MOUNT` is a cold-boot discovery path: it calls
`AtaInit()` and probes all 8 devices x 4 partitions with raw MBR/BPB reads,
re-running mount bookkeeping against an already-full volume table, which can
stall the emulated ATA/AHCI path on the M-03 kernel. Use `VOL` and `DRIVE` to
inspect what is mounted. Only the explicit `MOUNT <device> <partition>` form
(device 0-7, partition 0-3) is useful for a newly attached disk, and even that
is normally handled at boot.

### Q: Can I use a real ATAPI CD or read programs from ISO9660 directly?

**A: No.** There is no ISO9660 filesystem driver in the kernel. ATA/ATAPI
devices are detected but ATAPI is deliberately skipped; the AHCI path speaks
LBA28/LBA48 to SATA disks only. End-user programs are not read from the CD's
ISO9660 filesystem — they arrive as a GRUB Multiboot module (`PROGRAMS.IMG`)
that the kernel mounts as a RAM-backed FAT volume on `X:`. Firmware-level ISO
boot works (El Torito), but guest-side ISO9660 reads do not exist.

### Q: Where are the bundled applications, and how do I launch one?

**A: On `X:` (the `PROGRAMS.IMG` RAM disk).** Type `X:` then `DIR`, or launch by
bare name — the shell searches the active drive and then every mounted drive,
trying `.EXE` first and then `.E32`. Examples: `CALC`, `CALC.EXE`, `PING 5`,
`SNAKE`, `X:SNAKE`, `X:\GAMES\SNAKE.E32`. `make userland-exe` writes byte-identical
`<NAME>.EXE` aliases into `build/user/` for any custom disk; the shipped ISO
keeps all `.E32` names and adds `CALC.EXE` as a Symbian-style demo. The full
inventory (~70 apps: utilities, network tools,
GUI demos, games, test fixtures) is in
[`USER_APPS_CATALOG.md`](USER_APPS_CATALOG.md).

### Q: My program prints `(not found)` or fails to load with an EUSER error. Why?

**A: The EUSER DLL import is usually missing.** Most E32 images carry a genuine
PE2 import of `EUSER.DLL` ordinal 915 (`RDebug::Printf`) for their serial
markers. `mkgrubiso.py` only stages `EUSER.DLL` into `PROGRAMS.IMG` if a copy
exists at `qemu-port/EUSER.DLL` (it is *not* produced by `make`). A known-good
copy lives at
`/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll`;
copy it to `qemu-port/EUSER.DLL` before `make isogrub`. Without it, ordinal 915
cannot resolve and the app fails at load.

## Networking

### Q: Does `DNSRES` use a loopback responder or a hardcoded `example.com` address?

**A: No — not since R7.** `DNSRES.E32` is built from `user/dnsnet.c`: the
`build/user/DNSRES.E32` rule in `user/user.mk` simply links `dnsnet.elf`, so it
is the *same* genuine resolver as `DNSNET`. It ARPs for the SLIRP gateway
`10.0.2.2`, sends a real RFC1035 UDP/53 A query to SLIRP's DNS proxy `10.0.2.3`
and parses the real reply from the e1000 netbridge RX ring. `example.com` now
resolves to a real public address (e.g. `172.66.147.243`) returned by SLIRP —
not the old hardcoded `93.184.216.34` and not an in-process loopback fake. The
former `DnsResolverServer`/IPC loopback design survives only in
`user/dnsresolv.c`, which is historical and **not built**. Any guide or test
expecting `DNSRES` + a loopback answer / `93.184.216.34` predates R7.

## Processes & the self-test

### Q: Why does `KILL 1` say `kill: error`?

**A: Because pid 1 is `ekern.exe` — the kernel/shell itself — and it is
protected.** `KILL <id>` calls `E32Kill()` on *user* processes only; find a real
user PID with `PS` and kill that, e.g. `KILL 7`. The same guard was added to the
user-space path: `KILLPROC 1` is now blocked by object-identity comparison
against the kernel process. Previously the self-only numeric check let a ring-3
caller pass the kernel's pid and fault the kernel while calling `Die()` — that
was a genuine kernel-fault bug, now fixed in `variant/e32load.cpp` (`E32Kill`).

### Q: `PS` still shows a process I just killed. Why?

**A: Fixed in R7.** A killed/exited process stays in the kernel container as a
zombie until `E32Reap` drops its code-segment creator reference, but
`DProcess::Die()` sets `iExitType` synchronously. The `PS` path now filters out
every entry whose `iExitType != EExitPending`, so killed processes disappear
from the live census immediately: `kill` → reap → the process exits for good.
Post-mortem status remains available via `EXITS`, `WAIT` and `REAP` — the
filtering is purely cosmetic. A transcript that lists a dead PID under `PS`
was captured before R7.

### Q: `SELFTEST` FAILs with `ipc` or `netbridge` missing. Is the OS broken?

**A: Usually no — this is historical, and the current image fixes it.** A pass
looks like the `ST: ... failures=0 / ST: RESULT PASS` transcript in
[`r1_boot_and_selftest.md`](r1_boot_and_selftest.md), covering euser, malloc,
chunk, file, ipc and netbridge. Notes:

- **netbridge** is the kernel↔userland e1000 frame bridge (execs 247/248). It
  needs an e1000 on the QEMU command line — include
  `-netdev user,id=n0 -device e1000,netdev=n0` (SLIRP gateway `10.0.2.2`, guest
  `10.0.2.15`). Without the NIC there is nothing to bridge and the check fails.
- **ipc** used to be exercised through a bare `ipc_connect` path; that direct
  path was removed/blocked. The self-test now uses the supported server/session
  round trip, so an old SELFTEST.E32 against a new kernel (or vice versa) can
  still report a missing ipc step. Rebuild both sides with `make userland` and
  `make isogrub`.
- If a genuinely current image still fails, capture the full `ST:` log and run
  `python3 tools/r1_selftest.py` for the host-driven version.

### Q: `selfcheck` failed once but a re-run passes. What happened?

**A: Shared-image contention, not a code regression.** The host test scripts
use fixed disk/log names under `/tmp/opencode/` (e.g. `/tmp/opencode/r1_*.img`).
If a previous run was interrupted, an orphaned `qemu-system-i386` can still be
holding a writable image, so the next QEMU gets a corrupted or locked FAT image
and reports a bogus failure. Clean up and re-run:

```
pgrep -af 'qemu-system.*r1_'        REM find stale QEMU processes
pkill -f 'qemu-system.*r1_'          REM kill only your stale test QEMUs
python3 tools/r1_os_selfcheck.py
```

Check that no other agent is mid-test on the same image before killing anything
— use a unique `/tmp/opencode/<agent>*.img` name when running in parallel
(see [`parallel_work.md`](parallel_work.md) and
[`qa_inventory_R0.md`](qa_inventory_R0.md)).

## Building & prerequisites

### Q: `ModuleNotFoundError: No module named 'pycdlib'` / missing `mkfs.fat`, `mcopy`, GRUB tools.

**A: These are host prerequisites, not in-tree code.** Install them:

```
python3 -m pip install pycdlib          # ISO mastering (tools/mkiso.py, mkgrubiso.py)
sudo dnf install mtools grub2-tools     # mkfs.fat/mcopy/mdir + grub2-mkimage
```

`make check-env` verifies gcc/as/ld/make/QEMU and freestanding `-m32` builds,
but explicitly does **not** check pycdlib/mtools/GRUB — verify those with
`python3 -c 'import pycdlib'` and `command -v mkfs.fat mcopy mdir
grub2-mkimage`. On Fedora the GRUB modules must be present under
`/usr/lib/grub/i386-pc`; otherwise `make isogrub` cannot build and `X:` will be
empty. See [`BUILD_GUIDE.md`](BUILD_GUIDE.md) for the full toolchain table.

### Q: I got `ENTRY MOVED (... != 00100030)`, `BOOTSECT TOO BIG`, or `KERNEL TOO BIG`.

**A: These are bootsect guard failures.** The hand-written bootsect hardcodes
the jump address `0x100030`; if `_mb_entry` moves in `boot/link.ld`, the
bootsect and guard must change together. A quick workaround is to boot via
GRUB/multiboot (which reads the ELF header instead). `BOOTSect TOO BIG` means
`bootsect.bin` no longer fits in 4096 bytes; `KERNEL TOO BIG (>4M)` means the
streaming loader budget was exceeded — build with the standard flags, never
`-O0` (frame-pointer prologues corrupt the bare-entry stack math). See
[`BUILD_GUIDE.md`](BUILD_GUIDE.md) §6.

### Q: Can I run `make clean` to fix a weird stale build?

**A: Not while other agents are building or testing.** `make clean` is a plain
`rm -rf build`: it removes both kernels, all E32s, generated headers, the staged
`PROGRAMS.IMG`, `build/grubwork` and every ISO. A concurrent agent's smoke test
will then fail on missing `kernfull.elf`/`PROGRAMS.IMG`. Prefer an incremental
`make`, or a targeted `rm build/kernfull.elf`, and do full clean builds in a
separate worktree.

### Q: The compiler prints `offsetof within non-standard-layout type`, RWX LOAD segment, and `-fno-rtti on C` warnings. Is the build broken?

**A: No, all three are benign.** The genuine EKA2 headers use the compiler's
own ABI layout (`_FOFF` wraps `__builtin_offsetof`), RWX data pages are
intentional for a freestanding ring-0 kernel with a hand-written identity page
table, and GCC silently ignores `-fno-rtti` on C inputs. Only `ENV-FAIL` /
missing pycdlib, mkfs.fat or grub2-mkimage are real failures.

## Project rules & history

### Q: Why is `kernelhwsrv/` frozen? Can I patch a genuine source file to fix this?

**A: No — it is genuine EKA2 source, read-only by project rule.** Every port
change lives in `qemu-port/` (variant glue, Makefile, tools, docs, userland),
and the build only *reads* `.cia`/`.cpp`/headers from `../kernelhwsrv/`. When a
genuine routine needs different behaviour, the port re-implements or patches
it from the Variant side — for example the slow-exec table entries 240, 241,
247-251 and 268-270 are repurposed at runtime precisely because the genuine
source cannot be edited. Never commit or push `kernelhwsrv/`.

### Q: Why did an older guide say "console is serial-only" / "no X:" / "text is tiny"?

**A: Those symptoms were real in earlier revisions and were fixed.** R4 added
the 2x scaled font/18 px rows and the LFB console mirror; R5 added the GRUB
`PROGRAMS.IMG` module → dev 8 RAM disk → `X:` auto-mount. Likewise the IPC and
netbridge self-test gaps were closed, and `KILLPROC 1` was hardened. R7 added
the real `DNSRES` resolver (no loopback fake), a re-armed PS/2 window keyboard,
a backspace that actually erases, and a `PS` list with dead processes filtered.
If you are reading a dated design note (many `r1_*_v0` docs were written
*before* implementation), trust the running code, this FAQ and
[`r1_boot_and_selftest.md`](r1_boot_and_selftest.md) instead.

## Quick pointers

| Symptom | First thing to check |
|---|---|
| No programs on `X:` | `make isogrub`, `build/user/*.E32`, `EUSER.DLL`, mtools |
| `MOUNT` hangs | Don't run it; use `VOL` / `DRIVE` |
| Blank screen | `-vga std`, no `-display none` |
| Window keyboard dead | `kbd` for IRQ1 stats; `kbd arm` re-arms PS/2 |
| Backspace doesn't erase | Fixed in R7 (`\b`, space, `\b`); BS and DEL |
| `DNSRES` loopback / `93.184.216.34` | Fixed in R7: real DNS via dnsnet.c → 10.0.2.3 |
| `PS` shows killed process | Fixed in R7: dead filtered; `EXITS`/`REAP` for status |
| `KILL 1` error | Expected; use `PS` then `KILL <pid>` |
| `netbridge` selftest fail | e1000 in QEMU args |
| Selfcheck flaky | Stale `/tmp/opencode/r1_*.img` QEMU; kill and re-run |
| Import/load failure | `EUSER.DLL` ordinal 915 on `X:` |
| Build env failure | pycdlib, mtools, grub2-tools |

Related reading: [`QUICK_REFERENCE.md`](QUICK_REFERENCE.md),
[`SHELL_REFERENCE.md`](SHELL_REFERENCE.md),
[`USER_APPS_CATALOG.md`](USER_APPS_CATALOG.md),
[`BUILD_GUIDE.md`](BUILD_GUIDE.md),
[`GRUB_BOOT_INTERNALS.md`](GRUB_BOOT_INTERNALS.md),
[`NETWORK_TESTING.md`](NETWORK_TESTING.md),
[`GUI_AUDIO.md`](GUI_AUDIO.md),
[`DOCS_INDEX.md`](DOCS_INDEX.md).

### Q: How do I get a *real* graphical desktop? Why is the console text-based?

**A: VBE graphics mode is already active** — the port boots into VBE 2.0 mode
0x118 (1024x768x32) on QEMU's standard VGA, and the `EKA2>`/`X:/>` console you
see is *text drawn onto that same linear framebuffer* by the kernel teletype
(`display_putc()`), not a separate text mode. To get a real graphical desktop,
type `desktop` (once `DESKTOP.E32` is built), `guimenu` or `breakout` at the
prompt; `guishell`, `guihello` and `saver` are also full-screen graphical
programs. Alternatively, boot the desktop GRUB entry if your ISO offers one
(current `mkgrubiso.py` ships only the full-kernel and nanokernel entries, so
launching from the shell is the supported path). Use `-vga std` — without it
the Bochs VBE path is unavailable and you fall back to 320x200x8. Quit with Esc
where supported. See [`GRAPHICAL_DESKTOP.md`](GRAPHICAL_DESKTOP.md) for details,
and note the honest limitation: this is a cooperative single-app-at-a-time
desktop, not a preemptive multi-window compositor.

### Q: Mouse doesn't move in the desktop. Is the guest driver broken?

**A: Usually it is a host/display setup issue, not the guest.** The guest uses
the **PS/2 mouse on i8042 IRQ12**, not a USB tablet. The default `-M q35` already
exposes a PS/2 mouse — do **not** add `-usbdevice tablet` or `-device usb-mouse`,
because the guest PS/2 driver will not receive their absolute HID events. You
also need a display that delivers pointer events: `-display gtk` with a focused
window, or VNC. `-display none` delivers no pointer events at all.

Note that **XTEST / host-window pointer detection can fail in headless
environments even when the guest code works** — a synthetic host pointer test
that reports "no movement" says nothing about the guest PS/2 path. Verify the
guest directly: type `mouse info` at the shell before launching the desktop to
check packet/ISR counts, and look for `DT: mouse x=... y=...` markers on motion.
For an end-to-end check, inject relative PS/2 motion with QEMU HMP `mouse_move`
and compare screendumps (pixel diff); `mouse test` injects a synthetic AUX
packet from inside the guest. Clicks are not wired into applications, so only
cursor motion is expected. See
[`GRAPHICAL_DESKTOP.md`](GRAPHICAL_DESKTOP.md) §12 for details.

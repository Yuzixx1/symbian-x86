# EKA2 Shell Command Reference (R1 QEMU full-kernel image)

The `EKA2>` shell is a DOS-style, line-oriented interactive console that runs
as a genuine kernel `DThread` (`ShellSpawnFull()` in `variant/kern_shell.cpp`,
spawned from `QemuVariantAsic::Init3()`). It is mirrored to COM1 (serial) and to
the VGA/LFB text layer, so the same prompt is visible in the QEMU serial
monitor and on the framebuffer. Input arrives from the PS/2 keyboard ring, the
COM1 receive ring, USB HID, and xHCI keyboards.

Type `HELP` for the built-in summary, or `HELP <command>` / `<command> /?` for
the detailed usage line and one-line description. Command names are
case-insensitive. The line editor supports Backspace (and DEL), Escape to
clear the whole line, and double quotes to group names containing spaces
(e.g. `TYPE "long name.txt"`). A maximum of eight words (argv slots) is parsed.

## Conventions and path rules

- A path may carry an optional `X:` drive prefix (`C:\FILE.TXT`, `X:GAMES`). The
  prefix temporarily switches to that volume for the duration of the command
  and restores the previously active drive afterwards (the exception is `CD`,
  which stays switched). A bare `X:` switches the active drive permanently.
- Filenames are 8.3 (`NAME.EXT`); copy/write destinations must be 8.3. `DIR`
  accepts `*`, `*.*` and `?` wildcards. With `/W` it prints a wide listing with
  file/directory totals.
- Errors are reported as short names: `not found`, `not mounted`, `in use (dir
  not empty?)`, `already exists`, `disk full`, `too big`, `too deep`, `corrupt`,
  `timed out`, `bad argument`, `not supported`, `general error`.
- Commands that write files serialize through the PortFile service
  (`UserFilesLock`/`UserFilesUnlock`). While a user program leases console
  input, the shell yields the keyboard to it and reclaims the lease only if the
  owning process dies.

## Drive letters (C:, X:, VOL, DRIVE)

- **`C:`** is the default drive: at boot the R1 file server mounts the first
  FAT32 partition of the first AHCI disk (dev 4, part 0) as the working volume.
- **`X:`** is the bundled-programs drive. In an ISO-only boot the end-user E32
  programs arrive as a GRUB Multiboot module, snapshotted into high RAM as a
  read-only block device (dev 8) and aliased to `X:` with `FatMountAlias(8,0,'X')`;
  when an AHCI disk is attached, dev 4 part 0 is additionally aliased to `X:` for
  the older `-drive` based tests. Boot prints `X: mounted (programs from GRUB
  module)` (or `X: mounted (programs)`). On a diskless boot `X:` becomes the
  active drive so `DIR` and program launches work immediately.
- **`VOL`** lists all mounted volumes and their backing device/partition.
  **`DRIVE`** with no argument shows the current drive and the volume table;
  `DRIVE X` switches to a mounted drive. Typing **`X:`** (any letter) is the
  DOS-style equivalent of `DRIVE X` and changes the prompt to that volume.
- Bare program names are searched on the active drive first, then on **every**
  other mounted drive, so a program bundled in `PROGRAMS.IMG` is found even when
  `C:` is active — see *Programs* below.

### Why `MOUNT` hangs — do not type it

FAT32 is already mounted before the shell starts: the boot path calls the FAT
backend mount (dev 4, part 0) and then `FatMountAlias` for `C:`/`X:`. The
`MOUNT` command with no arguments is a *discovery* path meant for a cold boot:
it calls `AtaInit()` and then probes all 8 devices × 4 partitions, doing raw
`AtaRead` MBR and BPB reads for each candidate and calling `FatMount` on every
FAT32 partition it finds. On this image that re-probes the already-mounted
emulated disks and re-runs mount bookkeeping (the volume table is already full:
`C:` + `X:`), which can stall the emulated ATA/AHCI path against the shell. Use
`VOL` / `DRIVE` to inspect what is already mounted instead. Only the explicit
form `MOUNT <device> <partition>` (device 0-7, partition 0-3) is useful for a
newly attached disk, and even then it is normally handled at boot.

## Files

### `DIR [/W] [X:][path [pattern]]`
List a directory without changing the active drive. `*. *` and `?` filter
names; sizes and file/dir totals are shown. Example: `DIR X:\*.E32`. Wide form:
`DIR /W`. A `path` with a `X:` prefix lists that volume without switching.

### `CD [X:][path|..|\]`
Change directory. `CD` alone returns to the root of the current drive; `CD ..`
moves up one level; `CD \` goes to the root. `CD D:` or `CD D:dir` switches
drives and **stays** switched (unlike other path-taking commands). Example:
`CD X:\GAMES`.

### `TYPE [X:]path`
Display a text file in this or another directory. Handles a `X:` prefix, so
`TYPE X:\README.TXT` works from `C:`. Reports `not found` if absent.

### `WRITE [X:]path [text...]`
Replace (create/overwrite) a file. With text on the same line the arguments are
rejoined with single spaces and a CRLF: `WRITE C:\NOTE.TXT Hello world`. With
only a path it enters multiline mode: type lines, terminate with a lone `.`,
and the accumulated lines (up to a 4 KB buffer) are written. Example:
`WRITE X:\SCORE.TXT 100`.

### `COPY [/Y] [X:]source [Y:]dest`
Copy a file up to 64 KiB between volumes using a bounded 64 KiB buffer (no
silent truncation). Overwrite is refused unless `/Y` is given. Output names
must be 8.3. Example: `COPY C:\A.TXT X:\B.TXT` or `COPY /Y A.TXT B.TXT`.
Aliases in help: source/destination is reported with `ErrName` on failure.

### `REN [X:]old [X:]new`
Rename an 8.3 file in place — no data copying, no overwrite, and source and
destination must be on the same drive and directory. `RENAME` is an alias.
Example: `REN OLD.TXT NEW.TXT`.

### `DEL [X:]file`
Delete one file or an empty directory. `ERASE` is an alias. A non-empty
directory yields `in use (dir not empty?)`. Example: `DEL C:\TEMP.TXT`.

### `MKDIR [X:]path`
Create an 8.3 directory at the given path. `MD` is an alias. Example:
`MKDIR X:\GAMES`.

### `RMDIR [X:]name`
Remove an empty directory. `RD` is an alias. Example: `RMDIR X:\GAMES`.

### `CLS`
Clear the VGA screen and the serial terminal (resets the teletype state and
emits ANSI `\033[2J\033[H`). Example: `CLS`.

## Drives

### `MOUNT [device [partition]]`
Mount every FAT32 partition found (no args) or one device (0-7) and partition
(0-3). **Avoid the no-argument form** — see *Why `MOUNT` hangs* above; FAT is
already mounted at boot. Example (only for a newly attached disk):
`MOUNT 4 0`.

### `VOL`
Show mounted drives and their backing devices/partitions (calls `FatList`).
Example: `VOL` → lists `C:` (AHCI dev 4) and `X:` (programs module/dev 8 alias).

### `DRIVE [X]`
Show drives / current drive, or switch to a mounted drive; `X:` is the shorter
equivalent. Example: `DRIVE X` or simply `X:` to switch to the programs drive.

### `FORMAT device yes`
**DESTROYS the whole specified disk** and creates one FAT32 partition. The
literal `yes` confirmation is mandatory; anything else prints help. Example:
`FORMAT 4 yes`. After success the new volume is mounted and reported.

## System

### `VERSION`
Show kernel and CPU information: the genuine EKA2 full-kernel banner, CPUID
feature word `feats`, `CR0`, and tick rate (1 ms). Example: `VERSION`.

### `DATE [YYYY-MM-DD HH:MM:SS]`
Show RTC date and time, or set the CMOS clock (years 2000-2099, strict calendar
validation) and read it back. Both arguments must be present to set. Example:
`DATE 2026-11-20 14:30:00`.

### `UPTIME`
Show milliseconds and seconds since boot via `NKern::TickCount()`. Example:
`UPTIME` → `uptime: 12345 ms (12 s)`.

### `MEMINFO`
Show heap base/max/total, free RAM blocks in the user pool (4096-byte blocks),
and object counts for threads, processes, chunks and timers, plus the free
message-pool count. Example: `MEMINFO`.

### `PS`
List processes and their container IDs, plus the count of open PortFile
channels. Use the IDs with `KILL`, `ARGS`, `WAIT`, `PZOMB`. Example: `PS`.

### `THREADS`
List kernel threads with priority and state (`READY`, `SUSP`, `WSEM`, `SLEEP`,
`BLOCK`, `DEAD`, `DFC`). Example: `THREADS`.

### `KILL id`
Terminate a **user** process by container id (find it with `PS`). It calls
`E32Kill(id)` and never targets the kernel. `KILL 1` reports an error because
pid 1 is the kernel/shell itself; `KILL` works for user E32 PIDs. Example:
`KILL 7` → `killed 7`.

### `REAP`
Release dead user-image code segments (`E32Reap`, also run automatically on
the next program launch). Reports the number reaped. Example: `REAP`.

### `EXITS`
Show recent user-process exit records; these survive reaping, unlike `PS` rows.
Example: `EXITS`.

### `WAIT id [seconds]`
Block until a process dies (default 30 s, capped at 300 s), then report its
recorded exit type/reason. Polls liveness with `hlt` between checks so a busy
program cannot hang the shell. Example: `WAIT 7 60`.

### `REBOOT`
Restart via the 8042 controller (writes `0xFE` to port `0x64`), then halts if
the controller ignores it. Example: `REBOOT`.

### Additional system/diagnostic commands
- `ARGS id` — show a process's command line from `DProcess::iCommandLine`.
- `SEGS` — code-segment census with access counts and attached process.
- `PZOMB id` — inspect a lingering (zombie) process object and its mutex/segs.
- `HEAPTEST` — kernel heap alloc/free sanity check (`total -> rise -> fall`).
- `GBOOT [cmd]` — forward a command to the R1 `G` bootstrap (`R1GCommand`).

## Programs (implicit execution / E32 launch)

Any unrecognized first word is treated as an executable name and passed to
`RunImage()` (`Exec()` falls through to it). The rest of the line becomes the
process command line (`aTail`), retrievable by the program via `command_line`.

- **Bare name** — `CALC`, `PING 5`, `SNAKE`. `RunImage` first tries the name as
  given on the active drive, then searches **all mounted drives** (`A:`-`Z:`)
  until the file is found, leaving that volume active for `E32Run` and
  restoring the caller's drive afterwards.
- **`X:name`** — an explicit drive prefix restricts the search to that volume
  only. Example: `X:CALC` or `X:\GAMES\SNAKE.E32`.
- **`.E32` auto-append** — if the name contains no `.`, `RunImage` retries with
  `.E32` appended. Thus `HELLO`, `HELLO.E32`, and `X:HELLO` all launch the same
  image.
- Before launching, dead user images are reaped; output is `reaped N dead user
  image(s)` (if any), then `<name> started (user process)` or an error, e.g.
  `<name> (not found)` / `(disk full)`.

**Ctrl+C / ABORT:** Press Ctrl+C (console byte `0x03`) while a program runs to
kill it and return to the `EKA2>` prompt (`aborted pid=N`). Portable fallback:
type `ABORT`, `CTRLC`, or `!` at the prompt. Note a PTY/host terminal may
intercept raw `0x03` as SIGINT (killing QEMU); use a pipe for serial-fed QEMU.
Only one foreground child is tracked; `ABORT`/`!` typed while a console-leasing
program runs goes to that program (use `0x03` there).
- All ~70 bundled end-user programs live in `PROGRAMS.IMG` on `X:` — see
  `USER_APPS_CATALOG.md`.

## Hardware

All hardware commands are implemented in the shell dispatch; most perform
idempotent boot-time `*_init()` calls before showing state.

- `PCI` — list PCI devices (`PciList`). Example: `PCI`.
- `DISPLAY [TEST]` — show the VBE framebuffer mode/LFB; `TEST` clears blue and
  draws a border plus an EKA2 GUI banner. Example: `DISPLAY TEST`.
- `MOUSE [INFO|POLL|TEST]` — show PS/2 mouse cursor/buttons/packets; `INFO`
  details the device, `POLL` drains the 8042 once, `TEST` injects a synthetic
  packet through IRQ12. Example: `MOUSE INFO`.
- `IDE` — list ATA/AHCI disks and their FAT32 partitions (dev 0-3 ATA, 4-7
  AHCI). Example: `IDE`.
- `AHCI` — show SATA controller details (`AhciInfo`). Example: `AHCI`.
- `VBLK [INFO|TEST]` — show virtio-blk PCI/capacity/queue state; `TEST` reads
  sector 0 and writes/reads back a magic pattern on sector 1 through the
  virtqueue. Example: `VBLK TEST`.
- `RNG [INFO|TEST]` — show virtio-RNG state; `TEST` fetches 16 entropy bytes
  through the virtqueue. Example: `RNG TEST`.
- `AUDIO [PLAY]` — show AC97 BARS/vendor ID; `PLAY` emits a 1 kHz test tone.
  Example: `AUDIO PLAY`.
- `USB [POLL|INFO]` — probe HID keyboards; `POLL` dumps one debug report, `INFO`
  shows OHCI controller/ports. Example: `USB INFO`.
- `EHCI [INFO]` — show EHCI USB2 version/port count; `INFO` dumps per-port
  connect/enable/speed. Example: `EHCI INFO`.
- `XHCI` — show USB3 controller status (`XhciInfo`). Example: `XHCI`.
- `XHCIPORTS` — list USB3 root-port status. Example: `XHCIPORTS`.
- `NIC [ARP [count]]` — show e1000 link/MAC; `NIC ARP` sends ARP requests and
  waits for SLIRP replies (default 3 tries, 1-9). Example: `NIC ARP 3`.
- `VNET [INFO]` — show virtio-net PCI/MAC/queue state (alternative NIC backend
  to e1000). Example: `VNET INFO`.
- `NETBRIDGE [ms]` — pump the kernel<->userland e1000 frame bridge for up to
  `ms` milliseconds (default 6000, max 120000). Example: `NETBRIDGE 2000`.

xHCI bring-up diagnostics (also in the dispatch): `XHCISTART` (start
controller), `XHCINOOP` (no-op), `XHCISLOT port`, `XHCIADDR port`,
`XHCIDESC port`, `XHCIKBD port` — ports are 1-8.

## Aliases quick reference

| Alias | Command | Alias | Command |
|-------|---------|-------|---------|
| `MD` | `MKDIR` | `RD` | `RMDIR` |
| `ERASE` | `DEL` | `RENAME` | `REN` |

## Source reference

Everything above is derived from `variant/kern_shell.cpp`: the `kHelp[]` table
and `PrintHelp()` (help text/groups), `Exec()` (dispatch and tokenization),
`RunImage()` (program launch), `CopyFile()`, `WriteMultiline()`, the `DATE`
parsers, and the hardware/system command handlers. Drive setup is in
`variant/qemu_variant_asic.cpp::Init3()` and `variant/fat32.cpp`
(`FatMount`, `FatMountAlias`, `FatCurLetter`, `FatUse`).

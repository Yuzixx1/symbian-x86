# Getting started — EKA2 x86 bare-metal research port (QEMU i386)

Genuine Symbian EKA2 microkernel sources (`../kernelhwsrv`, Eclipse-licensed
Nokia contribution) running on bare-metal QEMU i386 via our own GRUB-free
boot sector. No bldmake, no ActivePerl, no Windows host, no xorriso.

## Prerequisites (verified on Fedora 44 x86_64)

| Tool | Version used | Purpose |
|---|---|---|
| `gcc`, `g++` | 16.2.1 | freestanding `-m32` kernel C++ (`-D__GCC32__`) |
| `as`, `ld` | binutils 2.46.1 | GAS `--32`, `ld -m elf_i386` |
| `qemu-system-i386` | 10.2.2 | target (`-m 128`, `-serial stdio`, `-cdrom`) |
| `make` | any GNU Make | build (relative paths only — workspace has a space) |
| `python3` + `pycdlib` | 3.x + 1.21.0 (`pip install --user pycdlib`) | ISO mastering (no xorriso needed) |
| host `grub2-*` | **not required** | El Torito uses our own `boot/bootsect.S` |

No 32-bit libc needed (`-nostdlib` throughout). No NASM. `tools/check_env.sh`
probes all of this (`make check-env`).

## Build

From this directory (`qemu-port/`):

```sh
make check-env     # verify toolchain (gcc/as/ld/qemu)
make check-slice   # verify the 37 genuine EKA2 inputs exist
make               # build/build/multiboot.elf  (Multiboot1 kernel ELF)
make iso           # + build/bootsect.bin, build/symbian-pc.iso (88 KiB)
```

`make iso` asserts two invariants and fails loudly otherwise:

- kernel ELF ≤ 32768 bytes (boot sector reads a fixed 2×8 sectors);
- kernel entry `_mb_entry` stays `0x100040` (boot sector hardcodes it).

## Run

Fast path (QEMU multiboot `-kernel`, same ELF the ISO carries):

```sh
make qemu            # 5 s smoke boot, serial banner on stdio
```

CD-ROM path (out-of-the-box, also how bare metal boots it):

```sh
make qemu-cdrom      # qemu-system-i386 -cdrom build/symbian-pc.iso -boot d
```

Manual equivalent:

```sh
qemu-system-i386 -cdrom build/symbian-pc.iso -boot d -m 128 \
  -display none -serial stdio -no-reboot
```

Burning `build/symbian-pc.iso` to CD/USB (via `isohybrid`-style tools or
`dd`) boots the same image on physical i386 test machines with a CD drive
(needs: 486+, PSE/PGE for 4M pages and global mappings, 16550A COM1 for the
boot log, i8259/i8254, ≥32 MiB RAM; EDD BIOS required).

## Interactive shell

Both paths land at an `EKA2>` prompt (VGA 80×25 + COM1 mirror). Type over
serial stdin, or PS/2 (`-display gtk` + typing, or monitor `sendkey`).

```
EKA2> help       # list commands
EKA2> version    # port info (cpu_features, CR0, tick, phases)
EKA2> uptime     # ms/s from NTimerQ::TickCount
EKA2> threads    # nanokernel thread table (id/pri/state/switches/stack)
EKA2> meminfo    # RAM map, CR3/PG/PSE/PGE, heap bounds, direct map
EKA2> ps date pci ide usb            # processes, RTC, PCI scan, IDE, USB HID
EKA2> mount vol drive dir cd type write del mkdir format  # FAT32 storage
EKA2> exec <file.e32>  # validate a Symbian executable (see below)
EKA2> reboot     # 8042 reset (double banner proves it)
```

### Running Symbian executables (`exec`, stage 3a)

`tools/mke32.py` builds a minimal valid E32 EXE (import-free, uncompressed,
x86) and `tools/e32read.py` validates it on the host — both implement the
genuine header checks from `f32image.h` (`E32ImageHeaderV::ValidateHeader`)
with the genuine CRC32/UID-checksum algorithms from `euser/us_func.cpp`:

```sh
python3 tools/mke32.py build/hello.e32
python3 tools/e32read.py build/hello.e32   # exit 0 = valid
```

Copy it onto a seeded disk (`tools/mkdisk.py` includes it automatically
when present), boot with the disk attached, and validate on-device:

```
EKA2> mount
EKA2> exec HELLO.E32
E32 EXE uids 1000007a 10000001 10000002
  entry=0x0 codeBase=0x400000 code=64 data=0 bss=0
  heap=4096/1048576 stack=8192 (validated, not executed yet)
```

The in-kernel validator (`variant/e32load.cpp`) agrees with the host tool
on every verdict (corrupt files print `corrupt`, non-E32 files are
rejected). Actual user-process bring-up (`Kern::ProcessCreate`, relocs,
DLL imports) is the defined next step — see `docs/usermode_plan.md`.

Serial capture to a file: `make qemu-capture` (writes `build/serial.log`).

## Layout

- `boot/multiboot.S`, `boot/link.ld` — Multiboot1 bridge + link (load @1M).
- `boot/bootsect.S`, `boot/bootsect.ld` — El Torito boot sector (reads the
  kernel by INT13 EDD into staging, PHDR-correct load to linked addresses,
  protected-mode jump). Uses only discrete `movl`, no string instructions.
- `boot/ported/` — GAS ports of genuine `bootcpu.asm`/`bootmain.asm`
  (GDT/IDT/CPUID/ResetEntry skeleton).
- `variant/` — minimal QEMU base-port (`Asic`-shaped: COM1, i8259 remap to
  `0x20`, i8254 1 kHz, IRQ stubs, ROM/SuperPage synth, `KernelMain`).
- `mmu/` — Direct-model identity paging (32×4M, CR0.PG/CR4.PGE).
- `rom/` — compatible-prefix `TRomHeader`/`SSuperPageBase` synthesizer.
- `nk/` — genuine-named nanokernel scheduler (`SThreadStack`-exact switch).
- `console/` — VGA driver, PS/2 Set-1 keyboard, `EKA2>` shell thread.
- `inc/` — QEMU variant config (`qemu_config.h`, `qemu_boot.inc`).
- `tools/` — `check_env.sh`, `mkiso.py` (pycdlib El Torito, boot-info patch).

## Known porting notes (earned the hard way)

- `.cia` files compile as-is with `g++ -D__GCC32__` (they already carry GCC
  branches); pure-MASM `.asm` bootstrap was hand-ported to GAS (originals
  untouched in `kernelhwsrv/`).
- The boot sector uses polled COM1 + INT10/INT13/INT12/INT15 only; after the
  last disk read it uses no BIOS, no calls, no stack state.
- Real-mode x86 string instructions (`rep movs*`, `lods*`, `stos*`) proved
  unreliable in this QEMU/SeaBIOS configuration; all boot-sector moves use
  discrete `movl`/`movb` (documented at each site).
- SeaBIOS EDD on the El Torito drive addresses 2048-byte CD blocks (like
  GRUB's own `cdboot`, which divides its catalog count by 4 the other way);
  do not scale LBA/count by 4.
- The kernel is entered with a GRUB-style `EAX=2BADB002/EBX=MBI` contract
  synthesized by the boot sector; the MBI carries flags + mem sizes only.

## What this is not (yet)

Full genuine `NKern`/`Kern` object compilation (needs the bldmake-exported
`plat_priv.h` + variant DFC/wait objects), demand paging, user processes,
device drivers beyond UART/PIC/PIT/keyboard/VGA-text. See `README.md` phase
records for scope honesty per milestone.

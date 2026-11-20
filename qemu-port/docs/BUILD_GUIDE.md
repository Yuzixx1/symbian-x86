# Build Guide — EKA2 x86 QEMU port (`qemu-port/`)

This is the authoritative from-scratch build recipe for the standalone
`qemu-port/` tree. Everything is driven by the top-level `Makefile` plus
`toolchain.mk` and `user/user.mk`. All paths in the Makefile are **relative**
(the workspace root contains a space, `"Sybian REAL"`, which GNU Make cannot
handle in targets), so always run `make` from `qemu-port/` itself.

`../kernelhwsrv/` is READ-ONLY source: the build only reads `.cia`/`.cpp`/header
files from it. Never edit, commit or push that tree when building.

## 1. Toolchain requirements

No Windows host, ActivePerl, `bldmake` or MSVC is needed. Everything is
Freestanding (`-nostdlib`), so **no 32-bit glibc** (`glibc-devel.i686`) is
required. The toolchain was proven with gcc 16.2.1 / binutils 2.46.1 / QEMU
10.2.2, but any modern gcc/g++/binutils with a 32-bit backend works.

| Requirement | Why / where used |
|---|---|
| `gcc`, `g++` with `-m32` | kernel, variant and user `.c`/`.cpp` compiles (`CC`, `CXX`); `gcc -m32 -print-libgcc-file-name` supplies `LIBGCC` for `__udivdi3` |
| GNU `as` with `--32` and `--64` | 32-bit boot/asm objects; `as --64` for `boot/efi/trans64.S` |
| GNU `ld` with `-m elf_i386` and `-mi386pep` | ELF kernel/user links and the PE32+ UEFI loader |
| `objcopy`, `file`, `nm`, `size` | UEFI section stripping, ELF header/symbol checks |
| `make` | top-level driver |
| `python3` + `pycdlib` | ISO mastering (`tools/mkiso.py`, `tools/mkgrubiso.py`) and every `tools/*.py` E32 tool |
| mtools: `mkfs.fat`, `mcopy`, `mdir` | builds/verifies the FAT32 `PROGRAMS.IMG` inside the GRUB ISO (skipped gracefully if absent) |
| GRUB 2.x tools: `grub2-mkimage` + modules in `/usr/lib/grub/i386-pc` | BIOS `i386-pc-eltorito` and UEFI `x86_64-efi` `BOOTX64.EFI` for `isogrub` |
| `perl` | `build/export/exec_enum.h` via genuine `kernel/execs.txt` + `genexec.pl` |
| `qemu-system-i386` (and `qemu-system-x86_64` for `qemu-uefi`) | smoke boots |
| `timeout`, `head` | smoke-boot targets |

Quick check: `make check-env` runs `tools/check_env.sh`, which verifies gcc,
as, ld, make, QEMU, freestanding `-m32` compilation, `ld -m elf_i386` and
`as --32`. It does **not** check pycdlib/mtools/GRUB — verify those with
`python3 -c 'import pycdlib'`, `command -v mkfs.fat mcopy mdir grub2-mkimage`.

## 2. Make targets and what they build

The default goal (`.DEFAULT_GOAL := all`) is:

```
all: $(BRIDGE_ELF) iso isofull isogrub userland
```

so a bare `make` refreshes both kernels, all three ISO flavours and every user
E32. Individual targets:

- `make check-env` — toolchain probe (see above).
- `make` / `build/multiboot.elf` (`BRIDGE_ELF`) — nanokernel/bridge image:
  `boot/multiboot.S` + ported `bootcpu_gas.S`, `bootmain_gas.S`, `mmu_gas.S`,
  variant glue (`qemu_assp`, `irq_gas`, `genuine_glue`, `genuine_console`) and
  the genuine `GEN_OBJS` (`build/genuine/*`), linked with `boot/link.ld`.
- `make kernfull` — builds `build/kernfull.elf` from `KERNFULL_OBJS`, then
  smoke-boots it with `-kernel` for 12 s (exit 124 = still running, expected).
  `KERNFULL_OBJS` = bridge/asm objects + all variant drivers (PCI, ATA, AHCI,
  NIC, virtio, display, audio, xHCI, FAT32, e32load, paging, user files,
  console, R1 FS core/FAT backend, shell) **plus** `KNK_OBJS` (genuine
  nanokernel, `build/knk/*`: nkern, sched, nk_timer, dfcs, nklib, ncglob,
  nk_entry, nkerns + `*_cpp`/`.cia` variants) **plus** `KERN_OBJS_1..4`
  (genuine Kern personality, `build/kern/*`: x86 `c*` files and hand-written
  `.cia` dispatchers, Direct memory-model files, core/klib sources). Link
  rule: `ld -m elf_i386 -nostdlib -static -T boot/link.ld ... $(LIBGCC)`.
- `make userland` — builds all E32 programs in `build/user/` (ECHO, TYPE,
  CALC, SNAKE, APPLAUNCH, PORTFILESRV, SVCMGR, SELFTEST, NETSTACK, HTTPGET,
  GUISHELL, ...). Each `.c`/`.cpp` compiles with `USER_FLAGS` (`-m32
  -march=i486 -Os -ffreestanding ... -fno-rtti`), links to `*.elf` with
  `user/link.ld`, then `tools/elf2e32.py` (or `mke32*.py`) makes the PE2 `.E32`
  and `tools/e32read.py --quiet` validates it.
- `make iso` — requires `BRIDGE_ELF`, `bootsect.bin`, `mbr.bin`,
  `BOOTX64.EFI`; checks ELF size ≤ 4 MiB and entry `0x100030`, then
  `tools/mkiso.py` → `build/symbian-pc.iso` (hand-written bootsect El Torito).
- `make isofull` — same checks on `kernfull.elf`, `tools/mkiso.py
  build/kernfull.elf build/symbian-pc-full.iso`.
- `make isogrub` — `build/kernfull.elf` + `build/multiboot.elf` →
  `tools/mkgrubiso.py build/symbian-grub.iso` (GRUB BIOS+UEFI combo ISO; see
  `GRUB_BOOT_INTERNALS.md`).
- Smoke boots: `qemu`, `qemu-capture`, `qemu-cdrom`, `qemu-cdfull`, `qemu-hd`,
  `qemu-uefi`. `headers` dumps the multiboot header/sections; `check-slice`
  verifies every genuine EKA2 input file exists.

## 3. Build order (dependency chain)

1. Generated headers: `perl genexec.pl` → `build/export/exec_enum.h`,
   `exec_user.h`, `kernel/exec_kernel.h`.
2. Kernel/asm objects: `build/knk/*.o` (`KNK_OBJS`) and `build/kern/*.o`
   (`KERN_OBJS_1..4`) and `build/genuine/*.o` (`GEN_OBJS`), plus variant
   `build/*.o`. `.cia` files are C++ with GNU-as branches and need
   `-x c++` + `-D__CIA__ -D__GCC32__` and, for `cexec.cia`, a preprocess +
   `tools/fix_cia_immediates.py` fixup step. `-O2` is mandatory: at `-O0`
   frame-pointer prologues corrupt the bare-entry stack math.
3. Link `build/kernfull.elf` (and `build/multiboot.elf`) with `boot/link.ld`.
4. `make userland` → `build/user/*.E32` (independent of the kernel link).
5. Disk images: `tools/mkgrubiso.py` stages a curated set of E32s (≥ 30 KB)
   into a 32 MB MBR + 16 MB FAT32 `PROGRAMS.IMG` using `mkfs.fat`/`mcopy`,
   verifies with `mdir`, and embeds it at `/programs.img` in the ISO.
6. ISO mastering: `pycdlib` writes El Torito images; the GRUB BIOS boot info
   table is patched in a two-pass (size-stable) write.

## 4. Common warnings (all benign)

- **`warning: offsetof within non-standard-layout type ...
  [-Winvalid-offsetof]`** from genuine EKA2 C++ headers. The kernel defines
  `_FOFF(c,f)=((TInt)__builtin_offsetof(c,f))` and the source layout is the
  compiler's own ABI, so the offsets are exactly what the genuine code expects.
- **`readelf: Warning: LOAD segment ... RWX`** / GNU_STACK notes. The image is
  a Freestanding ring-0 kernel with a hand-written identity page table;
  executable data pages are intentional at this stage. It does not indicate a
  build failure.
- **`-fno-rtti` applied to C sources** (`USER_FLAGS`/`KERN_FLAGS` are shared by
  `CC` and `CXX`). GCC silently ignores C++-only flags on C inputs; it is
  harmless and keeps one flags variable for both compilers.
- **`ENV-FAIL`/missing `pycdlib`, `mkfs.fat`, `grub2-mkimage`** are real
  failures — see troubleshooting.

## 5. Incremental vs clean rebuild

`make` is incremental: object timestamps decide what recompiles, and the ISO
scripts restage the FAT image on every `isogrub` run. This is the normal path
and is **strongly preferred** when several agents share a workspace.

`make clean` is a single `rm -rf build`. **Do not run it when other agents are
building/testing.** It deletes every object, both kernels, all user E32s, the
`build/export` generated headers, the staged `PROGRAMS.IMG`, `build/grubwork`
and all ISOs at once. A concurrent agent's ISO smoke test will then fail on
missing `kernfull.elf`/`PROGRAMS.IMG`, and rebuilding the whole genuine slice
is expensive. If a rebuild is genuinely required, prefer targeted removal
(e.g. `rm build/kernfull.elf`) or coordinate with the other agents first. A
full from-scratch verify is best done in a separate worktree.

## 6. Troubleshooting

- **`ModuleNotFoundError: No module named 'pycdlib'`** — install it
  (`python3 -m pip install pycdlib`) or use the project venv. Every `*.iso`
  target and `boot_grub.sh`'s extraction helper imports it.
- **Programs missing from `X:` / `EUSER.DLL` absent** — `mkgrubiso.py` only
  adds `EUSER.DLL` if it exists at `qemu-port/EUSER.DLL` (it is *not* produced
  by `make`). The genuine DLL also lives in the cache path used by the test
  harnesses: `/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll`
  (and `portfilesrv` in `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/`).
  Copy it to `qemu-port/EUSER.DLL` before `make isogrub`; otherwise PE2 import
  ordinal 915 (`RDebug::Printf`) cannot resolve and apps fail at load.
- **`ENTRY MOVED (... != 00100030)`** from `iso`/`isofull` — the hand-written
  bootsect hardcodes jump address `0x100030`; if `_mb_entry` moves in
  `boot/link.ld`, the bootsect and the guard must be updated together. Use
  `multiboot`/GRUB boot (which reads the ELF header) as a workaround.
- **`BOOTSECT TOO BIG` / `BSS < 64M` sanity failure** — `bootsect.bin` must fit
  4096 bytes and its BSS-zero loop rejects per-segment BSS ≥ 0x4000000
  (`kernfull .bss` is ~2 MB). Keep kernel data/stacks/heap in the fixed RAM
  region, not in the ELF `.bss` (see `variant/kernfull_glue.cpp`).
- **`KERNEL TOO BIG (> 4M)`** — the bootsect loader's streaming budget; build
  with the standard flags (never `-O0`) and avoid linking debug bloat.
- **GRUB ISO boots but no `X:`** — `grub2-mkimage` or `/usr/lib/grub/i386-pc`
  missing means `isogrub` cannot build; also confirm `build/user/*.E32` exist
  (≥ 30 KB) and `mtools` is installed, else `PROGRAMS.IMG` is skipped and the
  `module /programs.img` line is omitted from `grub.cfg`.

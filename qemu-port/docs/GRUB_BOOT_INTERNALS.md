# GRUB ISO Boot Internals — from El Torito to `X:`

This document traces the `build/symbian-grub.iso` boot path end to end: how the
ISO is mastered, what GRUB does, the Multiboot1 handoff, and how the kernel
turns a GRUB `module` into a mounted FAT volume at `X:`. All details are taken
from `tools/mkgrubiso.py`, `tools/boot_grub.sh`, `boot/multiboot.S`,
`variant/bootinfo.cpp`, `variant/kernfull_glue.cpp`, `variant/qemu_ata.cpp`
and `variant/qemu_variant_asic.cpp`. See `BUILD_GUIDE.md` for build commands.

## 1. Why a GRUB ISO at all

The original `symbian-pc*.iso` images use a hand-written bootsector with an EDD
streaming loader. On real firmware (notably VirtualBox's BIOS) the *second*
64-bit EDD read hangs with no error return. GRUB 2.12 supplies its own BIOS
disk drivers, which sidestep that path entirely. The kernels already carry a
valid Multiboot1 header (`flags=3`: align modules to 4 K + memory info), so
GRUB's `multiboot` command delivers exactly the contract `_mb_entry` expects:
32-bit protected mode, paging off, A20 on, interrupts off.

## 2. ISO mastering (`tools/mkgrubiso.py`)

Called by `make isogrub` as `python3 tools/mkgrubiso.py build/symbian-grub.iso`.
It requires `build/kernfull.elf` and `build/multiboot.elf` up front, then:

1. **Stages programs.** `build_programs_image()` creates a 32 MB MBR disk with
   a 16 MB FAT32 partition at LBA 2048 (`mkfs.fat -F 32`, `mcopy`, verified by
   `mdir`). It copies every `build/user/*.E32` ≥ 30 KB, plus `qemu-port/EUSER.DLL`
   if present, plus generated `NOTES.TXT`/`README.TXT`, and saves the result as
   `build/PROGRAMS.IMG` (and a work copy in `build/grubwork/programs.img`).
2. **Builds the BIOS core.** All `*.mod` files from `/usr/lib/grub/i386-pc`
   are copied under `/boot/grub/i386-pc` (Rock Ridge lowercase names so GRUB's
   lookups hit); `grub2-mkimage -O i386-pc-eltorito -p (cd)/boot/grub` with
   modules `biosdisk iso9660 multiboot normal configfile echo serial terminal`
   produces `/boot/grub/boot.img`.
3. **Builds the UEFI core.** `grub2-mkimage -O x86_64-efi -p /boot/grub -c
   efi_early.cfg` with `iso9660 search multiboot normal configfile echo`
   produces `BOOTX64.EFI`, wrapped by `mkfat.build_fat16` into a 4 MB
   unpartitioned `efiboot.img`. An early config searches the ISO by file for
   `/boot/grub/grub.cfg`, so no device-name guessing is needed.
4. **Writes the ISO with `pycdlib`** (`interchange_level=3`, `rock_ridge=1.09`,
   vol id `SYMBIAN_GRUB`). Included files:
   - `/boot/kernfull.elf` ← `build/kernfull.elf`
   - `/boot/nano.elf` ← `build/multiboot.elf`
   - `/boot/grub/grub.cfg`, `boot.img`, all i386-pc modules, `efiboot.img`
   - `/programs.img` ← the FAT32 image above (only when it exists)
   Two El Torito entries are added: BIOS (`add_eltorito("/BOOT/GRUB/BOOT.IMG;1")`)
   and, unless `--bios-only`, EFI (`platform_id=0xef, efi=True`).
5. **Patches the boot info table** (two-pass, like `mkiso.py`): GRUB's
   `cdboot.S` requires `bi_pvd/bi_file/bi_length/bi_csum` at offset 8 of
   `boot.img`; the file's own ISO LBA is only known after mastering, so the
   script masters → finds the LBA via `mkiso.iso_extent()` → patches offset 8
   → rewrites the ISO → verifies the LBA is stable (`GRUB ISO layout unstable`
   otherwise).

`make isogrub` writes `build/symbian-grub.iso`; `--bios-only` produces
`build/symbian-grub-bios.iso` for legacy BIOS / USB `dd` use.

## 3. GRUB menu and the multiboot handoff

Both BIOS and UEFI `grub.cfg` set `serial --unit=0 --speed=115200`, send
input/output to `serial console`, use `set timeout=10` and `set default=0`
(~10 s countdown, full kernel auto-selected). Two entries:

```
menuentry "Symbian EKA2 full kernel (genuine)" {
    set gfxpayload=text
    echo Booting full kernel...
    multiboot /boot/kernfull.elf
    module /programs.img          # only if the image was staged
    echo Multiboot ok, booting...
    boot
}
menuentry "Symbian EKA2 nanokernel" {
    multiboot /boot/nano.elf
    boot
}
```

The `module` line is generated dynamically by `configs(with_programs)` and is
omitted if `PROGRAMS.IMG` was not staged — otherwise GRUB would abort the entry
on a missing file. GRUB validates the Multiboot header magic `0x1BADB002` and
enters `_mb_entry` with **`EAX = 0x2BADB002`**, **`EBX = multiboot_info*`**,
`CS/DS/ES/FS/GS` = 32-bit data segments. `boot/multiboot.S` checks the magic,
saves EAX/EBX into `qemu_handoff` (+4 magic, +8 MBI pointer) and jumps to the
ported `ResetEntry`. The MBI layout uses flag bit 0 (mem_lower/mem_upper) and
**bit 3 (mods)**: `mods_count` at MBI+20, `mods_addr` at MBI+24; each module
entry is `{ mod_start, mod_end, string, reserved }` (16 bytes). `PROGRAMS.IMG`
is module 0.

## 4. Kernel side: capture, snapshot, mount

1. **`BootInfo::Capture()`** (`variant/bootinfo.cpp`, called from
   `variant/qemu_kernel.cpp`/`kernfull_glue.cpp` with the saved MBI address)
   rejects addresses below `0x1000`, stores flags and memory sizes, and when
   `flags & 0x08` reads `mods_count`, `mods_addr` and the first module's
   `modStart`/`modEnd`.
2. **`HwInit0`** (`variant/kernfull_glue.cpp`) calls `BootInfo::Capture()` and
   immediately `QemuRamdiskInit(BootInfo::modStart, BootInfo::modEnd)`, *before*
   the kernel-data `memclr` and before user-page allocation.
3. **`QemuRamdiskInit()`** (`variant/qemu_ata.cpp`) copies the module to a safe
   high-RAM buffer (default top 128 MB, clamped to `memUpperKB`, `dst` must be
   ≥ `0x2000000`), records `gRamDiskSectors = len/512` and logs
   `RAMDISK: GRUB module ...`. **Why copy:** GRUB loads modules in low pages
   that the kernel's data clear / user-page allocator will recycle, so reading
   the image in place would be corrupted. The copy is exposed as read-only
   block device **8** (`AtaRead` memcpy's from it; `AtaWrite` returns
   `KErrNotSupported`; `AtaFlush` is a no-op), model string
   `"GRUB-MODULE RAMDISK"`. Device numbering: 0–3 IDE, 4–7 AHCI, 8 RAM module.
4. **`FatMountAlias(8, 0, 'X')`** (`variant/fat32.cpp`, invoked in
   `variant/qemu_variant_asic.cpp`) mounts device 8 partition 0 under drive
   letter `X:`. On a diskless boot `X:` becomes the active volume; with an
   attached AHCI disk (dev 4 = `C:`) `X:` is just an extra alias. The serial
   console prints `X: mounted (programs from GRUB module)` (vs the legacy
   `X: mounted (programs)` when aliasing dev 4). `VOL`, `DIR`, `DRIVE`, `TYPE`
   all read the same volume table, and `SvcMgrBootLaunch()` starts the service
   manager from the mounted FAT image.

## 5. Plain boot command (no `-drive` needed)

```
qemu-system-i386 -M q35 -m 128 -vga std -serial stdio -no-reboot \
  -cdrom build/symbian-grub.iso -boot d \
  -netdev user,id=n0 -device e1000,netdev=n0
```

`-vga std` gives the EKA2 VGA console (mirrored with COM1 serial); `e1000`
provides the SLIRP network used by NETSTACK/HTTPGET/PING etc. After the ~10 s
countdown and the multiboot banner, the `EKA2>` prompt appears. At the prompt:

```
X:          # select the module-backed volume (already active on diskless boot)
DIR         # list programs
CALC        # or SNAKE, APPLAUNCH, FILEMGR, ...
```

`X:` therefore means "the curated end-user E32 set from `/programs.img`,
delivered by GRUB as a Multiboot module and served from high RAM as a
read-only FAT volume" — not a QEMU disk. `VOL` reports the volume, `DIR` lists
the FAT directory, `CALC` is one of the bundled calculators.

## 6. Alternate launcher: `tools/boot_grub.sh`

Legacy/optional path for attaching `PROGRAMS.IMG` as a **real AHCI disk**
instead of relying on the module:

```
tools/boot_grub.sh [path/to/iso] [extra QEMU args...]
# env: SYMBIAN_ISO=<iso>, PROGRAMS_IMG=<extraction path>
```

It extracts `/PROGRAMS.IMG;1` from the ISO with `pycdlib` (QEMU cannot use a
file nested in an ISO as a block device), then boots with
`-M q35 -cdrom "$ISO" -boot d -display none -serial stdio` plus
`-drive file=$IMG,format=raw,snapshot=on,if=none,id=d0` and
`-device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0`. `-boot d` is
essential: without it the attached data disk wins boot order and the machine
hangs in the FAT32 partition's (bootable but empty) MBR. In this mode the
programs appear at dev 4 and the kernel prints `X: mounted (programs)`; the
plain ISO boot above needs no launcher at all.

#!/usr/bin/env python3
"""mkgrubiso.py — GRUB2 combo ISO (BIOS + UEFI) for the genuine EKA2 kernels.

Why: our hand-written bootsect's EDD streaming hangs VirtualBox's BIOS on
the 2nd read (true firmware hang, no error return). GRUB's own disk
drivers replace that path entirely.

The kernels already carry a valid Multiboot1 header (flags=3), so GRUB's
`multiboot` command loads them with the exact contract our _mb_entry
expects (EAX=2BADB002, EBX=MBI, PM, paging off, A20 on).

ISO layout:
  BIOS: El Torito no-emulation boot.img (grub2-mkimage i386-pc-eltorito,
        prefix (cd)/boot/grub, modules on disc) + Rock Ridge lowercase
        names so GRUB's /boot/kernfull.elf lookups hit exactly.
  UEFI: El Torito EFI entry -> FAT image with GRUB x86_64-efi BOOTX64.EFI
        + grub.cfg + both kernels (own copies, no ISO9660 driver needed).
Both menus offer full kernel + nanokernel.
Usage: mkgrubiso.py [out.iso]
"""
import os
import shutil
import subprocess
import sys

from pycdlib import PyCdlib

import mkfat
import mkiso


def patch_boot_info_table(staged_img, iso_lba):
    """El Torito boot info table (what xorriso --boot-info-table writes and
    GRUB's cdboot.S requires): bi_pvd/bi_file/bi_length/bi_csum at image
    offset 8. Without it cdboot prints 'no boot info' immediately."""
    with open(staged_img, "r+b") as f:
        data = bytearray(f.read())
        import struct
        struct.pack_into("<IIII", data, 8, 16, iso_lba, len(data), 0)
        f.seek(0)
        f.write(data)
    print(f"GRUB-BIOTAB: {staged_img} file_lba={iso_lba} len={len(data)}")

HERE = os.path.dirname(os.path.abspath(__file__))
QPORT = os.path.normpath(os.path.join(HERE, ".."))
BUILD = os.path.join(QPORT, "build")
FULL = os.path.join(BUILD, "kernfull.elf")
NANO = os.path.join(BUILD, "multiboot.elf")
GRUB_PC = "/usr/lib/grub/i386-pc"


def run(cmd):
    print("+", " ".join(cmd))
    subprocess.run(cmd, check=True)


BIOS_CFG = """serial --unit=0 --speed=115200
terminal_input serial console
terminal_output serial console
set timeout=10
set default=0
menuentry "Symbian EKA2 full kernel (genuine)" {
    set gfxpayload=text
    echo Booting full kernel...
    multiboot /boot/kernfull.elf
    echo Multiboot ok, booting...
    boot
}
menuentry "Symbian EKA2 nanokernel" {
    set gfxpayload=text
    echo Booting nanokernel...
    multiboot /boot/nano.elf
    boot
}
"""

EFI_CFG = """set timeout=10
set default=0
menuentry "Symbian EKA2 full kernel (genuine)" {
    echo Booting full kernel...
    multiboot /boot/kernfull.elf
    boot
}
menuentry "Symbian EKA2 nanokernel" {
    echo Booting nanokernel...
    multiboot /boot/nano.elf
    boot
}
"""


def main():
    bios_only = "--bios-only" in sys.argv
    if len(sys.argv) > 1 and not sys.argv[1].startswith("--"):
        out = sys.argv[1]
    elif bios_only:
        out = os.path.join(BUILD, "symbian-grub-bios.iso")
    else:
        out = os.path.join(BUILD, "symbian-grub.iso")
    for f in (FULL, NANO):
        if not os.path.exists(f):
            sys.exit(f"{f} missing: build it first")
    work = os.path.join(BUILD, "grubwork")
    shutil.rmtree(work, ignore_errors=True)
    iso_root = os.path.join(work, "iso")
    os.makedirs(os.path.join(iso_root, "boot", "grub", "i386-pc"))

    with open(os.path.join(iso_root, "boot", "grub", "grub.cfg"), "w") as f:
        f.write(BIOS_CFG)
    for name in os.listdir(GRUB_PC):
        if name.endswith(".mod"):
            shutil.copyfile(os.path.join(GRUB_PC, name),
                            os.path.join(iso_root, "boot", "grub", "i386-pc", name))
    run(["grub2-mkimage", "-O", "i386-pc-eltorito", "-o",
         os.path.join(iso_root, "boot", "grub", "boot.img"),
         "-p", "(cd)/boot/grub",
         "biosdisk", "iso9660", "multiboot", "normal", "configfile",
         "echo", "serial", "terminal"])
    shutil.copyfile(FULL, os.path.join(iso_root, "boot", "kernfull.elf"))
    shutil.copyfile(NANO, os.path.join(iso_root, "boot", "nano.elf"))

    # UEFI GRUB: served off the ISO9660 side (firmware boots BOOTX64.EFI from
    # the FAT image, but GRUB only ever sees the CD as (cd0)). Early config
    # locates the shared /boot/grub/grub.cfg by file search — no memdisk,
    # no device-name assumptions.
    efi_core = os.path.join(work, "BOOTX64.EFI")
    efi_early = os.path.join(work, "efi_early.cfg")
    with open(efi_early, "w") as f:
        f.write("search --no-floppy --file /boot/grub/grub.cfg --set=root\n"
                "set prefix=($root)/boot/grub\n"
                "configfile /boot/grub/grub.cfg\n")
    run(["grub2-mkimage", "-O", "x86_64-efi", "-o", efi_core,
         "-p", "/boot/grub", "-c", efi_early,
         "iso9660", "search", "multiboot", "normal", "configfile", "echo"])
    with open(efi_core, "rb") as f:
        core = f.read()
    efi_img = os.path.join(work, "efiboot.img")
    mkfat.build_fat16([(("EFI", "BOOT", "BOOTX64.EFI"), core)],
                      4, efi_img, partitioned=False)
    shutil.copyfile(efi_img, os.path.join(iso_root, "boot", "grub", "efiboot.img"))

    def write_iso():
        iso = PyCdlib()
        iso.new(interchange_level=3, sys_ident="LINUX", vol_ident="SYMBIAN_GRUB",
                rock_ridge="1.09")
        iso.add_directory("/BOOT", rr_name="boot")
        iso.add_directory("/BOOT/GRUB", rr_name="grub")
        iso.add_directory("/BOOT/GRUB/I386PC", rr_name="i386-pc")
        iso.add_file(os.path.join(iso_root, "boot", "kernfull.elf"),
                     "/BOOT/KERNFULL.ELF;1", rr_name="kernfull.elf")
        iso.add_file(os.path.join(iso_root, "boot", "nano.elf"),
                     "/BOOT/NANO.ELF;1", rr_name="nano.elf")
        iso.add_file(os.path.join(iso_root, "boot", "grub", "grub.cfg"),
                     "/BOOT/GRUB/GRUB.CFG;1", rr_name="grub.cfg")
        iso.add_file(os.path.join(iso_root, "boot", "grub", "boot.img"),
                     "/BOOT/GRUB/BOOT.IMG;1", rr_name="boot.img")
        for name in os.listdir(os.path.join(iso_root, "boot", "grub", "i386-pc")):
            iso.add_file(os.path.join(iso_root, "boot", "grub", "i386-pc", name),
                         f"/BOOT/GRUB/I386PC/{name.upper()};1", rr_name=name)
        iso.add_file(os.path.join(iso_root, "boot", "grub", "efiboot.img"),
                     "/BOOT/GRUB/EFIBOOT.IMG;1", rr_name="efiboot.img")
        iso.add_eltorito("/BOOT/GRUB/BOOT.IMG;1")
        if not bios_only:
            iso.add_eltorito("/BOOT/GRUB/EFIBOOT.IMG;1", platform_id=0xef, efi=True)
        iso.write_fp(open(out, "wb"))
        iso.close()

    # Two-pass like mkiso.py: the boot info table needs the image's own ISO
    # LBA, which is only known after mastering. The patch is size-stable.
    write_iso()
    staged_boot = os.path.join(iso_root, "boot", "grub", "boot.img")
    lba, _ = mkiso.iso_extent(out, "/BOOT/GRUB/BOOT.IMG;1")
    patch_boot_info_table(staged_boot, lba)
    write_iso()
    lba2, _ = mkiso.iso_extent(out, "/BOOT/GRUB/BOOT.IMG;1")
    if lba2 != lba:
        sys.exit("GRUB ISO layout unstable, aborting")
    print(f"GRUB-ISO-OK: {out} ({os.path.getsize(out)} bytes)")


if __name__ == "__main__":
    main()

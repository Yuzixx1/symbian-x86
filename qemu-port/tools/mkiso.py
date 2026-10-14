#!/usr/bin/env python3
"""mkiso.py — assemble build/symbian-pc.iso (GRUB-free El Torito + raw kernel).

Boot path: SeaBIOS -> bootsect.bin (El Torito no-emulation boot image,
real-mode code written for this project) -> INT13 loads /BOOT/EKERN.ELF
file bytes to 0x100000 -> our _mb_entry. No GRUB, no xorriso: ISO mastering
via pycdlib (pip). Layout on ISO:
  /BOOT/EKERN.ELF   kernel ELF, loaded raw (bootsect parses e_entry + BSS)
  /BOOT/BOOTSECT.BIN  El Torito boot image (also present as a file)
Patched post-mastering (two-pass, sector values are size-stable):
  bootsect markers QLBA/QNSC -> kernel LBA + size in ISO sectors.
"""
import os
import shutil
import struct
import subprocess
import sys
import time

from pycdlib import PyCdlib

import mkfat

HERE = os.path.dirname(os.path.abspath(__file__))
QPORT = os.path.normpath(os.path.join(HERE, ".."))
BUILD = os.path.join(QPORT, "build")
ISO_DIR = os.path.join(BUILD, "iso")
ISO_OUT_DEFAULT = os.path.join(BUILD, "symbian-pc.iso")
ELF_DEFAULT = os.path.join(BUILD, "multiboot.elf")

MARK_LBA = 0x514C4241    # 'QLBA'
MARK_SECS = 0x514E5343   # 'QNSC'
MARK_HBA = 0x51484241    # 'QHBA'
MARK_BBL = 0x5142424C    # 'QBBL'
MARK_BDT = 0x51424454    # 'QBDT'


def run(*args):
    print("+", " ".join(args))
    subprocess.run(args, check=True)


def iso_extent(iso_path, iso_file):
    iso = PyCdlib()
    iso.open_fp(open(iso_path, "rb"))
    rec = iso.get_record(iso_path=iso_file)
    ext = rec.extent_location()
    size = rec.data_length
    iso.close()
    return ext, size


def patch_bootsect(path, lba, secs):
    with open(path, "r+b") as f:
        data = bytearray(f.read())
    n1 = data.find(struct.pack("<I", MARK_LBA))
    n2 = data.find(struct.pack("<I", MARK_SECS))
    n3 = data.find(struct.pack("<I", MARK_HBA))
    n4 = data.find(struct.pack("<I", MARK_BDT))
    if n1 < 0 or n2 < 0 or n3 < 0 or n4 < 0:
        sys.exit("patch markers missing in bootsect.bin")
    struct.pack_into("<I", data, n1, lba)
    struct.pack_into("<I", data, n2, secs)
    struct.pack_into("<I", data, n3, lba * 4)
    struct.pack_into("<I", data, n4, int(time.time()))
    with open(path, "wb") as f:
        f.write(data)
    print(f"BOOTSECT-PATCH: {path} kernel_lba={lba} kernel_secs={secs} hdd_lba={lba*4}")


def write_hybrid_mbr(iso_path, boot_lba, efi_lba=0, efi_secs=0):
    """USB/HDD boot: MBR in ISO sector 0 chainloads the El Torito image.

    CD boot ignores sector 0 (El Torito catalog rules); a `dd`ed USB stick
    boots as a hard disk, where the BIOS runs this MBR with DL=drive. The
    MBR loads the 2048B boot image at boot_lba to 0x7C00 and jumps there.
    A second partition entry exposes the EFI FAT image as an ESP for
    UEFI USB boot (type 0xEF).
    """
    mbr_path = os.path.join(BUILD, "mbr.bin")
    with open(mbr_path, "rb") as f:
        code = bytearray(f.read())
    if len(code) < 446:
        sys.exit("mbr.bin too small")
    n = code.find(struct.pack("<I", MARK_BBL))
    if n < 0 or n >= 446:
        sys.exit("QBBL marker missing in mbr.bin")
    # UNIT CONVERSION: ISO extents are 2048B CD sectors, but the HDD/USB EDD
    # path addresses 512B sectors (El Torito CDs use CD-sector units, which
    # is why the bootsect QLBA needs no conversion on the CD path).
    struct.pack_into("<I", code, n, boot_lba * 4)
    size = os.path.getsize(iso_path)
    sectors = size // 512
    table = bytearray(64)
    # Single active partition, type 0x96 (ISO9660), whole image, LBA-mapped.
    table[0] = 0x80
    table[1], table[2], table[3] = 0x00, 0x02, 0x00   # start CHS 0/0/1
    table[4] = 0x96
    table[5], table[6], table[7] = 0xFE, 0xFF, 0xFF   # end CHS max
    struct.pack_into("<II", table, 8, 0, sectors)
    if efi_lba and efi_secs:
        # ESP for UEFI USB-HDD boot (same FAT image as the El Torito entry).
        table[16] = 0x00
        table[17], table[18], table[19] = 0xFE, 0xFF, 0xFF
        table[20] = 0xEF
        table[21], table[22], table[23] = 0xFE, 0xFF, 0xFF
        struct.pack_into("<II", table, 24, efi_lba * 4, efi_secs * 4)
    sector0 = bytes(code[:446]) + bytes(table) + b"\x55\xaa"
    with open(iso_path, "r+b") as f:
        f.write(sector0)
    print(f"MBR-HYBRID: {iso_path} bootimg_lba={boot_lba} sectors={sectors}")


sys.path.insert(0, os.path.join(QPORT, "tools"))
import mkfat


def build_iso(iso_out):
    iso = PyCdlib()
    iso.new(interchange_level=3, sys_ident="LINUX",
            vol_ident="SYMBIAN_EKA2")
    iso.add_directory("/BOOT")
    iso.add_file(os.path.join(ISO_DIR, "BOOT", "EKERN.ELF"),
                 "/BOOT/EKERN.ELF;1")
    iso.add_file(os.path.join(ISO_DIR, "BOOT", "BOOTSECT.BIN"),
                 "/BOOT/BOOTSECT.BIN;1")
    iso.add_file(os.path.join(ISO_DIR, "BOOT", "EFIBOOT.IMG"),
                 "/BOOT/EFIBOOT.IMG;1")
    iso.add_eltorito("/BOOT/BOOTSECT.BIN;1")
    iso.add_eltorito("/BOOT/EFIBOOT.IMG;1", platform_id=0xef, efi=True)
    iso.write_fp(open(iso_out, "wb"))
    iso.close()


def main():
    elf = sys.argv[1] if len(sys.argv) > 1 else ELF_DEFAULT
    iso_out = sys.argv[2] if len(sys.argv) > 2 else ISO_OUT_DEFAULT
    if not os.path.exists(elf):
        sys.exit(f"{elf} missing: build it first")
    bootsect = os.path.join(BUILD, "bootsect.bin")
    if not os.path.exists(bootsect):
        sys.exit("bootsect.bin missing: run `make build/bootsect.bin` first")
    if not os.path.exists(os.path.join(BUILD, "mbr.bin")):
        sys.exit("mbr.bin missing: run `make build/mbr.bin` first")

    shutil.rmtree(ISO_DIR, ignore_errors=True)
    os.makedirs(os.path.join(ISO_DIR, "BOOT"), exist_ok=True)
    shutil.copyfile(elf, os.path.join(ISO_DIR, "BOOT", "EKERN.ELF"))
    # Pad the kernel image up to a whole 8-sector (16KB) multiple so every
    # EDD chunk is a full 8-sector transfer (bounds + alignment hygiene).
    ekern = os.path.join(ISO_DIR, "BOOT", "EKERN.ELF")
    with open(ekern, "ab") as f:
        size = f.tell()
        pad = (-size) % (8 * 2048)
        if pad:
            f.write(bytes(pad))
    shutil.copyfile(os.path.join(BUILD, "bootsect.bin"),
                    os.path.join(ISO_DIR, "BOOT", "BOOTSECT.BIN"))
    # UEFI FAT image (own kernel copy inside, so the loader needs no ISO9660
    # driver): El Torito EFI entry + MBR ESP partition share this file.
    loader = os.path.join(BUILD, "efi", "BOOTX64.EFI")
    if not os.path.exists(loader):
        sys.exit("BOOTX64.EFI missing: build it first")
    with open(loader, "rb") as f:
        ldr = f.read()
    with open(ekern, "rb") as f:
        kern = f.read()
    mkfat.build_fat16([(("EFI", "BOOT", "BOOTX64.EFI"), ldr),
                       (("EKA2", "EKERN.ELF"), kern)],
                      4, os.path.join(ISO_DIR, "BOOT", "EFIBOOT.IMG"),
                      partitioned=False)

    # Pass 1 to learn the on-disc LBA, then patch + rebuild (patch changes
    # bytes, not sizes, so the layout — and the LBA — is stable).
    build_iso(iso_out)
    lba, size = iso_extent(iso_out, "/BOOT/EKERN.ELF;1")
    blba, _ = iso_extent(iso_out, "/BOOT/BOOTSECT.BIN;1")
    elba, esize = iso_extent(iso_out, "/BOOT/EFIBOOT.IMG;1")
    esecs = (esize + 2047) // 2048
    secs = (size + 2047) // 2048
    patch_bootsect(os.path.join(ISO_DIR, "BOOT", "BOOTSECT.BIN"), lba, secs)
    build_iso(iso_out)
    lba2, _ = iso_extent(iso_out, "/BOOT/EKERN.ELF;1")
    if lba2 != lba:
        patch_bootsect(os.path.join(ISO_DIR, "BOOT", "BOOTSECT.BIN"), lba2, secs)
        build_iso(iso_out)
        lba3, _ = iso_extent(iso_out, "/BOOT/EKERN.ELF;1")
        if lba3 != lba2:
            sys.exit("ISO layout unstable, aborting")
    elba2, _ = iso_extent(iso_out, "/BOOT/EFIBOOT.IMG;1")
    if elba2 != elba:
        sys.exit("EFI layout unstable, aborting")
    write_hybrid_mbr(iso_out, blba, elba, esecs)
    size = os.path.getsize(iso_out)
    print(f"ISO-OK: {iso_out} ({size} bytes)")


if __name__ == "__main__":
    main()

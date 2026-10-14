#!/usr/bin/env python3
"""mkdisk.py — build test hard-disk images for the storage phase.

Usage:
  mkdisk.py blank <out.img> <size-mb>     zeroed image (format it in the shell)
  mkdisk.py seeded <out.img> <size-mb>    Linux-FAT32 image with test files
                                          (needs mtools/mformat/mcopy)

The blank image exercises the whole guest path (ide -> format -> write ->
read). The seeded image cross-validates our reader against Linux's writer.
Raw images attach directly in QEMU (-drive) and VirtualBox.
"""
import os
import shutil
import subprocess
import sys


def run(*args):
    print("+", " ".join(args))
    subprocess.run(args, check=True)


def blank(path, mb):
    with open(path, "wb") as f:
        f.truncate(mb * 1024 * 1024)
    print(f"BLANK: {path} ({mb}MB)")


def seeded(path, mb):
    for tool in ("mcopy", "mdir"):
        if not shutil.which(tool):
            sys.exit(f"{tool} missing (mtools)")
    if not shutil.which("mkfs.vfat"):
        sys.exit("mkfs.vfat missing (dosfstools)")
    if os.path.exists(path):
        os.remove(path)
    total = mb * 1024 * 1024 // 512
    with open(path, "wb") as f:
        f.truncate(total * 512)
    # One MBR partition (type 0x0C), 1MB-aligned, by hand (no root needed).
    part_lba, part_n = 2048, total - 2048
    with open(path, "r+b") as f:
        f.seek(446)
        f.write(bytes([0x00, 0x20, 0x21, 0x00, 0x0C,
                       0xFE, 0xFF, 0xFF]) +
                part_lba.to_bytes(4, "little") +
                part_n.to_bytes(4, "little"))
        f.seek(510)
        f.write(bytes([0x55, 0xAA]))
    run("mkfs.vfat", "-F", "32", "--offset", "2048", f"{path}")
    here = os.path.dirname(os.path.abspath(__file__))
    hello = os.path.join("/tmp", "ekafat_hello.txt")
    with open(hello, "w") as f:
        f.write("Hello from the Linux side.\r\nSecond line here.\r\n")
    longf = os.path.join("/tmp", "ekafat_long name.txt")
    with open(longf, "w") as f:
        f.write("Long filename content.\r\n")
    run("mcopy", "-i", f"{path}@@2048S", hello, "::HELLO.TXT")
    run("mcopy", "-i", f"{path}@@2048S", longf, "::")
    # E32 test binary for the `exec` verb (stage 3a): include when built.
    e32 = os.path.join(here, "..", "build", "hello.e32")
    if os.path.exists(e32):
        run("mcopy", "-i", f"{path}@@2048S", e32, "::HELLO.E32")
    run("mdir", "-i", f"{path}@@2048S", "::")
    print(f"SEEDED: {path} ({mb}MB)")


if __name__ == "__main__":
    if len(sys.argv) != 4 or sys.argv[1] not in ("blank", "seeded"):
        sys.exit("usage: mkdisk.py [blank|seeded] <out.img> <size-mb>")
    mode, path, mb = sys.argv[1], sys.argv[2], int(sys.argv[3])
    if mb < 64:
        sys.exit("FAT32 needs >=64MB here")
    (blank if mode == "blank" else seeded)(path, mb)

#!/usr/bin/env python3
"""A failed COPY /Y must leave the pre-existing FAT32 file readable."""
import pathlib
import struct
import subprocess
import sys

from test_cli import DISK, LOG, ROOT, wait_for, run_command

OLD = pathlib.Path(ROOT) / "cli-old.txt"
NEW = pathlib.Path(ROOT) / "cli-new.txt"


def exhaust_clusters():
    # Mark free FAT entries occupied in BOTH copies, leaving already-allocated
    # file chains alone. This simulates a full disk without editing the kernel.
    with open(DISK, "r+b") as disk:
        disk.seek(2048 * 512)
        bpb = disk.read(512)
        reserved = struct.unpack_from("<H", bpb, 14)[0]
        fats = bpb[16]
        size = struct.unpack_from("<I", bpb, 36)[0]
        spc = bpb[13]
        total = struct.unpack_from("<I", bpb, 32)[0]
        clusters = (total - reserved - fats * size) // spc
        fat_start = (2048 + reserved) * 512
        for sector in range(size):
            disk.seek(fat_start + sector * 512)
            block = bytearray(disk.read(512))
            changed = False
            for j in range(128):
                index = sector * 128 + j
                if index < 2 or index >= clusters + 2:
                    continue
                off = 4 * j
                if struct.unpack_from("<I", block, off)[0] & 0x0FFFFFFF == 0:
                    struct.pack_into("<I", block, off, 0x0FFFFFFF)
                    changed = True
            if changed:
                for copy in range(fats):
                    disk.seek(fat_start + (copy * size + sector) * 512)
                    disk.write(block)


def main():
    subprocess.run([sys.executable, "tools/mkdisk.py", "seeded", DISK, "64"],
                   check=True, stdout=subprocess.DEVNULL)
    OLD.write_bytes(b"Old destination must survive!\r\n")
    NEW.write_bytes(b"A different replacement payload\r\n")
    for source, name in ((OLD, "DEST.TXT"), (NEW, "SOURCE.TXT")):
        subprocess.run(["mcopy", "-o", "-i", DISK + "@@2048S", str(source),
                        "::" + name], check=True)
    exhaust_clusters()
    with open(LOG, "wb") as log:
        vm = subprocess.Popen(
            ["qemu-system-i386", "-M", "q35", "-m", "128", "-display", "none",
             "-serial", "stdio", "-kernel", "build/kernfull.elf",
             "-drive", f"file={DISK},format=raw,if=none,id=d0",
             "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
             "-no-reboot"], stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log, b"EKA2> ", timeout=30)
            vm.stdin.write(b"mount\n")
            vm.stdin.flush()
            pos = wait_for(log, b"C:/> ", pos)
            pos = run_command(vm, log, pos, "COPY /Y SOURCE.TXT DEST.TXT", b"copy: write disk full")
            run_command(vm, log, pos, "TYPE DEST.TXT", b"Old destination must survive!")
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    actual = subprocess.check_output(["mtype", "-i", DISK + "@@2048S", "::DEST.TXT"])
    assert actual == OLD.read_bytes(), (actual, OLD.read_bytes())
    print("OK old destination survives disk-full overwrite")


if __name__ == "__main__":
    main()

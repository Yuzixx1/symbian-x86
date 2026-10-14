#!/usr/bin/env python3
"""QEMU integration test for E32 initialized-data relocation.

Creates its own FAT32 disk under /tmp/opencode, runs the DREL.E32 test
process, then reads the process BSS through the QEMU monitor. The
process writes the success word only after dereferencing data[8] -> data[0].
"""
import os
import re
import socket
import subprocess
import sys
import time

from mke32 import build_datarel_e32
from e32read import validate_relocs

ROOT = "/tmp/opencode"
IMAGE = ROOT + "/datarel-test.img"
PROGRAM = ROOT + "/datarel-test.e32"
MON = ROOT + "/datarel-test.sock"
LOG = ROOT + "/datarel-test.log"


def wait_for(log, marker, seconds):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        log.flush()
        with open(LOG, "rb") as source:
            if marker in source.read():
                return True
        time.sleep(0.2)
    return False


def main():
    blob = build_datarel_e32(0x10000001, 0x10000002, 0x10000002)
    with open(PROGRAM, "wb") as out:
        out.write(blob)
    if validate_relocs(blob, len(blob) - 20, 16):
        raise RuntimeError("generated data relocation failed validation")
    subprocess.run([sys.executable, "tools/mkdisk.py", "seeded", IMAGE, "64"],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-o", "-i", IMAGE + "@@2048S", PROGRAM, "::DREL.E32"],
                   check=True)
    try:
        os.unlink(MON)
    except FileNotFoundError:
        pass
    with open(LOG, "wb") as log:
        vm = subprocess.Popen(
            ["qemu-system-i386", "-M", "q35", "-m", "128", "-display", "none",
             "-serial", "stdio", "-kernel", "build/kernfull.elf",
             "-drive", f"file={IMAGE},format=raw,if=none,id=d0",
             "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
             "-monitor", f"unix:{MON},server=on,wait=off", "-no-reboot"],
            stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT)
        try:
            if not wait_for(log, b"EKA2> ", 25):
                raise RuntimeError("shell did not boot")
            vm.stdin.write(b"mount\n")
            vm.stdin.flush()
            if not wait_for(log, b"mounted C:", 45):
                raise RuntimeError("FAT mount did not finish")
            if not wait_for(log, b"C:/> ", 60):
                raise RuntimeError("mount-all did not return to the shell")
            vm.stdin.write(b"exec C:DREL.E32\n")
            vm.stdin.flush()
            if not wait_for(log, b"DREL.E32 started", 30):
                raise RuntimeError("E32 process did not start")
            time.sleep(2)
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as monitor:
                monitor.connect(MON)
                monitor.settimeout(2)
                try:
                    monitor.recv(4096)
                except socket.timeout:
                    pass
                monitor.sendall(b"xp /1wx 0x2200010\n")
                time.sleep(0.5)
                result = monitor.recv(4096).decode("latin1")
            vm.stdin.write(b"ps\n")
            vm.stdin.flush()
            time.sleep(1)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    with open(LOG, "rb") as log:
        output = log.read().decode("latin1")
    print("exec started:", "DREL.E32 started (user process)" in output)
    values = re.findall(r"0000000002200010:\s*(0x[0-9a-fA-F]+)", result)
    print("user BSS result:", values[-1] if values else "not returned")
    faults = "FAULT" in output or re.search(r"\bExc [0-9a-fA-F]{2}\b", output)
    print("fault:", bool(faults))
    if ("DREL.E32 started (user process)" not in output or faults or
            not values or values[-1].lower() != "0x0a200a52"):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

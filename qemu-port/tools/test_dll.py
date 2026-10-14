#!/usr/bin/env python3
"""QEMU integration test for real DLL-based imports.

Builds EUSER.DLL (export table) + IMP.E32 (EUSER imports), runs the EXE
from FAT32, and checks the loader resolved imports from the DLL file.
"""
import re
import socket
import subprocess
import sys
import time

import test_cli
from mke32 import build_euser_dll, build_imp_e32

ROOT = "/tmp/opencode"
DISK = ROOT + "/dll-test.img"
LOG = ROOT + "/dll-test.log"

test_cli.DISK = DISK
test_cli.LOG = LOG


def main():
    dll = build_euser_dll()
    exe = build_imp_e32(0x10000001, 0x10000002, 0x10000002)
    with open(ROOT + "/dll-euser.dll", "wb") as f:
        f.write(dll)
    with open(ROOT + "/dll-imp.e32", "wb") as f:
        f.write(exe)
    subprocess.run([sys.executable, "tools/mkdisk.py", "seeded", DISK, "64"],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-o", "-i", DISK + "@@2048S",
                    ROOT + "/dll-euser.dll", "::EUSER.DLL"], check=True)
    subprocess.run(["mcopy", "-o", "-i", DISK + "@@2048S",
                    ROOT + "/dll-imp.e32", "::IMP.E32"], check=True)
    with open(LOG, "wb") as log:
        vm = subprocess.Popen(
            ["qemu-system-i386", "-M", "q35", "-m", "128", "-display", "none",
             "-serial", "stdio", "-kernel", "build/kernfull.elf",
             "-drive", f"file={DISK},format=raw,if=none,id=d0",
             "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
             "-monitor", f"unix:{ROOT}/dll-test.sock,server=on,wait=off",
             "-no-reboot"], stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = test_cli.wait_for(log, b"EKA2> ", timeout=30)
            vm.stdin.write(b"mount\n")
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b"C:/> ", pos)
            pos = test_cli.run_command(vm, log, pos, "exec C:IMP.E32",
                                       b"IMP.E32 started (user process)", b"C:/> ")
            # Check the actual PutC side effect, rather than just binding logs.
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as monitor:
                monitor.settimeout(2)
                monitor.connect(ROOT + "/dll-test.sock")
                monitor.recv(4096)
                monitor.sendall(b"xp /2000hx 0xb8000\n")
                result = ""
                while "00000000000b8000:" not in result or not result.endswith("(qemu) "):
                    result += monitor.recv(4096).decode("latin1")
                # Shell logging can scroll the cell after the user exits.
                assert "0x0e50" in result, "PutC's yellow P absent from VGA"
                print("DLL PutC wrote P to VGA")
            log.flush()
            with open(LOG, "rb") as src:
                out = src.read().decode("latin1")
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    print("dll resident:", "EUSER.DLL resident" in out)
    print("dll bound:", "imports bound dll" in out)
    print("fault:", bool(re.search(r"\bExc [0-9a-fA-F]{2}\b", out)))
    if "imports bound dll" not in out:
        return 1
    if re.search(r"\bExc [0-9a-fA-F]{2}\b", out):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

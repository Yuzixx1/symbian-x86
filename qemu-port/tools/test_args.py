#!/usr/bin/env python3
"""QEMU integration test for process command lines.

Builds EUSER.DLL (Exit/PutC/CmdLen) + ARGS.E32, runs it with and without
arguments, and checks:
  - the user thread's CmdLen readback (BSS slot via monitor),
  - the `args <id>` verb showing the genuine stored command line.
"""
import re
import subprocess
import sys

import test_cli
from mke32 import build_euser_dll, build_args_e32

ROOT = "/tmp/opencode"
DISK = ROOT + "/args-test.img"
LOG = ROOT + "/args-test.log"

test_cli.DISK = DISK
test_cli.LOG = LOG

ARGS = "hello world"


def read_bss(vm, log, data_run):
    import socket
    import time
    addr = data_run + 16
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as monitor:
        monitor.connect(ROOT + "/args-test.sock")
        monitor.settimeout(2)
        try:
            monitor.recv(4096)
        except socket.timeout:
            pass
        monitor.sendall(f"xp /1wx 0x{addr:x}\n".encode())
        time.sleep(0.5)
        result = monitor.recv(4096).decode("latin1")
    values = re.findall(rf"{addr:013x}:\s*(0x[0-9a-fA-F]+)", result)
    assert values, f"BSS not returned: {result!r}"
    return int(values[-1], 16)


def main():
    with open(ROOT + "/args-euser.dll", "wb") as f:
        f.write(build_euser_dll())
    with open(ROOT + "/args-prog.e32", "wb") as f:
        f.write(build_args_e32(0x10000001, 0x10000002, 0x10000002))
    subprocess.run([sys.executable, "tools/mkdisk.py", "seeded", DISK, "64"],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["mcopy", "-o", "-i", DISK + "@@2048S",
                    ROOT + "/args-euser.dll", "::EUSER.DLL"], check=True)
    subprocess.run(["mcopy", "-o", "-i", DISK + "@@2048S",
                    ROOT + "/args-prog.e32", "::ARGS.E32"], check=True)
    try:
        import os
        os.unlink(ROOT + "/args-test.sock")
    except FileNotFoundError:
        pass
    with open(LOG, "wb") as log:
        vm = subprocess.Popen(
            ["qemu-system-i386", "-M", "q35", "-m", "128", "-display", "none",
             "-serial", "stdio", "-kernel", "build/kernfull.elf",
             "-drive", f"file={DISK},format=raw,if=none,id=d0",
             "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
             "-monitor", f"unix:{ROOT}/args-test.sock,server=on,wait=off",
             "-no-reboot"], stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = test_cli.wait_for(log, b"EKA2> ", timeout=30)
            vm.stdin.write(b"mount\n")
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b"C:/> ", pos)
            pos = test_cli.run_command(vm, log, pos, f"exec C:ARGS.E32 {ARGS}",
                                       b"ARGS.E32 started (user process)", b"C:/> ")
            log.flush()
            with open(LOG, "rb") as src:
                out = src.read().decode("latin1")
            runs = re.findall(r"e32run: bss preresume [0-9a-fA-F]+ run=([0-9a-fA-F]+)", out)
            assert runs, "no data-run address logged"
            value = read_bss(vm, log, int(runs[-1], 16))
            print(f"CmdLen readback with args: {value}")
            pos = test_cli.run_command(vm, log, pos, "ps", b"ARGS", b"C:/> ")
            with open(LOG, "rb") as src:
                out2 = src.read().decode("latin1")
            ids = re.findall(r"ARGS\[10000002\]0001 id=(\d+)", out2)
            assert ids, "ARGS process not in ps"
            pos = test_cli.run_command(vm, log, pos, f"args {ids[-1]}",
                                       f"len={len(ARGS)} \r\n{ARGS}".encode(), b"C:/> ")
            print(f"args verb: id={ids[-1]} shows {ARGS!r}")
            # Same binary without arguments: CmdLen must read back 0.
            pos = test_cli.run_command(vm, log, pos, "exec C:ARGS.E32",
                                       b"ARGS.E32 started (user process)", b"C:/> ")
            log.flush()
            with open(LOG, "rb") as src:
                out3 = src.read().decode("latin1")
            runs3 = re.findall(r"e32run: bss preresume [0-9a-fA-F]+ run=([0-9a-fA-F]+)", out3)
            assert runs3, "no data-run address logged for no-args run"
            value0 = read_bss(vm, log, int(runs3[-1], 16))
            print(f"CmdLen readback without args: {value0}")
            assert value0 == 0, f"expected 0, got {value0}"
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    if value != len(ARGS):
        print(f"NOTE: CmdLen returned {value}, shell sent {len(ARGS)} chars")
        return 1
    if re.search(r"\bExc [0-9a-fA-F]{2}\b", out3):
        print("fault in log")
        return 1
    print("OK command line round-trips shell -> process -> user thread")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Exercise DOS-style shell commands on a disposable QEMU FAT32 disk."""
import os
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK = ROOT + "/cli-test.img"
LOG = ROOT + "/cli-test.log"


def wait_for(log, marker, start=0, timeout=60):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        log.flush()
        with open(LOG, "rb") as source:
            source.seek(start)
            data = source.read()
            if marker in data:
                return start + data.index(marker) + len(marker)
        time.sleep(0.2)
    raise RuntimeError("shell did not print " + repr(marker) + " (see " + LOG + ")")


def run_command(vm, log, pos, command, expected, prompt=b"C:/> "):
    vm.stdin.write(command.encode() + b"\n")
    vm.stdin.flush()
    end = wait_for(log, prompt, pos, 30)
    log.flush()
    with open(LOG, "rb") as source:
        source.seek(pos)
        response = source.read(end - pos)
    if expected not in response:
        raise AssertionError(f"{command}: missing {expected!r} in {response!r}")
    print("OK", command)
    return end


def main():
    subprocess.run([sys.executable, "tools/mkdisk.py", "seeded", DISK, "64"],
                   check=True, stdout=subprocess.DEVNULL)
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
            cases = [
                ("HELP", b"HELP command for details", b"C:/> "),
                ("HELP COPY", b"COPY [/Y]", b"C:/> "),
                ("COPY /?", b"COPY [/Y]", b"C:/> "),
                ("FORMAT /?", b"FORMAT device yes", b"C:/> "),
                ("DATE", b"20", b"C:/> "),
                ("DATE 2027-02-28 12:34:56", b"2027-02-28 12:34:5", b"C:/> "),
                ("DATE", b"2027-02-28", b"C:/> "),
                ("DATE 2027-02-30 00:00:00", b"use DATE", b"C:/> "),
                ("DATE 1999-01-01 00:00:00", b"use DATE", b"C:/> "),
                ("DATE bogus", b"DATE [YYYY-MM-DD", b"C:/> "),
                ("DIR /W", b"HELLO.TXT", b"C:/> "),
                ("DIR *.TXT", b"file(s)", b"C:/> "),
                ("DIR C:\\*.E32", b"HELLO.E32", b"C:/> "),
                ("DIR C:\\NOPE*.ZZZ", b"dir: not found", b"C:/> "),
                ("CoPy HELLO.TXT TMP.TXT", b"copy: 47 bytes", b"C:/> "),
                ("copy HELLO.TXT TMP.TXT", b"destination already exists", b"C:/> "),
                ("COPY /Y HELLO.TXT TMP.TXT", b"copy: 47 bytes", b"C:/> "),
                ("REN TMP.TXT NEW.TXT", b"ren: done", b"C:/> "),
                ("TYPE NEW.TXT", b"Hello from the Linux side.", b"C:/> "),
                ("md TESTDIR", b"created", b"C:/> "),
                ("md TESTDIR\\INNER", b"created", b"C:/> "),
                ("CD TESTDIR\\MISSING", b"cd: not found", b"C:/> "),
                ("WRITE C:\\TESTDIR\\INNER\\NOTE.TXT nested content", b"wrote", b"C:/> "),
                ("TYPE TESTDIR\\INNER\\NOTE.TXT", b"nested content", b"C:/> "),
                ("COPY TESTDIR\\INNER\\NOTE.TXT C:\\TESTDIR\\COPY.TXT", b"copy: 16 bytes", b"C:/> "),
                ("REN TESTDIR\\COPY.TXT C:\\TESTDIR\\MOVED.TXT", b"ren: done", b"C:/> "),
                ("TYPE C:\\TESTDIR\\MOVED.TXT", b"nested content", b"C:/> "),
                ("REN TESTDIR\\MOVED.TXT TESTDIR\\INNER\\OTHER.TXT", b"ren: not supported", b"C:/> "),
                ("DEL TESTDIR\\INNER\\NOTE.TXT", b"deleted", b"C:/> "),
                ("TYPE TESTDIR\\INNER\\NOTE.TXT", b"type: not found", b"C:/> "),
                ("DEL TESTDIR\\MOVED.TXT", b"deleted", b"C:/> "),
                ("RD TESTDIR\\INNER", b"deleted", b"C:/> "),
                ("CD C:\\TESTDIR", b"", b"C:/TESTDIR/> "),
                ("WRITE C:\\TESTDIR\\MULTI.TXT", b"type lines, lone . ends", b"C:/TESTDIR/> "),
                ("hello from multiline", b"", b"C:/TESTDIR/> "),
                (".", b"wrote 22 bytes", b"C:/TESTDIR/> "),
                ("TYPE C:\\TESTDIR\\MULTI.TXT", b"hello from multiline", b"C:/TESTDIR/> "),
                ("DEL C:\\TESTDIR\\MULTI.TXT", b"deleted", b"C:/TESTDIR/> "),
                ("CD C:\\", b"", b"C:/> "),
                ("DIR /W TESTDIR", b"C:/TESTDIR/", b"C:/> "),
                ("DIR C:\\TESTDIR", b"C:/TESTDIR/", b"C:/> "),
                ("DIR C:\\BADDIR", b"dir: not found", b"C:/> "),
                ("DIR /W", b"C:/", b"C:/> "),
                ("cd TESTDIR", b"", b"C:/TESTDIR/> "),
                ("DIR C:\\", b"C:/", b"C:/TESTDIR/> "),
                ("DIR", b"C:/TESTDIR/", b"C:/TESTDIR/> "),
                ("cd ..", b"", b"C:/> "),
                ("rd TESTDIR", b"deleted", b"C:/> "),
                ("ERASE NEW.TXT", b"deleted", b"C:/> "),
                ("TYPE NEW.TXT", b"type: not found", b"C:/> "),
            ]
            for command, expected, prompt in cases:
                pos = run_command(vm, log, pos, command, expected, prompt)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    # Check that REN moved the entry, not merely the shell's display.
    listing = subprocess.check_output(["mdir", "-i", DISK + "@@2048S", "::"])
    assert b"NEW      TXT" not in listing and b"TMP      TXT" not in listing


if __name__ == "__main__":
    main()

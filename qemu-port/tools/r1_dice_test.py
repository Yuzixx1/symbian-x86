#!/usr/bin/env python3
"""r1_dice_test.py -- QEMU test for the end-user DICE ROLLER E32 program.

Stages build/user/DICE.E32 (a PE2 image importing EUSER.DLL ordinal 915) plus
EUSER.DLL on a FAT32 disk, boots the R1 kernel, launches `dice` from the serial
shell, drives the `selftest` and `quit` commands through prompt-synchronised
console input, and asserts the DC: markers, EUSER ord 915 binding and the
absence of any guest exception or kernel panic.  Exits 0 only if every check
passes.

Usage: python3 tools/r1_dice_test.py [--no-rebuild-disk] [--log LOG]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISK = "/tmp/opencode/r1_dice.img"
FS = "/tmp/opencode/r1_dice_fs.img"
LOG = "/tmp/opencode/r1_dice.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/DICE.E32")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def build_disk():
    for path in (DISK, FS):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "DICE.E32"), (DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", FS, src, f"::{name}", check=True)
    with open(FS, "rb") as f:
        data = f.read()
    if len(data) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    with open(DISK, "r+b") as f:
        f.seek(OFFSET)
        f.write(data)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + LBA.to_bytes(4, "little") + SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK}@@{OFFSET}", "::", check=True,
             capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for wanted in ("dicee32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: DICE.E32 and EUSER.DLL staged")


def wait_for(log_path, marker, start=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        if marker in data:
            return start + data.index(marker) + len(marker)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {marker!r} (see {log_path})")


def run_qemu(log_path):
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio",
        "-kernel", KERNEL,
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, driving DICE via serial console ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            # FAT is mounted at boot by Asic::Init3; the shell prompt is C:/>.
            pos = wait_for(log_path, b"C:/> ", timeout=60)

            def send(text, expect, timeout=30):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("dice", b"DC> ", 60)        # launch the dice roller
            send("selftest", b"DC> ", 30)    # roll/flip/pick + selftest OK
            send("quit", b"DC: DONE", 30)
            time.sleep(1.0)   # let the guest flush its final marker and exit
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text):
    ok = True

    def report(label, good):
        nonlocal ok
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {label}")

    for marker in ("DC: DICE ROLLER",
                   "DC> ",
                   "DC: selftest OK",
                   "DC: DONE"):
        report(marker, marker in text)

    report("2d6 roll marker", bool(re.search(
        r"DC: 2d6 -> \[\d+(,\d+){1}\] sum=\d+", text)))
    report("flip marker", bool(re.search(
        r"DC: flip -> (heads|tails)", text)))
    report("pick marker", bool(re.search(
        r"DC: pick -> [A-Za-z0-9_]+", text)))

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    report("EUSER ord 915 bound", any("ord 915" in ln for ln in binds))

    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    report("no Exc/PanicKernExec/FAULT", not exc)
    for ln in exc:
        print("     ", ln[:200])
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 DICE ROLLER E32 QEMU test")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(t):
            sys.exit(f"missing tool {t}")
    if not (args.no_rebuild_disk and os.path.exists(DISK)):
        build_disk()
    run_qemu(args.log)
    with open(args.log, errors="replace") as f:
        text = f.read()
    print(f"serial log: {args.log}")
    print("\n== marker results")
    ok = check(text)
    if not ok:
        print("\n== relevant log lines:")
        for line in text.splitlines():
            if "DC:" in line or "dice" in line.lower() or "e32run" in line:
                print("   ", line[:220])
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_zmachine_test.py -- QEMU test for the Z-MACHINE (ZM) E32.

Stages build/user/ZMACHINE.E32 (a PE2 image importing EUSER.DLL ordinal 915),
EUSER.DLL and a genuine Infocom Z-code story (Mini-Zork I, a freely
distributable v3 demo from the IF Archive) on FAT, boots the R1 kernel,
launches "zmachine" from the serial shell and drives real game commands
(look / north / get lamp / quit / y) through the PortCon lease.  It asserts
the ZM: markers, actual Infocom game text and a clean serial log.

Usage: python3 tools/r1_zmachine_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK_IMG = ROOT + "/r1_zmachine.img"
FS_IMG = ROOT + "/r1_zmachine_fs.img"
LOG_DEFAULT = ROOT + "/r1_zmachine.log"

DISK_MB = 32
PART_LBA, PART_SECTORS = 2048, 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
ZM_E32 = REPO + "/build/user/ZMACHINE.E32"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
KERNEL = REPO + "/build/kernfull.elf"
STORY = REPO + "/ports/zmachine/minizork.z3"   # Mini-Zork I, Infocom 1988

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def build_disk():
    for p in (DISK_IMG, FS_IMG):
        if os.path.exists(p):
            os.remove(p)
    # mkfs.fat needs real blocks on this tmpfs; a sparse truncate() image
    # makes its reserved-sector write fail.
    for path, size in ((DISK_IMG, DISK_MB * 1024 * 1024), (FS_IMG, FS_SIZE)):
        with open(path, "wb") as f:
            if hasattr(os, "posix_fallocate"):
                os.posix_fallocate(f.fileno(), 0, size)
            else:
                f.truncate(size)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)

    for src, name in ((ZM_E32, "ZMACHINE.E32"),
                      (EUSER_DLL, "EUSER.DLL"),
                      (STORY, "MINIZORK.Z3")):
        if not os.path.exists(src):
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)

    with open(FS_IMG, "rb") as f:
        fs = f.read()
    if len(fs) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    with open(DISK_IMG, "r+b") as f:
        f.seek(OFFSET)
        f.write(fs)
    entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
             + PART_LBA.to_bytes(4, "little")
             + PART_SECTORS.to_bytes(4, "little"))
    mbr = bytearray(512)
    mbr[446:462] = entry
    mbr[510] = 0x55
    mbr[511] = 0xAA
    with open(DISK_IMG, "r+b") as f:
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK_IMG}@@{OFFSET}", "::",
             check=True, capture_output=True, text=True)
    print(out.stdout)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for want in ("zmachinee32", "euserdll", "minizorkz3"):
        if want not in norm:
            sys.exit(f"disk missing {want}: {out.stdout}")
    print("mdir OK: ZMACHINE.E32, EUSER.DLL and MINIZORK.Z3 staged")


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
        "-drive", f"file={DISK_IMG},format=raw,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, driving Mini-Zork I via serial console ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, b"C:/> ", timeout=60)

            def send(text, expect, timeout=60):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("zmachine", b"ZM: Z-MACHINE READY", 60)
            pos = wait_for(log_path, b"ZM: >", pos, 60)  # story reaches prompt
            send("look", b"ZM: >", 60)
            send("north", b"ZM: >", 60)
            send("get lamp", b"ZM: >", 60)
            send("quit", b"ZM: >", 60)
            send("y", b"ZM: DONE", 60)
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text):
    markers = [
        "ZM: Z-MACHINE READY",
        "ZM: game MINIZORK.Z3 version=3",
        "ZM: MINI-ZORK I: The Great Underground Empire",
        "ZM: Release 34 / Serial number 871124",
        "ZM: West of House",
        "ZM: There is a small mailbox here.",
        "ZM: North of House",
        "ZM: You are facing the north side of a white house.",
        "ZM: You can't see any lamp here!",
        "ZM: Your score is 0 (of 350 points)",
        "ZM: DONE",
    ]
    ok = True
    for m in markers:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {m}")
    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok &= bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT")
    for ln in exc:
        print("     ", ln[:200])
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 Z-MACHINE E32 QEMU test")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(t):
            sys.exit(f"missing tool {t}")
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk()
    run_qemu(args.log)
    with open(args.log, errors="replace") as f:
        text = f.read()
    print(f"serial log: {args.log}")
    print("\n== marker results")
    ok = check(text)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

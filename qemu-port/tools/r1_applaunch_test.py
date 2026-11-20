#!/usr/bin/env python3
"""r1_applaunch_test.py -- QEMU test for the APPLICATION LAUNCHER / BOOT MENU E32.

Stages build/user/APPLAUNCH.E32 (a PE2 image importing EUSER.DLL ordinal 915)
plus EUSER.DLL and two launchable apps (CALC.E32, TEXTEDIT.E32) into a FAT32
disk, boots the R1 kernel, launches "applaunch" from the serial shell, drives
list / numeric selection / `run` / quit through prompt-synchronised console
input, and asserts the APP: markers with no guest exception/panic.

Usage: python3 r1_applaunch_test.py [--no-rebuild-disk] [--log LOG]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK_IMG = ROOT + "/r1_applaunch.img"
FS_IMG = ROOT + "/r1_applaunch_fs.img"
LOG_DEFAULT = ROOT + "/r1_applaunch.log"

DISK_MB = 32
PART_LBA, PART_SECTORS = 2048, 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
APPLAUNCH_E32 = REPO + "/build/user/APPLAUNCH.E32"
CALC_E32 = REPO + "/build/user/CALC.E32"
TEXTEDIT_E32 = REPO + "/build/user/TEXTEDIT.E32"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
KERNEL = REPO + "/build/kernfull.elf"

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def build_disk():
    for p in (DISK_IMG, FS_IMG):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK_IMG, "wb") as f:
        f.truncate(DISK_MB * 1024 * 1024)
    with open(FS_IMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)

    for src, name in ((APPLAUNCH_E32, "APPLAUNCH.E32"),
                      (CALC_E32, "CALC.E32"),
                      (TEXTEDIT_E32, "TEXTEDIT.E32"),
                      (EUSER_DLL, "EUSER.DLL")):
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
    for want in ("applaunch", "calc", "textedit", "euserdll"):
        if want not in norm:
            sys.exit(f"disk missing {want}: {out.stdout}")
    print("mdir OK: APPLAUNCH.E32, EUSER.DLL, CALC.E32, TEXTEDIT.E32 staged")


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
    print("== booting QEMU, driving APPLICATION LAUNCHER via serial console ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, b"C:/> ", timeout=60)

            def send(text, expect, timeout=30):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("applaunch", b"APP> ", 60)   # launch + menu
            send("list", b"APP> ")            # list again
            send("1", b"APP> ")               # numeric selection -> TEXTEDIT
            send("run calc", b"APP> ")        # named selection -> CALC
            send("quit", b"APP: DONE")
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
        "APP: === EKA2 PC OS APPLICATION LAUNCHER ===",
        "1. TEXTEDIT",
        "9. NETBRIDGE",
        "APP: launching TEXTEDIT.E32 r=",
        "APP: launching CALC.E32 r=",
        "APP: DONE",
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

    # Report whether exec 268 actually launched an image, or the fallback ran.
    pids = [int(x) for x in re.findall(r"APP: launching \S+ r=(\d+)", text)]
    launched = any(p > 0 for p in pids)
    fallback = "APP: selected " in text and "(use shell:" in text
    print(f"  [INFO] exec 268 launch worked: {launched}; fallback used: {fallback}")
    if not (launched or fallback):
        print("  [FAIL] neither exec 268 launch nor fallback reported")
        ok = False
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 APPLICATION LAUNCHER E32 QEMU test")
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

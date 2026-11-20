#!/usr/bin/env python3
"""r1_wc_test.py -- QEMU test for the WORD COUNT (WC) E32.

Stages build/user/WC.E32 (a PE2 image importing EUSER.DLL ordinal 915),
EUSER.DLL and a small FAT text file (NOTES.TXT), boots the R1 kernel, launches
"wc" from the serial shell, drives the built-in selftest and a
"wc NOTES.TXT" count via prompt-synchronised console input, then asserts the
WC: markers and a clean log.  Exits 0 only if all checks pass and no guest
exception/panic appears.

Usage: python3 tools/r1_wc_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK_IMG = ROOT + "/r1_wc.img"
FS_IMG = ROOT + "/r1_wc_fs.img"
LOG_DEFAULT = ROOT + "/r1_wc.log"

DISK_MB = 32
PART_LBA, PART_SECTORS = 2048, 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
WC_E32 = REPO + "/build/user/WC.E32"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
KERNEL = REPO + "/build/kernfull.elf"

NOTES_TXT = ROOT + "/r1_wc_NOTES.TXT"
README_TXT = ROOT + "/r1_wc_README.TXT"
# 33 bytes: "alpha beta\r\n" (12) + "gamma delta epsilon\r\n" (21).
# -> 2 lines, 5 whitespace-delimited words, 33 characters.
NOTES_CONTENT = b"alpha beta\r\ngamma delta epsilon\r\n"
NOTES_MARKER = "WC: NOTES.TXT: 2 lines 5 words 33 chars"

# Same embedded string as user/wc.c -> 2 lines, 4 words, 23 chars.
BUILTIN_MARKER = "WC: BUILTIN: 2 lines 4 words 23 chars"

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

    with open(NOTES_TXT, "wb") as f:
        f.write(NOTES_CONTENT)
    with open(README_TXT, "wb") as f:
        f.write(b"wc test disk\r\n")
    for src, name in ((WC_E32, "WC.E32"),
                      (EUSER_DLL, "EUSER.DLL"),
                      (NOTES_TXT, "NOTES.TXT"),
                      (README_TXT, "README.TXT")):
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
    for want in ("wc", "euserdll", "notestxt", "readmetxt"):
        if want not in norm:
            sys.exit(f"disk missing {want}: {out.stdout}")
    print("mdir OK: WC.E32, EUSER.DLL and test files staged")


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
    print("== booting QEMU, driving WORD COUNT via serial console ...")
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

            send("wc", b"WC> ", 60)             # launch + startup selftest
            send("selftest", b"WC> ")           # embedded-string count again
            send("wc NOTES.TXT", b"WC> ")       # FAT file count
            send("quit", b"WC: DONE")
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
        "WC: WORD COUNT",
        "WC: selftest OK",
        BUILTIN_MARKER,
        NOTES_MARKER,
        "WC: DONE",
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
    ap = argparse.ArgumentParser(description="R1 WORD COUNT E32 QEMU test")
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

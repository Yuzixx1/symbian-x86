#!/usr/bin/env python3
"""r1_hexdump_test.py -- QEMU test for the HEX DUMP E32.

Stages build/user/HEXDUMP.E32 (a PE2 image importing EUSER.DLL ordinal 915),
EUSER.DLL and a small FAT text file (NOTES.TXT), boots the R1 kernel, launches
"hexdump" from the serial shell, drives the built-in selftest and a
"hex NOTES.TXT" dump via prompt-synchronised console input, then asserts the
HD: markers (including the canonical offset/hex/ASCII lines) and a clean log.
Exits 0 only if all checks pass and no guest exception/panic appears.

Usage: python3 tools/r1_hexdump_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK_IMG = ROOT + "/r1_hexdump.img"
FS_IMG = ROOT + "/r1_hexdump_fs.img"
LOG_DEFAULT = ROOT + "/r1_hexdump.log"

DISK_MB = 32
PART_LBA, PART_SECTORS = 2048, 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
HEXDUMP_E32 = REPO + "/build/user/HEXDUMP.E32"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
KERNEL = REPO + "/build/kernfull.elf"

NOTES_TXT = ROOT + "/r1_hexdump_NOTES.TXT"
README_TXT = ROOT + "/r1_hexdump_README.TXT"
# 44 bytes -> three dump lines (16 + 16 + 12).
NOTES_CONTENT = b"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdef\r\n"
NOTES_BYTES = len(NOTES_CONTENT)

# Same embedded payload as user/hexdump.c (23 bytes -> two dump lines).
BUILTIN = (b"Hello, world!\r\n\x00\x01\x02\x1f\x7f\x80\xff\x41")
BUILTIN_BYTES = len(BUILTIN)

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def dump_lines(data):
    """Render the exact canonical line format emitted by the E32."""
    lines = []
    for off in range(0, len(data), 16):
        cols = []
        for i in range(16):
            if off + i < len(data):
                cols.append("%02X " % data[off + i])
            else:
                cols.append("   ")
        hexpart = "".join(cols)
        asc = ""
        for i in range(16):
            if off + i < len(data):
                b = data[off + i]
                asc += chr(b) if 0x20 <= b < 0x7f else "."
            else:
                asc += " "
        lines.append("HD: %08X  %s %s" % (off, hexpart, asc))
    return lines


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
        f.write(b"hexdump test disk\r\n")
    for src, name in ((HEXDUMP_E32, "HEXDUMP.E32"),
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
    for want in ("hexdump", "euserdll", "notestxt", "readmetxt"):
        if want not in norm:
            sys.exit(f"disk missing {want}: {out.stdout}")
    print("mdir OK: HEXDUMP.E32, EUSER.DLL and test files staged")


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
    print("== booting QEMU, driving HEX DUMP via serial console ...")
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

            send("hexdump", b"HD> ", 60)      # launch + startup selftest dump
            send("selftest", b"HD> ")         # embedded-array dump again
            send("hex NOTES.TXT", b"HD> ")    # FAT file dump
            send("quit", b"HD: DONE")
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
        "HD: HEXDUMP START",
        "HD: selftest OK",
    ]
    # Exact canonical lines from the built-in and the FAT file.
    markers += dump_lines(BUILTIN)
    markers += ["HD: %d bytes" % BUILTIN_BYTES]
    markers += dump_lines(NOTES_CONTENT)
    markers += ["HD: %d bytes" % NOTES_BYTES]
    markers += ["HD: DONE"]
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
    ap = argparse.ArgumentParser(description="R1 HEX DUMP E32 QEMU test")
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

#!/usr/bin/env python3
"""r1_zip_test.py -- QEMU test for the zlib-compatible ZIP32 E32 utility.

Stages build/user/ZIP32.E32 (a PE2 image importing EUSER.DLL ordinal 915),
EUSER.DLL and a repetitive HELLO.TXT into a FAT32 disk, boots the R1 kernel,
launches "zip32" from the serial shell, and drives selftest / deflate /
inflate / gzip through prompt-synchronised console input.  Asserts the ZP:
markers, the in-guest round-trip CRC, a clean exception log, and -- using the
host's gzip(1) -- that the .gz member produced in the guest is a genuine,
interoperable gzip file.

Usage: python3 r1_zip_test.py [--no-rebuild-disk] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time
import zlib

ROOT = "/tmp/opencode"
DISK_IMG = ROOT + "/r1_zip.img"
FS_IMG = ROOT + "/r1_zip_fs.img"
LOG_DEFAULT = ROOT + "/r1_zip.log"

DISK_MB = 32
PART_LBA, PART_SECTORS = 2048, 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
ZIP32_E32 = REPO + "/build/user/ZIP32.E32"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
KERNEL = REPO + "/build/kernfull.elf"

HELLO_TXT = ROOT + "/r1_zip_HELLO.TXT"
SEED = b"HELLO ZLIB WORLD 0123456789 ABCABCABC "
HELLO_CONTENT = (SEED * 128)[:4096]
EXPECTED_CRC = zlib.crc32(HELLO_CONTENT) & 0xFFFFFFFF

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

    with open(HELLO_TXT, "wb") as f:
        f.write(HELLO_CONTENT)
    for src, name in ((ZIP32_E32, "ZIP32.E32"),
                      (EUSER_DLL, "EUSER.DLL"),
                      (HELLO_TXT, "HELLO.TXT")):
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
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for want in ("zip32", "euserdll", "hellotxt"):
        if want not in norm:
            sys.exit(f"disk missing {want}: {out.stdout}")
    print("mdir OK: ZIP32.E32, EUSER.DLL and HELLO.TXT staged")


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
    print("== booting QEMU, driving ZIP32 via serial console ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, b"C:/> ", timeout=60)

            def send(text, expect, timeout=40):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("zip32", b"ZP> ", 60)                       # startup selftest
            send("selftest", b"ZP> ")                        # explicit selftest
            send("deflate HELLO.TXT HELLO.DEF", b"ZP> ")     # real deflate
            send("inflate HELLO.DEF OUT.TXT", b"ZP> ")       # real inflate
            send("gzip HELLO.TXT", b"ZP> ")                  # genuine .gz member
            send("quit", b"ZP: DONE")
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text):
    ok = True
    checks = [
        ("E32 ready", "ZP: MINIZ TOOL READY" in text),
        ("selftest round-trip", "ZP: selftest OK 1024 -> " in text),
        ("deflate file", "ZP: deflate 4096 -> " in text),
        ("inflate file", "ZP: inflate " in text and " OK" in text),
        ("gzip written", "ZP: gzip HELLO.TXT -> HELLO.GZ" in text),
        ("in-guest CRC", "ZP: crc=0x%08x" % EXPECTED_CRC in text),
        ("clean exit", "ZP: DONE" in text),
    ]
    for name, good in checks:
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {name}")
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
    # Report the real compression ratios observed in the guest.
    for ln in text.splitlines():
        if ln.startswith("ZP: selftest") or ln.startswith("ZP: deflate") \
           or ln.startswith("ZP: inflate") or ln.startswith("ZP: gzip"):
            print("    ", ln.strip()[:160])
    return ok


def verify_gzip_on_host():
    """Pull the guest-produced HELLO.GZ out of the disk and validate it."""
    gz = ROOT + "/r1_zip_from_guest.GZ"
    raw = ROOT + "/r1_zip_from_guest.bin"
    for p in (gz, raw):
        if os.path.exists(p):
            os.remove(p)
    r = sh("mcopy", "-i", f"{DISK_IMG}@@{OFFSET}", "::HELLO.GZ", gz,
           capture_output=True, text=True)
    if r.returncode != 0:
        print("  [FAIL] extract HELLO.GZ from guest FAT:", r.stderr.strip())
        return False
    ok = True
    t = sh("gzip", "-t", gz, capture_output=True, text=True)
    print(f"  [{'PASS' if t.returncode == 0 else 'FAIL'}] host gzip -t HELLO.GZ")
    ok &= (t.returncode == 0)
    d = sh("gzip", "-dc", gz, capture_output=True)
    same = d.returncode == 0 and d.stdout == HELLO_CONTENT
    print(f"  [{'PASS' if same else 'FAIL'}] host gzip -dc matches original "
          f"({len(d.stdout)} bytes)")
    ok &= same
    if not same:
        print("     gzip stderr:", d.stderr[:200])
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 ZIP32 E32 QEMU test")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "gzip"):
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
    print("\n== host gzip interoperability")
    ok &= verify_gzip_on_host()
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

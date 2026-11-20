#!/usr/bin/env python3
"""r1_web_browser_test.py -- QEMU test for the text web-browser E32 fixture.

Stages build/user/ca_test.e32 (PE2 image importing EUSER.DLL ordinal 915) plus
EUSER.DLL into a FAT32 disk, boots the R1 kernel, launches "ca_test" from the
serial shell and checks the browser markers, in order.  Exits 0 only if all pass.

Usage: python3 r1_web_browser_test.py [--no-rebuild-disk] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

DISK_IMG = "/tmp/opencode/r1_web_browser.img"
FS_IMG = "/tmp/opencode/r1_web_browser_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_web_browser.log"
DISK_MB = 32
PART_LBA, PART_SECTORS = 2048, 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

CA_E32 = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/user/ca_test.e32"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"

MARKERS = [
    "WEB: browser start",
    "WEB: URL http://example.com/",
    "WEB: DNS query A example.com",
    "WEB: DNS answer 93.184.216.34",
    "WEB: TCP connect 93.184.216.34:80",
    "WEB: GET / HTTP/1.0",
    "WEB: HTTP 200 OK",
    "WEB: render: Example Domain",
    "WEB: render: This domain is for use in illustrative examples.",
    "WEB: page rendered OK",
    "WEB: browser exit",
]


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
    for src, name in ((CA_E32, "ca_test"), (EUSER_DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)
    with open(FS_IMG, "rb") as f:
        fs = f.read()
    with open(DISK_IMG, "r+b") as f:
        f.seek(OFFSET)
        f.write(fs)
    entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
             + PART_LBA.to_bytes(4, "little")
             + PART_SECTORS.to_bytes(4, "little"))
    mbr = bytearray(512)
    mbr[446:462] = entry
    mbr[510:511] = b"\x55"
    mbr[511:512] = b"\xAA"
    with open(DISK_IMG, "r+b") as f:
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK_IMG}@@{OFFSET}", "::",
             check=True, capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for want in ("ca_test", "euserdll"):
        if want not in norm:
            sys.exit(f"disk missing {want}: {out.stdout}")
    print("mdir OK: ca_test and EUSER.DLL staged")


def run_qemu(log):
    qemu = [
        "timeout", "60", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio",
        "-kernel", KERNEL,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    # Asic::Init3 already mounts the FAT partition at boot; a shell `mount`
    # hangs on the M-03 per-process-page-table kernel (see r1_qemu_smoke.py).
    feeder = "sleep 1.5; printf '\\n'; sleep 3.0; printf 'ca_test\\n'; sleep 5.0"
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit {r.returncode} (124 == timeout, expected)")
    # timeout's SIGTERM can reap qemu before its redirected serial stdio is
    # fully flushed; give the file a moment so marker checks are deterministic.
    time.sleep(1.0)
    try:
        with open(log, errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def main():
    ap = argparse.ArgumentParser(description="R1 web-browser E32 QEMU test")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(t):
            sys.exit(f"missing tool {t}")
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk()
    text = run_qemu(args.log)
    ok = True
    pos = [text.find(m) for m in MARKERS]
    for m, p in zip(MARKERS, pos):
        good = p >= 0
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {m}")
    ordered = all(p >= 0 for p in pos) and all(pos[i] < pos[i + 1]
                                                for i in range(len(pos) - 1))
    ok &= ordered
    print(f"  [{'PASS' if ordered else 'FAIL'}] marker order")
    binds = [l for l in text.splitlines() if "e32run: bind slot" in l]
    bound915 = any("ord 915" in l for l in binds)
    ok &= bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")
    print(f"serial log: {args.log}")
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_largefile_test.py -- QEMU proof of genuine large-file streaming.

Stages a >2 MiB C:\\BIGFILE.BIN (offset-derived byte pattern) together with
build/user/LARGEFILE.E32 and EUSER.DLL on a FAT32 disk, boots the R1 kernel
and launches `largefile` from the serial shell.  The guest calls file_size()
and file_read_at() at the start, middle and end plus file_seek()+file_read();
all of these go through the PortBigFile streaming device (FAT cluster-chain
walk, no 64 KiB snapshot) -- the same path DOOM uses for a multi-MB WAD.
Exits 0 only if every marker is present, in order, with no corruption FAIL,
no guest exception and no kernel panic.

Usage: python3 tools/r1_largefile_test.py [--no-rebuild-disk] [--no-build]
                                          [--log PATH]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISK = "/tmp/opencode/r1_largefile.img"
FS = "/tmp/opencode/r1_largefile_fs.img"
LOG = "/tmp/opencode/r1_largefile.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/LARGEFILE.E32")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
BIGFILE = "/tmp/opencode/r1_largefile_BIGFILE.BIN"
SIZE = 2 * 1024 * 1024 + 37          # 2097173 bytes
MID = SIZE // 2
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

MARKERS = [
    "LF: LARGE FILE TEST START",
    f"LF: size={SIZE} read at 0/",
    f"LF: middle={MID} OK",
    "LF: result PASS",
    "LF: DONE",
]
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT", "MMU #PF")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def scan_exceptions(text):
    return [line for line in text.splitlines()
            if any(k in line for k in EXC_KEYS)]


def build_bigfile():
    # byte[i] = (i / 4096) & 0xff -- easy to verify at arbitrary offsets.
    block = bytes(((n // 4096) & 0xff) for n in range(4096))
    with open(BIGFILE, "wb") as f:
        full, rest = divmod(SIZE, 4096)
        for _ in range(full):
            f.write(block)
        if rest:
            f.write(block[:rest])
    assert os.path.getsize(BIGFILE) == SIZE


def build_disk():
    for path in (DISK, FS):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    build_bigfile()
    for src, name in ((E32, "LARGEFILE.E32"), (DLL, "EUSER.DLL"),
                      (BIGFILE, "BIGFILE.BIN")):
        if not os.path.exists(src):
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", FS, src, f"::{name}", check=True)
    with open(FS, "rb") as f:
        data = f.read()
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
    for wanted in ("largefilee32", "euserdll", "bigfilebin"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print(f"mdir OK: LARGEFILE.E32, EUSER.DLL and BIGFILE.BIN "
          f"({SIZE} bytes) staged")


def run_qemu(log):
    qemu = ["timeout", "90", "qemu-system-i386", "-M", "q35", "-m", "128",
            "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    feeder = "sleep 1.5; printf '\\n'; sleep 3.0; printf 'largefile\\n'; sleep 10.0"
    cmd = ("{ " + feeder + "; } | "
           + " ".join(shlex.quote(a) for a in qemu)
           + " > " + shlex.quote(log) + " 2>&1")
    print("== QEMU (90s cap), feeding 'largefile' ==")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit: {r.returncode} (124 == timeout expected)")
    time.sleep(1.0)
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")

    if not args.no_build:
        sh("make", "-C", ROOT, "build/kernfull.elf",
           "build/user/LARGEFILE.E32", check=True)
    if not (args.no_rebuild_disk and os.path.exists(DISK)):
        build_disk()

    text = run_qemu(args.log)

    ok = True
    pos = [text.find(m) for m in MARKERS]
    for marker, p in zip(MARKERS, pos):
        good = p >= 0
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")
    ordered = (all(p >= 0 for p in pos)
               and all(pos[i] < pos[i + 1] for i in range(len(pos) - 1)))
    ok = ok and ordered
    print(f"  [{'PASS' if ordered else 'FAIL'}] marker order")

    fails = [ln for ln in text.splitlines() if "FAILED" in ln or "FAIL " in ln]
    no_fail = not fails
    ok = ok and no_fail
    print(f"  [{'PASS' if no_fail else 'FAIL'}] no LF failure marker")
    for ln in fails:
        print("   ", ln[:220])

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok = ok and bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    exc_lines = scan_exceptions(text)
    clean = not exc_lines
    ok = ok and clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT in log")
    for line in exc_lines:
        print("   ", line[:220])

    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        keys = ("lf:", "largefile", "e32run", "exc ")
        shown = set()
        for line in text.splitlines():
            low = line.lower()
            if any(k in low for k in keys) and line not in shown:
                print("   ", line[:220])
                shown.add(line)
    print(f"\nRESULT: {'PASS' if ok else 'FAIL'} (file size {SIZE})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_hanoi_test.py -- QEMU test for the HANOI Tower-of-Hanoi puzzle E32.

Stages build/user/HANOI.E32 (a PE2 image importing EUSER.DLL ordinal 915 =
RDebug::Printf) plus the genuine EUSER.DLL on a FAT32 disk, boots the R1
kernel, launches `hanoi` from the serial shell, drives `selftest` and `quit`
through prompt-synchronised PortCon input, then asserts the HN markers:
startup banner, the 3-disk solve header, the seven individual moves in their
canonical order, the final peg state (all disks on C), selftest OK and DONE.
Also checks E32 validation, ordinal-915 binding, and the absence of guest
exceptions/panics.

Usage: python3 tools/r1_hanoi_test.py [--no-rebuild-disk] [--log PATH]
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
DISK = ROOT + "/r1_hanoi.img"
FS = ROOT + "/r1_hanoi_fs.img"
LOG = ROOT + "/r1_hanoi.log"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
E32 = os.path.join(REPO, "build/user/HANOI.E32")
KERNEL = os.path.join(REPO, "build/kernfull.elf")
DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
       "euser.dll")

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")
PROMPT = b"HN> "
MOVE_RE = re.compile(r"^HN: move [1-4] [ABC]->[ABC]$")

# The canonical 3-disk solution A -> C via B: seven moves in order.
MOVES = [
    "HN: move 1 A->C",
    "HN: move 2 A->B",
    "HN: move 1 C->B",
    "HN: move 3 A->C",
    "HN: move 1 B->A",
    "HN: move 2 B->C",
    "HN: move 1 A->C",
]

# Each marker goes through EUSER ord 915 *and* the DebugPrint fallback, so
# duplicates in the log are expected.  Positions are searched sequentially so
# repeated move strings are still checked in order.
MARKERS = (["HN: TOWER OF HANOI", "HN: SELFTEST begin",
            "HN: solve 3 disks -> 7 moves"] + MOVES +
           ["HN: peg A: - B: - C: 321", "HN: SELFTEST OK", "HN: DONE"])


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
    for src, name in ((E32, "HANOI.E32"), (DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src} (build it first)")
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
    for wanted in ("hanoie32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: HANOI.E32 and EUSER.DLL staged")


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
        "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, driving HANOI via serial console ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            # Asic::Init3 has already mounted the FAT partition at boot.
            pos = wait_for(log_path, b"C:/> ", timeout=60)

            def send(text, expect, timeout=30):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("hanoi", PROMPT, 60)      # launch + banner
            send("selftest", PROMPT)       # solve 3 disks, print moves + board
            send("quit", b"HN: DONE")      # exit the puzzle
            time.sleep(1.0)                # let the final marker flush
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")
    if not (args.no_rebuild_disk and os.path.exists(DISK)):
        build_disk()

    try:
        run_qemu(args.log)
    except RuntimeError as exc:
        print("QEMU drive failed:", exc)
    with open(args.log, errors="replace") as f:
        text = f.read()

    ok = True
    # Sequential search handles the repeated "move 1 A->C" marker correctly.
    cursor = 0
    for marker in MARKERS:
        p = text.find(marker, cursor)
        good = p >= 0
        if good:
            cursor = p + len(marker)
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")

    move_lines = [ln.strip() for ln in text.splitlines()
                  if MOVE_RE.match(ln.strip())]
    enough = len(move_lines) >= 7
    ok = ok and enough
    print(f"  [{'PASS' if enough else 'FAIL'}] seven moves emitted (log lines: {len(move_lines)})")

    val = "e32run: validate r=0" in text
    ok = ok and val
    print(f"  [{'PASS' if val else 'FAIL'}] E32 validates (r=0)")

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok = ok and bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    exc_lines = [ln for ln in text.splitlines()
                 if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines
    ok = ok and clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT in log")
    for line in exc_lines:
        print("   ", line[:220])

    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        shown = set()
        for line in text.splitlines():
            if "hn:" in line.lower() and line not in shown:
                print("   ", line[:220])
                shown.add(line)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_units_test.py -- QEMU test for the end-user UNIT CONVERTER E32 program.

Stages build/user/UNITS.E32 (a PE2 image importing EUSER.DLL ordinal 915 =
RDebug::Printf) plus the genuine EUSER.DLL on a FAT32 disk, boots the R1
kernel, launches `units` from the serial shell, drives the built-in `selftest`
and `quit` commands through prompt-synchronised PortCon input, and asserts the
UN: markers (startup, 1000 m -> ft, 100 C -> 212 F, 1 kg -> lb, selftest OK,
DONE) with no guest exception or kernel panic.  Exits 0 only if all pass.

Usage: python3 tools/r1_units_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK = ROOT + "/r1_units.img"
FS = ROOT + "/r1_units_fs.img"
LOG = ROOT + "/r1_units.log"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
E32 = os.path.join(REPO, "build/user/UNITS.E32")
KERNEL = os.path.join(REPO, "build/kernfull.elf")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

# Expected markers in order.  Each goes through EUSER ord 915 *and* the
# DebugPrint fallback, so duplicates are not compared.
MARKERS = [
    "UN: UNIT CONVERTER",
    "UN: 1000 m = 3280.839 ft",
    "UN: 100 C = 212 F",
    "UN: 1 kg = 2.204 lb",
    "UN: selftest OK",
    "UN: DONE",
]
EXC_KEYS = ("Exc ", "PanicKernExec")
PROMPT = b"UN> "


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
    for src, name in ((E32, "UNITS.E32"), (DLL, "EUSER.DLL")):
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
    for wanted in ("unitse32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: UNITS.E32 and EUSER.DLL staged")


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
    print("== booting QEMU, driving UNIT CONVERTER via serial console ...")
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

            send("units", PROMPT, 60)      # launch + startup markers
            send("selftest", PROMPT)       # conversions + selftest OK
            send("quit", b"UN: DONE")      # exit the converter
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
    pos = [text.find(m) for m in MARKERS]
    for marker, p in zip(MARKERS, pos):
        good = p >= 0
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")
    ordered = (all(p >= 0 for p in pos)
               and all(pos[i] < pos[i + 1] for i in range(len(pos) - 1)))
    ok = ok and ordered
    print(f"  [{'PASS' if ordered else 'FAIL'}] marker order")

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok = ok and bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    exc_lines = [ln for ln in text.splitlines()
                 if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines
    ok = ok and clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec in log")
    for line in exc_lines:
        print("   ", line[:220])

    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        shown = set()
        for line in text.splitlines():
            if "un:" in line.lower() and line not in shown:
                print("   ", line[:220])
                shown.add(line)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

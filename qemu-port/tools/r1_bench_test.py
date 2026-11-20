#!/usr/bin/env python3
"""r1_bench_test.py -- QEMU test for the CPU BENCHMARK E32 program.

Stages build/user/BENCH.E32 and the genuine EUSER.DLL (ordinal 915 =
RDebug::Printf) on a FAT32 disk, boots the R1 kernel, launches `bench` from
the serial shell and checks that the benchmark markers are emitted (including
nonzero tick count, iteration rate and checksum), with no guest exception or
kernel panic.  Exits 0 only if every check passes.

Usage: python3 tools/r1_bench_test.py [--no-rebuild-disk] [--log PATH]
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
DISK = "/tmp/opencode/r1_bench.img"
FS = "/tmp/opencode/r1_bench_fs.img"
LOG = "/tmp/opencode/r1_bench.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/BENCH.E32")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

MARKERS = [
    "BENCH: CPU BENCHMARK START",
    "BENCH: ticks=",
    "BENCH: 10000000 iterations in ",
    "BENCH: ~",
    "BENCH: checksum=",
    "BENCH: DONE",
]
EXC_KEYS = ("Exc ", "PanicKernExec")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def scan_exceptions(text):
    return [line for line in text.splitlines()
            if any(k in line for k in EXC_KEYS)]


def build_disk():
    for path in (DISK, FS):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "BENCH.E32"), (DLL, "EUSER.DLL")):
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
    for wanted in ("benche32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: BENCH.E32 and EUSER.DLL staged")


def run_qemu(log):
    qemu = ["timeout", "60", "qemu-system-i386", "-M", "q35", "-m", "128",
            "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    # Asic::Init3 already mounts the FAT partition at boot; a shell `mount`
    # hangs on the M-03 per-process-page-table kernel (see r1_qemu_smoke.py).
    feeder = "sleep 1.5; printf '\\n'; sleep 3.0; printf 'bench\\n'; sleep 6.0"
    cmd = ("{ " + feeder + "; } | "
           + " ".join(shlex.quote(a) for a in qemu)
           + " > " + shlex.quote(log) + " 2>&1")
    print("== QEMU (60s cap), feeding 'bench' ==")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit: {r.returncode} (124 == timeout expected)")
    time.sleep(1.0)   # let timeout's SIGTERM flush redirected serial stdio
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")
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

    # Numeric values must be present and nonzero (ticks, rate, checksum).
    m_ticks = re.search(r"BENCH: ticks=(\d+)", text)
    m_iters = re.search(r"BENCH: (\d+) iterations in (\d+) ticks", text)
    m_rate = re.search(r"BENCH: ~(\d+) iter/sec \(HZ=100\)", text)
    m_check = re.search(r"BENCH: checksum=(\d+)", text)
    values = {
        "nonzero ticks": bool(m_ticks and int(m_ticks.group(1)) > 0),
        "iterations=10000000": bool(m_iters and int(m_iters.group(1)) == 10000000),
        "nonzero rate": bool(m_rate and int(m_rate.group(1)) > 0),
        "nonzero checksum": bool(m_check and int(m_check.group(1)) > 0),
    }
    for label, good in values.items():
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {label}")
    if m_ticks and m_rate and m_iters:
        print(f"  measured: {m_iters.group(1)} iterations in "
              f"{m_ticks.group(1)} ticks, ~{m_rate.group(1)} iter/sec (HZ=100), "
              f"checksum={m_check.group(1) if m_check else '?'}")

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok = ok and bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    exc_lines = scan_exceptions(text)
    clean = not exc_lines
    ok = ok and clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc or PanicKernExec in log")
    if exc_lines:
        for line in exc_lines:
            print("   ", line[:220])

    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        keys = ("bench", "e32run", "exc ")
        shown = set()
        for line in text.splitlines():
            low = line.lower()
            if any(k in low for k in keys) and line not in shown:
                print("   ", line[:220])
                shown.add(line)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

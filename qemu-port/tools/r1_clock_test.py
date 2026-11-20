#!/usr/bin/env python3
"""r1_clock_test.py -- QEMU test for the end-user CLOCK / TIME E32 program.

Stages build/user/CLOCK.E32 and the genuine EUSER.DLL (ordinal 915 =
RDebug::Printf) on a FAT32 disk, boots the R1 kernel with the R1 device set,
launches `clock` from the serial shell and checks that the clock markers are
emitted through EUSER ordinal 915, that the tick rate observed between the
sampled segments matches this port's 64 Hz system tick (EExecTickCount), and
that no guest exception or kernel panic occurs.  Exits 0 only if all passes.

Usage: python3 tools/r1_clock_test.py [--no-rebuild-disk] [--log PATH]
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
DISK = "/tmp/opencode/r1_clock.img"
FS = "/tmp/opencode/r1_clock_fs.img"
LOG = "/tmp/opencode/r1_clock.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/CLOCK.E32")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

MARKERS = [
    "CLK: CLOCK UTILITY START",
    "CLK: ticks=",
    "HZ=64",
    "CLK: segments 1/2/3",
    "CLK: segment 1 ticks=",
    "CLK: segment 2 ticks=",
    "CLK: segment 3 ticks=",
    "CLK: DONE",
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
    for src, name in ((E32, "CLOCK.E32"), (DLL, "EUSER.DLL")):
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
    for wanted in ("clocke32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: CLOCK.E32 and EUSER.DLL staged")


def run_qemu(log):
    qemu = ["timeout", "60", "qemu-system-i386", "-M", "q35", "-m", "128",
            "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
            "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
            "-vga", "std", "-display", "none", "-serial", "stdio",
            "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    # The kernel mounts the FAT partition at boot; just wake the shell and run
    # the app.  Three 1-second timer sleeps need ~3 s, so allow a generous 6 s.
    feeder = "sleep 1.5; printf '\\n'; sleep 3.0; printf 'clock\\n'; sleep 6.0"
    cmd = ("{ " + feeder + "; } | "
           + " ".join(shlex.quote(a) for a in qemu)
           + " > " + shlex.quote(log) + " 2>&1")
    print("== QEMU (60s cap), feeding 'clock' ==")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit: {r.returncode} (124 == timeout expected)")
    time.sleep(1.0)
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def tick_samples(text):
    """Return (label, ticks) for the start and the three segment readings."""
    out = []
    m = re.search(r"CLK: ticks=(\d+) uptime=", text)
    if m:
        out.append(("start", int(m.group(1))))
    for seg in (1, 2, 3):
        m = re.search(rf"CLK: segment {seg} ticks=(\d+) uptime=", text)
        if m:
            out.append((f"seg{seg}", int(m.group(1))))
    return out


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

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok = ok and bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    samples = tick_samples(text)
    deltas = [samples[i + 1][1] - samples[i][1]
              for i in range(len(samples) - 1)]
    # EExecTickCount (slow exec 5) reports SYSTEM ticks at 64 Hz
    # (ESystemTickPeriod=15625us = 1/64 s), so a 1 s sleep advances ~64 ticks.
    # Accept 50..80 to absorb timer/serial jitter while still rejecting the
    # 1000 Hz nanokernel/PIT tick (which would show ~1000).
    rate_ok = (len(deltas) == 3 and all(50 <= d <= 80 for d in deltas))
    ok = ok and rate_ok
    print(f"  [{'PASS' if rate_ok else 'FAIL'}] tick rate observed: "
          f"samples={samples} deltas={deltas} (~64 Hz system tick expected)")

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
        keys = ("CLK:", "clock", "e32run")
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

#!/usr/bin/env python3
"""r1_cal_test.py -- QEMU test for the end-user CALENDAR E32 program.

Stages build/user/CAL.E32 and the genuine EUSER.DLL (ordinal 915 =
RDebug::Printf) on a FAT32 disk, boots the R1 kernel, launches `cal` from the
serial shell and checks the CAL markers.  The `today` line is cross-checked
against an independent Python conversion of the raw exec-68 microsecond value
using the same assumed 0AD epoch and Gregorian leap rules.  Also checks that
EUSER ord 915 was bound and that no guest exception or kernel panic occurred.
Exits 0 only if every check passes.

Usage: python3 tools/r1_cal_test.py [--no-rebuild-disk] [--log PATH]
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
DISK = "/tmp/opencode/r1_cal.img"
FS = "/tmp/opencode/r1_cal_fs.img"
LOG = "/tmp/opencode/r1_cal.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/CAL.E32")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

MARKERS = [
    "CAL: CALENDAR",
    "CAL: raw exec68 us=",
    "CAL: epoch assumed 0000-01-01",
    "CAL: today ",
    "CAL: Su Mo Tu We Th Fr Sa",
    "CAL: DONE",
]
HEADER = "CAL: Su Mo Tu We Th Fr Sa"
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
    for src, name in ((E32, "CAL.E32"), (DLL, "EUSER.DLL")):
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
    for wanted in ("cale32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: CAL.E32 and EUSER.DLL staged")


def run_qemu(log):
    qemu = ["timeout", "60", "qemu-system-i386", "-M", "q35", "-m", "128",
            "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    feeder = "sleep 1.5; printf '\\n'; sleep 3.0; printf 'cal\\n'; sleep 5.0"
    cmd = ("{ " + feeder + "; } | "
           + " ".join(shlex.quote(a) for a in qemu)
           + " > " + shlex.quote(log) + " 2>&1")
    print("== QEMU (60s cap), feeding 'cal' ==")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit: {r.returncode} (124 == timeout expected)")
    time.sleep(1.0)   # let timeout's SIGTERM flush redirected serial stdio
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def civil_from_days(z):
    """Independent Gregorian civil-from-days; z is days since 1970-01-01."""
    z += 719468
    era = (z if z >= 0 else z - 146096) // 146097
    doe = z - era * 146097
    yoe = (doe - doe // 1460 + doe // 36524 - doe // 146096) // 365
    y = yoe + era * 400
    doy = doe - (365 * yoe + yoe // 4 - yoe // 100)
    mp = (5 * doy + 2) // 153
    day = doy - (153 * mp + 2) // 5 + 1
    mon = mp + 3 if mp < 10 else mp - 9
    return y + (1 if mon <= 2 else 0), mon, day


def check_date(text, ok):
    """Cross-check the app's today line against the raw exec-68 µs reading."""
    raw = re.search(r"CAL: raw exec68 us=(\d+)", text)
    today = re.search(r"CAL: today (\d{4})-(\d{2})-(\d{2}) \(day (\d+)\)", text)
    if not raw or not today:
        print("  [FAIL] raw µs / today line present")
        return False, None
    us = int(raw.group(1))
    shown = (int(today.group(1)), int(today.group(2)), int(today.group(3)))
    day_number = int(today.group(4))
    z = (us // 86400000000) - 719528   # µs per day: 86400 s * 1e6
    expected = civil_from_days(z)
    good = (shown == expected and day_number == us // 86400000000)
    print(f"  [{'PASS' if good else 'FAIL'}] date matches raw µs: "
          f"us={us} day={day_number} shown={shown} expected={expected}")
    return (ok and good), f"{shown[0]:04d}-{shown[1]:02d}-{shown[2]:02d}"


def check_grid(text, ok):
    """The month grid follows the weekday header with 3..6 day rows."""
    lines = text.splitlines()
    try:
        i = next(i for i, ln in enumerate(lines) if HEADER in ln)
    except StopIteration:
        print("  [FAIL] weekday header present")
        return False
    rows = []
    for ln in lines[i + 1:]:
        if "CAL: DONE" in ln:
            break
        if not ln.startswith("CAL: "):
            break
        # Each marker is emitted twice (EUSER 915 + DebugPrint fallback).
        if rows and ln == rows[-1]:
            continue
        if re.search(r"\d", ln):
            rows.append(ln)
    good = 3 <= len(rows) <= 6
    print(f"  [{'PASS' if good else 'FAIL'}] month grid rows: "
          f"{len(rows)} (3..6 expected)")
    for r in rows:
        print("     ", r[:120])
    return ok and good


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

    ok, shown_date = check_date(text, ok)
    ok = check_grid(text, ok)

    exc_lines = scan_exceptions(text)
    clean = not exc_lines
    ok = ok and clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc or PanicKernExec in log")
    if exc_lines:
        for line in exc_lines:
            print("   ", line[:220])

    print(f"serial log: {args.log}")
    print(f"date shown: {shown_date}")
    if not ok:
        print("\n== relevant log lines:")
        keys = ("cal:", "cal", "e32run", "exc ")
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

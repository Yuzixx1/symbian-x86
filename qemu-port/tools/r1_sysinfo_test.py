#!/usr/bin/env python3
"""r1_sysinfo_test.py -- QEMU test for the end-user SYSTEM INFO E32 program.

Stages build/user/SYSINFO.E32 and the genuine EUSER.DLL (ordinal 915 =
RDebug::Printf) on a FAT32 disk, boots the R1 kernel with the R1 device set
(e1000, AC97, VBE, AHCI/IDE), launches `sysinfo` from the serial shell and
checks that the CPUID/OS/memory/device/uptime markers and the final DONE marker
are emitted through EUSER ordinal 915, with no guest exception or kernel panic.
Exits 0 only if every check passes.

Usage: python3 tools/r1_sysinfo_test.py [--no-rebuild-disk] [--log PATH]
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
DISK = "/tmp/opencode/r1_sysinfo.img"
FS = "/tmp/opencode/r1_sysinfo_fs.img"
LOG = "/tmp/opencode/r1_sysinfo.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/SYSINFO.E32")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

MARKERS = [
    "SYS: === EKA2 PC OS SYSTEM INFO ===",
    "SYS: features FPU/SSE/SSE2/HTT",
    "SYS: RAM 128M",
    "SYS: heap ",
    "SYS: EKA2 kernel (genuine), EUSER.DLL 2503 exports, paging ON",
    "SYS: devices e1000=",
    "SYS: ticks=",
    "SYS: DONE",
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
    for src, name in ((E32, "SYSINFO.E32"), (DLL, "EUSER.DLL")):
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
    for wanted in ("sysinfoe32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: SYSINFO.E32 and EUSER.DLL staged")


def run_qemu(log):
    # Mirror the R1 smoke device set so the device report is meaningful.
    qemu = ["timeout", "60", "qemu-system-i386", "-M", "q35", "-m", "128",
            "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
            "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
            "-vga", "std", "-display", "none", "-serial", "stdio",
            "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    # Asic::Init3 already mounts the FAT partition at boot; a shell `mount`
    # hangs on the M-03 per-process-page-table kernel (see r1_qemu_smoke.py).
    feeder = "sleep 1.5; printf '\\n'; sleep 3.0; printf 'sysinfo\\n'; sleep 5.0"
    cmd = ("{ " + feeder + "; } | "
           + " ".join(shlex.quote(a) for a in qemu)
           + " > " + shlex.quote(log) + " 2>&1")
    print("== QEMU (60s cap), feeding 'sysinfo' ==")
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

    # CPUID vendor must be a real vendor string, not an empty/garbled field.
    cpu_lines = [ln for ln in text.splitlines() if "SYS: CPU " in ln]
    cpu_ok = any(re.search(r"SYS: CPU [A-Za-z][A-Za-z0-9]+ family=\d+ "
                           r"model=\d+ stepping=\d+", ln) for ln in cpu_lines)
    ok = ok and cpu_ok
    print(f"  [{'PASS' if cpu_ok else 'FAIL'}] CPUID vendor/family/model/stepping: "
          f"{cpu_lines[0].strip() if cpu_lines else '(none)'}")

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
        keys = ("SYS:", "sysinfo", "e32run", "exc ")
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

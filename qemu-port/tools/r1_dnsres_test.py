#!/usr/bin/env python3
"""r1_dnsres_test.py -- QEMU smoke test for DNSRES.E32.

Stages build/user/DNSRES.E32 and the genuine EUSER.DLL (ordinal 915 =
RDebug::Printf) on a FAT32 disk, boots the R1 kernel, launches the resolver
from the shell and checks its serial markers.  Exits 0 only if all markers
and the EUSER ordinal bind line are present.

Usage: python3 tools/r1_dnsres_test.py [--log PATH]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISK = "/tmp/opencode/r1_dnsres.img"
FS = "/tmp/opencode/r1_dnsres_fs.img"
LOG = "/tmp/opencode/r1_dnsres.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/DNSRES.E32")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

MARKERS = [
    "DNSRES: DNS resolver E32 start (EUSER ord 915)",
    "DNSRES: built A/IN query (RFC 1035)",
    "DNSRES: UDP 10.0.0.2:49152 -> 8.8.8.8:53",
    "DNSRES: UDP reply received",
    "DNSRES: DnsResolverServer ready (EResolve=0)",
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def build_disk():
    for p in (DISK, FS):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "DNSRES"), (DLL, "EUSER.DLL")):
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
    for wanted in ("dnsres", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK")


def run(log):
    qemu = ["timeout", "90", "qemu-system-i386", "-M", "q35", "-m", "128",
            "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    feeder = ("sleep 1.0; printf '\\n'; sleep 2.0; printf 'mount\\n'; "
              "sleep 8.0; printf 'DNSRES\\n'; sleep 6.0; printf 'DNSRES\\n'; "
              "sleep 6.0; printf 'DNSRES\\n'; sleep 8.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    print("== QEMU (60s cap) ==")
    r = subprocess.run(["bash", "-c"], check=False) if False else subprocess.run(["bash", "-c", cmd])
    print(f"qemu wrapper exit: {r.returncode} (124 == timeout expected)")
    with open(log, "r", errors="replace") as f:
        return f.read()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log", default=LOG)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(t):
            sys.exit(f"missing tool: {t}")
    if not (args.no_rebuild_disk and os.path.exists(DISK)):
        build_disk()
    text = run(args.log)
    results = {m: (m in text) for m in MARKERS}
    bind = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    results["EUSER ord 915 bind"] = any("ord 915" in ln for ln in bind)
    pos = [text.find(m) for m in MARKERS]
    results["marker order"] = all(p >= 0 for p in pos) and all(
        pos[i] < pos[i + 1] for i in range(len(pos) - 1))
    ok = True
    for k, v in results.items():
        ok = ok and v
        print(f"  [{'PASS' if v else 'FAIL'}] {k}")
    print(f"serial log: {args.log}")
    if not ok:
        for line in text.splitlines():
            low = line.lower()
            if any(k in low for k in ("dnsres", "e32run", "exc 06")):
                print("   ", line[:220])
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

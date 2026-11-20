#!/usr/bin/env python3
"""r1_qemu_dns_test.py -- end-to-end QEMU test for DNSRES.E32.

Builds a 32 MB raw disk with a bootable FAT32 partition, stages DNSRES.E32
and the genuine EUSER.DLL, boots build/kernfull.elf with QEMU user-mode
networking, runs `dnsresolv` from the serial shell and checks that:
  * the PE2 E32 validates and binds EUSER ordinal 915 (RDebug::Printf),
  * the resolver emits its DNSRES markers on the serial console,
  * a hostname is resolved through the RFC 1035 UDP/53 query path.

Usage: python3 tools/r1_qemu_dns_test.py [--e32 E32] [--kernel KERN] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/r1_dns_test.img"
FS_IMG = "/tmp/opencode/r1_dns_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_dns_test.log"
E32_DEFAULT = "build/user/DNSRES.E32"
EUSER_DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
             "euser.dll")
KERNEL_DEFAULT = "build/kernfull.elf"
MBR_LBA = 2048
PART_SECTORS = 16384
FS_SIZE = PART_SECTORS * 512


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def build_disk(e32):
    for path in (DISK_IMG, FS_IMG):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK_IMG, "wb") as f:
        f.truncate(32 * 1024 * 1024)
    with open(FS_IMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)
    for src, name in ((e32, "dnsresolv"), (EUSER_DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src} (build it first)")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)
    with open(FS_IMG, "rb") as f:
        fat = f.read()
    with open(DISK_IMG, "r+b") as f:
        f.seek(MBR_LBA * 512)
        f.write(fat)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + MBR_LBA.to_bytes(4, "little")
                 + PART_SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510] = 0x55
        mbr[511] = 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK_IMG}@@{MBR_LBA * 512}", "::",
             check=True, capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    assert "dnsresolv" in norm and "euserdll" in norm, out.stdout


def run_qemu(kernel, log):
    qemu = [
        "timeout", "90", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    # Asic::Init3 already mounts the FAT partition; an explicit `mount`
    # hangs the shell on the M-03 per-process-page-table kernel.
    feeder = ("sleep 1.5; printf '\\n'; "
              "sleep 3.0; printf 'dnsresolv\\n'; "
              "sleep 10.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text):
    dns_lines = [ln for ln in text.splitlines() if "DNSRES:" in ln]
    return {
        "kernel boot": "EKA2" in text.upper(),
        "mounted C:": "mounted C:" in text,
        "E32 validates (r=0)": "e32run: validate r=0" in text,
        "EUSER ord 915 bound": "ord 915" in text,
        "DNSRES process started": "dnsresolv started (user process)" in text,
        "DNSRES markers emitted": len(dns_lines) >= 3,
        "DNS start marker": any("DNS resolver" in ln for ln in dns_lines),
        "DNS over UDP/53": any(":53" in ln or "UDP" in ln for ln in dns_lines),
        "IP result printed": any("->" in ln or "resolved" in ln
                                 for ln in dns_lines),
    }


def main():
    ap = argparse.ArgumentParser(description="R1 DNS resolver QEMU test")
    ap.add_argument("--e32", default=E32_DEFAULT)
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk(args.e32)
    text = run_qemu(args.kernel, args.log)
    print("== DNS test results")
    ok = True
    for name, passed in check(text).items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {name}")
    print(f"serial log: {args.log}")
    if not ok:
        for ln in text.splitlines():
            if any(k in ln.lower() for k in ("dnsres", "e32run", "fault", "panic")):
                print("   ", ln[:200])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

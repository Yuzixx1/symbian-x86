#!/usr/bin/env python3
"""r1_wadprobe_test.py -- in-guest ground truth for the DOOM IWAD path.

Boots the DEFAULT 64 MB GRUB ISO (build/symbian-grub.iso).  Its /PROGRAMS.IMG
RAMDISK is auto-mounted at X: and already carries WADPROBE.E32, EUSER.DLL and
FDOOM1.WAD (the Freedoom IWAD).  Feeds `wadprobe` at the EKA2 shell prompt and
prints the exact file_open()/file_size()/file_read_at() results for every
candidate path spelling -- definitive ground truth for the WAD-open failure.

Usage: python3 tools/r1_wadprobe_test.py [--no-build] [--log PATH]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = "/tmp/opencode"
ISO = os.path.join(ROOT, "build/symbian-grub.iso")
LOG = TMP + "/r1_wadprobe.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/WADPROBE.E32")

# Fallback disk-boot layout (only used with --disk-boot).
DISK = TMP + "/r1_wadprobe.img"
SRC_DISK = TMP + "/r1_doom.img"
WAD = "/tmp/opencode/doom/freedoom-0.13.0/freedoom1.wad"
DLL = os.path.join(ROOT, "EUSER.DLL")
DISK_MB, LBA, SECTORS = 64, 2048, 98304
OFFSET = LBA * 512
FS_SIZE = SECTORS * 512


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args))
    return subprocess.run(args, **kw)


def verify_iso():
    """Confirm the default ISO's PROGRAMS.IMG stages probe + WAD + DLL."""
    from pycdlib import PyCdlib
    out = TMP + "/r1_wadprobe_programs.img"
    iso = PyCdlib()
    iso.open(ISO)
    try:
        iso.get_file_from_iso(out, iso_path="/PROGRAMS.IMG;1")
    finally:
        iso.close()
    listing = sh("mdir", "-i", f"{out}@@{OFFSET}", "::", check=True,
                 capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    for want in ("wadprobe", "fdoom1wad", "euserdll"):
        if want not in norm:
            sys.exit(f"ISO /PROGRAMS.IMG missing {want}:\n{listing}")
    print("ISO OK: X: stages WADPROBE.E32 + FDOOM1.WAD + EUSER.DLL")


def build_disk():
    """Rebuild the AHCI disk with probe + WAD + DLL (--disk-boot only)."""
    for path in (DISK,):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(DISK_MB * 1024 * 1024)
    fs = TMP + "/r1_wadprobe_fs.img"
    with open(fs, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", fs, check=True)
    files = [(E32, "WADPROBE.E32"), (WAD, "FDOOM1.WAD"), (DLL, "EUSER.DLL")]
    for src, name in files:
        sh("mcopy", "-i", fs, "-o", src, "::" + name, check=True)
    with open(fs, "rb") as f:
        payload = f.read()
    with open(DISK, "r+b") as f:
        f.seek(OFFSET)
        f.write(payload)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + LBA.to_bytes(4, "little")
                 + SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)
    print("disk OK: WADPROBE.E32 + FDOOM1.WAD + EUSER.DLL staged")


def wait_for_bytes(log_path, needles, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with open(log_path, "rb") as f:
                buf = f.read()
            if any(n in buf for n in needles):
                return True
        except FileNotFoundError:
            pass
        time.sleep(0.2)
    return False


def run_qemu(log, disk_boot):
    qemu = ["timeout", "80", "qemu-system-i386", "-M", "q35", "-m", "128",
            "-vga", "std", "-display", "none", "-serial", "stdio",
            "-no-reboot"]
    if disk_boot:
        qemu += ["-kernel", KERNEL,
                 "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
                 "-device", "ich9-ahci,id=sata",
                 "-device", "ide-hd,drive=d0,bus=sata.0"]
    else:
        qemu += ["-cdrom", ISO, "-boot", "d"]
    print("== QEMU (80 s cap): booting, then feeding 'wadprobe' ==")
    with open(log, "wb") as lf:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=lf, stderr=subprocess.STDOUT)
        try:
            wait_for_bytes(log, [b"X:/> ", b"C:/> ", b"EKA2> "], 70)
            time.sleep(0.5)
            vm.stdin.write(b"\n")
            vm.stdin.flush()
            time.sleep(0.5)
            vm.stdin.write(b"wadprobe\n")
            vm.stdin.flush()
            wait_for_bytes(log, [b"WP: DONE", b"WP: WAD PROBE START"], 30)
            time.sleep(1.0)
        finally:
            try:
                vm.terminate()
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    with open(log, "r", errors="replace") as f:
        return f.read()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--disk-boot", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()

    for tool in ("qemu-system-i386", "timeout"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")

    if not args.no_build:
        sh("make", "-C", ROOT, "build/kernfull.elf",
           "build/user/WADPROBE.E32", check=True,
           stdout=subprocess.DEVNULL)

    if args.disk_boot:
        build_disk()
    else:
        verify_iso()

    text = run_qemu(args.log, args.disk_boot)

    print("\n== probe markers ==")
    seen = []
    for line in text.splitlines():
        if "WP:" in line or "WP [" in line:
            if line not in seen:
                seen.append(line)
                print("   ", line[:200])
    ok = any("WP: WAD PROBE START" in ln for ln in seen) and \
         any("WP: DONE" in ln for ln in seen)
    print(f"\nRESULT: {'PASS' if ok else 'FAIL'} (serial log: {args.log})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

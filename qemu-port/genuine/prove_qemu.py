#!/usr/bin/env python3
"""prove_qemu.py -- run the genuine-source proof E32 under the ported kernel.

Stages GENUINE.E32 + the genuine EUSER.DLL on a FAT32 disk using the same
layout as tools/r1_qemu_smoke.py, boots build/kernfull.elf in QEMU, launches
`genuine` from the serial shell, and asserts that the genuine compiled code
printed its markers through the EUSER.DLL ordinal 915 PE2 import.

Usage:
  python3 genuine/prove_qemu.py [--log PATH] [--no-rebuild-disk]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path("/tmp/opencode")
DISK = ROOT / "genuine_proof.img"
FS = ROOT / "genuine_proof_fs.img"
LOG = ROOT / "genuine_proof.log"

PORT = Path(__file__).resolve().parent.parent
KERNEL = PORT / "build" / "kernfull.elf"
E32 = PORT / "genuine" / "build" / "GENUINE.E32"
PORTFILESrv = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "
MARKERS = [
    b"GENUINE: e32std.h compiled with gcc -m32 -ffreestanding",
    b"GENUINE: EUSER uc_utl.cpp compiled + called",
    b"GENUINE HELLO: printf via EUSER.DLL ordinal 915",
]
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def build_disk():
    for path in (DISK, FS):
        if path.exists():
            path.unlink()
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "GENUINE.E32"), (EUSER_DLL, "EUSER.DLL"),
                      (PORTFILESrv, "portfilesrv")):
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
    for wanted in ("genuinee32", "euserdll", "portfilesrv"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: GENUINE.E32, EUSER.DLL, portfilesrv staged")


def wait_for(path, needle, pos, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, "rb") as f:
            f.seek(pos)
            data = f.read()
        i = data.find(needle)
        if i >= 0:
            return pos + i + len(needle)
        time.sleep(0.1)
    raise RuntimeError(f"missing {needle!r} in serial log")


def run_qemu(log_path):
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-kernel", str(KERNEL),
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, launching `genuine` from the serial shell ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, PROMPT, 0, 60)
            vm.stdin.write(b"genuine\n")
            vm.stdin.flush()
            for marker in MARKERS:
                pos = wait_for(log_path, marker, pos, 30)
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log", default=str(LOG))
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")
    if os.path.exists(args.log):
        os.remove(args.log)
    if not (args.no_rebuild_disk and DISK.exists()):
        build_disk()
    try:
        run_qemu(args.log)
    except RuntimeError as exc:
        print("QEMU drive failed:", exc)
    text = Path(args.log).read_text(errors="replace")
    ok = True
    for marker in MARKERS:
        hit = marker.decode() in text
        ok &= hit
        print(f"  [{'PASS' if hit else 'FAIL'}] {marker.decode()}")
    bound915 = any("ord 915" in ln for ln in text.splitlines()
                   if "e32run: bind slot" in ln)
    ok &= bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound by loader")
    exc_lines = [ln for ln in text.splitlines()
                 if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no guest exception/panic")
    for line in exc_lines[:10]:
        print("   ", line[:200])
    if not ok:
        print("\n== relevant serial lines:")
        shown = set()
        for line in text.splitlines():
            if any(k in line.lower() for k in ("genuine", "e32run", "e32load",
                                               "e32validate", "loaded r")):
                if line not in shown:
                    print("   ", line[:220])
                    shown.add(line)
    print(f"serial log: {args.log}")
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

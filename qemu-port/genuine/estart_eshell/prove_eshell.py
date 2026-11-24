#!/usr/bin/env python3
"""prove_eshell.py -- run the GENUINE ESTART/ESHELL E32s under the ported kernel.

Stages the genuine-source images built by build_estart_eshell.py
(genuine/build/estart_.E32, etshell_.E32) plus the genuine EUSER.DLL on a
FAT32 disk, boots build/kernfull.elf in QEMU, and from the serial shell:
  1. runs `estart`   -- expects the ESTART: banner printed via EUSER ord 915
  2. runs `eshell`   -- expects the ESHELL: banner, the genuine CShell::DoBanner
                        output ("ESHELL ... CFG=", "Copyright (c) 1998 Symbian
                        Ltd"), the "C:\\>" editor prompt, then drives the real
                        command loop: types HELP (expects the ATTRIB/CHKDSK
                        help lines) and EXIT (expects E32Main returned 0).
Asserts ord-915 PE2 binds for both images and no kernel fault lines.

Usage: python3 genuine/estart_eshell/prove_eshell.py [--log PATH] [--no-rebuild-disk]
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
DISK = ROOT / "genuine_eshell.img"
FS = ROOT / "genuine_eshell_fs.img"
LOG = ROOT / "genuine_eshell.log"

PORT = Path(__file__).resolve().parents[2]
KERNEL = PORT / "build" / "kernfull.elf"
ESTART = PORT / "genuine" / "build" / "estart_.E32"
ESHELL = PORT / "genuine" / "build" / "etshell_.E32"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")

# markers expected in the serial log, in order
MARKERS = [
    b"ESTART: genuine E32 image loaded",          # glue banner via ord 915
    b"ESHELL: genuine E32 image loaded",          # glue banner via ord 915
    b"CFG=",                                      # CShell::DoBanner
    b"Copyright (c) 1998 Symbian Ltd",            # DoBanner (genuine ts_edshl.cpp)
    b"C:\\>",                                     # CLineEdit prompt (session path C:\)
    b"ATTRIB",                                    # HELP output (genuine iCommand table)
    b"CHKDSK",
    b"ESHELL: E32Main returned 0",                # EXIT -> E32Main returns
]


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
    for src, name in ((ESTART, "ESTART.E32"), (ESHELL, "ESHELL.E32"),
                      (EUSER_DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src} (run build_estart_eshell.py first)")
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
    for wanted in ("estarte32", "eshelle32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: ESTART.E32, ESHELL.E32, EUSER.DLL staged")


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
    raise RuntimeError(f"missing {needle!r} in serial log {path}")


def run_qemu(log_path):
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio",
        "-kernel", str(KERNEL),
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, E32Run genuine estart + eshell from the serial shell ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, PROMPT, 0, 90)

            def send(text, expect, timeout=60):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("estart", b"ESTART: genuine E32 image loaded")
            time.sleep(2.0)                      # let genuine estart run/panic
            send("eshell", b"ESHELL: genuine E32 image loaded")
            for m in (b"CFG=", b"Copyright (c) 1998 Symbian Ltd", b"C:\\>"):
                pos = wait_for(log_path, m, pos, 60)
            send("help", b"ATTRIB", 60)
            pos = wait_for(log_path, b"CHKDSK", pos, 30)
            send("exit", b"ESHELL: E32Main returned 0", 60)
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
    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    b915 = [ln for ln in binds if "ord 915" in ln]
    good = len(b915) >= 2
    ok &= good
    print(f"  [{'PASS' if good else 'FAIL'}] EUSER ord 915 bound ({len(b915)} images)")
    exc_lines = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no kernel exception/panic/fault")
    for line in exc_lines[:10]:
        print("   ", line[:200])
    if not ok:
        print("\n== relevant serial lines:")
        shown = set()
        for line in text.splitlines():
            if any(k in line.lower() for k in ("estart", "eshell", "e32run", "panic", "exc ", "fault")):
                if line not in shown:
                    print("   ", line[:220])
                    shown.add(line)
    print(f"serial log: {args.log}")
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

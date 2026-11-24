#!/usr/bin/env python3
"""prove_hal_dm.py -- QEMU smoke run for the genuine HAL + domain manager builds.

Stages hal_smoke.E32 (genuine HAL code as an EXE, since E32Run refuses the
DLL-UID1 hal_halu.E32 as a process) and dm_domainsrv.E32 (genuine domainsrv)
plus the genuine EUSER.DLL on a FAT32 disk, boots build/kernfull.elf in QEMU,
launches both from the serial shell, and asserts the markers print through
the EUSER.DLL ordinal-915 PE2 import with no guest exception/panic.

The DLL artifact itself (genuine/build/hal_halu.E32) is validated with
e32read.py (UID1=0x10000079, KImageDll, 6 exports from bx86/halu.def) --
see genuine/HAL_DM_NOTES.md.

Usage:
  python3 genuine/prove_hal_dm.py [--log PATH] [--no-rebuild-disk]
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
DISK = ROOT / "hal_dm_proof.img"
FS = ROOT / "hal_dm_proof_fs.img"
LOG = ROOT / "hal_dm_proof.log"

PORT = Path(__file__).resolve().parent.parent
KERNEL = PORT / "build" / "kernfull.elf"
BUILD = PORT / "genuine" / "build"
PORTFILESrv = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "

# (shell command, [markers expected after launching it])
RUNS = [
    ("hal_smoke", [
        b"HALU: genuine hal_main.cpp+userhal.cpp loaded",
        b"HALU: HAL::Get(ECPU) -> r=0 value=2 (ECPU_X86=2)",
        b"HALU: HAL::Get(EMemoryPageSize) -> r=0 value=4096",
        b"HALU: HAL::Set/Get(ELanguageIndex) -> r=0 roundtrip=7",
        b"HALU: HAL::GetAll -> r=0 entries=",
        b"HALU SMOKE: done, exiting cleanly",
    ]),
    ("dm_domainsrv", [
        b"DM: genuine domainsrv.cpp+domaincfg.cpp loaded",
        b"DM: TTransitionConfig(spec).CheckValues -> r=0 (KErrNone=0)",
        b"DM: TTransitionConfig(bad).CheckValues -> r=-260 (KDmErrBadDomainSpec=-260)",
        b"DM: TTransitionConfig(TDmDomainState=2) constructed, iState=2",
        b"DM SMOKE: done, exiting cleanly",
    ]),
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
    files = [
        (BUILD / "hal_smoke.E32", "HAL_SMOKE.E32"),
        (BUILD / "dm_domainsrv.E32", "DM_DOMAINSRV.E32"),
        (EUSER_DLL, "EUSER.DLL"),
        (PORTFILESrv, "portfilesrv"),
    ]
    for src, name in files:
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
    for wanted in ("hal_smokee32", "dm_domainsrve32", "euserdll", "portfilesrv"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: HAL_SMOKE.E32, DM_DOMAINSRV.E32, EUSER.DLL, portfilesrv staged")


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
    print("== booting QEMU, launching hal_smoke + dm_domainsrv from the serial shell ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, PROMPT, 0, 60)
            for cmd, markers in RUNS:
                vm.stdin.write(cmd.encode() + b"\n")
                vm.stdin.flush()
                for marker in markers:
                    pos = wait_for(log_path, marker, pos, 30)
                time.sleep(0.5)
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
    for cmd, markers in RUNS:
        for marker in markers:
            hit = marker.decode() in text
            ok &= hit
            print(f"  [{'PASS' if hit else 'FAIL'}] {marker.decode()}")
    bound915 = any("ord 915" in ln for ln in text.splitlines()
                   if "e32run: bind slot" in ln)
    ok &= bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound by loader")
    loaded = text.count("e32run: Loaded r=0")
    print(f"  [{'PASS' if loaded >= 2 else 'FAIL'}] both images loaded (Loaded r=0 x{loaded})")
    ok &= loaded >= 2
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
            if any(k in line.lower() for k in ("halu", "dm:", "e32run", "e32load",
                                                "e32validate", "loaded r")):
                if line not in shown:
                    print("   ", line[:220])
                    shown.add(line)
    print(f"serial log: {args.log}")
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_genuine_fat_io_test.py -- B5: genuine EFSRV file I/O against the REAL
FAT32 volume.

Boots build/kernfull.elf in QEMU with a 32 MB raw disk whose MBR has one
bootable 0x0C FAT32 partition at LBA 2048.  The FAT image contains:
  * PORTFILESRV.E32  -- genuine EFSRV host (genuine/efsrv_min/EFSRVMIN.E32),
                        now backed by the disk FAT via PortFile/PortBigFile.
  * FATIOCLIENT.E32  -- genuine TFsMessage IPC client (opcodes 30/31/34/35/28).
  * F32PROBE.E32     -- unmodified genuine F32 BPB parsing (supporting evidence).
  * EUSER.DLL        -- genuine 2503-export EUSER.
  * R1TEST/SEED.TXT  -- pre-staged file that exists ONLY on the FAT volume.

The client (1) opens+reads SEED.TXT through genuine IPC -- the old in-memory
backend has no such file, so this is an in-guest disk-read discriminator; then
(2) creates C:\\R1TEST\\FATIO.TXT, writes a unique marker, closes (commit);
(3) reopens and reads the marker back byte-for-byte, asserting no Exc/Panic/
FAULT.  After QEMU exits the test reads FATIO.TXT back from the disk image with
mtools and asserts the SAME marker is physically present in the FAT volume.
A marker not in the staged seed data guarantees a pass cannot come from the
in-memory backend.

Usage: python3 tools/r1_genuine_fat_io_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK = ROOT + "/r1_fat_io.img"
FSIMG = ROOT + "/r1_fat_io_fs.img"
SEEDFILE = ROOT + "/r1_fat_io_seed.txt"
LOG_DEFAULT = ROOT + "/r1_fat_io.log"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

KERNEL = REPO + "/build/kernfull.elf"
EUSER = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
SERVER = REPO + "/genuine/efsrv_min/EFSRVMIN.E32"
CLIENT = REPO + "/genuine/efsrv_min/FATIOCLIENT.E32"
F32PROBE = REPO + "/genuine/f32_probe/F32PROBE.E32"

SEED = b"SEED-DATA-99887"
MARKER = "FATIO-MARKER-42A7"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
EXC_KEYS = ("Exc ", "PanicKernExec", "Panic", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "mtype"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def validate_inputs():
    ok = True
    for p in (KERNEL, EUSER, SERVER, CLIENT, F32PROBE):
        if not os.path.isfile(p):
            print(f"[FAIL] missing {p}")
            ok = False
    if not ok:
        sys.exit(1)
    for p in (SERVER, CLIENT, F32PROBE):
        r = sh("python3", REPO + "/tools/e32read.py", p, "--quiet")
        print(f"[{'PASS' if r.returncode == 0 else 'FAIL'}] E32 valid: {os.path.basename(p)}")
        ok &= r.returncode == 0
    return ok


def build_disk():
    for p in (DISK, FSIMG, SEEDFILE):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FSIMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FSIMG, check=True)
    sh("mmd", "-i", FSIMG, "::R1TEST", check=True)
    with open(SEEDFILE, "wb") as f:
        f.write(SEED)
    for src, name in ((EUSER, "::EUSER.DLL"),
                      (F32PROBE, "::F32PROBE.E32"),
                      (SERVER, "::PORTFILESRV.E32"),
                      (CLIENT, "::FATIOCLIENT.E32"),
                      (SEEDFILE, "::R1TEST/SEED.TXT")):
        sh("mcopy", "-i", FSIMG, src, name, check=True)
    with open(FSIMG, "rb") as f:
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
    with open(DISK, "r+b") as f:
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK}@@{OFFSET}", "::", check=True,
             capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for want in ("euserdll", "f32probe", "portfilesrv", "fatioclient", "r1test"):
        if want not in norm:
            sys.exit(f"disk root missing {want}: {out.stdout}")
    sub = sh("mdir", "-i", f"{DISK}@@{OFFSET}", "::R1TEST", check=True,
             capture_output=True, text=True)
    if "seedtxt" not in "".join(sub.stdout.lower().split()).replace(".", ""):
        sys.exit(f"R1TEST missing SEED.TXT: {sub.stdout}")
    print("mdir OK: genuine EFSRV host, FATIO client, F32PROBE, R1TEST/SEED.TXT staged")


def wait_for(path, marker, start=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, "rb") as f:
            f.seek(start)
            data = f.read()
        if marker in data:
            return start + data.index(marker) + len(marker)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {marker!r} (see {path})")


def run_qemu(log_path):
    # NOTE: no snapshot=on -- writes must persist to DISK for host verification.
    qemu = ["qemu-system-i386", "-M", "q35", "-m", "128",
            "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    print("== booting QEMU: genuine EFSRV -> real FAT32 volume ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, b"C:/> ", timeout=60)

            def send(text, expect, timeout=60):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("PORTFILESRV.E32", b"GENUINEFS: starting", 40)
            pos = wait_for(log_path, b"C:/> ", pos, 30)
            send("f32probe", b"F32PROBE: done", 40)
            pos = wait_for(log_path, b"C:/> ", pos, 30)
            send("FATIOCLIENT.E32", b"FATIOCLIENT: done", 60)
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def verify_on_disk():
    """Read FATIO.TXT from the FAT partition with mtools; this can only succeed
    with the marker if the server committed it to the real FAT volume."""
    r = sh("mtype", "-i", f"{DISK}@@{OFFSET}", "::R1TEST/FATIO.TXT",
           capture_output=True)
    data = r.stdout.decode("utf-8", "replace") if r.returncode == 0 else ""
    good = (r.returncode == 0) and (MARKER in data)
    print(f"[{'PASS' if good else 'FAIL'}] marker present on FAT volume "
          f"(mtype r={r.returncode}, bytes={data.strip()!r})")
    return good


def check(text):
    markers = [
        "GENUINEFS: starting",
        "F32PROBE: genuine FAT32 BPB IsValid=1",   # genuine F32 parsing
        "F32PROBE: genuine FAT type=32",
        "F32PROBE: genuine FirstDataSector=1056",
        "F32PROBE: done",
        "FATIOCLIENT: start",
        "FATIOCLIENT: connected",
        "SEED-DATA-99887",                          # read from FAT volume
        "FATIOCLIENT: seed MATCH",
        "FATIO-MARKER-42A7",                        # read back after reopen
        "FATIOCLIENT: marker MATCH",
        "FATIOCLIENT: round trip OK",
        "FATIOCLIENT: done",
    ]
    ok = True
    for m in markers:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {m}")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    good = not exc
    ok &= good
    print(f"  [{'PASS' if good else 'FAIL'}] no Exc/Panic/FAULT")
    if exc:
        for ln in exc[:8]:
            print("    " + ln)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    check_tools()
    ok = validate_inputs()
    if not args.no_rebuild_disk:
        build_disk()
    try:
        run_qemu(args.log)
    except (RuntimeError, OSError) as e:
        print(f"[FAIL] QEMU run: {e} (see {args.log})")
        return 1
    text = open(args.log, "r", errors="replace").read()
    ok &= check(text)
    ok &= verify_on_disk()
    print(f"[{'PASS' if ok else 'FAIL'}] r1_genuine_fat_io_test (log {args.log})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

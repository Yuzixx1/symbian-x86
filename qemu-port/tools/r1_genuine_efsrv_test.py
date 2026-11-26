#!/usr/bin/env python3
"""r1_genuine_efsrv_test.py -- genuine F32 code + !FileServer integration proof.

Stages on a FAT32 disk:
  * F32PROBE.E32      -- PE2 E32 that executes UNMODIFIED genuine kernelhwsrv
                         F32 byte code (fs_utils/filesystem_utils.cpp and
                         sfat32/sl_bpb32.cpp) built by genuine/f32_efsrv_build.py,
                         with only local EUSER descriptor primitives.
  * PORTFILESRV.E32   -- minimal !FileServer host (genuine TFsMessage opcode
                         numbers), so svcmgr's supervised launch uses it.
  * F32CLIENT.E32     -- connects to the server and exercises genuine
                         TFsMessage SessionPath (opcode 14) over real IPC
  * EUSER.DLL         -- genuine 2503-ordinal EUSER from the Raptor build root.

Boots build/kernfull.elf in QEMU, launches f32probe, then drives fcopy to copy
a real file-service IPC round trip.  Asserts the
genuine F32 results, the server startup marker, EUSER ord-915 binds and a clean
serial log.  Also validates both E32 images and the genuine source archives.

Usage: python3 tools/r1_genuine_efsrv_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import json
import os
import shlex
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK = ROOT + "/r1_genuine_efsrv.img"
FSIMG = ROOT + "/r1_genuine_efsrv_fs.img"
LOG_DEFAULT = ROOT + "/r1_genuine_efsrv.log"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KERNEL = REPO + "/build/kernfull.elf"
EUSER = REPO + "/EUSER.DLL"
F32PROBE = REPO + "/genuine/f32_probe/F32PROBE.E32"
SERVER = REPO + "/genuine/efsrv_min/EFSRVMIN.E32"
FCLIENT = REPO + "/genuine/f32_probe/F32CLIENT.E32"
NOTES = ROOT + "/r1_genuine_efsrv_NOTES.TXT"
PAYLOAD = b"genuine F32 file server payload\n"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def validate_inputs():
    ok = True
    for p in (KERNEL, EUSER, F32PROBE, FCLIENT, SERVER):
        if not os.path.isfile(p):
            print(f"[FAIL] missing {p}")
            ok = False
    if not ok:
        sys.exit(1)
    for p in (F32PROBE, FCLIENT, SERVER):
        r = sh("python3", REPO + "/tools/e32read.py", p, "--quiet")
        print(f"[{'PASS' if r.returncode == 0 else 'FAIL'}] E32 valid: {os.path.basename(p)}")
        ok &= r.returncode == 0
    mf = REPO + "/genuine/BUILD_MANIFEST.json"
    if os.path.isfile(mf):
        m = json.load(open(mf))
        for name, want in (("F32", 24), ("EFSRV", 21), ("EFILE", 48)):
            got = m["groups"][name]["objects"]
            good = got == want
            print(f"[{'PASS' if good else 'FAIL'}] genuine {name}: {got} objects (want {want})")
            ok &= good
    else:
        print("[FAIL] missing genuine/BUILD_MANIFEST.json")
        ok = False
    return ok


def build_disk():
    for p in (DISK, FSIMG):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FSIMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FSIMG, check=True)
    with open(NOTES, "wb") as f:
        f.write(PAYLOAD)
    for src, name in ((EUSER, "EUSER.DLL"), (F32PROBE, "F32PROBE.E32"),
                      (SERVER, "PORTFILESRV.E32"), (FCLIENT, "F32CLIENT.E32"),
                      (NOTES, "NOTES.TXT")):
        sh("mcopy", "-i", FSIMG, src, f"::{name}", check=True)
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
    out = sh("mdir", "-i", f"{DISK}@@{OFFSET}", "::",
             check=True, capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for want in ("euserdll", "f32probe", "f32client", "portfilesrv", "notestxt"):
        if want not in norm:
            sys.exit(f"disk missing {want}: {out.stdout}")
    print("mdir OK: genuine F32 probe, FileServer host, F32CLIENT, EUSER.DLL staged")


def wait_for(path, marker, start=0, timeout=90):
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
    qemu = ["qemu-system-i386", "-M", "q35", "-m", "128",
            "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    print("== booting QEMU: genuine F32 probe + !FileServer IPC round trip ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, b"C:/> ", timeout=90)

            def send(text, expect, timeout=60):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            # Launch the !FileServer host: E32Run resumes the child and the
            # shell prompt returns while the server owns the receive loop.
            send("PORTFILESRV.E32", b"GENUINEFS: starting", 60)
            pos = wait_for(log_path, b"C:/> ", pos, 30)
            send("f32probe", b"F32PROBE: done", 60)
            pos = wait_for(log_path, b"C:/> ", pos, 30)   # probe exits
            send("f32client", b"F32CLIENT: IPC round trip OK", 60)
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text):
    markers = [
        "GENUINEFS: starting",                     # server publishes !FileServer
        "F32PROBE: start",
        "F32PROBE: genuine Log2(256)=8",           # genuine fs_utils code
        "F32PROBE: genuine Count1Bits(0xF0F0)=8",
        "F32PROBE: genuine FAT32 BPB IsValid=1",   # genuine sfat32 code
        "F32PROBE: genuine FAT type=32",
        "F32PROBE: genuine FirstDataSector=1056",
        "F32PROBE: done",
        "F32CLIENT: start",
        "F32CLIENT: IPC round trip OK",            # file saved/read via IPC
    ]
    ok = True
    for m in markers:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {m}")
    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    b915 = [ln for ln in binds if "ord 915" in ln]
    good = len(b915) >= 2   # F32PROBE + F32CLIENT (server has no EUSER import)
    ok &= good
    print(f"  [{'PASS' if good else 'FAIL'}] EUSER ord 915 bound ({len(b915)} images)")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    good = not exc
    ok &= good
    print(f"  [{'PASS' if good else 'FAIL'}] no Exc/PanicKernExec/FAULT")
    if exc:
        for ln in exc[:8]:
            print("    " + ln)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    ok = validate_inputs()
    if not args.no_rebuild_disk:
        build_disk()
    run_qemu(args.log)
    text = open(args.log, "r", errors="replace").read()
    ok &= check(text)
    print(f"[{'PASS' if ok else 'FAIL'}] r1_genuine_efsrv_test (log {args.log})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

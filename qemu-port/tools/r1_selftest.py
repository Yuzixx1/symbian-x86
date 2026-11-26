#!/usr/bin/env python3
"""r1_selftest.py -- QEMU test for the single comprehensive OS self-test E32.

Stages build/user/SELFTEST.E32 and the genuine EUSER.DLL (ordinal 915 =
RDebug::Printf) on a FAT32 disk, boots the R1 kernel, launches `selftest`
from the serial shell and checks that every core service reports OK and the
program ends with a single RESULT PASS verdict, with no guest exception or
kernel panic.  Exits 0 only if every check passes.

Usage: python3 tools/r1_selftest.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import fcntl
import os
import shlex
import shutil
import subprocess
import sys
import time
from contextlib import contextmanager

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Per-run (per-PID) paths: concurrent selfcheck/final-verify runs must never
# share or truncate each other's disk images or serial log (see
# docs/SELFTEST_MARKER_REGRESSION.md).
RUNDIR = f"/tmp/opencode/r1_selftest_{os.getpid()}"
DISK = os.path.join(RUNDIR, "r1_selftest.img")
FS = os.path.join(RUNDIR, "r1_selftest_fs.img")
LOG = os.path.join(RUNDIR, "r1_selftest.log")
# Exclusive lock so only one selftest runs at a time.
LOCK = "/tmp/opencode/r1_selftest.lock"
# QEMU prints this line itself when `timeout` SIGTERMs it; its presence means
# the redirected serial log is complete.
TERMINATOR = "terminating on signal"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/SELFTEST.E32")
DLL = os.path.join(ROOT, "EUSER.DLL")
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

# One marker per service, in the order selftest.c runs them.  Each marker is
# emitted through EUSER ordinal 915 and the DebugPrint fallback.
MARKERS = [
    "ST: SELFTEST START - EKA2 PC OS core services",
    "ST: euser OK",
    "ST: malloc OK",
    "ST: chunk OK",
    "ST: file OK",
    "ST: ipc OK",
    "ST: netbridge OK",
    "ST: failures=0",
    "ST: RESULT PASS - all core services operational",
]
EXC_KEYS = ("Exc ", "PanicKernExec")


@contextmanager
def selftest_lock():
    """Exclusive flock so only one selftest runs at a time.

    A parent harness (r1_os_selfcheck.py / r1_final_verify.py) that already
    holds the lock exports R1_SELFTEST_LOCK_HELD=1; in that case this is a
    no-op so a nested r1_selftest.py never deadlocks on the same lock file.
    """
    if os.environ.get("R1_SELFTEST_LOCK_HELD") == "1":
        yield
        return
    os.makedirs(os.path.dirname(LOCK), exist_ok=True)
    fh = open(LOCK, "a+")
    try:
        fcntl.flock(fh.fileno(), fcntl.LOCK_EX)
        os.environ["R1_SELFTEST_LOCK_HELD"] = "1"   # inherited by children
        print(f"== selftest lock acquired: {LOCK}", flush=True)
        yield
    finally:
        try:
            fcntl.flock(fh.fileno(), fcntl.LOCK_UN)
        finally:
            fh.close()


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def scan_exceptions(text):
    return [line for line in text.splitlines()
            if any(k in line for k in EXC_KEYS)]


def build_disk():
    os.makedirs(RUNDIR, exist_ok=True)
    for path in (DISK, FS):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "SELFTEST.E32"), (DLL, "EUSER.DLL")):
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
    for wanted in ("selfteste32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: SELFTEST.E32 and EUSER.DLL staged")


def run_qemu(log):
    qemu = ["timeout", "90", "qemu-system-i386", "-M", "q35", "-m", "128",
            "-netdev", "user,id=n0",
            "-device", "e1000,netdev=n0",
            "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    # Asic::Init3 already mounts the FAT partition at boot; a shell `mount`
    # hangs on the M-03 per-process-page-table kernel (see r1_qemu_smoke.py).
    # Only SELFTEST.E32 and EUSER.DLL are staged on the FAT image, so a
    # `portfilesrv` feed only ever draws "portfilesrv: not found".  Selftest's
    # file test is served by the kernel's genuine PortFile/PortBigFile devices
    # (variant/userfiles.cpp), which need no user-side server.  The extra sleep
    # keeps the pre-existing serial timing envelope exactly (1.5 + 3.0 + 6.0).
    feeder = ("sleep 1.5; printf '\\n'; sleep 9.0; printf 'selftest\\n'; "
              "sleep 10.0")
    cmd = ("{ " + feeder + "; } | "
           + " ".join(shlex.quote(a) for a in qemu)
           + " > " + shlex.quote(log) + " 2>&1")
    print("== QEMU (90s cap), feeding 'selftest' ==")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit: {r.returncode} (124 == timeout expected)")
    # Serial stdio can lag the wrapper's exit: re-read the log until QEMU's
    # own terminating line shows up (or a short grace deadline passes)
    # instead of trusting one fixed-delay snapshot.
    deadline = time.monotonic() + 10.0
    while True:
        try:
            with open(log, "r", errors="replace") as f:
                text = f.read()
        except FileNotFoundError:
            text = ""
        if TERMINATOR in text or time.monotonic() >= deadline:
            return text
        time.sleep(0.25)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")
    os.makedirs(RUNDIR, exist_ok=True)
    with selftest_lock():
        if not (args.no_rebuild_disk and os.path.exists(DISK)):
            build_disk()
        text = run_qemu(args.log)
    # Keep the per-PID log for post-mortem; drop the bulky images so repeated
    # runs do not fill the log filesystem.
    for path in (DISK, FS):
        try:
            os.remove(path)
        except OSError:
            pass

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

    excs = scan_exceptions(text)
    ok = ok and not excs
    print(f"  [{'PASS' if not excs else 'FAIL'}] no guest exceptions ({len(excs)})")

    if excs:
        print("EXCEPTION LINES:")
        for e in excs[:8]:
            print("  " + e)

    print(f"RESULT: {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

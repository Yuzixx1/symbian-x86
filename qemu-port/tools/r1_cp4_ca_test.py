#!/usr/bin/env python3
"""r1_cp4_ca_test.py — automated QEMU test for the CP4 CActive fixture.

Builds a 32 MB raw disk with an MBR (one bootable 0x0C FAT32 partition at
LBA 2048) and a standalone FAT32 image embedded at byte offset 1048576.
ca_test (built by tools/mke32_ca_test.py) and EUSER.DLL are staged into the
FAT image with mtools before it is embedded. QEMU is booted like the
known-working smoke-test command, the serial console is fed timed input and
the serial log is checked for the expected markers (the disk auto-mounts at
boot, so no `mount` command is sent). Exits 0 only if all markers pass and
in order. The fixture binds the genuine EUSER RTimer/CTimer and request
ordinals (887, 881, 885, 925, 928, 929, 617, 626) alongside the scheduler
ordinals, and proves the Start() -> WaitForAnyRequest() -> RunL(vptr) ->
Stop() -> Start() returns round trip.

Usage:
    python3 r1_cp4_ca_test.py [--kernel KERN] [--no-rebuild-disk] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

DISK_IMG = "/tmp/opencode/r1_ca_test.img"
FS_IMG = "/tmp/opencode/r1_ca_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_ca_test.log"

DISK_MB = 32
PART_START_LBA = 2048
PART_SECTORS = 16384            # 8 MB partition
FS_SIZE = PART_SECTORS * 512    # standalone FAT image == partition size
OFFSET_BYTES = PART_START_LBA * 512  # 1048576

# Output of tools/mke32_ca_test.py (may be overridden with --e32).
CA_TEST_E32 = "/tmp/opencode/ca_test.e32"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
DEFAULT_KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"

# Ordinals the fixture imports from EUSER.DLL (see tools/mke32_ca_test.py).
# Genuine Start()/Stop() round trip: RunL() is dispatched by the real scheduler
# via the object vptr; WaitForAnyRequest() is the genuine int 0x20 fast exec,
# woken by the kernel timer device (the executive beneath RTimer::After).
# Scheduler/active: 430 ctor, 428 Install, 426 Start, 425 Stop, 1091 CActive
# ctor, 1090 SetActive, 915 RDebug::Printf (markers), 424 Add.
# RTimer/CTimer + request surface: 887, 881, 885, 925, 928, 929, 617, 626.
BIND_ORDS = [430, 428, 426, 425, 1091, 1090, 915, 424,
             887, 881, 885, 925, 928, 929, 617, 626]

# Emitted by the fixture, in order, only if the CP4 round trip succeeds.
MARKERS = [
    "ca_test: CActiveScheduler::Install ok",
    "ca_test: Start() entered",
    "ca_test: RunL() reached",
    "ca_test: Stop() called",
    "ca_test: Start() returned",
    "ca_test: clean exit",
]

FAT_FILES = [
    (CA_TEST_E32, "ca_test"),
    (EUSER_DLL, "EUSER.DLL"),
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_disk(e32):
    print(f"== building disk image {DISK_IMG} ({DISK_MB} MB)")
    for p in (DISK_IMG, FS_IMG):
        if os.path.exists(p):
            os.remove(p)

    with open(DISK_IMG, "wb") as f:
        f.truncate(DISK_MB * 1024 * 1024)

    with open(FS_IMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)

    for src, name in FAT_FILES:
        if src == CA_TEST_E32:
            src = e32
        if not os.path.exists(src):
            sys.exit(f"source file missing: {src} (run tools/mke32_ca_test.py)")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)

    with open(FS_IMG, "rb") as f:
        fs_data = f.read()
    if len(fs_data) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    with open(DISK_IMG, "r+b") as f:
        f.seek(OFFSET_BYTES)
        f.write(fs_data)

    entry = (
        bytes([0x80, 0x00, 0x02, 0x00, 0x0C, 0x00, 0x02, 0x00])
        + PART_START_LBA.to_bytes(4, "little")
        + PART_SECTORS.to_bytes(4, "little")
    )
    mbr = bytearray(512)
    mbr[446:462] = entry
    mbr[510] = 0x55
    mbr[511] = 0xAA
    with open(DISK_IMG, "r+b") as f:
        f.write(mbr)
    print(f"MBR: part0 bootable type=0x0C start_lba={PART_START_LBA} sectors={PART_SECTORS}")

    out = sh("mdir", "-i", f"{DISK_IMG}@@{OFFSET_BYTES}", "::",
             check=True, capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for wanted in ("ca_test", "euserdll"):
        if wanted not in norm:
            print(f"mdir did not list {wanted}:\n{out.stdout}")
            sys.exit(1)
    print("mdir OK: disk lists staged files")


def run_qemu(kernel, log):
    qemu = [
        "timeout", "60", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    qemu_str = " ".join(shlex.quote(a) for a in qemu)
    # Asic::Init3 already mounts the FAT partition at boot; a shell `mount`
    # hangs on the M-03 per-process-page-table kernel (see r1_web_browser_test.py).
    # Timed serial feed: leading newline, then launch the CA fixture directly.
    feeder = "sleep 1.5; printf '\\n'; sleep 3.0; printf 'ca_test\\n'; sleep 5.0"
    cmd = f"{{ {feeder}; }} | {qemu_str} > {shlex.quote(log)} 2>&1"
    print("== starting qemu (60s cap), feeding serial console ...")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit code: {r.returncode} (124 == timeout 60 killed qemu, expected)")
    # timeout's SIGTERM can reap qemu before redirected serial stdio is flushed.
    # Poll until the log size stops growing so the final markers are captured.
    last = -1
    for _ in range(20):
        try:
            size = os.path.getsize(log)
        except FileNotFoundError:
            size = -1
        if size == last and size >= 0:
            break
        last = size
        time.sleep(0.5)
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def check_markers(text):
    results = {m: (m in text) for m in MARKERS}
    # Presence alone is not enough: the six markers must appear in the order
    # that proves the genuine Start()/Stop() round trip through the scheduler.
    pos = [text.find(m) for m in MARKERS]
    results["marker order (Install->Start->RunL->Stop->returned->exit)"] = (
        all(p >= 0 for p in pos)
        and all(pos[i] < pos[i + 1] for i in range(len(pos) - 1))
    )
    bind_lines = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    for ordn in BIND_ORDS:
        key = f"e32run: bind slot ... ord {ordn}"
        results[key] = any(f"ord {ordn}" in ln for ln in bind_lines)
    return results


def main():
    ap = argparse.ArgumentParser(description="R1 CP4 CActive QEMU test")
    ap.add_argument("--kernel", default=DEFAULT_KERNEL,
                    help="kernel ELF (default: %(default)s)")
    ap.add_argument("--e32", default=CA_TEST_E32,
                    help="ca_test E32 pack (default: %(default)s)")
    ap.add_argument("--no-rebuild-disk", action="store_true",
                    help="reuse existing disk image instead of rebuilding")
    ap.add_argument("--log", default=LOG_DEFAULT,
                    help="path to save the serial log (default: %(default)s)")
    args = ap.parse_args()

    check_tools()

    if args.no_rebuild_disk and os.path.exists(DISK_IMG):
        print(f"== reusing existing disk image {DISK_IMG}")
    else:
        build_disk(args.e32)

    text = run_qemu(args.kernel, args.log)
    results = check_markers(text)

    print("\n== marker results")
    all_pass = True
    for m, ok in results.items():
        all_pass = all_pass and ok
        print(f"  [{'PASS' if ok else 'FAIL'}] {m}")
    print(f"serial log: {args.log}")

    if not all_pass:
        print("\n== relevant log lines:")
        keys = ("e32run", "ca_test", "exc 06", "kern-heap")
        shown = set()
        for line in text.splitlines():
            low = line.lower()
            if any(k in low for k in keys) and line not in shown:
                print("   ", line[:220])
                shown.add(line)
        print("\nRESULT: FAIL")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_qemu_smoke.py — automated QEMU smoke test for the R1 E32 port.

Builds a 32 MB raw disk image with an MBR partition table (one bootable
0x0C FAT32 partition starting at LBA 2048) and a standalone FAT32 image
embedded at byte offset 2048*512 (1048576). portfilesrv, EUSER.DLL and
optionally /tmp/opencode/test_hello.e32 are staged into the FAT image
with mtools before it is embedded. QEMU is then booted exactly like the
known-working reference command, the serial console is fed timed input
(a leading newline, then "mount", then "portfilesrv") via a bash
subprocess with sleeps, and the captured serial log is checked for the
expected markers. Prints PASS/FAIL per marker; exits 0 only if all pass.

Usage:
    python3 r1_qemu_smoke.py [--kernel KERN] [--no-rebuild-disk] [--log LOG]

Only this file is created/edited; no other project file is touched.
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/r1_test.img"
FS_IMG = "/tmp/opencode/r1_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_qemu_smoke.log"

DISK_MB = 32
PART_START_LBA = 2048
PART_SECTORS = 16384            # 8 MB partition
FS_SIZE = PART_SECTORS * 512    # standalone FAT image == partition size
OFFSET_BYTES = PART_START_LBA * 512  # 1048576

PORTFILESrv = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
HELLO_E32 = "/tmp/opencode/test_hello.e32"
DEFAULT_KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"

MARKERS = [
    "E32Validate: expDesc tail OK expDescSize=45 expDescType=2",
    "E32LoadDll: E32Validate r=0",
    "e32run: EUSER.DLL resident",
    "exports=2503",
    "e32run: bind slot 12348 ord 1221",
    "e32run: bind slot 12352 ord 1953",
    "imports bound dll (3 ordinals)",
    "ProcessCreate r=0",
    "Loaded r=0",
    "e32run: Resume()...",
    "no Exc or PanicKernExec in log",
]

EXC_KEYS = ("Exc ", "PanicKernExec")


def scan_exceptions(text):
    """Return serial-log lines that indicate a guest exception or kernel panic."""
    return [line for line in text.splitlines()
            if any(k in line for k in EXC_KEYS)]


FAT_FILES = [
    (PORTFILESrv, "portfilesrv"),
    (EUSER_DLL, "EUSER.DLL"),
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_disk():
    print(f"== building disk image {DISK_IMG} ({DISK_MB} MB)")
    for p in (DISK_IMG, FS_IMG):
        if os.path.exists(p):
            os.remove(p)

    # 32 MB raw image, zeroed.
    with open(DISK_IMG, "wb") as f:
        f.truncate(DISK_MB * 1024 * 1024)

    # Standalone FAT32 image, sized exactly to the partition.
    with open(FS_IMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)

    # Stage files into the standalone FAT image with mtools (before embedding).
    copied_hello = False
    for src, name in FAT_FILES:
        if not os.path.exists(src):
            sys.exit(f"source file missing: {src}")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)
    if os.path.exists(HELLO_E32):
        sh("mcopy", "-i", FS_IMG, HELLO_E32, "::hello.e32", check=True)
        copied_hello = True
    else:
        print(f"skip: {HELLO_E32} not present")

    # Embed the FAT image at LBA 2048 (byte offset 1048576).
    with open(FS_IMG, "rb") as f:
        fs_data = f.read()
    if len(fs_data) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    with open(DISK_IMG, "r+b") as f:
        f.seek(OFFSET_BYTES)
        f.write(fs_data)

    # MBR at sector 0: bootable 0x0C partition, start LBA 2048, 16384 sectors.
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

    # Verify with mtools (partition offset is in BYTES: 1048576).
    out = sh("mdir", "-i", f"{DISK_IMG}@@{OFFSET_BYTES}", "::",
             check=True, capture_output=True, text=True)
    # mdir may show 8.3 short names ("EUSER DLL") or long names; normalize
    # whitespace/dots so either form matches.
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for wanted in ("portfilesrv", "euserdll") + (("helloe32",) if copied_hello else ()):
        if wanted not in norm:
            print(f"mdir did not list {wanted}:\n{out.stdout}")
            sys.exit(1)
    print("mdir OK: disk lists staged files")


def run_qemu(kernel, log):
    qemu = [
        "timeout", "50", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    qemu_str = " ".join(shlex.quote(a) for a in qemu)
    # Timed serial feed inside a bash subprocess: leading newline, "mount",
    # delay, "portfilesrv", delay. A pipe would race; bash sleeps give the
    # guest shell time to initialize between lines.
    feeder = (
        "sleep 0.8; printf '\\n'; "
        "sleep 0.8; printf 'mount\\n'; "
        "sleep 2.0; printf 'portfilesrv\\n'; "
        "sleep 3.0"
    )
    cmd = f"{{ {feeder}; }} | {qemu_str} > {shlex.quote(log)} 2>&1"
    print("== starting qemu (50s cap), feeding serial console ...")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit code: {r.returncode} (124 == timeout 50 killed qemu, expected)")
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def check_markers(text):
    results = {m: (m in text) for m in MARKERS}
    # The final marker is condition-based, not a literal substring check:
    # it passes only when no exception/panic lines appear anywhere in the log.
    results["no Exc or PanicKernExec in log"] = not scan_exceptions(text)
    return results


def main():
    ap = argparse.ArgumentParser(description="R1 QEMU smoke test")
    ap.add_argument("--kernel", default=DEFAULT_KERNEL,
                    help="kernel ELF (default: %(default)s)")
    ap.add_argument("--no-rebuild-disk", action="store_true",
                    help="reuse existing disk image instead of rebuilding")
    ap.add_argument("--log", default=LOG_DEFAULT,
                    help="path to save the serial log (default: %(default)s)")
    args = ap.parse_args()

    check_tools()

    if args.no_rebuild_disk and os.path.exists(DISK_IMG):
        print(f"== reusing existing disk image {DISK_IMG}")
    else:
        build_disk()

    text = run_qemu(args.kernel, args.log)
    results = check_markers(text)

    # Fail on any guest exception or kernel panic in the serial log, even if
    # the shell-side markers above still printed (avoids false passes when a
    # fixture thread dies right after start).
    exc_lines = scan_exceptions(text)
    if exc_lines:
        print("\n== exception/panic lines detected in serial log:")
        for line in exc_lines:
            print("   ", line[:220])

    print("\n== marker results")
    all_pass = True
    for m in MARKERS:
        ok = results[m]
        all_pass = all_pass and ok
        print(f"  [{'PASS' if ok else 'FAIL'}] {m}")
    print(f"serial log: {args.log}")

    if not all_pass:
        print("\n== relevant log lines:")
        keys = ("e32validate", "e32loaddll", "e32run", "imports bound",
                "processcreate", " loaded", "portfilesrv started")
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

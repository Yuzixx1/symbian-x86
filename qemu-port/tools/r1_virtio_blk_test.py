#!/usr/bin/env python3
"""r1_virtio_blk_test.py -- automated QEMU test for the new virtio-blk driver.

Creates an 8 MiB raw disk at /tmp/opencode/vblk_test.img, boots the full Kern
ELF with that disk attached as a virtio-blk PCI device (in addition to any
existing AHCI test disk), feeds `vblk` and `vblk info` to the shell, and checks
the serial log for the virtio-blk capacity marker. Exits 0 only if every marker
passes AND no guest exception / PanicKernExec line appears.

Usage:
    python3 tools/r1_virtio_blk_test.py [--kernel KERN] [--no-rebuild-disk] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/vblk_test.img"
EXISTING_DISK = "/tmp/opencode/r1_test.img"       # optional AHCI disk from r1 smoke
DEFAULT_KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"
LOG_DEFAULT = "/tmp/opencode/r1_virtio_blk_test.log"

DISK_MB = 8
EXPECTED_SECTORS = DISK_MB * 1024 * 1024 // 512   # 16384
EXPECTED_MB = DISK_MB                             # 8
MAGIC = b"VBLKOK!!"                               # seeded into sector 0

MARKERS = [
    "virtio-blk:",
    "capacity=%d sectors (%d MB)" % (EXPECTED_SECTORS, EXPECTED_MB),
    "vblk: info ok",
    "vblk test: read=0 first=56:42:4c:4b:4f:4b:21:21",  # "VBLKOK!!" via virtqueue
    "vblk test: write=0 readback=0 first=56:42:4c:4b:57:52:54:21",  # "VBLKWRT!" write+readback
    "no Exc or PanicKernExec in log",
]

EXC_KEYS = ("Exc ", "PanicKernExec")


def scan_exceptions(text):
    return [line for line in text.splitlines()
            if any(k in line for k in EXC_KEYS)]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_disk():
    print(f"== creating {DISK_MB} MiB virtio-blk image {DISK_IMG}")
    os.makedirs(os.path.dirname(DISK_IMG), exist_ok=True)
    with open(DISK_IMG, "wb") as f:
        f.truncate(DISK_MB * 1024 * 1024)
        f.seek(0)
        f.write(MAGIC)                            # magic at sector 0 for read test
    print(f"   capacity should be {EXPECTED_SECTORS} sectors ({EXPECTED_MB} MB)")


def run_qemu(kernel, log):
    qemu = [
        "timeout", "50", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0",
        "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0",
        "-device", "AC97,audiodev=a0",
        "-vga", "std",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},if=virtio,format=raw",
    ]
    # Include the existing r1 AHCI disk as well, when present.
    if os.path.exists(EXISTING_DISK):
        qemu += [
            "-drive", f"file={EXISTING_DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
        ]
    qemu += ["-monitor", "none", "-no-reboot"]

    qemu_str = " ".join(shlex.quote(a) for a in qemu)
    # Timed serial feed: boot, then `vblk`, `vblk info`, `vblk test`.
    feeder = (
        "sleep 1.5; printf '\\n'; "
        "sleep 2.0; printf 'vblk\\n'; "
        "sleep 1.0; printf 'vblk info\\n'; "
        "sleep 1.0; printf 'vblk test\\n'; "
        "sleep 4.0"
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
    results["no Exc or PanicKernExec in log"] = not scan_exceptions(text)
    return results


def main():
    ap = argparse.ArgumentParser(description="virtio-blk QEMU smoke test")
    ap.add_argument("--kernel", default=DEFAULT_KERNEL,
                    help="kernel ELF (default: %(default)s)")
    ap.add_argument("--no-rebuild-disk", action="store_true",
                    help="reuse existing virtio-blk image instead of recreating it")
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

    print("\n== virtio-blk / vblk log lines:")
    for line in text.splitlines():
        if "virtio-blk" in line or line.lower().startswith("vblk ") or line.lower().startswith("vblk:"):
            print("   ", line[:220])

    print(f"serial log: {args.log}")
    if not all_pass:
        print("\nRESULT: FAIL")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

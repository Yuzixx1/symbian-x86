#!/usr/bin/env python3
"""r1_virtio_net_test.py -- QEMU test for the alternative virtio-net backend.

Boots build/kernfull.elf on q35 with BOTH the existing e1000 and a new
virtio-net-pci device on user-mode SLIRP, feeds the shell `vnet` and
`vnet info`, and checks for the driver's init markers plus a non-zero MAC.
Exits 0 only when every marker passes and no Exc/PanicKernExec appears.

Usage:
    python3 tools/r1_virtio_net_test.py [--kernel KERN] [--log LOG]
                                       [--no-build] [--no-rebuild-disk]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys

ROOT = "/home/yuzi/Documents/Sybian REAL/qemu-port"
DEFAULT_KERNEL = ROOT + "/build/kernfull.elf"
LOG_DEFAULT = "/tmp/opencode/r1_virtio_net_test.log"
DISK_IMG = "/tmp/opencode/r1_test.img"

EXC_KEYS = ("Exc ", "PanicKernExec")
# virtio-net: BAR0=<hex> MAC=<byte>:<byte>:... queues ready
MAC_RE = re.compile(r"virtio-net: BAR0=([0-9a-fA-F]+) MAC=((?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}) queues ready")
LITERAL_MARKERS = (
    "virtio-net: BAR0=",
    "queues ready",
    "vnet: BAR0=",
    "vnet: MAC=",
    "vnet: RX n=",
    "vnet: info ok",
)


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "timeout"):
        if not shutil.which(t):
            sys.exit("required tool missing: " + t)


def maybe_build(no_build):
    if no_build:
        print("== skipping build (--no-build)")
        return
    sh("make", "-C", ROOT, "build/kernfull.elf", check=True)


def maybe_disk(no_rebuild):
    """Reuse the disk built by r1_qemu_smoke.py when present; else a blank one."""
    if os.path.exists(DISK_IMG):
        print("== reusing disk image " + DISK_IMG)
        return
    print("== creating blank 32MB disk " + DISK_IMG)
    with open(DISK_IMG, "wb") as f:
        f.truncate(32 * 1024 * 1024)


def run_qemu(kernel, log):
    qemu = [
        "timeout", "50", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        # existing e1000 backend (kept alongside the new one)
        "-netdev", "user,id=n0",
        "-device", "e1000,netdev=n0",
        # new alternative backend under test
        "-netdev", "user,id=v0",
        "-device", "virtio-net-pci,netdev=v0",
        "-vga", "std",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", "file=" + DISK_IMG + ",format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    qemu_str = " ".join(shlex.quote(a) for a in qemu)
    feeder = (
        "sleep 1.5; printf '\\n'; "
        "sleep 2.5; printf 'vnet\\n'; "
        "sleep 1.0; printf 'vnet info\\n'; "
        "sleep 3.0"
    )
    cmd = "{ " + feeder + "; } | " + qemu_str + " > " + shlex.quote(log) + " 2>&1"
    print("== starting qemu (50s cap), feeding vnet commands ...")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print("qemu wrapper exit code: %d (124 == timeout, expected)" % r.returncode)
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def scan_exceptions(text):
    return [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]


def check(text):
    results = {}
    for m in LITERAL_MARKERS:
        results[m] = m in text
    matches = MAC_RE.findall(text)
    mac = matches[0][1] if matches else ""
    results["non-zero virtio-net MAC"] = bool(mac) and any(
        b != "00" for b in mac.split(":"))
    results["no Exc or PanicKernExec in log"] = not scan_exceptions(text)
    return results, mac


def main():
    ap = argparse.ArgumentParser(description="virtio-net QEMU test")
    ap.add_argument("--kernel", default=DEFAULT_KERNEL)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()

    check_tools()
    maybe_build(args.no_build)
    maybe_disk(args.no_rebuild_disk)
    text = run_qemu(args.kernel, args.log)
    results, mac = check(text)

    exc_lines = scan_exceptions(text)
    if exc_lines:
        print("\n== exception/panic lines detected:")
        for ln in exc_lines:
            print("   ", ln[:220])

    print("\n== marker results")
    all_pass = True
    for m, ok in results.items():
        all_pass = all_pass and ok
        print("  [%s] %s" % ("PASS" if ok else "FAIL", m))
    print("virtio-net MAC observed: %s" % (mac or "(none)"))
    print("serial log: " + args.log)

    if not all_pass:
        print("\n== virtio/vnet log lines:")
        for ln in text.splitlines():
            if "virtio" in ln.lower() or "vnet" in ln.lower():
                print("   ", ln[:220])
        print("\nRESULT: FAIL")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_usb_ohci_test.py — QEMU smoke test for the polled OHCI USB backend.

Boots build/kernfull.elf with a QEMU OHCI controller (pci-ohci) and one
USB mouse, feeds `usb` and `usb info` to the EKA2 shell over serial, and
checks the serial log for:

  * an `ohci: rev=<nn> ... ports=<n>` marker (boot constructor + `usb info`),
  * a downstream port count > 0,
  * no guest exception / kernel panic lines.

QEMU 10.x names the OHCI PCI controller `pci-ohci` (the `usb-ohci` id in
older docs is not valid here). The mouse is attached with
`-device usb-mouse,bus=ohci.0` so it lands on OHCI rather than the q35
default EHCI controller, making device-presence detection observable.
Nothing is built or written here except the log: no disk image is needed
because the kernel is loaded with -kernel.

Usage:
    python3 tools/r1_usb_ohci_test.py [--kernel KERN] [--log LOG]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys

DEFAULT_KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"
LOG_DEFAULT = "/tmp/opencode/r1_usb_ohci_test.log"
EXC_KEYS = ("Exc ", "PanicKernExec", "Panic ")
REV_RE = re.compile(r"ohci:\s*rev=([0-9a-fA-F]+)\s+bar=([0-9a-fA-F]+)"
                    r"\s+pci=([0-9a-fA-F:.]+)\s+ports=(\d+)")
CONN_RE = re.compile(r"ohci:\s*port\s+\d+\s+status=[0-9a-fA-F]+\s+conn=1")


def scan_exceptions(text):
    return [line for line in text.splitlines()
            if any(k in line for k in EXC_KEYS)]


def check_tools(kernel):
    for t in ("qemu-system-i386", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")
    if not os.path.exists(kernel):
        sys.exit(f"kernel missing: {kernel} (run: make kernfull)")


def run_qemu(kernel, log):
    qemu = [
        "timeout", "40", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-device", "pci-ohci,id=ohci",   # USB 1.1 OHCI host (class 0c.03.10)
        "-device", "usb-mouse,bus=ohci.0",  # attach the mouse to OHCI, not EHCI
        "-vga", "std",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-monitor", "none", "-no-reboot",
    ]
    # Timed serial feed in a bash pipeline: a plain pipe races the guest
    # shell's line editor, so give it time to boot before sending input.
    feeder = (
        "sleep 2.0; printf '\\n'; "
        "sleep 3.0; printf 'usb\\n'; "
        "sleep 2.0; printf 'usb info\\n'; "
        "sleep 3.0"
    )
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    print("== booting QEMU (40s cap), feeding `usb` + `usb info` ...")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit code: {r.returncode} (124 == timeout, expected)")
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def main():
    ap = argparse.ArgumentParser(description="OHCI USB QEMU smoke test")
    ap.add_argument("--kernel", default=DEFAULT_KERNEL)
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()

    check_tools(args.kernel)
    text = run_qemu(args.kernel, args.log)
    exc_lines = scan_exceptions(text)
    hits = REV_RE.findall(text)

    markers = []
    markers.append(("OHCI revision marker", bool(hits),
                    f"hits={len(hits)}"))
    ports = int(hits[0][3]) if hits else 0
    markers.append(("downstream ports > 0", ports > 0,
                    f"ports={ports}"))
    markers.append(("`usb info` dump (>=2 rev lines)", len(hits) >= 2,
                    f"hits={len(hits)}"))
    conn = len(CONN_RE.findall(text))
    markers.append(("device present on a root port", conn > 0,
                    f"connected ports={conn}"))
    markers.append(("no Exc/PanicKernExec", not exc_lines,
                    f"lines={len(exc_lines)}"))

    print("\n== marker results")
    all_pass = True
    for name, ok, detail in markers:
        all_pass = all_pass and ok
        print(f"  [{'PASS' if ok else 'FAIL'}] {name} ({detail})")
    if hits:
        print(f"revision={hits[0][0]} bar={hits[0][1]} "
              f"pci={hits[0][2]} ports={hits[0][3]}")
    if exc_lines:
        print("\n== exception/panic lines:")
        for line in exc_lines:
            print("   ", line[:220])
    print(f"serial log: {args.log}")
    print("\nRESULT: " + ("PASS" if all_pass else "FAIL"))
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_usb_ehci_test.py — QEMU smoke test for the polled EHCI USB2 backend.

Boots build/kernfull.elf on the q35 machine (which exposes the ICH9 USB2
EHCI controller, class 0c.03.20, by default) with one USB mouse, feeds
`ehci` and `ehci info` to the EKA2 shell over serial, and checks that:

  * an `ehci: ver=<hex> ... ports=<n>` marker appears (boot constructor and
    the explicit `ehci` / `ehci info` commands),
  * the reported downstream port count is > 0,
  * the `ehci info` dump printed the marker at least twice,
  * no guest exception / kernel panic lines appear in the log.

Nothing is built or written here except the serial log: the kernel is loaded
with -kernel, so no disk image is needed.

Usage:
    python3 tools/r1_usb_ehci_test.py [--kernel KERN] [--log LOG]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys

DEFAULT_KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"
LOG_DEFAULT = "/tmp/opencode/r1_usb_ehci_test.log"
EXC_KEYS = ("Exc ", "PanicKernExec", "Panic ")
VER_RE = re.compile(r"ehci:\s*ver=([0-9a-fA-F]+)\s+bar=([0-9a-fA-F]+)"
                    r"\s+pci=([0-9a-fA-F:.]+)\s+caplen=([0-9a-fA-F]+)"
                    r"\s+hccp=([0-9a-fA-F]+)\s+ports=(\d+)")
CONN_RE = re.compile(r"ehci:\s*port\s+\d+\s+status=[0-9a-fA-F]+\s+conn=1")


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
        "-usbdevice", "mouse",        # mouse on the q35 EHCI bus by default
        "-vga", "std",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-monitor", "none", "-no-reboot",
    ]
    # Timed serial feed in a bash pipeline: a plain pipe races the guest
    # shell's line editor, so give it time to boot before sending input.
    feeder = (
        "sleep 2.0; printf '\\n'; "
        "sleep 3.0; printf 'ehci\\n'; "
        "sleep 2.0; printf 'ehci info\\n'; "
        "sleep 3.0"
    )
    cmd = (f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} "
           f"> {shlex.quote(log)} 2>&1")
    print("== booting QEMU q35 (40s cap), feeding `ehci` + `ehci info` ...")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit code: {r.returncode} (124 == timeout, expected)")
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def main():
    ap = argparse.ArgumentParser(description="EHCI USB2 QEMU smoke test")
    ap.add_argument("--kernel", default=DEFAULT_KERNEL)
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()

    check_tools(args.kernel)
    text = run_qemu(args.kernel, args.log)
    exc_lines = scan_exceptions(text)
    hits = VER_RE.findall(text)

    markers = []
    markers.append(("EHCI version marker", bool(hits), f"hits={len(hits)}"))
    ports = int(hits[0][5]) if hits else 0
    markers.append(("downstream ports > 0", ports > 0, f"ports={ports}"))
    markers.append(("boot + `ehci info` dump (>=2 markers)", len(hits) >= 2,
                    f"hits={len(hits)}"))
    conn = len(CONN_RE.findall(text))
    markers.append(("mouse connected on a root port", conn > 0,
                    f"connected ports={conn}"))
    markers.append(("no Exc/PanicKernExec", not exc_lines,
                    f"lines={len(exc_lines)}"))

    print("\n== marker results")
    all_pass = True
    for name, ok, detail in markers:
        all_pass = all_pass and ok
        print(f"  [{'PASS' if ok else 'FAIL'}] {name} ({detail})")
    if hits:
        print(f"version={hits[0][0]} bar={hits[0][1]} pci={hits[0][2]} "
              f"caplen={hits[0][3]} hccp={hits[0][4]} ports={hits[0][5]}")
    if exc_lines:
        print("\n== exception/panic lines:")
        for line in exc_lines:
            print("   ", line[:220])
    # Show the EHCI lines even on failure to make bring-up debugging easy.
    ehci_lines = [ln for ln in text.splitlines() if ln.strip().startswith("ehci:")]
    if ehci_lines:
        print("\n== EHCI serial lines:")
        for line in ehci_lines[:30]:
            print("   ", line[:220])
    print(f"serial log: {args.log}")
    print("\nRESULT: " + ("PASS" if all_pass else "FAIL"))
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())

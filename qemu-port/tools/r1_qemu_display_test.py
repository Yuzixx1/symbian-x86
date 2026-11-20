#!/usr/bin/env python3
"""r1_qemu_display_test.py — automated QEMU framebuffer/display HAL test.

Boots the full Kern image with a standard VGA (Bochs VBE DISPI), feeds the
serial shell `display` then `display test`, and checks the serial log for the
framebuffer markers: VBE mode set, LFB base, geometry, and the test banner.
Also fails on any guest exception or PanicKernExec in the log.

The disk image / toolchain setup is reused from r1_qemu_smoke.py so this test
boots exactly the same known-good environment (only the serial script differs).

Usage:
    python3 tools/r1_qemu_display_test.py [--kernel KERN] [--no-rebuild-disk]
                                          [--log LOG]
"""
import argparse
import os
import re
import shlex
import subprocess
import sys

import r1_qemu_smoke as smoke

LOG_DEFAULT = "/tmp/opencode/r1_qemu_display_test.log"

MARKERS = [
    "DISPLAY: VBE mode 0x",                       # boot-time mode set
    "DISPLAY: LFB base=0x",                       # boot-time LFB discovery
    "display: framebuffer LFB=0x",                # `display` command
    "DISPLAY TEST: cleared blue, border + text drawn",
    "DISPLAY TEST: pixel readback",
    "DISPLAY TEST: EKA2-GUI-READY framebuffer OK",
    "no Exc or PanicKernExec in log",
]

QEMU_ARGS = [
    "timeout", "50", "qemu-system-i386",
    "-M", "q35", "-m", "128",
    "-netdev", "user,id=n0",
    "-device", "e1000,netdev=n0",
    "-audiodev", "none,id=a0",
    "-device", "AC97,audiodev=a0",
    "-vga", "std",                     # Bochs VBE DISPI registers
    "-display", "none", "-serial", "stdio",
    "-kernel", None,
    "-drive", f"file={smoke.DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
    "-device", "ich9-ahci,id=sata",
    "-device", "ide-hd,drive=d0,bus=sata.0",
    "-monitor", "none", "-no-reboot",
]


def run_qemu(kernel, log):
    args = [kernel if a is None else a for a in QEMU_ARGS]
    qemu_str = " ".join(shlex.quote(a) for a in args)
    # Let boot settle, then drive the shell: info first, then the draw test.
    feeder = (
        "sleep 2.0; printf '\\n'; "
        "sleep 2.0; printf 'display\\n'; "
        "sleep 2.0; printf 'display test\\n'; "
        "sleep 3.0"
    )
    cmd = f"{{ {feeder}; }} | {qemu_str} > {shlex.quote(log)} 2>&1"
    print("== starting qemu (50s cap), feeding 'display' then 'display test' ...")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit code: {r.returncode} (124 == timeout 50 killed qemu, expected)")
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def check_markers(text):
    results = {m: (m in text) for m in MARKERS}
    results["no Exc or PanicKernExec in log"] = not smoke.scan_exceptions(text)
    return results


def observed(text):
    """Pull the VBE mode + LFB geometry out of the log for the report."""
    mode = re.search(r"DISPLAY: VBE mode (0x[0-9a-fA-F]+)", text)
    lfb = re.search(r"DISPLAY: LFB base=(0x[0-9a-fA-F]+) (\d+)x(\d+)x(\d+)", text)
    if mode or lfb:
        print("== observed framebuffer")
        if mode:
            print(f"   VBE mode  : {mode.group(1)}")
        if lfb:
            print(f"   LFB base  : {lfb.group(1)}")
            print(f"   geometry  : {lfb.group(2)}x{lfb.group(3)}x{lfb.group(4)}")


def main():
    ap = argparse.ArgumentParser(description="R1 QEMU framebuffer/display test")
    ap.add_argument("--kernel", default=smoke.DEFAULT_KERNEL,
                    help="kernel ELF (default: %(default)s)")
    ap.add_argument("--no-rebuild-disk", action="store_true",
                    help="reuse existing disk image instead of rebuilding")
    ap.add_argument("--log", default=LOG_DEFAULT,
                    help="path to save the serial log (default: %(default)s)")
    args = ap.parse_args()

    smoke.check_tools()
    if args.no_rebuild_disk and os.path.exists(smoke.DISK_IMG):
        print(f"== reusing existing disk image {smoke.DISK_IMG}")
    else:
        smoke.build_disk()

    text = run_qemu(args.kernel, args.log)
    results = check_markers(text)
    observed(text)

    exc_lines = smoke.scan_exceptions(text)
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
        for line in text.splitlines():
            if "DISPLAY" in line or "display:" in line:
                print("   ", line[:220])
        print("\nRESULT: FAIL")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

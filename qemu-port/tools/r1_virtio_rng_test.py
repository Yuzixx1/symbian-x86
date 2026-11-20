#!/usr/bin/env python3
"""r1_virtio_rng_test.py -- automated QEMU test for the new virtio-RNG driver.

Boots the full Kern ELF with an explicit virtio-rng-pci entropy device backed
by /dev/urandom, feeds `rng` and `rng test` to the shell, and checks the serial
log for the init marker, the queue state, and 16 hex bytes of random data.
Exits 0 only if every marker passes AND no guest exception / PanicKernExec
line appears.

Usage:
    python3 tools/r1_virtio_rng_test.py [--kernel KERN] [--log LOG]
"""
import argparse
import re
import shlex
import shutil
import subprocess
import sys

DEFAULT_KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"
LOG_DEFAULT = "/tmp/opencode/r1_virtio_rng_test.log"

EXC_KEYS = ("Exc ", "PanicKernExec")


def scan_exceptions(text):
    return [line for line in text.splitlines()
            if any(k in line for k in EXC_KEYS)]


def hex_bytes(text):
    """Return the list of 2-digit hex tokens printed by the rng test lines."""
    out = []
    for line in text.splitlines():
        if not line.lower().startswith("rng test:") or "bytes=" not in line:
            continue
        payload = line.split("bytes=", 1)[1]
        out += re.findall(r"\b[0-9a-fA-F]{2}\b", payload)
    return out


def sh(*args):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args)


def check_tools():
    for t in ("qemu-system-i386", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def run_qemu(kernel, log):
    qemu = [
        "timeout", "50", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-object", "rng-random,filename=/dev/urandom,id=rng0",
        "-device", "virtio-rng-pci,rng=rng0",
        "-vga", "std",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-monitor", "none", "-no-reboot",
    ]
    qemu_str = " ".join(shlex.quote(a) for a in qemu)
    # Timed serial feed: boot, then `rng`, `rng info`, `rng test`.
    feeder = (
        "sleep 1.5; printf '\\n'; "
        "sleep 2.0; printf 'rng\\n'; "
        "sleep 1.0; printf 'rng info\\n'; "
        "sleep 1.0; printf 'rng test\\n'; "
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
    hb = hex_bytes(text)
    results = {
        "virtio-rng init marker": ("virtio-rng:" in text and "queue ready" in text),
        "rng info state": ("vrng: BAR=" in text and "queue n=" in text),
        "rng test fetched 16 bytes": ("rng test: get=16" in text),
        "16 hex bytes printed": (len(hb) >= 16),
        "bytes are genuine entropy (not all zero)": (len(hb) >= 16 and any(b != "00" for b in hb[:16])),
        "no Exc or PanicKernExec in log": (not scan_exceptions(text)),
    }
    return results, hb


def main():
    ap = argparse.ArgumentParser(description="virtio-RNG QEMU smoke test")
    ap.add_argument("--kernel", default=DEFAULT_KERNEL,
                    help="kernel ELF (default: %(default)s)")
    ap.add_argument("--log", default=LOG_DEFAULT,
                    help="path to save the serial log (default: %(default)s)")
    args = ap.parse_args()

    check_tools()
    text = run_qemu(args.kernel, args.log)
    results, hb = check_markers(text)

    exc_lines = scan_exceptions(text)
    if exc_lines:
        print("\n== exception/panic lines detected in serial log:")
        for line in exc_lines:
            print("   ", line[:220])

    print("\n== marker results")
    all_pass = True
    for m, ok in results.items():
        all_pass = all_pass and ok
        print(f"  [{'PASS' if ok else 'FAIL'}] {m}")

    print("\n== virtio-rng / rng log lines:")
    for line in text.splitlines():
        if "virtio-rng" in line or line.lower().startswith("vrng ") or \
           line.lower().startswith("vrng:") or line.lower().startswith("rng ") or \
           line.lower().startswith("rng:"):
            print("   ", line[:220])

    print(f"\nrandom bytes observed: {':'.join(hb[:16]) if len(hb) >= 16 else hb}")
    print(f"serial log: {args.log}")
    if not all_pass:
        print("\nRESULT: FAIL")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

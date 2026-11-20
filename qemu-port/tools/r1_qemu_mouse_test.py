#!/usr/bin/env python3
"""r1_qemu_mouse_test.py — automated QEMU PS/2 mouse driver test.

Boots the full Kern image with standard VGA (Bochs VBE DISPI, so the cursor
can actually be painted), feeds the serial shell `mouse`, `mouse info`,
`mouse test` and `mouse info`, and checks the log for the driver markers:
8042 init with a device ID/packet size, the cursor/button state, and the
self-test proving a synthetic AUX packet travelled through the real IRQ12 ISR
(0xD3 injection -> IRQ12 -> MouseIsr -> packet decode).

A best-effort QEMU-monitor `mouse_move` injection is also attempted via a UNIX
socket monitor so real host pointer events are exercised when available.  It
is informational only: headless QEMU (`-display none`) does not always deliver
HMP mouse events to the i8042, so the hard assertions rely on `mouse test`
(the deterministic 0xD3 self-test) instead.

Fails on any guest exception or PanicKernExec in the log.  Disk/toolchain
setup is reused from r1_qemu_smoke.py so this boots the same known-good image.

Usage:
    python3 tools/r1_qemu_mouse_test.py [--kernel KERN] [--no-rebuild-disk]
                                        [--log LOG]
"""
import argparse
import os
import re
import shlex
import socket
import subprocess
import sys
import time

import r1_qemu_smoke as smoke

LOG_DEFAULT = "/tmp/opencode/r1_qemu_mouse_test.log"
MON_SOCK = "/tmp/opencode/r1_qemu_mouse_mon.sock"

MARKERS = [
    "MOUSE: PS/2 init",                       # boot-time 8042 init marker
    "mouse: x=",                              # shell info: cursor/buttons
    "mouse: id=0x",                           # shell info: device ID + IRQ state
    "MOUSE TEST: injected 3 AUX bytes",       # IRQ12 self-test marker
    "packets 0 -> 1",                         # self-test decoded one packet
    "mouse test: PASS",                       # shell self-test result
    "no Exc or PanicKernExec in log",
]

QEMU_ARGS = [
    "timeout", "50", "qemu-system-i386",
    "-M", "q35", "-m", "128",
    "-netdev", "user,id=n0",
    "-device", "e1000,netdev=n0",
    "-audiodev", "none,id=a0",
    "-device", "AC97,audiodev=a0",
    "-vga", "std",                     # Bochs VBE DISPI framebuffer
    "-display", "none", "-serial", "stdio",
    "-kernel", None,
    "-drive", f"file={smoke.DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
    "-device", "ich9-ahci,id=sata",
    "-device", "ide-hd,drive=d0,bus=sata.0",
    "-monitor", None,                   # filled in with the UNIX socket path
    "-no-reboot",
]


def inject_mouse_moves():
    """Best-effort HMP `mouse_move` injection. Returns True if delivered."""
    deadline = time.time() + 6.0
    s = None
    while time.time() < deadline and s is None:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(1.0)
            s.connect(MON_SOCK)
        except OSError:
            if s is not None:
                s.close()
            s = None
            time.sleep(0.3)
    if s is None:
        print("== monitor injection: monitor socket never came up (skipped)")
        return False
    try:
        try:
            s.recv(4096)                # drain the HMP banner
        except OSError:
            pass
        for dx, dy in ((40, -30), (25, 15)):
            s.sendall(f"mouse_move {dx} {dy}\n".encode())
            time.sleep(0.5)
        print("== monitor injection: sent 2 mouse_move events")
        return True
    except OSError as e:
        print(f"== monitor injection failed: {e}")
        return False
    finally:
        s.close()


def run_qemu(kernel, log):
    if os.path.exists(MON_SOCK):
        os.remove(MON_SOCK)
    args = []
    for a in QEMU_ARGS:
        if a is None:
            args.append(kernel)
        elif a == "-monitor":
            args += ["-monitor", f"unix:{MON_SOCK},server"]
        else:
            args.append(a)
    qemu_str = " ".join(shlex.quote(a) for a in args)
    # Drive the shell: baseline info, the deterministic IRQ12 self-test, then
    # confirm the cursor moved.
    feeder = (
        "sleep 2.0; printf '\\n'; "
        "sleep 2.0; printf 'mouse\\n'; "
        "sleep 1.5; printf 'mouse info\\n'; "
        "sleep 1.5; printf 'mouse test\\n'; "
        "sleep 2.0; printf 'mouse info\\n'; "
        "sleep 2.5"
    )
    cmd = f"{{ {feeder}; }} | {qemu_str} > {shlex.quote(log)} 2>&1"
    print("== starting qemu (50s cap), feeding mouse shell commands ...")
    proc = subprocess.Popen(["bash", "-c", cmd])
    time.sleep(4.5)
    injected = inject_mouse_moves()
    proc.wait()
    print(f"qemu wrapper exit code: {proc.returncode} (124 == timeout 50 killed qemu, expected)")
    try:
        with open(log, "r", errors="replace") as f:
            return f.read(), injected
    except FileNotFoundError:
        return "", injected


def check_markers(text):
    results = {m: (m in text) for m in MARKERS}
    results["no Exc or PanicKernExec in log"] = not smoke.scan_exceptions(text)
    return results


def observed(text):
    """Pull the observed device/cursor/ISR state out of the log for the report."""
    dev = re.search(r"MOUSE: PS/2 init id=(0x[0-9a-fA-F]+) pktsize=(\d+) ack=(\w+)", text)
    sts = re.findall(r"mouse: x=(\d+) y=(\d+) buttons=(\S+) packets=(\d+)", text)
    isrs = re.findall(r"mouse: isr=(\d+) isrbytes=(\d+)", text)
    st = sts[-1] if sts else None          # show the post-self-test state
    isr = isrs[-1] if isrs else None
    if dev or st or isr:
        print("== observed mouse state")
        if dev:
            print(f"   device   : id={dev.group(1)} pktsize={dev.group(2)} ack={dev.group(3)}")
        if st:
            print(f"   cursor   : x={st[0]} y={st[1]} buttons={st[2]} packets={st[3]}")
        if isr:
            print(f"   IRQ12 ISR: entries={isr[0]} bytes={isr[1]}")


def main():
    ap = argparse.ArgumentParser(description="R1 QEMU PS/2 mouse test")
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

    text, injected = run_qemu(args.kernel, args.log)
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
    print(f"host mouse_move injection attempted: {injected!r} (informational)")
    print(f"serial log: {args.log}")

    if not all_pass:
        print("\n== relevant log lines:")
        for line in text.splitlines():
            if "MOUSE" in line or "mouse" in line.lower():
                print("   ", line[:220])
        print("\nRESULT: FAIL")
        return 1
    print("\nRESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())

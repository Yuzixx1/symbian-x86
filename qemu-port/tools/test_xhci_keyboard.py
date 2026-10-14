#!/usr/bin/env python3
"""Boot the kernel with qemu-xhci and inject a USB-keyboard command via HMP.

Run from qemu-port after `make build/kernfull.elf`. Output is in
/tmp/opencode/xhci-keyboard-test.log; QEMU is always stopped on exit.
"""
import os
import socket
import subprocess
import time

LOG = "/tmp/opencode/xhci-keyboard-test.log"
MON = "/tmp/opencode/xhci-keyboard-test.sock"
try:
    os.unlink(MON)
except FileNotFoundError:
    pass

with open(LOG, "wb") as log:
    qemu = subprocess.Popen(
        ["qemu-system-i386", "-m", "128", "-display", "none",
         "-serial", "stdio", "-kernel", "build/kernfull.elf",
         "-device", "qemu-xhci", "-device", "usb-kbd", "-no-reboot",
         "-monitor", f"unix:{MON},server=on,wait=off"],
        stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
    )
    try:
        time.sleep(9)
        qemu.stdin.write(b"xhcikbd 5\n")
        qemu.stdin.flush()
        time.sleep(3)
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as monitor:
            monitor.connect(MON)
            monitor.settimeout(1)
            try:
                monitor.recv(1024)
            except socket.timeout:
                pass
            # Only the USB keyboard is attached. The injected command must
            # appear on the serial console if the xHCI HID path is live.
            for key in ("v", "e", "r", "s", "i", "o", "n", "ret"):
                monitor.sendall(f"sendkey {key}\n".encode("ascii"))
                time.sleep(0.22)
        time.sleep(3)
        qemu.stdin.write(b"usb\n")
        qemu.stdin.flush()
        time.sleep(2)
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=3)
        except subprocess.TimeoutExpired:
            qemu.kill()
            qemu.wait()

with open(LOG, "rb") as log:
    output = log.read().decode("latin1")
print("keyboard init:", "xhci: keyboard ok" in output)
print("USB-typed version command:", "EKA2> version" in output)
print("xHCI key counter:", "key(s)" in output)
print("fault:", "FAULT" in output)
print("log:", LOG)

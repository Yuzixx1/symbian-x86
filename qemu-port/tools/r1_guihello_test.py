#!/usr/bin/env python3
"""r1_guihello_test.py -- end-to-end GUI toolkit E32 test on the VBE LFB.

Builds GUIHELLO.E32 (a PE2 demo of the reusable guilib.c toolkit) and the
bootable GRUB ISO that carries it as /PROGRAMS.IMG, boots with QEMU's standard
VGA (-vga std, Bochs VBE DISPI) and launches `guihello` from the serial shell.
The program paints a titled window, three focusable buttons and a live status
panel through GUI slow execs 249/250/251; the test drives ANSI arrow keys,
Enter and Esc through PortCon, asserts the serial markers and tries an HMP
`screendump` of the real framebuffer to /tmp/opencode/guihello.png.

Usage: python3 tools/r1_guihello_test.py [--iso ISO] [--log LOG] [--no-rebuild-iso]
"""
import argparse
import os
import re
import shlex
import shutil
import socket
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
ISO_DEFAULT = "build/symbian-grub.iso"
LOG_DEFAULT = ROOT + "/r1_guihello_test.log"
PNG_DEFAULT = ROOT + "/guihello.png"
MON_SOCK = ROOT + "/r1_guihello_mon.sock"
E32 = "build/user/GUIHELLO.E32"

BOOT_MARKERS = [
    "DISPLAY: VBE mode 0x",
    "GUI: execs 249/250/251 installed",
    "GUI: pixel readback bg=0x000080 title=0x0080c0 OK",
    "guihello started (user process",
]
GH_MARKERS = [
    "GH: GUI HELLO START",
    "GH: framebase=0x",
    "GH: shared ready geometry=1024x768",
    "GH: frame 1 drawn cmds=",
    "GH: key DOWN focus=EDIT",
    "GH: key RIGHT focus=QUIT",
    "GH: key UP focus=EDIT",
    "GH: key LEFT focus=OPEN",
    "GH: key ENTER focus=OPEN",
    "GH: ESC received",
    "GH: mouse x=",
    "GH: exit presses=1",
    "GH: DONE",
]
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "make"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def rebuild(iso, no_rebuild):
    if not (no_rebuild and os.path.exists(iso)):
        sh("make", "build/user/GUIHELLO.E32", check=True)
        sh("make", "isogrub", check=True)
    if not os.path.exists(iso):
        sys.exit(f"missing GRUB ISO {iso}")
    # Confirm the demo really made it onto the ISO's FAT32 programs image.
    listing = sh("mdir", "-i", "build/programs.img@@2048S", "::",
                 check=True, capture_output=True, text=True).stdout
    if "GUIHELLO" not in listing.upper():
        sys.exit("GUIHELLO.E32 missing from ISO programs image")


def wait_for(log_path, marker, start=0, timeout=70):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        if marker in data:
            return start + data.index(marker) + len(marker)
        time.sleep(0.15)
    raise RuntimeError(f"serial log missing {marker!r} (see {log_path})")


def screendump(png):
    """Best-effort HMP screendump of the standard-VGA framebuffer."""
    # Do NOT unlink the socket here: QEMU is already listening on that path.
    deadline = time.monotonic() + 4.0
    s = None
    while time.monotonic() < deadline and s is None:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(2.0)
            s.connect(MON_SOCK)
        except OSError:
            if s is not None:
                s.close()
            s = None
            time.sleep(0.25)
    if s is None:
        return "monitor socket unavailable"
    try:
        try:
            s.recv(4096)                    # HMP banner
        except OSError:
            pass
        if os.path.exists(png):
            os.remove(png)
        s.sendall(f"screendump {png}\n".encode())
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            if os.path.exists(png) and os.path.getsize(png) > 100:
                # QEMU HMP writes a Netpbm PPM here; make it a real PNG when
                # Pillow is available (the path stays /tmp/opencode/guihello.png).
                try:
                    from PIL import Image
                    img = Image.open(png).convert("RGB")
                    img.save(png, format="PNG")
                except Exception:
                    pass
                return f"OK ({os.path.getsize(png)} bytes)"
            time.sleep(0.2)
        return "screendump produced no file (headless QEMU limitation)"
    except OSError as e:
        return f"screendump failed: {e}"
    finally:
        s.close()


def run_qemu(iso, log_path, png):
    if os.path.exists(MON_SOCK):
        os.remove(MON_SOCK)
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-cdrom", iso, "-boot", "d",
        "-vga", "std",                       # Bochs VBE DISPI framebuffer
        "-display", "none", "-serial", "stdio",
        "-monitor", f"unix:{MON_SOCK},server,nowait",
        "-no-reboot",
    ]
    print("== booting GRUB ISO, launching guihello via serial console ...")
    shot = "not attempted"
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, b"X:/> ", timeout=80)
            vm.stdin.write(b"guihello\n")
            vm.stdin.flush()
            pos = wait_for(log_path, b"GH: frame 1 drawn", pos, 60)
            for seq, marker in ((b"\x1b[B", b"GH: key DOWN"),
                                (b"\x1b[C", b"GH: key RIGHT"),
                                (b"\x1b[A", b"GH: key UP"),
                                (b"\x1b[D", b"GH: key LEFT"),
                                (b"\r",     b"GH: key ENTER")):
                vm.stdin.write(seq)
                vm.stdin.flush()
                pos = wait_for(log_path, marker, pos, 30)
            vm.stdin.write(b"\x1b")           # Esc: quit
            vm.stdin.flush()
            wait_for(log_path, b"GH: DONE", pos, 60)
            # Capture after the app stopped redrawing: QEMU's display surface
            # can lag a few ms behind the guest LFB while frames are painted.
            time.sleep(0.5)
            shot = screendump(png)
            return shot
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text):
    results = {}
    for m in BOOT_MARKERS + GH_MARKERS:
        results[m] = m in text
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    n = re.search(r"GH: frame 1 drawn cmds=(\d+)", text)
    results["frame uses < 160 display commands"] = bool(n) and int(n.group(1)) < 160
    return results, exc, n.group(1) if n else "?"


def main():
    ap = argparse.ArgumentParser(description="R1 GUI toolkit E32 QEMU test")
    ap.add_argument("--iso", default=ISO_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--png", default=PNG_DEFAULT)
    ap.add_argument("--no-rebuild-iso", action="store_true")
    args = ap.parse_args()

    check_tools()
    rebuild(args.iso, args.no_rebuild_iso)
    shot = "not attempted"
    try:
        shot = run_qemu(args.iso, args.log, args.png)
    except RuntimeError as e:
        print("QEMU drive failed:", e)
    with open(args.log, errors="replace") as f:
        text = f.read()

    results, exc, cmds = check(text)
    print("== guihello test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print(f"display-list commands in frame 1: {cmds}")
    print(f"screendump: {shot} -> {args.png}")
    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("GH:", "guihello", "GUI:", "DISPLAY",
                                     "E32Run", "fault", "panic")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

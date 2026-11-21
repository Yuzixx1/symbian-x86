#!/usr/bin/env python3
"""r1_dtedit_test.py -- end-to-end WINDOWED TEXT EDITOR E32 test on the VBE LFB.

Builds DTEDIT.E32 (a PE2 desktop application built on guilib.c) and the bootable
GRUB ISO that carries it as /PROGRAMS.IMG, boots with QEMU's standard VGA
(-vga std) and an X-backed GTK display so real PS/2 mouse events can be injected
via QMP, then launches `dtedit` from the serial shell.  The test:

  * types "Hello EKA2 desktop!" through PortCon (real keyboard typing),
  * moves the real PS/2 cursor and clicks the on-screen SAVE toolbar button via
    HMP mouse_move + QMP input-send-event (real mouse click),
  * presses Ctrl-S to exercise the keyboard save shortcut,
  * presses ESC to quit (X: is a read-only GRUB ramdisk, so file_save and
    TYPE readback are intentionally not asserted),
  * screendumps the live framebuffer to /tmp/opencode/dtedit.png.

Hard assertions cover the DTE markers, the mouse click and the absence of any
guest exception/panic.  Usage:

    python3 tools/r1_dtedit_test.py [--iso ISO] [--log LOG] [--png PNG]
                                    [--no-rebuild-iso]
"""
import argparse
import json
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
LOG_DEFAULT = ROOT + "/r1_dtedit_test.log"
PNG_DEFAULT = ROOT + "/dtedit.png"
MON_SOCK = ROOT + "/r1_dtedit_mon.sock"
QMP_SOCK = ROOT + "/r1_dtedit_qmp.sock"
E32 = "build/user/DTEDIT.E32"

BOOT_MARKERS = [
    "DISPLAY: VBE mode 0x",
    "GUI: execs 249/250/251 installed",
    "GUI: pixel readback",                      # generic first-frame check (OK or MISMATCH)
    "dtedit started (user process",
]
DTE_MARKERS = [
    "DTE: WINDOWED EDITOR READY",
    "DTE: window 1000x520",
    "DTE: frame drawn cmds=",
    "DTE: chars=19",
    "DTE: click SAVE",
    "DTE: key ^S",
    "DTE: DONE",
]
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "make", "mdir"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def rebuild(iso, no_rebuild):
    if not (no_rebuild and os.path.exists(iso)):
        sh("make", E32, check=True)
        sh("make", "isogrub", check=True)
    if not os.path.exists(iso):
        sys.exit(f"missing GRUB ISO {iso}")
    listing = sh("mdir", "-i", "build/programs.img@@2048S", "::",
                 check=True, capture_output=True, text=True).stdout
    norm = "".join(listing.lower().split()).replace(".", "")
    if "dtedit" not in norm:
        sys.exit("DTEDIT.E32 missing from ISO programs image")
    if "euserdll" not in norm:              # mdir prints "EUSER  DLL"
        sys.exit("EUSER.DLL missing from ISO programs image")


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


def read_log(log_path):
    with open(log_path, errors="replace") as f:
        return f.read()


def connect(path, timeout=10.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(10.0)
            s.connect(path)
            return s
        except OSError:
            time.sleep(0.2)
    raise RuntimeError(f"socket {path} never came up")


class Qmp:
    def __init__(self, path):
        self.sock = connect(path)
        self.stream = self.sock.makefile("rwb")
        self.stream.readline()                      # QMP greeting
        self.next_id = 1
        self.cmd("qmp_capabilities")

    def cmd(self, execute, arguments=None):
        ident = self.next_id
        self.next_id += 1
        obj = {"execute": execute, "id": ident}
        if arguments is not None:
            obj["arguments"] = arguments
        self.stream.write((json.dumps(obj) + "\n").encode())
        self.stream.flush()
        while True:
            line = self.stream.readline()
            if not line:
                raise RuntimeError("QMP closed while waiting for reply")
            msg = json.loads(line)
            if msg.get("id") == ident:
                return msg

    def close(self):
        self.stream.close()
        self.sock.close()


def hmp(command):
    """One human-monitor command over the UNIX socket."""
    s = connect(MON_SOCK)
    try:
        try:
            s.recv(8192)                            # HMP banner
        except OSError:
            pass
        s.sendall((command + "\n").encode())
        time.sleep(0.35)
        out = b""
        while True:
            try:
                chunk = s.recv(8192)
            except socket.timeout:
                break
            if not chunk:
                break
            out += chunk
        return out.decode(errors="replace")
    finally:
        s.close()


def mouse_position(text):
    found = re.findall(r"DTE: mouse x=(\d+) y=(\d+)", text)
    return (int(found[-1][0]), int(found[-1][1])) if found else (512, 384)


def click_left(qmp):
    """Inject a real left-button click; success is judged from DTE markers."""
    qmp.cmd("input-send-event", {"events": [
        {"type": "btn", "data": {"button": "left", "down": True}}]})
    time.sleep(0.25)
    qmp.cmd("input-send-event", {"events": [
        {"type": "btn", "data": {"button": "left", "down": False}}]})


def screendump(png):
    if os.path.exists(png):
        os.remove(png)
    hmp(f"screendump {png}")
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        if os.path.exists(png) and os.path.getsize(png) > 100:
            try:
                from PIL import Image
                img = Image.open(png).convert("RGB")
                img.save(png, format="PNG")
            except Exception:
                pass
            return f"OK ({os.path.getsize(png)} bytes)"
        time.sleep(0.2)
    return "no screendump file (headless QEMU limitation)"


def run_qemu(iso, log_path, png):
    for path in (MON_SOCK, QMP_SOCK):
        if os.path.exists(path):
            os.remove(path)
    qemu_cmd = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-cdrom", iso, "-boot", "d",
        "-vga", "std",                       # Bochs VBE DISPI framebuffer
        "-display", "gtk",                   # X-backed: real PS/2 mouse handler
        "-serial", "stdio",
        "-monitor", f"unix:{MON_SOCK},server,nowait",
        "-qmp", f"unix:{QMP_SOCK},server,nowait",
        "-no-reboot",
    ]
    print("== booting GRUB ISO, launching dtedit via serial console ...")
    click_ok = False
    shot = "not attempted"
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu_cmd, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        qmp = None
        try:
            pos = wait_for(log_path, b"X:/> ", timeout=80)
            vm.stdin.write(b"dtedit\n")
            vm.stdin.flush()
            pos = wait_for(log_path, b"DTE: WINDOWED EDITOR READY", pos, 60)
            wait_for(log_path, b"DTE: frame drawn cmds=", pos, 60)

            # Keyboard typing through PortCon.
            vm.stdin.write(b"Hello EKA2 desktop!\r")
            vm.stdin.flush()
            pos = wait_for(log_path, b"DTE: chars=19", pos, 40)

            # Real mouse click on the SAVE toolbar button (centre ~108,88).
            qmp = Qmp(QMP_SOCK)
            for _ in range(3):
                x, y = mouse_position(read_log(log_path))
                dx, dy = 108 - x, 88 - y
                if abs(dx) <= 2 and abs(dy) <= 2:
                    break
                hmp(f"mouse_move {dx} {dy}")
                time.sleep(0.35)
            click_left(qmp)
            time.sleep(0.4)
            # A successful click is proven by the editor's own SAVE marker,
            # not by the QMP input-send-event reply.
            try:
                wait_for(log_path, b"DTE: click SAVE", 0, 5)
                click_ok = True
            except RuntimeError:
                click_ok = False

            # Keyboard Ctrl-S shortcut (save again).
            vm.stdin.write(b"\x13")
            vm.stdin.flush()
            pos = wait_for(log_path, b"DTE: key ^S", pos, 40)

            # Screenshot while the editor window and typed text are on screen.
            time.sleep(0.3)
            shot = screendump(png)

            vm.stdin.write(b"\x1b")           # ESC: quit
            vm.stdin.flush()
            pos = wait_for(log_path, b"DTE: DONE", pos, 60)
            return shot, click_ok
        finally:
            if qmp is not None:
                try:
                    qmp.close()
                except Exception:
                    pass
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text):
    results = {m: (m in text) for m in BOOT_MARKERS + DTE_MARKERS}
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    cmds = re.search(r"DTE: frame drawn cmds=(\d+)", text)
    results["frame fits GUI_MAX_CMDS (640)"] = bool(cmds) and int(cmds.group(1)) <= 640
    return results, exc, cmds.group(1) if cmds else "?"


def main():
    ap = argparse.ArgumentParser(description="R1 windowed text editor E32 test")
    ap.add_argument("--iso", default=ISO_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--png", default=PNG_DEFAULT)
    ap.add_argument("--no-rebuild-iso", action="store_true")
    args = ap.parse_args()

    check_tools()
    rebuild(args.iso, args.no_rebuild_iso)
    shot, click_ok = "not attempted", False
    try:
        shot, click_ok = run_qemu(args.iso, args.log, args.png)
    except (RuntimeError, subprocess.SubprocessError, OSError) as e:
        print("QEMU drive failed:", e)
    text = read_log(args.log)

    results, exc, cmds = check(text)
    print("== dtedit test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print(f"real mouse-click injection: {'delivered' if click_ok else 'not delivered'}")
    print(f"display-list commands in first frame: {cmds}")
    print(f"screendump: {shot} -> {args.png}")
    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("DTE:", "dtedit", "GUI:", "DISPLAY",
                                     "E32Run", "fault", "panic", "TYPE")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

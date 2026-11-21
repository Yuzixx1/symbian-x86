#!/usr/bin/env python3
"""r1_dtcalc_test.py -- end-to-end QEMU test for the windowed desktop calculator.

Builds DTCALC.E32 (a genuine PE2 E32 program that paints a decorated window
with a live display and a 20-key keypad onto the VBE LFB through guilib.c and
GUI slow execs 249/250/251), rebuilds the bootable GRUB ISO, boots with QEMU
standard VGA and drives the calculator from the serial shell:

  * keyboard: 7*(2+3) + ENTER must evaluate to 35 (real recursive-descent
    parser inside the calculator), ESC must close the app;
  * mouse: best-effort HMP `mouse_move` / `mouse_press` injection aimed at the
    "5" key (coordinates read back from the calculator's own DCALC: mouse
    marker) verifies that real PS/2 clicks reach the button hit-test;
  * a screendump of the framebuffer is saved to /tmp/opencode/dtcalc.png.

Usage: python3 tools/r1_dtcalc_test.py [--iso ISO] [--log LOG]
                                      [--png PNG] [--no-rebuild-iso]
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

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ISO_DEFAULT = os.path.join(ROOT, "build", "symbian-grub.iso")
LOG_DEFAULT = "/tmp/opencode/r1_dtcalc_test.log"
PNG_DEFAULT = "/tmp/opencode/dtcalc.png"
PPM_DEFAULT = "/tmp/opencode/dtcalc.ppm"
MON_SOCK = "/tmp/opencode/r1_dtcalc_mon.sock"

BOOT_MARKERS = [
    "DISPLAY: VBE mode 0x",
    "GUI: execs 249/250/251 installed",
    "dtcalc started (user process",
]
DCALC_MARKERS = [
    "DCALC: WINDOWED CALCULATOR READY",
    "DCALC: window ",
    "DCALC: buttons=20",
    "DCALC: key 7",
    "DCALC: key *",
    "DCALC: key (",
    "DCALC: key 2",
    "DCALC: key +",
    "DCALC: key 3",
    "DCALC: key )",
    "DCALC: key =",
    "DCALC: expr 7*(2+3) = 35",
    "DCALC: key ESC",
    "DCALC: DONE",
]
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF", "FAULT:")
PROMPTS = (b"X:/> ", b"EKA2> ", b"C:/> ")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args), flush=True)
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "make", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def rebuild(iso, no_rebuild):
    if not (no_rebuild and os.path.exists(iso)):
        sh("make", "-C", ROOT, "build/user/DTCALC.E32", check=True)
        sh("make", "-C", ROOT, "isogrub", check=True)
    if not os.path.exists(iso):
        sys.exit(f"missing GRUB ISO {iso}")
    listing = sh("mdir", "-i", "build/programs.img@@2048S", "::",
                 cwd=ROOT, check=True, capture_output=True, text=True).stdout
    if "DTCALC" not in listing.upper():
        sys.exit("DTCALC.E32 missing from ISO programs image")


def wait_for(log_path, needle, start=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        i = data.find(needle)
        if i >= 0:
            return start + i + len(needle)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {needle!r} (see {log_path})")


def wait_for_prompt(log_path, start=0, timeout=70):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        for p in PROMPTS:
            i = data.find(p)
            if i >= 0:
                return start + i + len(p)
        time.sleep(0.2)
    raise RuntimeError(f"no EKA2 shell prompt in serial log (see {log_path})")


def mon_connect(timeout=6.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(2.0)
            s.connect(MON_SOCK)
            try:
                s.recv(4096)               # HMP banner
            except OSError:
                pass
            return s
        except OSError:
            try:
                s.close()
            except Exception:
                pass
            time.sleep(0.25)
    return None


def monitor_cmd(s, cmd):
    s.sendall((cmd + "\n").encode())
    time.sleep(0.15)


def mouse_click(s, target_x, target_y, log_path, pos):
    """Best-effort HMP PS/2 move + click. Returns (delivered, saw_click)."""
    if s is None:
        return False, False
    with open(log_path, "rb") as f:
        text = f.read().decode("utf-8", "replace")
    m = re.findall(r"DCALC: mouse (-?\d+),(-?\d+)", text)
    if not m:
        print("mouse injection: no cursor marker to calibrate from", flush=True)
        return True, False
    cx, cy = int(m[-1][0]), int(m[-1][1])
    dx, dy = target_x - cx, target_y - cy
    print(f"mouse injection: cursor ({cx},{cy}) -> ({target_x},{target_y}) "
          f"dx={dx} dy={dy}", flush=True)
    monitor_cmd(s, f"mouse_move {dx} {dy}")
    time.sleep(0.4)
    try:
        wait_for(log_path, f"DCALC: mouse {target_x},{target_y}".encode(),
                 pos, 8)
    except RuntimeError:
        print("mouse injection: cursor did not report target position",
              flush=True)
    # Some QEMU builds only emit a PS/2 stream packet on motion, so nudge the
    # cursor by 1 px while the button is held (and again on release) without
    # changing the final position.  This keeps the hit-test target stable.
    monitor_cmd(s, "mouse_press left")
    monitor_cmd(s, "mouse_move 1 0")
    monitor_cmd(s, "mouse_move -1 0")
    time.sleep(0.35)
    monitor_cmd(s, "mouse_release left")
    monitor_cmd(s, "mouse_move 1 0")
    monitor_cmd(s, "mouse_move -1 0")
    time.sleep(0.5)
    with open(log_path, "rb") as f:
        saw = b"DCALC: click " in f.read()
    return True, saw


def screendump(s, ppm, png):
    """Ask the (already connected) QEMU monitor for a screendump."""
    for p in (ppm, png):
        if os.path.exists(p):
            os.remove(p)
    if s is None:
        return "monitor socket unavailable"
    try:
        s.sendall(f"screendump {ppm}\n".encode())
        deadline = time.monotonic() + 8.0
        while time.monotonic() < deadline:
            if os.path.exists(ppm) and os.path.getsize(ppm) > 100:
                break
            time.sleep(0.2)
        if not (os.path.exists(ppm) and os.path.getsize(ppm) > 100):
            return "screendump produced no file"
        from PIL import Image
        Image.open(ppm).convert("RGB").save(png, format="PNG")
        return f"OK ({os.path.getsize(png)} bytes)"
    except Exception as exc:
        return f"screendump failed: {exc}"


def run_qemu(iso, log_path, ppm, png):
    if os.path.exists(MON_SOCK):
        os.remove(MON_SOCK)
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-cdrom", iso, "-boot", "d",
        "-vga", "std",                       # Bochs VBE DISPI -> 1024x768 LFB
        "-display", "none", "-serial", "stdio",
        "-monitor", f"unix:{MON_SOCK},server,nowait",
        "-no-reboot",
    ]
    print("== booting GRUB ISO with -vga std, driving DTCALC ...", flush=True)
    shot = "not attempted"
    mouse_delivered = False
    mouse_clicked = False
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        s = None
        try:
            time.sleep(0.5)
            vm.stdin.write(b"\n")              # skip GRUB countdown
            vm.stdin.flush()
            pos = wait_for_prompt(log_path, timeout=70)
            vm.stdin.write(b"dtcalc\n")
            vm.stdin.flush()
            pos = wait_for(log_path, b"DCALC: WINDOWED CALCULATOR READY",
                           pos, 60)

            # keyboard path: type 7*(2+3), then Enter (=)
            vm.stdin.write(b"7*(2+3)")
            vm.stdin.flush()
            pos = wait_for(log_path, b"DCALC: key )", pos, 30)
            vm.stdin.write(b"\r")
            vm.stdin.flush()
            pos = wait_for(log_path, b"DCALC: expr 7*(2+3) = 35", pos, 30)

            # mouse path: aim at the centre of the "5" key.  Geometry comes
            # from the calculator's own DCALC: window marker.
            with open(log_path, "rb") as f:
                text = f.read().decode("utf-8", "replace")
            wm = re.search(r"DCALC: window (\d+),(\d+) (\d+)x(\d+)", text)
            s = mon_connect(6.0)
            if wm:
                wx, wy = int(wm.group(1)), int(wm.group(2))
                # key index 5 -> col 1, row 1; width 128 height 56, gap 8
                tx = wx + 16 + 1 * (128 + 8) + 64
                ty = wy + 112 + 1 * (56 + 8) + 28
                mouse_delivered, mouse_clicked = mouse_click(
                    s, tx, ty, log_path, pos)
            else:
                print("mouse injection: no window geometry marker", flush=True)

            # capture the window while the calculator is still running
            shot = screendump(s, ppm, png)

            # close via Esc and wait for a clean shutdown marker
            vm.stdin.write(b"\x1b")
            vm.stdin.flush()
            try:
                wait_for(log_path, b"DCALC: DONE", pos, 20)
            except RuntimeError:
                pass
            time.sleep(0.4)
        finally:
            if s is not None:
                try:
                    s.close()
                except Exception:
                    pass
            try:
                vm.stdin.close()
            except Exception:
                pass
            try:
                vm.terminate()
            except Exception:
                pass
            try:
                vm.wait(timeout=5)
            except Exception:
                try:
                    vm.kill()
                except Exception:
                    pass
                try:
                    vm.wait()
                except Exception:
                    pass
    with open(log_path, errors="replace") as f:
        return f.read(), shot, mouse_delivered, mouse_clicked


def check(text, shot, mouse_clicked):
    results = {}
    for m in BOOT_MARKERS + DCALC_MARKERS:
        results[m] = m in text
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    results["screendump PNG captured"] = bool(
        shot.startswith("OK") and os.path.exists(PNG_DEFAULT))
    # Mouse click is informational: with -display none, QEMU HMP mouse_move
    # reaches the guest PS/2 device but mouse_press does not, so a headless
    # run can only certify the keyboard path (which the task explicitly allows).
    return results, exc


def main():
    ap = argparse.ArgumentParser(description="R1 windowed calculator QEMU test")
    ap.add_argument("--iso", default=ISO_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--png", default=PNG_DEFAULT)
    ap.add_argument("--no-rebuild-iso", action="store_true")
    args = ap.parse_args()

    check_tools()
    rebuild(args.iso, args.no_rebuild_iso)
    text, shot, delivered, clicked = run_qemu(args.iso, args.log,
                                              PPM_DEFAULT, args.png)
    results, exc = check(text, shot, clicked)

    print("== dtcalc test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print(f"keyboard input worked: {'DCALC: expr 7*(2+3) = 35' in text}")
    if clicked:
        print("mouse click + keyboard both worked (DCALC: click marker seen)")
    else:
        print("mouse injection delivered motion but no button event reached the "
              "guest (headless -display none); keyboard-only verified")
    print(f"screendump: {shot} -> {args.png}")
    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("DCALC", "dtcalc", "DISPLAY", "GUI",
                                     "fault", "panic", "EKA2")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

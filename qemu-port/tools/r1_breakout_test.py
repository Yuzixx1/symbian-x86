#!/usr/bin/env python3
"""r1_breakout_test.py -- end-to-end QEMU test for the BREAKOUT E32 game.

Boots build/symbian-grub.iso with the GRUB bootloader and QEMU standard VGA
(-vga std, Bochs VBE DISPI -> 1024x768x32 LFB).  GRUB auto-mounts the bundled
PROGRAMS.IMG at X:, so the EKA2 shell can launch `breakout` from a bare name.
The test drives real A/D keys plus ESC through the serial console, asserts the
sparse BRK: serial markers (proving genuine frame-by-frame animation, score and
clean shutdown), checks for guest exceptions/panics, and captures a screendump
to /tmp/opencode/breakout.png through the QEMU monitor socket.

Usage: python3 tools/r1_breakout_test.py [--no-rebuild-iso] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ISO = os.path.join(ROOT, "build", "symbian-grub.iso")
LOG_DEFAULT = "/tmp/opencode/r1_breakout.log"
SHOT_PPM = "/tmp/opencode/breakout.ppm"
SHOT_PNG = "/tmp/opencode/breakout.png"
MON_SOCK = "/tmp/opencode/breakout-mon.sock"
EUSER_DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
             "euser.dll")

EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF", "FAULT:")
BOOT_MARKERS = [
    "DISPLAY: VBE mode 0x",
    "GUI: execs 249/250/251 installed",
    "ord 915",
    "breakout started (user process",
]
BRK_MARKERS = [
    "BRK: BREAKOUT READY",
    "BRK: framebase=0x",
    "BRK: geometry ",
    "BRK: playing",
    "BRK: frame ",
    "BRK: DONE",
]
PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args), flush=True)
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def rebuild_iso():
    """Regenerate the GRUB ISO so /PROGRAMS.IMG includes BREAKOUT.E32."""
    sh("make", "-C", ROOT, "isogrub", check=True)
    if not os.path.exists(ISO):
        sys.exit(f"ISO not produced: {ISO}")


def wait_for_bytes(log_path, needle, start=0, timeout=60):
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


def wait_for_prompt(log_path, start=0, timeout=60):
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


def send(vm, text, newline=True):
    data = text.encode() + (b"\n" if newline else b"")
    vm.stdin.write(data)
    vm.stdin.flush()


def take_screendump(timeout=10):
    """Ask QEMU monitor for a screendump, then convert PPM -> PNG via PIL."""
    for p in (SHOT_PPM, SHOT_PNG):
        if os.path.exists(p):
            os.remove(p)
    if not os.path.exists(MON_SOCK):
        print("screendump: monitor socket missing", flush=True)
        return None
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(3)
        s.connect(MON_SOCK)
        s.sendall(("screendump " + SHOT_PPM + "\n").encode())
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline and not (
                os.path.exists(SHOT_PPM) and os.path.getsize(SHOT_PPM) > 0):
            time.sleep(0.1)
        s.close()
        if not (os.path.exists(SHOT_PPM) and os.path.getsize(SHOT_PPM) > 0):
            print("screendump: no PPM produced", flush=True)
            return None
        from PIL import Image
        Image.open(SHOT_PPM).save(SHOT_PNG)
        print(f"screendump: {SHOT_PNG} ({os.path.getsize(SHOT_PNG)} bytes)",
              flush=True)
        return SHOT_PNG
    except Exception as exc:             # screendump is best-effort
        print(f"screendump: failed: {exc}", flush=True)
        return None


def run_qemu(log_path):
    if os.path.exists(MON_SOCK):
        os.remove(MON_SOCK)
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-vga", "std",                       # Bochs VBE DISPI -> VBE LFB
        "-display", "none", "-serial", "stdio",
        "-monitor", "unix:" + MON_SOCK + ",server,nowait",
        "-cdrom", ISO, "-boot", "d",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-no-reboot",
    ]
    print("== booting GRUB ISO with -vga std, driving BREAKOUT ...", flush=True)
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        shot = None
        try:
            time.sleep(0.5)
            send(vm, "")                       # skip the ~10s GRUB countdown
            pos = wait_for_prompt(log_path, timeout=60)
            send(vm, "X:")                     # select the auto-mounted volume
            time.sleep(0.3)
            send(vm, "breakout")

            # Wait for the game to acquire the console lease and start animating.
            pos = wait_for_bytes(log_path, b"BRK: BREAKOUT READY",
                                 start=pos, timeout=40)
            wait_for_bytes(log_path, b"BRK: playing", start=pos, timeout=20)

            # ~8s of real gameplay with A/D paddle control.
            for k in ("a", "d", "a", "d", "a"):
                time.sleep(1.0)
                send(vm, k, newline=False)
            time.sleep(2.0)

            shot = take_screendump()

            time.sleep(0.5)
            send(vm, "\x1b", newline=False)     # ESC -> quit
            try:
                wait_for_bytes(log_path, b"BRK: DONE", timeout=15)
            except RuntimeError:
                pass
            time.sleep(0.5)
        finally:
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
                vm.kill()
    with open(log_path, "r", errors="replace") as f:
        return f.read(), shot


def check(text, shot):
    results = {}
    for m in BOOT_MARKERS + BRK_MARKERS:
        results[m] = m in text
    frames = [int(n) for n in __import__("re").findall(
        r"BRK: frame (\d+)", text)]
    results[">= 2 sparse frame markers (animated)"] = len(frames) >= 2
    results["frame count advanced past 30"] = bool(frames) and max(frames) >= 30
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    results["screendump PNG captured"] = bool(shot and os.path.exists(shot))
    return results, exc, (max(frames) if frames else 0)


def main():
    ap = argparse.ArgumentParser(description="R1 BREAKOUT QEMU test")
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-iso", action="store_true")
    args = ap.parse_args()
    check_tools()
    if not args.no_rebuild_iso:
        rebuild_iso()
    if not os.path.exists(ISO):
        sys.exit(f"missing {ISO} (run: make isogrub)")

    text = ""
    shot = None
    for attempt in range(2):                 # busy host: retry a cold boot
        text, shot = run_qemu(args.log)
        res, _, _ = check(text, shot)
        if res.get("BRK: DONE"):
            break

    results, exc, maxframe = check(text, shot)
    print("== breakout test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    print(f"  max frame observed: {maxframe}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print(f"serial log: {args.log}")
    print(f"screenshot: {SHOT_PNG}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("BRK", "breakout", "DISPLAY", "GUI",
                                     "fault", "panic", "E32Run", "EKA2")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

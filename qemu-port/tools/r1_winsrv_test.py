#!/usr/bin/env python3
"""r1_winsrv_test.py -- QEMU test for the PE2 E32 WINDOW SERVER.

Builds a private GRUB ISO carrying build/user/WINSRV.E32 + EUSER.DLL, boots
with QEMU standard VGA (Bochs VBE -> 1024x768x32 LFB), types `winsrv` at the
serial shell, exercises the compositor/event dispatcher through synthetic
PortCon input, asserts the WSV: markers, checks for guest exceptions/panics and
grabs a PNG screendump to /tmp/opencode/winsrv.png.

WINSRV is RETIRED: the window system moved to the genuine EWSRV, and
src/user/winsrv.c, guishost.h and the ad-hoc GUI execs were deleted, so nothing
in the tree can rebuild build/user/WINSRV.E32.  This test therefore self-skips
(`RESULT: SKIP`, exit 0) instead of driving the deleted binary -- a stale
WINSRV.E32 left in build/user is not driven either, because nothing owns it.

Usage: python3 tools/r1_winsrv_test.py [--no-rebuild-iso] [--log LOG]
"""
import argparse
import glob
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
ROOT = "/tmp/opencode"
ISO = ROOT + "/r1_winsrv.iso"
SOCK = ROOT + "/r1_winsrv-qmp.sock"
LOG_DEFAULT = ROOT + "/r1_winsrv.log"
SHOT = ROOT + "/winsrv.png"
E32 = REPO + "/build/user/WINSRV.E32"
PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF", "Abort")
EXPECTED = [
    "WSV: WINDOW SERVER READY",
    "WSV: shared=",
    "WSV: windows=",
    "WSV: event 1 wid=",   # MOVE
    "WSV: event 2 wid=",   # DOWN / hit-test
    "WSV: event 4 wid=",   # KEY -> focus
    "WSV: composite 1",
    "WSV: DONE frames=",
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def retired():
    """Reason string when winsrv no longer exists in the source tree, else None."""
    for pat in ("src/*/winsrv.*", "src/winsrv.*", "user/winsrv.*",
                "genuine/*/winsrv.*"):
        if glob.glob(os.path.join(REPO, pat)):
            return None
    return "winsrv retired (source deleted; the window system is genuine EWSRV)"


def skip(msg):
    print("SKIP: " + msg)
    print("RESULT: SKIP")
    return 0


def check_tools():
    for t in ("qemu-system-i386", "grub2-mkimage", "mcopy", "mkfs.fat"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_iso():
    if os.path.exists(SOCK):
        os.remove(SOCK)
    sh("python3", "tools/mkgrubiso.py", ISO, cwd=REPO, check=True,
       stdout=subprocess.DEVNULL)
    listing = sh("mdir", "-i",
                 f"{REPO}/build/programs.img@@2048S", "::",
                 check=True, capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    for want in ("winsrv", "euserdll"):
        if want not in norm:
            sys.exit(f"programs image missing {want}")
    print("ISO OK: WINSRV.E32 + EUSER.DLL staged in /PROGRAMS.IMG")


def wait_for(log_path, marker, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            if marker in f.read():
                return True
        time.sleep(0.2)
    return False


def wait_for_prompt(log_path, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            data = f.read()
        if any(p in data for p in PROMPTS):
            return True
        time.sleep(0.2)
    return False


def qmp_screendump(path, tries=12):
    for _ in range(tries):
        if os.path.exists(SOCK):
            break
        time.sleep(0.25)
    else:
        print("[WARN] QMP socket never appeared")
        return False
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(SOCK)
        f = s.makefile("rwb")
        f.readline()                       # QMP greeting
        f.write(b'{"execute":"qmp_capabilities"}\n')
        f.flush()
        f.readline()                       # capabilities reply
        if os.path.exists(path):
            os.remove(path)
        qmp = ('{"execute":"screendump","arguments":{"filename":"'
               + path + '","format":"png"}}\n')
        f.write(qmp.encode())
        f.flush()
        for _ in range(10):
            line = f.readline()
            if not line:
                break
            if b"error" in line.lower():
                print("[WARN] QMP screendump error:", line[:200])
                break
            if b"return" in line:
                break
        f.close()
        s.close()
    except Exception as exc:
        print("[WARN] QMP screendump failed:", exc)
        return False
    for _ in range(20):
        if os.path.exists(path) and os.path.getsize(path) > 1000:
            break
        time.sleep(0.15)
    ok = os.path.exists(path) and os.path.getsize(path) > 1000
    if ok:
        try:
            from PIL import Image
            im = Image.open(path)
            ok = im.size[0] > 0 and im.size[1] > 0
            print(f"[INFO] screendump {path}: {im.size[0]}x{im.size[1]} "
                  f"({os.path.getsize(path)} bytes)")
        except Exception:
            pass
    return ok


def run_qemu(log_path):
    if os.path.exists(SOCK):
        os.remove(SOCK)
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-qmp", f"unix:{SOCK},server,nowait",
        "-cdrom", ISO, "-boot", "d",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting GRUB ISO, driving WINSRV via serial ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        shot_ok = False
        try:
            if not wait_for_prompt(log_path):
                print("[WARN] shell prompt not seen; continuing")
            time.sleep(0.5)
            vm.stdin.write(b"winsrv\n")
            vm.stdin.flush()
            if not wait_for(log_path, b"WSV: WINDOW SERVER READY"):
                print("[FAIL] window server never became ready")
            else:
                # Exercise MOVE / DOWN / KEY.  The server composites every
                # 25 ms; give it a few seconds to paint before the screendump.
                time.sleep(3.0)
                for k in (b"m", b"n", b" ", b"A", b"B"):
                    vm.stdin.write(k)
                    vm.stdin.flush()
                    time.sleep(0.4)
                time.sleep(2.0)
                shot_ok = qmp_screendump(SHOT)
                vm.stdin.write(b"\x1b")      # Esc -> clean exit
                vm.stdin.flush()
                wait_for(log_path, b"WSV: DONE")
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return shot_ok


def check(text, shot_ok):
    ok = True
    for m in EXPECTED:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {m}")
    bound = any("e32run: bind slot" in ln and "ord 915" in ln
                for ln in text.splitlines())
    ok &= bound
    print(f"  [{'PASS' if bound else 'FAIL'}] EUSER ord 915 bound")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT")
    for ln in exc:
        print("      ", ln[:200])
    if shot_ok:
        print(f"  [PASS] screendump {SHOT}")
        try:
            from PIL import Image
            im = Image.open(SHOT).convert("RGB")
            colors = im.resize((64, 48)).getcolors(64 * 48)
            ncolors = len(colors) if colors else 0
            diverse = ncolors >= 4
            print(f"  [{'PASS' if diverse else 'FAIL'}] compositing proof: "
                  f"{ncolors} distinct colours in frame")
            ok &= diverse
        except Exception as exc2:
            print("  [INFO] PIL colour check unavailable:", exc2)
    else:
        print(f"  [INFO] screendump unavailable ({SHOT})")
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 window server QEMU test")
    ap.add_argument("--no-rebuild-iso", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    reason = retired()
    if reason:
        return skip(reason)
    check_tools()
    if not os.path.exists(E32):
        return skip(f"{E32} not built (run: make build/user/WINSRV.E32)")
    if not (args.no_rebuild_iso and os.path.exists(ISO)):
        build_iso()
    shot_ok = run_qemu(args.log)
    with open(args.log, errors="replace") as f:
        text = f.read()
    print(f"serial log: {args.log}")
    print("\n== window-server marker results")
    ok = check(text, shot_ok)
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("WSV:", "WSRV:", "winsrv", "Exc", "Panic")):
                print("   ", ln[:220])
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

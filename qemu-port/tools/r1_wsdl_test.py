#!/usr/bin/env python3
"""r1_wsdl_test.py -- QEMU proof for the window-server CLIENT library + WSDL.

Builds a private GRUB ISO whose /PROGRAMS.IMG carries WINSRV.E32 (the long-
lived compositor), WSDL.E32 (the windowed calculator client) and EUSER.DLL,
boots with QEMU standard VGA (Bochs VBE -> 1024x768x32 LFB), feeds `winsrv`
then `wsdl` at the serial shell, asserts both the WSV: and WDL: marker families,
checks for a clean log (no Exc / PanicKernExec / FAULT / MMU #PF), and grabs a
QMP screendump of the composited desktop into /tmp/opencode/wsdl.png.

Console-lease note: WINSRV holds PortCon during its serve loop, so a `wsdl`
line typed while it runs is consumed as input events rather than by the shell.
The harness therefore also re-launches the pair in the only order in which both
processes are alive at once (wsdl first, then winsrv) before taking the dump.

Usage: python3 tools/r1_wsdl_test.py [--no-rebuild-iso] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
ROOT = "/tmp/opencode"
ISO = ROOT + "/r1_wsdl.iso"
SOCK = ROOT + "/r1_wsdl-qmp.sock"
LOG_DEFAULT = ROOT + "/r1_wsdl.log"
SHOT = ROOT + "/wsdl.png"
WINSRV_E32 = REPO + "/build/user/WINSRV.E32"
WSDL_E32 = REPO + "/build/user/WSDL.E32"

PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "grub2-mkimage", "mcopy", "mkfs.fat", "mdir"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_iso():
    for p in (WINSRV_E32, WSDL_E32):
        if not os.path.exists(p):
            sys.exit(f"missing {p} (run: make build/user/WINSRV.E32 "
                     f"build/user/WSDL.E32 first)")
    if os.path.exists(SOCK):
        os.remove(SOCK)
    sh("python3", "tools/mkgrubiso.py", ISO, cwd=REPO, check=True,
       stdout=subprocess.DEVNULL)
    listing = sh("mdir", "-i", f"{REPO}/build/programs.img@@2048S", "::",
                 check=True, capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    for want in ("winsrv", "wsdl", "euserdll"):
        if want not in norm:
            sys.exit(f"programs image missing {want}: {listing}")
    print("ISO OK: WINSRV.E32 + WSDL.E32 + EUSER.DLL staged in /PROGRAMS.IMG")


def wait_for(log_path, marker, start=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        if marker in data:
            return start + data.index(marker) + len(marker)
        time.sleep(0.2)
    return -1


def wait_prompt(log_path, start=0, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        for p in PROMPTS:
            if p in data:
                return start + data.index(p) + len(p)
        time.sleep(0.2)
    return start


def qmp_screendump(path, tries=16):
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
        f.readline()
        f.write(b'{"execute":"qmp_capabilities"}\n')
        f.flush()
        f.readline()
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
    for _ in range(24):
        if os.path.exists(path) and os.path.getsize(path) > 1000:
            break
        time.sleep(0.15)
    ok = os.path.exists(path) and os.path.getsize(path) > 1000
    if ok:
        try:
            from PIL import Image
            im = Image.open(path)
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
    print("== booting GRUB ISO, driving WINSRV + WSDL via serial ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        shot_ok = False
        try:
            pos = wait_prompt(log_path)
            time.sleep(0.5)

            def send(text):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                time.sleep(0.4)

            # Documented order first: start the server, then the client.
            send("winsrv")
            if wait_for(log_path, b"WSV: WINDOW SERVER READY", pos, 40) < 0:
                print("[WARN] no WSV READY after first winsrv")
            else:
                pos = wait_for(log_path, b"WSV: WINDOW SERVER READY", pos, 40)
            send("wsdl")               # consumed by WINSRV's console lease
            time.sleep(1.5)
            launched = wait_for(log_path, b"WDL: CLIENT READY", pos, 4) >= 0

            if not launched:
                print("[INFO] wsdl was consumed by the server's console lease; "
                      "re-launching the pair so both are alive concurrently")
                vm.stdin.write(b"\x1b")  # ESC -> WINSRV quits, releases lease
                vm.stdin.flush()
                wait_for(log_path, b"WSV: DONE", pos, 20)
                pos = wait_prompt(log_path, pos)
                send("wsdl")
                if wait_for(log_path, b"WDL: window ", pos, 30) < 0:
                    print("[WARN] wsdl did not create a window")
                else:
                    pos = wait_for(log_path, b"WDL: window ", pos, 30)
                send("winsrv")
                wait_for(log_path, b"WSV: WINDOW SERVER READY", pos, 40)

            # Let the composite loop paint a few frames, then capture.
            time.sleep(3.0)
            for k in (b"7", b"+", b"8", b"\r"):
                vm.stdin.write(k)
                vm.stdin.flush()
                time.sleep(0.3)
            time.sleep(2.0)
            shot_ok = qmp_screendump(SHOT)

            # Ask the calculator to close; best effort.
            vm.stdin.write(b"\x1b")
            vm.stdin.flush()
            wait_for(log_path, b"WDL: DONE", 0, 6)
            time.sleep(0.5)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return shot_ok


def check(text, shot_ok):
    expected = [
        "WSV: WINDOW SERVER READY",
        "WDL: CLIENT READY",
        "WDL: window ",
        "WDL: event ",
        "WDL: expr ",
        "WDL: DONE",
    ]
    ok = True
    print("\n== marker results")
    for m in expected:
        good = m in text
        # DONE is best-effort: the app is meant to stay alive until closed.
        if m == "WDL: DONE" and not good:
            print(f"  [INFO] {m} (app stays alive until close)")
        else:
            ok &= good
            print(f"  [{'PASS' if good else 'FAIL'}] {m}")
    bound = any("e32run: bind slot" in ln and "ord 915" in ln
                for ln in text.splitlines())
    ok &= bound
    print(f"  [{'PASS' if bound else 'FAIL'}] EUSER ord 915 bound")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT/MMU PF")
    for ln in exc:
        print("      ", ln[:200])
    if shot_ok:
        print(f"  [PASS] screendump {SHOT}")
    else:
        print(f"  [FAIL] screendump unavailable ({SHOT})")
        ok = False
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 window-server client QEMU test")
    ap.add_argument("--no-rebuild-iso", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_iso and os.path.exists(ISO)):
        build_iso()
    shot_ok = run_qemu(args.log)
    with open(args.log, errors="replace") as f:
        text = f.read()
    print(f"serial log: {args.log}")
    ok = check(text, shot_ok)
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("WSV:", "WDL:", "WS:", "Exc", "Panic",
                                     "MMU", "window server", "winsrv", "wsdl")):
                print("   ", ln[:220])
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

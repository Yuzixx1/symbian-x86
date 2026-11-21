#!/usr/bin/env python3
"""r1_guimenu_test.py -- QEMU test for the full-screen graphical launcher.

Builds a private GRUB ISO whose /PROGRAMS.IMG contains build/user/GUIMENU.E32
plus EUSER.DLL, boots it with QEMU's standard VGA (Bochs VBE DISPI), types
`guimenu` at the serial shell, drives W/S navigation + Enter + Esc through the
PortCon lease, asserts the GM: markers, confirms no guest exception and grabs
a screendump of the rendered menu over QMP.

Usage: python3 tools/r1_guimenu_test.py [--no-rebuild-iso] [--log LOG]
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

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
ROOT = "/tmp/opencode"
ISO = ROOT + "/r1_guimenu.iso"
SOCK = ROOT + "/r1_guimenu-qmp.sock"
LOG_DEFAULT = ROOT + "/r1_guimenu.log"
SHOT = ROOT + "/guimenu.png"
E32 = REPO + "/build/user/GUIMENU.E32"

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")
EXPECTED = [
    "GM: LAUNCHER READY",
    "GM: apps=16",
    "GM: select CALC",
    "GM: select FILEMGR",
    "GM: select TEXTEDIT",
    "GM: select PING",
    "GM: launching ",
    "GM: DONE",
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "grub2-mkimage", "mcopy", "mkfs.fat"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_iso():
    if not os.path.exists(E32):
        sys.exit(f"missing {E32} (run: make build/user/GUIMENU.E32 first)")
    if os.path.exists(SOCK):
        os.remove(SOCK)
    sh("python3", "tools/mkgrubiso.py", ISO, cwd=REPO, check=True,
       stdout=subprocess.DEVNULL)
    listing = sh("mdir", "-i",
                 f"{REPO}/build/programs.img@@2048S", "::",
                 check=True, capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    for want in ("guimenu", "euserdll"):
        if want not in norm:
            sys.exit(f"programs image missing {want}")
    print("ISO OK: GUIMENU.E32 + EUSER.DLL staged in /PROGRAMS.IMG")


def wait_for(log_path, marker, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            if marker in f.read():
                return True
        time.sleep(0.2)
    return False


def qmp_screendump(path, tries=12):
    """Ask QEMU to write a PNG screendump; return True on success."""
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
    except Exception as exc:  # screendump is best-effort
        print("[WARN] QMP screendump failed:", exc)
        return False
    for _ in range(20):                    # QEMU writes the PNG asynchronously
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
    print("== booting GRUB ISO, driving GUIMENU via serial ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        shot_ok = False
        try:
            if not wait_for(log_path, b"C:/> ") and \
               not wait_for(log_path, b"X:/> "):
                print("[WARN] shell prompt not seen; continuing")
            vm.stdin.write(b"guimenu\n")
            vm.stdin.flush()
            if not wait_for(log_path, b"GM: LAUNCHER READY"):
                print("[FAIL] launcher never became ready")
            else:
                # W/S walk down then back up; A/D column jump; do not send a
                # trailing newline with the navigation keys (that would launch).
                for k in (b"s", b"s", b"s", b"w", b"d"):
                    vm.stdin.write(k)
                    vm.stdin.flush()
                    time.sleep(0.45)
                # R silently repaints: kernel marker echoes scroll the LFB.
                vm.stdin.write(b"r")
                vm.stdin.flush()
                time.sleep(0.6)
                shot_ok = qmp_screendump(SHOT)
                vm.stdin.write(b"\n")            # Enter -> exec 268 launch
                vm.stdin.flush()
                if not wait_for(log_path, b"GM: launching "):
                    print("[WARN] no launch marker")
                time.sleep(0.8)
                vm.stdin.write(b"\x1b")          # Esc -> return to shell
                vm.stdin.flush()
                wait_for(log_path, b"GM: DONE")
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
    else:
        print(f"  [INFO] screendump unavailable ({SHOT})")
    pids = [int(x) for x in re.findall(r"GM: launching \S+ r=(\d+)", text)]
    launched = any(p > 0 for p in pids)
    fallback = "GM: fallback " in text
    print(f"  [INFO] exec 268 launch worked: {launched} "
          f"(pids={pids}); fallback used: {fallback}")
    if not (launched or fallback):
        print("  [FAIL] neither exec 268 launch nor fallback reported")
        ok = False
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 graphical launcher QEMU test")
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
    print("\n== guimenu marker results")
    ok = check(text, shot_ok)
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("GM:", "guimenu", "GUI:", "Exc", "Panic")):
                print("   ", ln[:220])
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

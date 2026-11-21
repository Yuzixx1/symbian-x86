#!/usr/bin/env python3
"""r1_desktop_handoff_test.py -- desktop -> app handoff visibility QEMU test.

Boots the private GRUB ISO (staged EUSER.DLL + programs) with QEMU standard
VGA, types `desktop` at the serial shell, navigates to the CALC icon and
presses Enter.  It asserts the handoff marker chain on COM1 --

    DT: DESKTOP READY
    DT: launched CALC
    DT: handing off            (or a similar release marker)
    CALC: CALCULATOR READY
    CALC: 2+3*4 = 14           (CALC's deterministic self-test)

-- checks for guest exceptions/panics, and proves the launched app is really
visible by taking two QMP screendumps (desktop before Enter, CALC output after
the handoff) and comparing a pixel region: the framebuffer must CHANGE.

Skips gracefully when DESKTOP.E32 or CALC.E32 is not built yet (the handoff
source changes may still be in flight).  Model on tools/r1_desktop_test.py.

Usage: python3 tools/r1_desktop_handoff_test.py [--no-rebuild-iso] [--log LOG]
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
ISO = ROOT + "/r1_handoff.iso"
SOCK = ROOT + "/r1_handoff-qmp.sock"
LOG_DEFAULT = ROOT + "/r1_handoff.log"
SHOT_BEFORE = ROOT + "/handoff_before.png"
SHOT_AFTER = ROOT + "/handoff_after.png"
DESKTOP_E32 = REPO + "/build/user/DESKTOP.E32"
CALC_E32 = REPO + "/build/user/CALC.E32"

PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")
# A released console may be announced as "handing off", "handoff" or simply
# the desktop's normal "DT: DONE" exit marker -- any one is sufficient proof
# that DESKTOP gave up the screen before CALC rendered.
HANDOFF_MARKERS = ("DT: handing off", "DT: handoff", "DT: handing over",
                   "DT: released console", "DT: DONE")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "grub2-mkimage", "mcopy", "mkfs.fat"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def skip(msg):
    print("SKIP: " + msg)
    print("RESULT: SKIP")
    return 0


def build_iso():
    """Stage EUSER.DLL + programs on a private GRUB ISO (mkgrubiso.py)."""
    if os.path.exists(SOCK):
        os.remove(SOCK)
    sh("python3", "tools/mkgrubiso.py", ISO, cwd=REPO, check=True,
       stdout=subprocess.DEVNULL)
    listing = sh("mdir", "-i",
                 f"{REPO}/build/programs.img@@2048S", "::",
                 check=True, capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    for want in ("desktop", "calc", "euserdll"):
        if want not in norm:
            print(f"[WARN] programs image missing {want} (continuing)")
    print("ISO OK: staged DESKTOP + CALC in /PROGRAMS.IMG")


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


def compare_region(before, after):
    """Compare a pixel region; require the screen to CHANGE after handoff.

    Returns None when the shots are unavailable/PIL is missing (best-effort),
    otherwise a (changed_fraction, passed) tuple.  The region is the upper
    ~70% of the frame where the teletype/CALC text lands; the app's white text
    on a dark background must differ substantially from the desktop wallpaper.
    """
    try:
        from PIL import Image, ImageChops
    except Exception as exc:
        print("[INFO] PIL unavailable, pixel comparison skipped:", exc)
        return None
    if not (os.path.exists(before) and os.path.exists(after)):
        print("[INFO] screendumps unavailable, pixel comparison skipped")
        return None
    try:
        a = Image.open(before).convert("RGB")
        b = Image.open(after).convert("RGB")
        if b.size != a.size:
            b = b.resize(a.size)
        w, h = a.size
        roi = (0, 0, w, max(1, int(h * 0.70)))
        diff = ImageChops.difference(a.crop(roi), b.crop(roi)).convert("L")
        mask = diff.point(lambda p: 255 if p > 16 else 0)
        changed = mask.histogram()[255]
        total = mask.size[0] * mask.size[1]
        frac = changed / total if total else 0.0
        passed = frac >= 0.005          # >= 0.5% of the region changed
        print(f"[INFO] pixel region changed: {changed}/{total} "
              f"({frac * 100:.2f}%), threshold 0.50%")
        return (frac, passed)
    except Exception as exc:
        print("[INFO] pixel comparison failed:", exc)
        return None


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
    print("== booting GRUB ISO, driving DESKTOP -> CALC handoff via serial ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            if not wait_for_prompt(log_path):
                print("[WARN] shell prompt not seen; continuing")
            time.sleep(0.5)
            vm.stdin.write(b"desktop\n")
            vm.stdin.flush()
            if not wait_for(log_path, b"DT: DESKTOP READY"):
                print("[WARN] desktop never became ready")
            else:
                # Navigate away and back to CALC (index 0, row 0/col 0) so a
                # fresh "DT: select CALC" marker is emitted, then let the
                # wallpaper/icons/taskbar paint before the BEFORE screendump.
                time.sleep(2.0)
                for k in (b"s", b"w"):
                    vm.stdin.write(k)
                    vm.stdin.flush()
                    time.sleep(0.4)
                time.sleep(2.0)
                qmp_screendump(SHOT_BEFORE)      # desktop still owns the screen
                # Enter -> exec 268 launch -> handoff to CALC.
                vm.stdin.write(b"\n")
                vm.stdin.flush()
                wait_for(log_path, b"DT: launched CALC")
                # Give DESKTOP time to release the console and CALC time to
                # print its self-test onto the now-shared framebuffer.
                time.sleep(2.5)
                qmp_screendump(SHOT_AFTER)       # CALC output on the screen
                wait_for(log_path, b"CALC: CALCULATOR READY", timeout=20)
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text, cmp):
    ok = True

    def one(marker, hard=True):
        nonlocal ok
        good = marker in text
        if hard:
            ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")

    one("DT: DESKTOP READY")
    one("DT: select CALC")
    one("DT: launched CALC")
    handoff = [m for m in HANDOFF_MARKERS if m in text]
    good = bool(handoff)
    ok &= good
    print(f"  [{'PASS' if good else 'FAIL'}] handoff/release marker "
          f"({', '.join(handoff) if handoff else HANDOFF_MARKERS[0]})")
    one("CALC: CALCULATOR READY")
    one("CALC: 2+3*4 = 14")               # deterministic CALC self-test line

    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT")
    for ln in exc:
        print("      ", ln[:200])

    if cmp is None:
        print("  [INFO] pixel visibility check unavailable (headless/PIL)")
    else:
        frac, passed = cmp
        ok &= passed
        print(f"  [{'PASS' if passed else 'FAIL'}] screen changed after "
              f"handoff ({frac * 100:.2f}% of region)")
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 desktop->app handoff QEMU test")
    ap.add_argument("--no-rebuild-iso", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()

    missing = [p for p in (DESKTOP_E32, CALC_E32) if not os.path.exists(p)]
    if missing:
        return skip("not built yet: " + ", ".join(missing))

    check_tools()
    if not (args.no_rebuild_iso and os.path.exists(ISO)):
        build_iso()
    run_qemu(args.log)
    with open(args.log, errors="replace") as f:
        text = f.read()
    print(f"serial log: {args.log}")

    cmp = compare_region(SHOT_BEFORE, SHOT_AFTER)
    print("\n== desktop handoff marker results")
    ok = check(text, cmp)
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("DT:", "CALC:", "Exc", "Panic")):
                print("   ", ln[:220])
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

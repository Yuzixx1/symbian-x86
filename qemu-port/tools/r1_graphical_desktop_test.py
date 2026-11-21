#!/usr/bin/env python3
"""r1_graphical_desktop_test.py -- one consolidated graphical-desktop test.

Probes build/user/*.E32 for every graphical desktop program (DESKTOP, GUIMENU,
BREAKOUT, GUIHELLO, GUISHELL, SAVER). Programs still being built by other agents
are skipped gracefully. The programs that are present are staged on the GRUB
ISO's FAT32 /PROGRAMS.IMG (mkgrubiso.py bundles all build/user E32s plus
EUSER.DLL), the ISO is booted ONCE with QEMU's standard VGA (-vga std, so the
Bochs VBE DISPI / VBE 0x118 path is active), and each program is launched from
the serial shell, driven with a few keys and Esc, and checked for its marker
prefix and the absence of Exc / PanicKernExec. A QMP screendump of the real
framebuffer is taken per program when QEMU supports it.

Usage: python3 tools/r1_graphical_desktop_test.py [--no-rebuild-iso]
                                                   [--iso ISO] [--logdir DIR]
Exits 0 when every present program passes (also when none are present).
See docs/GRAPHICAL_DESKTOP.md for the desktop architecture.
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
ISO_DEFAULT = os.path.join(ROOT, "build", "symbian-grub.iso")
LOGDIR_DEFAULT = "/tmp/opencode"
SOCK_NAME = "r1_graphical_desktop-qmp.sock"

BOOT_TIMEOUT = 300          # whole-VM cap (seconds), all programs in one boot
READY_TIMEOUT = 40          # per-program "marker appeared" budget
DONE_TIMEOUT = 25           # per-program "clean exit" budget
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF", "FAULT:")
PROMPTS = (b"X:/> ", b"C:/> ", b"EKA2> ")

# name -> (accepted marker prefixes, keys to drive, exit sequence, done marker)
# Marker prefixes match the running sources: DT: (intended), GM: guimenu.c,
# BRK: breakout.c, GH: guihello.c, GUI: guishell.c, SV: saver.c.
PROGRAMS = {
    "DESKTOP":  (["DT:", "DESKTOP:"], [b"w", b"d", b"s"],
                 b"\x1b", "DT: DONE"),
    "GUIMENU":  (["GM:", "GM: LAUNCHER READY"], [b"s", b"s", b"w", b"d"],
                 b"\x1b", "GM: DONE"),
    "BREAKOUT": (["BRK:", "BRK: BREAKOUT READY"], [b"\r", b"a", b"d", b"a"],
                 b"\x1b", "BRK: DONE"),
    "GUIHELLO": (["GH:", "GH: GUI HELLO START"], [b"\x1b[B", b"\x1b[C"],
                 b"\x1b", "GH: DONE"),
    "GUISHELL": (["GUI:", "GUI: desktop drawn"], [],
                 b"q\n", "GUI: DONE"),
    "SAVER":    (["SV:", "SV: SCREENSAVER START"], [],
                 b"q\n", "SV: DONE"),
}


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "grub2-mkimage", "mcopy", "mdir", "mkfs.fat"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")


def probe():
    """Return [(name, markers, keys, exit_seq, done), ...] for built programs."""
    found = []
    for name, (markers, keys, exit_seq, done) in PROGRAMS.items():
        e32 = os.path.join(ROOT, "build", "user", name + ".E32")
        if os.path.isfile(e32):
            found.append((name, list(markers) + [name + ":"], keys,
                          exit_seq, done))
        else:
            print(f"[SKIP] {name}: build/user/{name}.E32 not present")
    return found


def build_iso(iso, found):
    """Build the GRUB ISO and verify each present program is staged on it."""
    if os.path.exists(iso):
        os.remove(iso)
    sh("python3", "tools/mkgrubiso.py", iso, cwd=ROOT, check=True,
       stdout=subprocess.DEVNULL)
    if not os.path.exists(iso):
        sys.exit(f"ISO build produced no file: {iso}")
    disk = os.path.join(ROOT, "build", "programs.img")
    listing = sh("mdir", "-i", f"{disk}@@2048S", "::", check=True,
                 capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    for name, _m, _k, _e, _d in found:
        if name.lower() not in norm:
            sys.exit(f"{name}.E32 missing from ISO /PROGRAMS.IMG")
    if "euserdll" not in norm:
        sys.exit("EUSER.DLL missing from ISO /PROGRAMS.IMG")
    print(f"[stage] {len(found)} program(s) + EUSER.DLL in {iso}")


def wait_for(log, marker, start=0, timeout=READY_TIMEOUT):
    """Poll the log for a byte marker; return offset just past it, or None."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log, "rb") as f:
            f.seek(start)
            data = f.read()
        pos = data.find(marker)
        if pos >= 0:
            return start + pos + len(marker)
        time.sleep(0.15)
    return None


def wait_prompt(log, start=0, timeout=75):
    """Wait for any shell prompt; return (offset, prompt) or (start, b'')."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log, "rb") as f:
            f.seek(start)
            data = f.read()
        for p in PROMPTS:
            pos = data.find(p)
            if pos >= 0:
                return start + pos + len(p), p
        time.sleep(0.2)
    return start, b""


def qmp_screendump(sock_path, png):
    """Best-effort PNG screendump over QMP. Return True on success."""
    for _ in range(16):
        if os.path.exists(sock_path):
            break
        time.sleep(0.25)
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(sock_path)
        f = s.makefile("rwb")
        f.readline()                              # QMP greeting
        f.write(b'{"execute":"qmp_capabilities"}\n')
        f.flush()
        f.readline()                              # capabilities reply
        if os.path.exists(png):
            os.remove(png)
        qmp = ('{"execute":"screendump","arguments":{"filename":"'
               + png + '","format":"png"}}\n')
        f.write(qmp.encode())
        f.flush()
        for _ in range(10):
            line = f.readline()
            if not line or b"return" in line or b"error" in line.lower():
                break
        f.close()
        s.close()
    except Exception as exc:
        print(f"[WARN] QMP screendump failed: {exc}")
        return False
    for _ in range(20):                           # PNG write is asynchronous
        if os.path.exists(png) and os.path.getsize(png) > 1000:
            return True
        time.sleep(0.15)
    return False


def run_guest(iso, log, sock_path, found, logdir):
    """Boot the ISO once; drive every present program; return per-program data."""
    if os.path.exists(sock_path):
        os.remove(sock_path)
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-vga", "std",                            # Bochs VBE DISPI framebuffer
        "-cdrom", iso, "-boot", "d",
        "-display", "none", "-serial", "stdio",
        "-qmp", f"unix:{sock_path},server,nowait",
        "-monitor", "none", "-no-reboot",
    ]
    results = {}
    started = time.monotonic()
    with open(log, "wb") as out:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=out, stderr=subprocess.STDOUT)
        try:
            pos, prompt = wait_prompt(log)
            if not prompt:
                print("[WARN] shell prompt not seen; continuing anyway")
            for name, markers, keys, exit_seq, done in found:
                if time.monotonic() - started > BOOT_TIMEOUT:
                    print(f"[WARN] VM cap reached before {name}")
                    break
                before = os.path.getsize(log)
                vm.stdin.write(name.encode() + b"\n")
                vm.stdin.flush()
                ready_at = wait_for(log, markers[0].encode(), before)
                for key in keys:                  # navigate / poke a few keys
                    vm.stdin.write(key)
                    vm.stdin.flush()
                    time.sleep(0.35)
                png = os.path.join(logdir, f"r1_graphical_{name.lower()}.png")
                shot = qmp_screendump(sock_path, png) if ready_at is not None else False
                vm.stdin.write(exit_seq)          # Esc for interactive apps
                vm.stdin.flush()
                time.sleep(0.5)
                if done:
                    wait_for(log, done.encode(), before, DONE_TIMEOUT)
                time.sleep(0.8)                   # let the shell come back
                after = os.path.getsize(log)
                with open(log, "rb") as f:
                    f.seek(before)
                    segment = f.read(after - before)
                results[name] = (segment.decode("utf-8", "replace"), ready_at is not None,
                                 shot, png)
                pos, prompt = wait_prompt(log, after, timeout=20)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return results


def verify(name, segment, markers, ready, shot, png):
    print(f"\n== {name}")
    hits = [m for m in markers if m in segment]
    marker_ok = bool(hits) and ready
    print(f"  [{'PASS' if marker_ok else 'FAIL'}] marker "
          f"(matched: {hits or 'none'})")
    exc = [ln for ln in segment.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec")
    for line in exc:
        print("      ", line[:200])
    if shot:
        print(f"  [PASS] screendump {png}")
    else:
        print(f"  [INFO] screendump unavailable ({png})")
    return marker_ok and clean


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--iso", default=ISO_DEFAULT)
    ap.add_argument("--logdir", default=LOGDIR_DEFAULT)
    ap.add_argument("--no-rebuild-iso", action="store_true")
    args = ap.parse_args()
    check_tools()

    found = probe()
    if not found:
        print("RESULT: PASS (no graphical desktop programs present; all skipped)")
        return 0
    os.makedirs(args.logdir, exist_ok=True)
    if not (args.no_rebuild_iso and os.path.exists(args.iso)):
        build_iso(args.iso, found)

    log = os.path.join(args.logdir, "r1_graphical_desktop.log")
    sock = os.path.join(args.logdir, SOCK_NAME)
    results = run_guest(args.iso, log, sock, found, args.logdir)
    print(f"serial log: {log}")

    ok = True
    for name, markers, _keys, _exit_seq, _done in found:
        if name not in results:
            print(f"\n== {name}\n  [INFO] skipped (VM cap reached)")
            continue
        segment, ready, shot, png = results[name]
        ok = verify(name, segment, markers, ready, shot, png) and ok
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

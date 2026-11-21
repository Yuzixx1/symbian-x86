#!/usr/bin/env python3
"""r1_gui_apps_test.py -- consolidated QEMU test for the windowed GUI apps.

One test for the two windowed E32 applications being built for the graphical
desktop: DTCALC (calculator) and DTEDIT (text editor). Each app is staged with
EUSER.DLL on a private GRUB ISO, booted with QEMU standard VGA (Bochs VBE DISPI
-> 1024x768x32 LFB), launched from the serial shell, driven with a few keys,
and checked for its startup/exit markers and a clean guest log. A PNG
screendump of the framebuffer is grabbed over QMP for visual inspection.

An app whose build/user/<NAME>.E32 is not present yet is SKIPPED (the other
agents are still building them); the test passes when every available app
passes. Usage:

    python3 tools/r1_gui_apps_test.py [--no-rebuild-iso] [--log DIR]
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
ISO = ROOT + "/r1_guiapps.iso"
SOCK = ROOT + "/r1_guiapps-qmp.sock"
LOG_DIR = ROOT
PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")
BOOT_MARKERS = ("DISPLAY: VBE mode 0x", "GUI: execs 249/250/251 installed")
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")

# Candidate marker prefixes: the two apps are still being written, so accept
# any of the plausible prefixes for their start and clean-exit markers.
APPS = {
    "dtcalc": {
        "e32": "DTCALC.E32",
        # The app actually prints "DCALC:" (not "DTCALC:") markers.
        "start": ("DCALC:", "DTC:", "DTCALC:", "CALC: CALCULATOR READY",
                  "DT: CALC"),
        "done": ("DCALC: DONE", "DTC: DONE", "DTCALC: DONE", "CALC: DONE"),
        "keys": (b"2", b"+", b"3", b"\r"),
    },
    "dtedit": {
        "e32": "DTEDIT.E32",
        "start": ("DTE:", "DTEDIT:", "EDIT:", "DT: EDIT"),
        "done": ("DTE: DONE", "DTEDIT: DONE", "EDIT: DONE"),
        "keys": (b"H", b"i", b"\r", b"\x08"),
    },
}


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "grub2-mkimage", "mcopy", "mkfs.fat", "mdir"):
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def available_apps():
    """Probe build/user for each E32; return {name: spec} of what exists."""
    found = {}
    for name, spec in APPS.items():
        path = os.path.join(REPO, "build/user", spec["e32"])
        if os.path.isfile(path):
            found[name] = spec
        else:
            print(f"[SKIP] {spec['e32']} not built yet ({path})")
    return found


def build_iso():
    """Bundle every built E32 plus EUSER.DLL into /PROGRAMS.IMG on an ISO."""
    if os.path.exists(SOCK):
        os.remove(SOCK)
    sh("python3", "tools/mkgrubiso.py", ISO, cwd=REPO, check=True,
       stdout=subprocess.DEVNULL)
    listing = sh("mdir", "-i", f"{REPO}/build/programs.img@@2048S", "::",
                 check=True, capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    if "euserdll" not in norm:
        sys.exit("programs image missing EUSER.DLL")
    print("ISO OK: EUSER.DLL staged in /PROGRAMS.IMG")
    return norm


def wait_for(log_path, needle, timeout=70, start=0):
    """Wait for a byte string in the log; return True or False (never raises)."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            if needle in f.read():
                return True
        time.sleep(0.2)
    return False


def wait_for_prompt(log_path, timeout=70):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            data = f.read()
        if any(p in data for p in PROMPTS):
            return True
        time.sleep(0.2)
    return False


def qmp_screendump(path):
    """Best-effort QMP PNG screendump; return True on success."""
    for _ in range(16):
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
        f.readline()                            # QMP greeting
        f.write(b'{"execute":"qmp_capabilities"}\n')
        f.flush()
        f.readline()                            # capabilities reply
        if os.path.exists(path):
            os.remove(path)
        cmd = ('{"execute":"screendump","arguments":{"filename":"'
               + path + '","format":"png"}}\n')
        f.write(cmd.encode())
        f.flush()
        for _ in range(10):
            line = f.readline()
            if not line or b"return" in line or b"error" in line.lower():
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
        print(f"[INFO] screendump {path} ({os.path.getsize(path)} bytes)")
    return ok


def run_app(name, spec, log_path, png_path):
    """Boot the ISO once and drive a single windowed app; return shot_ok."""
    if os.path.exists(SOCK):
        os.remove(SOCK)
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-qmp", f"unix:{SOCK},server,nowait",
        "-cdrom", ISO, "-boot", "d", "-monitor", "none", "-no-reboot",
    ]
    print(f"== booting GRUB ISO, driving {name} via serial ...")
    shot_ok = False
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            if not wait_for_prompt(log_path):
                print(f"[WARN] {name}: shell prompt not seen; continuing")
            time.sleep(0.5)
            vm.stdin.write((name + "\n").encode())
            vm.stdin.flush()
            time.sleep(2.0)
            for key in spec["keys"]:                # feed a few app keys
                vm.stdin.write(key)
                vm.stdin.flush()
                time.sleep(0.5)
            time.sleep(1.5)
            shot_ok = qmp_screendump(png_path)
            vm.stdin.write(b"\x1b")                  # Esc: exit to shell
            vm.stdin.flush()
            time.sleep(1.5)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return shot_ok


def check_app(text, spec, shot_ok, png_path):
    """Assert boot, startup/exit markers, ord 915 and a clean log."""
    ok = True
    for m in BOOT_MARKERS:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] boot: {m}")
    for label, candidates in (("start", spec["start"]), ("done", spec["done"])):
        hits = [m for m in candidates if m in text]
        good = bool(hits)
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {label} marker "
              f"({', '.join(hits) if hits else 'none of ' + repr(candidates)})")
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
        print(f"  [PASS] screendump {png_path}")
    else:
        print(f"  [INFO] screendump unavailable ({png_path})")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-iso", action="store_true")
    ap.add_argument("--log-dir", default=LOG_DIR)
    args = ap.parse_args()
    check_tools()
    apps = available_apps()
    if not apps:
        print("RESULT: SKIP (no windowed GUI app E32s built yet)")
        return 0
    if not (args.no_rebuild_iso and os.path.exists(ISO)):
        build_iso()

    all_ok = True
    for name, spec in apps.items():
        log_path = f"{args.log_dir}/r1_guiapps_{name}.log"
        png_path = f"{ROOT}/guiapps_{name}.png"
        shot_ok = run_app(name, spec, log_path, png_path)
        with open(log_path, errors="replace") as f:
            text = f.read()
        print(f"\n== {name} results (serial log: {log_path})")
        all_ok &= check_app(text, spec, shot_ok, png_path)
        if not all_ok:
            for ln in text.splitlines():
                if any(k in ln.lower() for k in ("dtc", "dte", "calc", "edit",
                                                 "e32run", "exc", "panic")):
                    print("   ", ln[:220])

    print("\nRESULT: " + ("PASS" if all_ok else "FAIL"))
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())

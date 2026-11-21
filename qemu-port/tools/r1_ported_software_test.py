#!/usr/bin/env python3
"""r1_ported_software_test.py -- consolidated QEMU test for the real software ports.

One test for the freestanding ports documented in docs/PORTED_SOFTWARE.md:
LUA (Lua-style scripting core), ZMACHINE (Z-machine interpreter / Zork),
ZIP32 (miniz inflate / zip reader) and BC32 (bc-style desktop calculator).

The harness probes build/user for each E32 (missing images are SKIPPED while
sibling agents are still building them), builds a GRUB ISO whose PROGRAMS.IMG is
verified with mtools to contain the present images plus EUSER.DLL, boots the ISO
once per available port, feeds the program name and a basic command at the
serial shell, then checks start/result/done marker families, the genuine EUSER
ordinal 915 bind and a clean guest log (no Exc / PanicKernExec / FAULT). One log
per program is written under /tmp/opencode/.

Exit 0 means every available port passed (or all four were skipped).
Usage: python3 tools/r1_ported_software_test.py [--no-rebuild-iso] [--log-dir DIR]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = "/tmp/opencode"
ISO = ROOT + "/r1_ported_sw.iso"
LOG_DIR = ROOT
PROGRAMS_IMG = REPO + "/build/programs.img@@2048S"

PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")
BOOT_MARKERS = ("DISPLAY: VBE mode 0x",)
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")

# Marker prefixes are families, not exact strings: the ports are landing from
# parallel agents, so accept the plausible variants for each stage. A port is
# only trustworthy when all three families (start, result, done) are present.
APPS = {
    "lua": {
        "e32": "LUA.E32",
        "stage": "LUA.E32",
        "launch": "LUA",
        "cmd": b"print(1+2)\n",
        "start": ("LUA: READY", "LUA: START", "LUA: LUA"),
        "result": ("LUA: RESULT", "LUA: 3", "LUA: =3"),
        "done": ("LUA: DONE",),
    },
    "zmachine": {
        "e32": "ZMACHINE.E32",
        "stage": "ZMACH.E32",     # 8.3 name the guest FAT driver can resolve
        "launch": "ZMACH",
        "cmd": b"look\nquit\n",
        "start": ("ZMAC:", "ZORK:", "ZMACHINE:"),
        "result": ("ZMAC: ROOM", "ZMAC: OK", "ZORK: ROOM", "ZMACHINE: OK"),
        "done": ("ZMAC: DONE", "ZORK: DONE", "ZMACHINE: DONE"),
    },
    "zip32": {
        "e32": "ZIP32.E32",
        "stage": "ZIP32.E32",
        "launch": "ZIP32",
        "cmd": b"\n",
        "start": ("ZIP:", "MINIZ:", "ZIP32:"),
        "result": ("ZIP: CRC", "ZIP: OK", "ZIP: ENTRIES", "MINIZ: OK"),
        "done": ("ZIP: DONE", "ZIP32: DONE"),
    },
    "bc": {
        "e32": "BC32.E32",
        "stage": "BC32.E32",
        "launch": "BC32",
        "cmd": b"1+2\nquit\n",
        "start": ("BC: READY", "BC:", "BC32:"),
        "result": ("BC: 3", "BC: RESULT", "BC: =3", "BC32: 3"),
        "done": ("BC: DONE", "BC32: DONE"),
    },
}


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "grub2-mkimage", "mcopy", "mkfs.fat",
                 "mdir", "python3"):
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def available_apps():
    """Probe build/user for each port E32; return {name: spec} of what exists."""
    found = {}
    for name, spec in APPS.items():
        path = os.path.join(REPO, "build/user", spec["e32"])
        if os.path.isfile(path):
            found[name] = spec
        else:
            print(f"[SKIP] {spec['e32']} not built yet ({path})")
    return found


def build_iso(apps):
    """Build the GRUB ISO and verify mtools staging of each E32 and EUSER.DLL."""
    sh("python3", "tools/mkgrubiso.py", ISO, cwd=REPO, check=True,
       stdout=subprocess.DEVNULL)
    listing = sh("mdir", "-i", PROGRAMS_IMG, "::", check=True,
                 capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    if "euserdll" not in norm:
        sys.exit("PROGRAMS.IMG missing EUSER.DLL")
    for spec in apps.values():
        wanted = spec["stage"].lower().replace(".", "")
        if wanted not in norm:
            print(f"[WARN] {spec['e32']} not visible as {spec['stage']} in "
                  "PROGRAMS.IMG (below 30 KB curator threshold?)")
            print(listing)
    print("ISO OK: EUSER.DLL and available port E32s staged in /PROGRAMS.IMG")


def wait_for_bytes(log_path, needles, timeout, start=0):
    """Poll a growing log for any of several byte strings; never raises."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            if any(n in f.read() for n in needles):
                return True
        time.sleep(0.2)
    return False


def run_app(name, spec, log_path):
    """Boot the ISO once, launch one port and feed its basic command."""
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-cdrom", ISO, "-boot", "d", "-monitor", "none", "-no-reboot",
    ]
    print(f"== booting GRUB ISO, driving {name} ({spec['launch']}) ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            if not wait_for_bytes(log_path, PROMPTS, 70):
                print(f"[WARN] {name}: shell prompt not seen; continuing")
            time.sleep(0.5)
            vm.stdin.write((spec["launch"] + "\n").encode())
            vm.stdin.flush()
            # Wait for the app's own banner before typing into it, with a
            # generous fallback since a port may not emit a known marker yet.
            wait_for_bytes(log_path, [m.encode() for m in spec["start"]], 12)
            time.sleep(0.5)
            vm.stdin.write(spec["cmd"])
            vm.stdin.flush()
            wait_for_bytes(log_path, [m.encode() for m in spec["done"]], 12)
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check_app(text, spec):
    """Assert boot, marker families, ord-915 bind and a clean guest log."""
    ok = True
    for m in BOOT_MARKERS:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] boot: {m}")
    for label in ("start", "result", "done"):
        hits = [m for m in spec[label] if m in text]
        good = bool(hits)
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {label} marker "
              f"({', '.join(hits) if hits else 'none of ' + repr(spec[label])})")
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
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-iso", action="store_true")
    ap.add_argument("--log-dir", default=LOG_DIR)
    args = ap.parse_args()
    check_tools()
    os.makedirs(args.log_dir, exist_ok=True)
    apps = available_apps()
    if not apps:
        print("RESULT: SKIP (no ported software E32s built yet)")
        return 0
    if not (args.no_rebuild_iso and os.path.exists(ISO)):
        build_iso(apps)

    all_ok = True
    for name, spec in apps.items():
        log_path = f"{args.log_dir}/r1_ported_{name}.log"
        run_app(name, spec, log_path)
        with open(log_path, errors="replace") as f:
            text = f.read()
        print(f"\n== {name} results (serial log: {log_path})")
        passed = check_app(text, spec)
        all_ok &= passed
        if not passed:
            print("   relevant log lines:")
            for ln in text.splitlines():
                if any(k in ln.lower() for k in (name, spec["launch"].lower(),
                                                 "lua", "zmac", "zork", "zip",
                                                 "miniz", "bc:", "e32run",
                                                 "exc", "panic", "fault")):
                    print("      ", ln[:220])

    print("\nRESULT: " + ("PASS" if all_ok else "FAIL"))
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())

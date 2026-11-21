#!/usr/bin/env python3
"""r1_gui_regression.py -- one test for all new GUI / BASIC E32 programs.

Probes build/user/*.E32 for BREAKOUT, GUIMENU, GUIHELLO and BASIC. Missing
programs are skipped (other agents are still building them); for every program
that is present, the script stages it plus EUSER.DLL on a FAT32 disk, boots the
R1 kernel with QEMU's standard VGA (-vga std, so the Bochs VBE DISPI path is
active), launches the program from the serial shell, and checks that it emits a
known marker and produces no guest exception or kernel panic.

This is intentionally simple and lenient about marker text -- the programs are
being written concurrently, so a prefix match is enough to prove the program
ran. Tighten the MARKERS table once the programs' marker prefixes are frozen.

Usage: python3 tools/r1_gui_regression.py [--kernel PATH] [--euser PATH]
                                          [--logdir DIR] [--no-rebuild-disk]
Exits 0 when every present program passes (also when none are present).
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KERNEL_DEFAULT = os.path.join(ROOT, "build/kernfull.elf")
EUSER_DEFAULT = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/"
                 "epoc32/release/x86/urel/euser.dll")
LOGDIR_DEFAULT = "/tmp/opencode"

MB = 32
MBR_LBA = 2048
PART_SECTORS = 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = MBR_LBA * 512
BOOT_TIMEOUT = 55

# Each entry: accepted marker prefixes. The upper-case program name + ':' is
# also accepted automatically as a fallback marker.
PROGRAMS = [
    ("BREAKOUT", ["BO:", "BR:", "BK:", "BREAKOUT:"]),
    ("GUIMENU",  ["GM:", "MENU:", "ML:", "GUIMENU:"]),
    ("GUIHELLO", ["HG:", "GH:", "GUIHELLO:"]),
    ("BASIC",    ["BASIC:", "BA:", "BAS:"]),
]
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")


def probe():
    """Return [(name, e32_path, markers), ...] for programs that exist."""
    found = []
    for name, markers in PROGRAMS:
        e32 = os.path.join(ROOT, "build/user", name + ".E32")
        if os.path.exists(e32):
            found.append((name, e32, list(markers) + [name + ":"]))
        else:
            print(f"[SKIP] {name}: build/user/{name}.E32 not present")
    return found


def build_disk(name, e32, euser, disk, fs):
    for path in (disk, fs):
        if os.path.exists(path):
            os.remove(path)
    with open(disk, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(fs, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", fs, check=True)
    # Stage under the bare 8.3 name the shell types at the EKA2> prompt.
    for src, dest in ((e32, name), (euser, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", fs, src, "::" + dest, check=True)
    with open(fs, "rb") as f:
        fat = f.read()
    with open(disk, "r+b") as f:
        f.seek(OFFSET)
        f.write(fat)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + MBR_LBA.to_bytes(4, "little")
                 + PART_SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{disk}@@{OFFSET}", "::", check=True,
             capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    if name.lower() not in norm or "euserdll" not in norm:
        sys.exit(f"mdir staging failed for {name}:\n{out.stdout}")
    print(f"[stage] {name}.E32 and EUSER.DLL OK")


def run_qemu(kernel, name, disk, log):
    qemu = [
        "timeout", str(BOOT_TIMEOUT), "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-vga", "std",                 # Bochs VBE DISPI -> real framebuffer
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={disk},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    # Wake the shell, launch the program, then poke a few keys so interactive
    # programs (games/interpreters) get input and can exit before the cap.
    feeder = ("sleep 2.0; printf '\\n'; sleep 2.0; "
              f"printf '{name}\\n'; sleep 4.0; "
              "printf 'q\\n'; sleep 2.0; printf '\\n'; sleep 2.0")
    cmd = ("{ " + feeder + "; } | "
           + " ".join(shlex.quote(a) for a in qemu)
           + " > " + shlex.quote(log) + " 2>&1")
    print(f"== QEMU ({BOOT_TIMEOUT}s cap), feeding '{name}' ==")
    subprocess.run(["bash", "-c", cmd], check=False)
    time.sleep(1.0)   # let timeout's SIGTERM flush redirected serial stdio
    try:
        with open(log, "r", errors="replace") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def verify(text, markers):
    hits = [m for m in markers if m in text]
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    return hits, exc


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--euser", default=EUSER_DEFAULT)
    ap.add_argument("--logdir", default=LOGDIR_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()

    found = probe()
    if not found:
        print("RESULT: PASS (no new GUI/BASIC programs present; all skipped)")
        return 0
    os.makedirs(args.logdir, exist_ok=True)
    ok = True
    for name, e32, markers in found:
        disk = os.path.join(args.logdir, f"r1_gui_reg_{name.lower()}.img")
        fs = os.path.join(args.logdir, f"r1_gui_reg_{name.lower()}_fs.img")
        log = os.path.join(args.logdir, f"r1_gui_reg_{name.lower()}.log")
        if not (args.no_rebuild_disk and os.path.exists(disk)):
            build_disk(name, e32, args.euser, disk, fs)
        text = run_qemu(args.kernel, name, disk, log)

        hits, exc = verify(text, markers)
        marker_ok = bool(hits)
        clean = not exc
        sub = marker_ok and clean
        ok = ok and sub
        print(f"  [{('PASS' if marker_ok else 'FAIL')}] {name} marker "
              f"(matched: {hits or 'none'})")
        print(f"  [{('PASS' if clean else 'FAIL')}] {name} no Exc/"
              f"PanicKernExec")
        if exc:
            for line in exc:
                print("   ", line[:200])
        print(f"  serial log: {log}")
        print(f"  [{('PASS' if sub else 'FAIL')}] {name} overall")

    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

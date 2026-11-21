#!/usr/bin/env python3
"""r1_basic_test.py -- QEMU test for the SMALL BASIC INTERPRETER E32.

Stages build/user/BASIC.E32 (a PE2 image importing EUSER.DLL ordinal 915 =
RDebug::Printf) plus the genuine EUSER.DLL on a FAT32 disk, boots the R1
kernel, launches `basic` from the serial shell, loads the built-in DEMO
number-guessing game, and drives it with prompt-synchronised PortCon input:
the test binary-searches the secret from the TOO LOW / TOO HIGH hints until
the interpreter prints BA: WIN.  It then quits and asserts the BA: markers,
the ord 915 binding, and the absence of guest exceptions or kernel panics.

Usage: python3 tools/r1_basic_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK = ROOT + "/r1_basic.img"
FS = ROOT + "/r1_basic_fs.img"
LOG = ROOT + "/r1_basic.log"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
E32 = os.path.join(REPO, "build/user/BASIC.E32")
KERNEL = os.path.join(REPO, "build/kernfull.elf")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def build_disk():
    for path in (DISK, FS):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "BASIC.E32"), (DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", FS, src, f"::{name}", check=True)
    with open(FS, "rb") as f:
        data = f.read()
    if len(data) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    with open(DISK, "r+b") as f:
        f.seek(OFFSET)
        f.write(data)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + LBA.to_bytes(4, "little") + SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK}@@{OFFSET}", "::", check=True,
             capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for wanted in ("basice32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: BASIC.E32 and EUSER.DLL staged")


def tail_from(path, pos):
    with open(path, "rb") as f:
        f.seek(pos)
        return f.read()


def wait_any(log_path, markers, start=0, timeout=60):
    """Return (matching marker bytes, new position after ALL its copies).

    Every BA: marker is emitted twice (EUSER ord 915 + DebugPrint fallback)
    on consecutive lines, so consume the duplicate to avoid stale matches.
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        data = tail_from(log_path, start)
        found = None
        best = None
        for m in markers:
            i = data.find(m)
            if i >= 0 and (best is None or i < best):
                best, found = i, m
        if found is not None:
            pos = start + best + len(found)
            # Skip CRLF and any immediately repeated copy of the same marker.
            while True:
                nxt = tail_from(log_path, pos)
                q = 0
                while q < len(nxt) and nxt[q] in b"\r\n":
                    q += 1
                if nxt[q:q + len(found)] == found:
                    pos += q + len(found)
                else:
                    break
            return found, pos
        time.sleep(0.1)
    raise RuntimeError(f"serial log missing one of {markers!r} (see {log_path})")


def run_qemu(log_path):
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, driving BASIC via serial console ...")
    outcome = {"win": False}
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_any(log_path, [b"C:/> "], 0, 60)[1]

            def send(text, expect, timeout=60):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                _, pos = wait_any(log_path, [expect], pos, timeout)

            send("basic", b"BA: BASIC READY")
            send("LOAD DEMO", b"BA: LOADED DEMO")
            send("RUN", b"BA: RUN ")
            wait_any(log_path, [b"BA: GUESS A NUMBER"], pos, 30)
            _, pos = wait_any(log_path, [b"BA: GUESS?"], pos, 30)

            lo, hi = 1, 100
            for _ in range(10):
                guess = (lo + hi) // 2
                vm.stdin.write(str(guess).encode() + b"\n")
                vm.stdin.flush()
                hint, pos = wait_any(
                    log_path,
                    [b"BA: WIN", b"BA: TOO LOW", b"BA: TOO HIGH", b"BA: LOSE"],
                    pos, 30)
                if hint == b"BA: WIN":
                    outcome["win"] = True
                    break
                if hint == b"BA: TOO LOW":
                    lo = guess + 1
                elif hint == b"BA: TOO HIGH":
                    hi = guess - 1
                else:
                    break
                _, pos = wait_any(log_path, [b"BA: GUESS?"], pos, 30)

            # After END the interpreter is back at its command prompt.
            _, pos = wait_any(log_path, [b"BASIC> "], pos, 30)
            send("QUIT", b"BA: DONE")
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return outcome["win"]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")
    if not (args.no_rebuild_disk and os.path.exists(DISK)):
        build_disk()

    won = False
    try:
        won = run_qemu(args.log)
    except RuntimeError as exc:
        print("QEMU drive failed:", exc)
    with open(args.log, errors="replace") as f:
        text = f.read()

    ok = True

    def line_pos(pattern):
        m = re.search(r"(?m)^" + pattern + r"$", text)
        return m.start() if m else -1

    for marker in ("BA: BASIC READY", "BA: > LOAD DEMO",
                   "BA: > RUN", "BA: LOADED DEMO", "BA: RUN ",
                   "BA: GUESS A NUMBER FROM 1 TO 100", "BA: GUESS?",
                   "BA: > QUIT", "BA: DONE"):
        good = marker in text
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")

    hints = len(re.findall(r"(?m)^BA: TOO (LOW|HIGH)$", text))
    good = hints >= 1
    ok = ok and good
    print(f"  [{'PASS' if good else 'FAIL'}] guessing-game hints (found {hints})")

    good = won
    ok = ok and good
    print(f"  [{'PASS' if good else 'FAIL'}] game reached BA: WIN")

    p_ready = text.find("BA: BASIC READY")
    p_run = text.find("BA: RUN ")
    p_guess = text.find("BA: GUESS A NUMBER")
    p_end = text.find("BA: WIN")
    p_done = text.find("BA: DONE")
    ordered = all(p >= 0 for p in (p_ready, p_run, p_guess, p_end, p_done)) \
        and p_ready < p_run < p_guess < p_end < p_done
    ok = ok and ordered
    print(f"  [{'PASS' if ordered else 'FAIL'}] marker order")

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok = ok and bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    exc_lines = [ln for ln in text.splitlines()
                 if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines
    ok = ok and clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT in log")
    for line in exc_lines:
        print("   ", line[:220])

    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        shown = set()
        for line in text.splitlines():
            if "ba:" in line.lower() or "e32run" in line.lower():
                if line not in shown:
                    print("   ", line[:220])
                    shown.add(line)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

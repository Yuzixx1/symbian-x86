#!/usr/bin/env python3
"""r1_quiz_test.py -- QEMU test for the TRIVIA QUIZ game E32.

Stages build/user/QUIZ.E32 (a PE2 image importing EUSER.DLL ordinal 915 =
RDebug::Printf) plus the genuine EUSER.DLL on a FAT32 disk, boots the R1
kernel, launches `quiz` from the serial shell, drives the built-in `selftest`
(all six questions answered correctly -> 6/6) and `quit` through
prompt-synchronised PortCon input, and asserts the QZ: markers with no guest
exception or panic.

Usage: python3 tools/r1_quiz_test.py [--no-rebuild-disk] [--log PATH]
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
DISK = ROOT + "/r1_quiz.img"
FS = ROOT + "/r1_quiz_fs.img"
LOG = ROOT + "/r1_quiz.log"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
E32 = os.path.join(REPO, "build/user/QUIZ.E32")
KERNEL = os.path.join(REPO, "build/kernfull.elf")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")
PROMPT = b"QZ> "


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
    for src, name in ((E32, "QUIZ.E32"), (DLL, "EUSER.DLL")):
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
    for wanted in ("quize32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: QUIZ.E32 and EUSER.DLL staged")


def wait_for(log_path, marker, start=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        if marker in data:
            return start + data.index(marker) + len(marker)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {marker!r} (see {log_path})")


def run_qemu(log_path):
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, driving QUIZ via serial console ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, b"C:/> ", timeout=60)

            def send(text, expect, timeout=30):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("quiz", PROMPT, 60)     # launch + startup header marker
            send("selftest", PROMPT)     # all six answered -> final score 6/6
            send("quit", b"QZ: DONE")
            time.sleep(1.0)              # let the final marker flush
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


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

    try:
        run_qemu(args.log)
    except RuntimeError as exc:
        print("QEMU drive failed:", exc)
    with open(args.log, errors="replace") as f:
        text = f.read()

    ok = True

    def line_pos(pattern):
        m = re.search(r"(?m)^" + pattern + r"$", text)
        return m.start() if m else -1

    def count(pattern):
        return len(re.findall(r"(?m)^" + pattern + r"$", text))

    # Static markers, each emitted through ord 915 + the DebugPrint fallback.
    for marker in ("QZ: TRIVIA QUIZ (6 questions)",
                   "QZ: SELFTEST OK", "QZ: DONE"):
        good = line_pos(re.escape(marker)) >= 0
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")

    # All six question + options markers, and six correct-answer results.
    checks = [
        ("six question markers", r"QZ: Q[1-6]: .+", 6),
        ("six options markers", r"QZ: options: 1\).* 2\).* 3\).* 4\).*", 6),
        ("six correct answers", r"QZ: correct answer=[1-4] correct=[1-4]", 6),
    ]
    for name, pattern, want in checks:
        n = count(pattern)
        good = n >= want
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {name} (found {n}, want >={want})")

    # Selftest never answers wrongly, and the engine must report a perfect 6/6.
    wrong = count(r"QZ: wrong answer=[1-4] correct=[1-4]")
    good = wrong == 0
    ok = ok and good
    print(f"  [{'PASS' if good else 'FAIL'}] no wrong answers (found {wrong})")
    perfect = line_pos(re.escape("QZ: final score=6/6")) >= 0
    ok = ok and perfect
    print(f"  [{'PASS' if perfect else 'FAIL'}] QZ: final score=6/6")

    # Marker ordering: header -> questions -> final score -> DONE.
    p_header = line_pos(re.escape("QZ: TRIVIA QUIZ (6 questions)"))
    p_first_q = line_pos(r"QZ: Q1: .+")
    p_score = line_pos(re.escape("QZ: final score=6/6"))
    p_done = line_pos(re.escape("QZ: DONE"))
    ordered = all(p >= 0 for p in (p_header, p_first_q, p_score, p_done)) \
        and p_header < p_first_q < p_score < p_done
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
            if "qz:" in line.lower() and line not in shown:
                print("   ", line[:220])
                shown.add(line)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

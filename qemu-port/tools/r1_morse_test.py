#!/usr/bin/env python3
"""r1_morse_test.py -- end-to-end QEMU test for MORSE.E32.

Stages build/user/MORSE.E32 (a PE2 image importing EUSER.DLL ordinal 915) and
the genuine EUSER.DLL into a FAT32 disk, boots build/kernfull.elf on q35 with
an AC97 sound device, launches "morse" from the serial shell, drives the
built-in "selftest" and "quit" commands through prompt-synchronised console
input, and asserts that:
  * the PE2 E32 validates and binds EUSER ordinal 915 (RDebug::Printf),
  * the MRS markers (start / SOS encoding / symbols / DONE) all appear,
  * SOS encodes to the canonical "... --- ...",
  * no guest exception or kernel panic occurs.

Exits 0 only if every check passes.
Usage: python3 tools/r1_morse_test.py [--no-rebuild-disk] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK_IMG = ROOT + "/r1_morse_test.img"
FS_IMG = ROOT + "/r1_morse_test_fs.img"
LOG_DEFAULT = ROOT + "/r1_morse_test.log"

DISK_MB = 32
PART_LBA, PART_SECTORS = 2048, 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
MORSE_E32 = REPO + "/build/user/MORSE.E32"
EUSER_DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
             "euser.dll")
KERNEL = REPO + "/build/kernfull.elf"

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")
PROMPT = b"MRS> "


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def build_disk():
    for p in (DISK_IMG, FS_IMG):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK_IMG, "wb") as f:
        f.truncate(DISK_MB * 1024 * 1024)
    with open(FS_IMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)

    # 8.3 name "morse" is 5 chars: the shell's FatReadPath finds it as typed.
    for src, name in ((MORSE_E32, "morse"), (EUSER_DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src} (build it first)")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)

    with open(FS_IMG, "rb") as f:
        fs = f.read()
    if len(fs) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    with open(DISK_IMG, "r+b") as f:
        f.seek(OFFSET)
        f.write(fs)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + PART_LBA.to_bytes(4, "little")
                 + PART_SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510] = 0x55
        mbr[511] = 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK_IMG}@@{OFFSET}", "::",
             check=True, capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for want in ("morse", "euserdll"):
        if want not in norm:
            sys.exit(f"disk missing {want}: {out.stdout}")
    print("mdir OK: MORSE.E32 and EUSER.DLL staged")


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
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-display", "none", "-serial", "stdio",
        "-kernel", KERNEL,
        "-drive", f"file={DISK_IMG},format=raw,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, driving MORSE via serial console ...")
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

            send("morse", PROMPT, 60)      # launch + first prompt
            send("selftest", PROMPT)       # SOS encode + play
            send("quit", b"MRS: DONE")     # exit the tool
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text):
    ok = True

    sos_lines = [ln for ln in text.splitlines()
                 if "MRS: SOS -> ... --- ..." in ln]
    sos_ok = len(sos_lines) > 0
    print(f"  [{'PASS' if sos_ok else 'FAIL'}] SOS encodes to '... --- ...'")
    ok &= sos_ok

    markers = [
        "MRS: MORSE TOOL",
        "MRS: SOS encoded ... --- ...",
        "MRS: SELFTEST OK",
        "MRS: DONE",
    ]
    for m in markers:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {m}")

    playing = [ln for ln in text.splitlines() if "MRS: playing " in ln]
    play_ok = any("MRS: playing 9 symbols" in ln for ln in playing)
    ok &= play_ok
    print(f"  [{'PASS' if play_ok else 'FAIL'}] MRS: playing <N> symbols "
          f"(SOS = 9): {len(playing)} play line(s)")

    val = "e32run: validate r=0" in text
    ok &= val
    print(f"  [{'PASS' if val else 'FAIL'}] E32 validates (r=0)")

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok &= bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT")
    for ln in exc:
        print("     ", ln[:200])
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 MORSE tool E32 QEMU test")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk()
    run_qemu(args.log)
    with open(args.log, errors="replace") as f:
        text = f.read()
    print(f"serial log: {args.log}")
    print("\n== marker results")
    ok = check(text)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

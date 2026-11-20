#!/usr/bin/env python3
"""r1_qemu_audio_test.py — automated QEMU test for the AC97 PCM-OUT driver.

Boots build/kernfull.elf on q35 with an AC97 sound device, feeds `audio` and
`audio play` over the serial console, and checks the driver's serial markers:
non-zero codec vendor ID (Sigmatel STAC9700 in QEMU), codec ready, PCM-OUT DMA
completion with the expected 19200-byte transfer, and no guest exception or
kernel panic anywhere in the log.

Usage:
    python3 r1_qemu_audio_test.py [--kernel KERN] [--log LOG] [--rebuild-disk]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys

KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"
DISK_IMG = "/tmp/opencode/r1_test.img"
FS_IMG = "/tmp/opencode/r1_audio_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_qemu_audio.log"
PART_LBA = 2048
PART_SECTORS = 16384
EXC_KEYS = ("Exc ", "PanicKernExec")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "mkfs.fat", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_disk():
    """32 MB raw disk with one bootable FAT32 partition at LBA 2048."""
    print(f"== building minimal FAT32 disk {DISK_IMG}")
    size = PART_SECTORS * 512
    with open(FS_IMG, "wb") as f:
        f.truncate(size)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)
    with open(FS_IMG, "rb") as f:
        fs = f.read()
    with open(DISK_IMG, "wb") as f:
        f.truncate(32 * 1024 * 1024)
        f.seek(PART_LBA * 512)
        f.write(fs)
        entry = (bytes([0x80, 0x00, 0x02, 0x00, 0x0C, 0x00, 0x02, 0x00])
                 + PART_LBA.to_bytes(4, "little")
                 + PART_SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)


def run_qemu(kernel, log):
    qemu = [
        "timeout", "45", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0",
        "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0",
        "-device", "AC97,audiodev=a0",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    feeder = ("sleep 1.5; printf '\\n'; "
              "sleep 3.0; printf 'audio\\n'; "
              "sleep 1.5; printf 'audio play\\n'; "
              "sleep 8.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    print("== starting qemu (45s cap), feeding audio commands ...")
    r = subprocess.run(["bash", "-c", cmd], check=False)
    print(f"qemu wrapper exit code: {r.returncode}")
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text):
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    m = re.search(r"audio: vendor ID ([0-9a-fA-F]{4}):([0-9a-fA-F]{4})", text)
    vid = (int(m.group(1), 16), int(m.group(2), 16)) if m else (0, 0)
    return [
        ("vendor ID 8384:7600 (non-zero)", bool(m) and vid[0] != 0
         and vid[0] != 0xFFFF and f"{vid[0]:04x}:{vid[1]:04x}" == "8384:7600"),
        ("codec ready", "codec ready" in text),
        ("PCM play done", "PCM play done" in text),
        ("19200-byte transfer", "19200 bytes" in text
         and "PCM play done (19200 bytes" in text),
        ("no Exc/PanicKernExec", not exc),
    ], exc, vid


def main():
    ap = argparse.ArgumentParser(description="R1 QEMU AC97 audio test")
    ap.add_argument("--kernel", default=KERNEL)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--rebuild-disk", action="store_true")
    args = ap.parse_args()

    check_tools()
    if args.rebuild_disk or not os.path.exists(DISK_IMG):
        build_disk()

    text = run_qemu(args.kernel, args.log)
    results, exc, vid = check(text)
    print("\n== marker results")
    ok = True
    for name, passed in results:
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {name}")
    print(f"observed vendor ID: {vid[0]:04x}:{vid[1]:04x}")
    if exc:
        print("== exception/panic lines:")
        for line in exc:
            print("   ", line[:200])
    print(f"serial log: {args.log}")
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

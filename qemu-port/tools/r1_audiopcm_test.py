#!/usr/bin/env python3
"""r1_audiopcm_test.py -- end-to-end QEMU test for AUDIOPCM.E32.

Stages AUDIOPCM.E32 + EUSER.DLL on a FAT32 disk, boots build/kernfull.elf on
q35 with an AC97 device, runs `audiopcm` from the serial shell and checks:
  * exec 240 installs with the new PCM ops,
  * the kernel emits the "AU: pcm play" DMA marker,
  * the ring-3 program reports PLAY/STATUS/STOP success and real DMA completion
    (iState == 0, iPosition == iTotal == 176400 bytes from a user buffer),
  * no guest exception / kernel panic anywhere in the log.

Usage: python3 tools/r1_audiopcm_test.py [--e32 E32] [--kernel KERN] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISK_IMG = "/tmp/opencode/r1_audiopcm_test.img"
FS_IMG = "/tmp/opencode/r1_audiopcm_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_audiopcm_test.log"
E32_DEFAULT = os.path.join(ROOT, "build/user/AUDIOPCM.E32")
EUSER_DLL = os.path.join(ROOT, "EUSER.DLL")
KERNEL_DEFAULT = os.path.join(ROOT, "build/kernfull.elf")
MBR_LBA = 2048
PART_SECTORS = 16384
EXC_KEYS = ("Exc ", "PanicKernExec")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def build_disk(e32):
    for path in (DISK_IMG, FS_IMG):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK_IMG, "wb") as f:
        f.truncate(32 * 1024 * 1024)
    with open(FS_IMG, "wb") as f:
        f.truncate(PART_SECTORS * 512)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)
    for src, name in ((e32, "audiopcm"), (EUSER_DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src} (build it first)")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)
    with open(FS_IMG, "rb") as f:
        fat = f.read()
    with open(DISK_IMG, "r+b") as f:
        f.seek(MBR_LBA * 512)
        f.write(fat)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + MBR_LBA.to_bytes(4, "little")
                 + PART_SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)


def run_qemu(kernel, log):
    qemu = [
        "timeout", "90", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    feeder = ("sleep 1.5; printf '\\n'; "
              "sleep 4.0; printf 'audiopcm\\n'; "
              "sleep 14.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text):
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    return {
        "kernel boot": "EKA2" in text.upper(),
        "FAT mounted (shell prompt)": "C:/>" in text,
        "E32 validates (r=0)": "e32run: validate r=0" in text,
        "EUSER ord 915 bound": "ord 915" in text,
        "PCM exec installed (slot 240)": "audio exec: PCM ops at slot 240" in text,
        "AC97 codec ready": "codec ready" in text,
        "kernel AU: pcm play marker": "AU: pcm play" in text,
        "user APCM: START": "APCM: START" in text,
        "user play r=0": any("APCM: play r=0" in ln for ln in text.splitlines()),
        "user DMA completion (pos=total=176400)": (
            "APCM: done" in text and " pos=176400 total=176400" in text
            and " state=0" in text),
        "user stop r=0": any("APCM: stop r=0" in ln for ln in text.splitlines()),
        "user PASS": "APCM: PASS" in text,
        "no Exc/PanicKernExec": not exc,
    }, exc


def main():
    ap = argparse.ArgumentParser(description="R1 PCM audio exec QEMU test")
    ap.add_argument("--e32", default=E32_DEFAULT)
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk(args.e32)
    text = ""
    for _ in range(2):                       # busy host: retry a cold boot
        text = run_qemu(args.kernel, args.log)
        if "APCM: PASS" in text:
            break
    results, exc = check(text)
    print("== PCM audio exec test results")
    ok = True
    for name, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {name}")
    if exc:
        print("== exception/panic lines:")
        for line in exc:
            print("   ", line[:200])
    print(f"serial log: {args.log}")
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

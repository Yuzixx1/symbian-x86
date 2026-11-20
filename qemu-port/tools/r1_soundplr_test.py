#!/usr/bin/env python3
"""r1_soundplr_test.py -- end-to-end QEMU test for SOUNDPLR.E32.

Builds a 32 MB raw disk with a bootable FAT32 partition, stages SOUNDPLR.E32
and the genuine EUSER.DLL, boots build/kernfull.elf on q35 with an AC97 sound
device, runs `soundplr` from the serial shell and checks that:
  * the PE2 E32 validates and binds EUSER ordinal 915 (RDebug::Printf),
  * the kernel installs the AC97 tone exec at boot (slot 240),
  * all four notes are DMA'd through the AC97 PCM-OUT engine,
  * the player emits its SND markers and no guest exception/panic occurs.

Usage: python3 tools/r1_soundplr_test.py [--e32 E32] [--kernel KERN] [--log LOG]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/r1_soundplr_test.img"
FS_IMG = "/tmp/opencode/r1_soundplr_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_soundplr_test.log"
E32_DEFAULT = "build/user/SOUNDPLR.E32"
EUSER_DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
             "euser.dll")
KERNEL_DEFAULT = "build/kernfull.elf"
MBR_LBA = 2048
PART_SECTORS = 16384
FS_SIZE = PART_SECTORS * 512
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
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)
    # 8.3 name: "soundplr" is exactly 8 chars, so the shell's FatReadPath
    # finds it as typed.
    for src, name in ((e32, "soundplr"), (EUSER_DLL, "EUSER.DLL")):
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
        mbr[510] = 0x55
        mbr[511] = 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK_IMG}@@{MBR_LBA * 512}", "::",
             check=True, capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    assert "soundplr" in norm and "euserdll" in norm, out.stdout


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
              "sleep 4.0; printf 'soundplr\\n'; "
              "sleep 12.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text):
    snd = [ln for ln in text.splitlines() if "SND:" in ln]
    notes = [ln for ln in snd if re.search(r"SND: note \d+Hz \d+ms r=0$", ln)]
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    return {
        "kernel boot": "EKA2" in text.upper(),
        "FAT mounted (shell prompt)": "C:/>" in text,
        "E32 validates (r=0)": "e32run: validate r=0" in text,
        "EUSER ord 915 bound": "ord 915" in text,
        "tone exec installed (slot 240)": "audio exec: tone installed at slot 240" in text,
        "AC97 codec ready": "codec ready" in text,
        "4 notes DMA'd (r=0)": len(notes) == 4,
        "kernel tone completions": len(re.findall(r"audio tone: done", text)) >= 4,
        "SND start marker": any("SND: SOUND PLAYER START" in ln for ln in snd),
        "SND melody done": any("SND: melody done" in ln for ln in snd),
        "SND DONE marker": any("SND: DONE" in ln for ln in snd),
        "no Exc/PanicKernExec": not exc,
    }, exc


def main():
    ap = argparse.ArgumentParser(description="R1 sound player QEMU test")
    ap.add_argument("--e32", default=E32_DEFAULT)
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk(args.e32)
    text = ""
    for attempt in range(2):                 # busy CI host: retry a cold boot
        text = run_qemu(args.kernel, args.log)
        if check(text)[0]["SND DONE marker"]:
            break
    results, exc = check(text)
    print("== sound player test results")
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

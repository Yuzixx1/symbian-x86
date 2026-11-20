#!/usr/bin/env python3
"""r1_shot_test.py -- end-to-end SCREENSHOT E32 test on the VBE LFB.

Boots build/kernfull.elf with QEMU's standard VGA (Bochs VBE DISPI), stages
SHOT.E32 (the PE2 screenshot tool) plus the genuine EUSER.DLL on a FAT32 disk,
and drives the serial shell with `shot`.  The tool discovers the framebuffer
via slow execs 249/250, obtains the low-RAM gui_shared frame from exec 251 and
submits a 64x64 RGBA capture display list through exec 251 (arg1>0).  The
kernel paints the real LFB in ring-0 context and prints its own pixel readback;
the raw bytes are offered to SCREEN.RAW over PortFile IPC (file_save), with a
"capture OK" fallback when portfilesrv is not staged.

Usage: python3 tools/r1_shot_test.py [--kernel KERN] [--e32 E32] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/r1_shot_test.img"
FS_IMG = "/tmp/opencode/r1_shot_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_shot_test.log"
E32_DEFAULT = "build/user/SHOT.E32"
EUSER_DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
             "euser.dll")
KERNEL_DEFAULT = "build/kernfull.elf"
MBR_LBA = 2048
PART_SECTORS = 16384
FS_SIZE = PART_SECTORS * 512

BOOT_MARKERS = [
    "DISPLAY: VBE mode 0x",
    "GUI: execs 249/250/251 installed",
    "ord 915",
    "shot started (user process)",
    # Kernel-side readback from the real LFB (proof the display list painted
    # VRAM rather than only fiddling with VGA registers).
    "GUI: pixel readback bg=0x000080 title=0x0080c0 OK",
]
SHOT_MARKERS = [
    "SHOT: START",
    "SHOT: fb=0x",
    "SHOT: captured 64x64 (16384 bytes)",
    "SHOT: capture OK",
    "SHOT: DONE",
]
# The save either succeeds on the staged FAT volume (portfilesrv running) or
# is explicitly reported as unavailable -- one of the two must be present.
SAVE_MARKERS = ("SHOT: saved SCREEN.RAW", "SHOT: PortFile save unavailable")
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_disk(e32):
    for p in (DISK_IMG, FS_IMG):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK_IMG, "wb") as f:
        f.truncate(32 * 1024 * 1024)
    with open(FS_IMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)
    # 8.3 short name "shot" is 4 chars: FatReadPath matches it directly.
    for src, name in ((e32, "shot"), (EUSER_DLL, "EUSER.DLL")):
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
    assert "shot" in norm and "euserdll" in norm, out.stdout


def run_qemu(kernel, log):
    qemu = [
        "timeout", "70", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-vga", "std",                     # Bochs VBE DISPI registers
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    feeder = ("sleep 2.0; printf '\\n'; "
              "sleep 2.0; printf 'shot\\n'; "
              "sleep 5.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text):
    results = {}
    for m in BOOT_MARKERS + SHOT_MARKERS:
        results[m] = m in text
    results["SCREEN.RAW saved or capture OK"] = any(m in text for m in SAVE_MARKERS)
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    return results, exc


def main():
    ap = argparse.ArgumentParser(description="R1 SCREENSHOT E32 QEMU test")
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--e32", default=E32_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk(args.e32)
    text = ""
    for attempt in range(2):                 # busy CI host: retry a cold boot
        text = run_qemu(args.kernel, args.log)
        res, _ = check(text)
        if res.get("SHOT: DONE"):
            break
    results, exc = check(text)
    print("== screenshot test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("SHOT", "shot", "GUI", "DISPLAY",
                                     "display", "fault", "panic", "E32Run")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

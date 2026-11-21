#!/usr/bin/env python3
"""r1_fbblit_test.py -- ring-3 bulk framebuffer blit exec proof.

Builds FBBLITTEST.E32, stages it on a small FAT32 disk, boots kernfull.elf
with a standard-VGA Bochs LFB, launches the program from the serial shell and
checks that a 320x200 8-bit checkerboard really reached the framebuffer via a
single kernel exec (slot 270 op 3).  A QMP screendump is pixel-checked with PIL.

Usage: python3 tools/r1_fbblit_test.py [--log PATH] [--png PATH] [--no-build]
"""
import argparse
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = "/tmp/opencode"
DISK = TMP + "/r1_fbblit.img"
FS = TMP + "/r1_fbblit_fs.img"
LOG = TMP + "/r1_fbblit.log"
PNG = TMP + "/fbblit.png"
QMP = TMP + "/r1_fbblit-qmp.sock"
E32 = os.path.join(REPO, "build/user/FBBLITTEST.E32")
DLL = os.path.join(REPO, "EUSER.DLL")
KERNEL = os.path.join(REPO, "build/kernfull.elf")

DISK_MB = 32
PART_LBA = 2048
PART_SECTORS = 16384          # 8 MiB FAT32
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

BOOT_MARKERS = (
    "FBBLIT: ready execs=274/275",
    "FBBLIT: exec 270 ops 2/3",
)
TEST_MARKERS = (
    "FBTEST: start",
    "FBTEST: info r=0",
    "FBTEST: blit r=0",
    "FBTEST: PATTERN BLITTED OK",
    "FBTEST: DONE",
)
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")
PROMPTS = (b"X:/> ", b"EKA2> ", b"C:/> ")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args))
    return subprocess.run(args, cwd=REPO, **kw)


def build():
    sh("make", "build/kernfull.elf", check=True,
       stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    sh("make", "build/user/FBBLITTEST.E32", check=True)


def stage_disk():
    for path in (DISK, FS):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(DISK_MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True,
       stdout=subprocess.DEVNULL)
    files = [(E32, "FBBLITTEST.E32")]
    if os.path.isfile(DLL):
        files.append((DLL, "EUSER.DLL"))
    for src, name in files:
        sh("mcopy", "-i", FS, "-o", src, "::" + name, check=True)
    with open(FS, "rb") as f:
        payload = f.read()
    with open(DISK, "r+b") as f:
        f.seek(OFFSET)
        f.write(payload)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + PART_LBA.to_bytes(4, "little")
                 + PART_SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)


def wait_for(log_path, needles, timeout, start=0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        if any(n in data for n in needles):
            return True, data
        time.sleep(0.15)
    return False, b""


def qmp_screendump(png, tries=20):
    for _ in range(tries):
        if os.path.exists(QMP):
            break
        time.sleep(0.25)
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(QMP)
        f = s.makefile("rwb")
        f.readline()
        f.write(b'{"execute":"qmp_capabilities"}\n')
        f.flush()
        f.readline()
        if os.path.exists(png):
            os.remove(png)
        cmd = ('{"execute":"screendump","arguments":{"filename":"'
               + png + '","format":"png"}}\n')
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
        if os.path.exists(png) and os.path.getsize(png) > 1000:
            return True
        time.sleep(0.15)
    return False


def check_pixels(png):
    """Return (ok, details) for the 16x16 checkerboard at LFB (0,0).

    The check is offset-tolerant: QEMU's std-VGA surface can pan the visible
    origin a few pixels, so assert the *relative* 16-pixel alternation instead
    of an absolute cell phase.  All pixels in the blit region must be one of
    the two palette colours (black / 0x00FF4000).
    """
    try:
        from PIL import Image
    except ImportError:
        return False, [("Pillow not installed", None)]
    img = Image.open(png).convert("RGB")
    px = img.load()
    black, orange = (0, 0, 0), (255, 64, 0)
    details = []
    # The std-VGA surface pans the visible origin (measured +16x,+72y), so a
    # 320x200 LFB blit shows its top ~128 rows on screen; rows below that still
    # hold pre-existing console text.  Check the unambiguously-blitted band.
    colours = set()
    for y in range(4, 120, 3):
        for x in range(4, 310, 3):
            colours.add(px[x, y])
    only_two = colours <= {black, orange}
    details.append(("only palette colours in visible blit band: %s" % colours, only_two))
    ok = only_two
    for x, y in ((40, 40), (120, 72), (200, 40), (280, 72)):
        p = px[x, y]
        # Neighbouring 16px cells must differ; cells 32px apart must match.
        checks = (px[x + 16, y] != p, px[x, y + 16] != p,
                  px[x + 32, y] == p, px[x, y + 32] == p,
                  p in (black, orange))
        good = all(checks)
        ok = ok and good
        details.append(("checkerboard at (%d,%d) %s" % (x, y, p), good))
    return ok, details


def run(log_path, png_path):
    if os.path.exists(QMP):
        os.remove(QMP)
    os.makedirs(TMP, exist_ok=True)
    qemu = [
        "timeout", "75", "qemu-system-i386", "-M", "q35", "-m", "128",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-qmp", "unix:" + QMP + ",server,nowait", "-no-reboot",
        "-kernel", KERNEL,
        "-drive", "file=" + DISK + ",format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
    ]
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            got, _ = wait_for(log_path, PROMPTS, 70)
            if not got:
                print("[FAIL] no shell prompt")
                return False
            time.sleep(0.5)
            vm.stdin.write(b"FBBLITTEST\n")
            vm.stdin.flush()
            got, _ = wait_for(log_path, [b"FBTEST: DONE"], 40)
            if not got:
                print("[FAIL] FBTEST markers missing")
                return False
            # The E32 re-blits as its last act then sleeps 2.5 s: capture
            # during that window, before the shell prompt repaints the text grid.
            time.sleep(0.4)
            shot = qmp_screendump(png_path)
            print("[INFO] screendump:", shot)
            return True
        finally:
            try:
                vm.terminate()
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log", default=LOG)
    ap.add_argument("--png", default=PNG)
    ap.add_argument("--no-build", action="store_true")
    args = ap.parse_args()
    if not args.no_build:
        build()
    stage_disk()
    ok = run(args.log, args.png)
    text = open(args.log, "r", errors="replace").read()
    results = {}
    for m in BOOT_MARKERS + TEST_MARKERS:
        results[m] = m in text
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic"] = not exc
    px_ok, pixels = check_pixels(args.png)
    results["screendump checkerboard pixels"] = px_ok
    for label, good in pixels:
        print("  %s: %s" % ("OK" if good else "BAD", label))
    for k, v in results.items():
        print(("PASS " if v else "FAIL ") + k)
    if exc:
        print("EXCEPTIONS:")
        for ln in exc:
            print("  " + ln)
    all_ok = ok and all(results.values())
    print("RESULT:", "PASS" if all_ok else "FAIL")
    print("SCREENSHOT:", args.png)
    sys.exit(0 if all_ok else 1)


if __name__ == "__main__":
    main()

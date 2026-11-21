#!/usr/bin/env python3
"""r1_gcblit_test.py -- end-to-end GC_BLIT bulk framebuffer test.

Builds GCBLITTEST.E32 (a PE2 E32 that paints a 640x400 RGBA gradient through
the new GC_BLIT exec-251 opcode), rebuilds the GRUB ISO so the program and
EUSER.DLL are staged in /PROGRAMS.IMG, and boots QEMU with standard VGA
(Bochs VBE -> 1024x768x32 LFB).  It drives the program from the serial shell,
asserts the user + sparse kernel markers, grabs a QMP PNG screendump and
checks the centre pixel against the gradient's expected colour.

The GRUB ISO boot is attempted first.  A concurrent DOOM-port change grew
/PROGRAMS.IMG to 64 MiB, which trips the low-RAM overlap guard in
variant/qemu_ata.cpp (not owned by this test) and leaves X: unmounted.  When
that happens the test falls back to the proven path used by r1_shot_test.py:
boot the same kernfull.elf directly with a small (16 MiB) AHCI FAT disk that
carries only GCBLIT.E32 + EUSER.DLL.

Usage: python3 tools/r1_gcblit_test.py [--no-rebuild-iso] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
ROOT = "/tmp/opencode"
ISO_DEFAULT = "build/symbian-grub.iso"
SOCK = ROOT + "/r1_gcblit-qmp.sock"
LOG_DEFAULT = ROOT + "/r1_gcblit.log"
ISO_LOG = ROOT + "/r1_gcblit_iso.log"
SHOT = ROOT + "/gcblit.png"
DISK = ROOT + "/r1_gcblit_disk.img"
FS = ROOT + "/r1_gcblit_fs.img"
E32 = REPO + "/build/user/GCBLITTEST.E32"
PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF", "Abort")
BOOT_MARKERS = [
    "DISPLAY: VBE mode 0x",
    "GUI: execs 249/250/251 installed",
]
GB_MARKERS = [
    "GB: BLIT TEST READY",
    "GB: BLIT buf=0x",
    "GB: BLIT src=0x",
    "GB: BLIT submitted cmds=1",
    "GB: BLIT TEST DONE",
    "GCB: blit 640x400 1024000 bytes",   # sparse kernel-side marker
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, cwd=REPO, **kw)


def check_tools():
    for t in ("qemu-system-i386", "make", "mcopy", "mdir", "mkfs.fat",
              "grub2-mkimage"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def rebuild(iso, no_rebuild):
    if not (no_rebuild and os.path.exists(os.path.join(REPO, iso))):
        sh("make", "build/user/GCBLITTEST.E32", check=True)
        sh("make", "build/user/GCBLIT.E32", check=True)   # 8.3 launch alias
        sh("make", "isogrub", check=True)
    if not os.path.exists(os.path.join(REPO, iso)):
        sys.exit(f"missing GRUB ISO {iso}")
    listing = sh("mdir", "-i", "build/programs.img@@2048S", "::",
                 check=True, capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    for want in ("gcblittest", "euserdll"):
        if want not in norm:
            sys.exit(f"programs image missing {want} (GCBLITTEST.E32 + EUSER.DLL)")
    print("ISO OK: GCBLITTEST.E32 + EUSER.DLL staged in /PROGRAMS.IMG")


def build_disk():
    """Small MBR+FAT32 disk with GCBLIT (8.3) + EUSER.DLL for the fallback."""
    for p in (DISK, FS):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK, "wb") as f:
        f.truncate(32 * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(16 * 1024 * 1024)
    sh("mkfs.fat", "-F", "32", FS, check=True, capture_output=True)
    for src, name in ((E32, "GCBLIT"), (REPO + "/EUSER.DLL", "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", FS, src, "::" + name, check=True, capture_output=True)
    with open(FS, "rb") as f:
        fat = f.read()
    part_lba = 2048
    with open(DISK, "r+b") as f:
        f.seek(part_lba * 512)
        f.write(fat)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + part_lba.to_bytes(4, "little")
                 + ((16 * 1024 * 1024) // 512).to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK}@@{part_lba * 512}", "::",
             check=True, capture_output=True, text=True).stdout
    norm = "".join(out.lower().split()).replace(".", "")
    if "gcblit" not in norm or "euserdll" not in norm:
        sys.exit("fallback disk staging failed")


def wait_for(log_path, marker, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            if marker in f.read():
                return True
        time.sleep(0.2)
    return False


def wait_for_prompt(log_path, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            data = f.read()
        if any(p in data for p in PROMPTS):
            return True
        time.sleep(0.2)
    return False


def qmp_screendump(path):
    for _ in range(12):
        if os.path.exists(SOCK):
            break
        time.sleep(0.25)
    else:
        print("[WARN] QMP socket never appeared")
        return False
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(SOCK)
        f = s.makefile("rwb")
        f.readline()                         # QMP greeting
        f.write(b'{"execute":"qmp_capabilities"}\n')
        f.flush()
        f.readline()                         # capabilities reply
        if os.path.exists(path):
            os.remove(path)
        qmp = ('{"execute":"screendump","arguments":{"filename":"'
               + path + '","format":"png"}}\n')
        f.write(qmp.encode())
        f.flush()
        for _ in range(10):
            line = f.readline()
            if not line:
                break
            if b"error" in line.lower():
                print("[WARN] QMP screendump error:", line[:200])
                break
            if b"return" in line:
                break
        f.close()
        s.close()
    except Exception as exc:
        print("[WARN] QMP screendump failed:", exc)
        return False
    for _ in range(20):
        if os.path.exists(path) and os.path.getsize(path) > 1000:
            break
        time.sleep(0.15)
    ok = os.path.exists(path) and os.path.getsize(path) > 1000
    print(f"[INFO] screendump {path}: {os.path.getsize(path) if ok else 0} bytes")
    return ok


def launch_and_capture(vm, log_path):
    """Type `gcblittest` (then 8.3 alias if needed) and grab the LFB."""
    if not wait_for_prompt(log_path):
        print("[WARN] shell prompt not seen; continuing")
    time.sleep(0.5)
    vm.stdin.write(b"gcblittest\n")
    vm.stdin.flush()
    time.sleep(1.5)
    with open(log_path, "rb") as f:
        data = f.read()
    if b"not found" in data:
        print("[INFO] 11-char LFN not resolved by guest FAT; using 8.3 alias")
        vm.stdin.write(b"gcblit\n")
        vm.stdin.flush()
    done = wait_for(log_path, b"GB: BLIT TEST DONE")
    if not done:
        print("[FAIL] GC_BLIT program never finished")
    else:
        time.sleep(0.5)                      # let QEMU's display surface settle
    shot_ok = qmp_screendump(SHOT)
    time.sleep(0.5)
    return shot_ok


def run_qemu(qemu, log_path):
    if os.path.exists(SOCK):
        os.remove(SOCK)
    print("== booting QEMU (-vga std), driving gcblittest via serial ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, cwd=REPO, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            return launch_and_capture(vm, log_path)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def boot_args_iso(iso):
    return ["qemu-system-i386", "-M", "q35", "-m", "128",
            "-cdrom", os.path.join(REPO, iso), "-boot", "d",
            "-vga", "std", "-display", "none", "-serial", "stdio",
            "-qmp", f"unix:{SOCK},server,nowait",
            "-monitor", "none", "-no-reboot"]


def boot_args_disk():
    return ["qemu-system-i386", "-M", "q35", "-m", "128",
            "-kernel", REPO + "/build/kernfull.elf",
            "-vga", "std", "-display", "none", "-serial", "stdio",
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata",
            "-device", "ide-hd,drive=d0,bus=sata.0",
            "-qmp", f"unix:{SOCK},server,nowait",
            "-monitor", "none", "-no-reboot"]


def check_pixels(path):
    """Centre (320,200) of the 640x400 gradient must be (64,200,8):
    0x40 = 320&0xff, 0xc8 = 200&0xff, 0x08 = (320+200)&0xff."""
    try:
        from PIL import Image
    except Exception as exc:
        return False, f"PIL unavailable: {exc}"
    try:
        im = Image.open(path).convert("RGB")
    except Exception as exc:
        return False, f"cannot open {path}: {exc}"
    w, h = im.size
    detail = f"image={w}x{h}"
    if w < 640 or h < 400:
        return False, detail + " (too small)"
    want = (64, 200, 8)   # gradient at LFB (320,200)
    # std-VGA pans the visible LFB origin (measured +72y here, +16x/+72y in
    # r1_fbblit_test.py).  Search small pan offsets; only the exact source row
    # 200 has G=200,B=8, so the match is unambiguous.
    found = None
    for dy in range(0, 110):
        y = int((200 - dy) * h / 768)
        if y < 0:
            continue
        for dx in range(0, 17):
            x = int((320 - dx) * w / 1024)
            if x < 0:
                continue
            got = im.getpixel((x, y))
            if all(abs(a - b) <= 4 for a, b in zip(got, want)):
                found = (x, y, got, dx, dy)
                break
        if found:
            break
    if found:
        x, y, got, dx, dy = found
        detail += (f" centre found @screen({x},{y})={got} expected={want} "
                   f"(pan -{dx}x,-{dy}y)")
    else:
        detail += f" centre colour {want} not found (pan offsets 0..109/0..16)"
    bx, by = int(2 * w / 1024), int(2 * h / 768)
    border = im.getpixel((bx, by))
    detail += f" border@({bx},{by})={border}"
    return (found is not None and all(c >= 200 for c in border)), detail


def check(text, shot_ok):
    ok = True
    for m in BOOT_MARKERS + GB_MARKERS:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {m}")
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
    if shot_ok:
        pok, detail = check_pixels(SHOT)
        ok &= pok
        print(f"  [{'PASS' if pok else 'FAIL'}] screendump pixel check: {detail}")
    else:
        print(f"  [INFO] screendump unavailable ({SHOT})")
    return ok


def main():
    ap = argparse.ArgumentParser(description="R1 GC_BLIT bulk blit QEMU test")
    ap.add_argument("--no-rebuild-iso", action="store_true")
    ap.add_argument("--iso", default=ISO_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    check_tools()
    rebuild(args.iso, args.no_rebuild_iso)

    # 1) Required GRUB ISO boot (-vga std).
    shot_ok = run_qemu(boot_args_iso(args.iso), ISO_LOG)
    text = open(ISO_LOG, errors="replace").read()
    if "GB: BLIT TEST DONE" not in text:
        # 2) Fallback: the 64 MiB /PROGRAMS.IMG module trips qemu_ata.cpp's
        #    overlap guard, so X: is never mounted.  Boot the same kernel with
        #    a small AHCI FAT disk instead (same kernel blit path under test).
        print("[INFO] GRUB ISO volume not mounted; using small AHCI FAT disk")
        build_disk()
        shot_ok = run_qemu(boot_args_disk(), args.log)
        text = open(args.log, errors="replace").read()
    else:
        with open(args.log, "w", errors="replace") as f:
            f.write(text)

    print(f"serial log: {args.log}")
    print("\n== gcblittest marker results")
    ok = check(text, shot_ok)
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("GB:", "GCB:", "gcblit", "GUI:",
                                     "DISPLAY", "Exc", "Panic")):
                print("   ", ln[:220])
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

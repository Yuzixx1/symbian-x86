#!/usr/bin/env python3
"""r1_doom_test.py -- QEMU test scaffold for real DOOM (doomgeneric) on EKA2.

STATUS: SCAFFOLD. This script deliberately SKIPS with a clear message while
build/user/DOOM.E32 and the Freedoom IWAD are still being acquired/built. It is
safe to run today: no assets means `RESULT: SKIP` and exit 0, never a false pass.

When the assets land, this harness will:

  1. stage DOOM.E32 + FREEDOOM1.WAD (8.3 alias FDOOM1.WAD) + EUSER.DLL on a
     64 MB MBR disk with a 48 MB FAT32 partition at LBA 2048 (98304 sectors);
  2. build/reuse the GRUB ISO whose /PROGRAMS.IMG carries the same files;
  3. boot with -vga std, launch `DOOM` from the serial shell, feed gameplay keys;
  4. assert the DOOM: marker family, the EUSER ordinal-915 bind and a clean log;
  5. grab a QMP screendump of the real title screen to /tmp/opencode/doom.png.

The 48 MB partition is required: freedoom1.wad is 28,795,076 B (~27.5 MiB) and
does not fit the old 8 MB test partition (16384 sectors), nor the ISO's former
16 MB one. See docs/DOOM_PORTING.md §3 for the disk-layout rationale.

Usage: python3 tools/r1_doom_test.py [--no-rebuild-disk] [--no-build-iso]
                                     [--log PATH] [--png PATH]
"""
import argparse
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time

# ------------------------------------------------------------------ constants
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TMP = "/tmp/opencode"
DISK = TMP + "/r1_doom.img"
FS = TMP + "/r1_doom_fs.img"
LOG = TMP + "/r1_doom.log"
PNG = TMP + "/doom.png"
ISO = TMP + "/r1_doom.iso"
QMP = TMP + "/r1_doom-qmp.sock"
KERNEL = os.path.join(REPO, "build/kernfull.elf")

# Candidate asset locations. The WAD is acquired by another agent; accept a
# few natural drop-off points so the harness works without configuration.
E32_CANDIDATES = (
    os.path.join(REPO, "build/user/DOOM.E32"),
    os.path.join(REPO, "build/user/doom.e32"),
)
WAD_CANDIDATES = (
    os.path.join(REPO, "wads/freedoom1.wad"),
    os.path.join(REPO, "build/wad/freedoom1.wad"),
    os.path.join(REPO, "build/wad/FREEDOOM1.WAD"),
    os.path.join(REPO, "freedoom1.wad"),
    os.path.join(REPO, "FREEDOOM1.WAD"),
    "/tmp/opencode/doom/freedoom-0.13.0/freedoom1.wad",
    "/tmp/opencode/doom/freedoom1.wad",
)
DLL_CANDIDATES = (
    "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll",
    os.path.join(REPO, "EUSER.DLL"),
)

# Keep the legacy partition start (LBA 2048) but grow both the disk and the
# FAT32 partition: 64 MB disk, 98304 sectors = 48 MiB. The WAD alone is ~27.5
# MiB, so the old 16384/49152-sector layouts cannot hold it. Must stay in sync
# with DISK_MB/FS_MB/PART_LBA in tools/mkgrubiso.py.
DISK_MB = 64
PART_LBA = 2048
PART_SECTORS = 98304
FS_SIZE = PART_SECTORS * 512
OFFSET = PART_LBA * 512

MARKERS = (
    "DOOM: INIT",       # doomgeneric I_Init entered
    "DOOM: WAD OK",     # IWAD header + directory parsed
    "DOOM: TITLE",      # title screen frame submitted
    "DOOM: FRAME",      # at least one real 320x200 frame blitted
    "DOOM: KEY",        # a PS/2 key reached I_GetEvent
    "DOOM: DONE",       # orderly exit / self-test complete
)
BOOT_MARKERS = ("DISPLAY: VBE mode 0x", "X: mounted")
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")
PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")
GAME_KEYS = b"w w \x20 \x0d \x1b "     # move, fire, use, Escape (menu)

REQUIRED_TOOLS = ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "python3",
                  "timeout")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args))
    return subprocess.run(args, **kw)


def first_existing(paths):
    for path in paths:
        if os.path.isfile(path):
            return path
    return None


def check_assets():
    """Return (e32, wad, dll) or None -- the graceful SKIP gate."""
    e32 = first_existing(E32_CANDIDATES)
    wad = first_existing(WAD_CANDIDATES)
    if not e32:
        print("[SKIP] DOOM.E32 not present (looked in build/user/DOOM.E32).")
    if not wad:
        print("[SKIP] Freedoom IWAD not present (build/wad/, wads/ or "
              "/tmp/opencode/doom/).")
    if not e32 or not wad:
        print("[SKIP] DOOM binaries/assets not acquired yet; nothing to test.")
        print("       See docs/DOOM_PORTING.md. This is expected during staging.")
        return None
    dll = first_existing(DLL_CANDIDATES)
    if not dll:
        print("[WARN] EUSER.DLL not found; ordinal-915 bind check may fail.")
    return e32, wad, dll


def check_tools():
    for tool in REQUIRED_TOOLS:
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def build_disk(e32, wad, dll):
    """Stage the enlarged 48 MB FAT32 disk; verify with mtools."""
    for path in (DISK, FS):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(DISK_MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)

    # 8.3 names keep the guest FAT driver happy. FREEDOOM1.WAD has a
    # nine-character basename, so stage the IWAD under the short alias.
    files = [(e32, "DOOM.E32"), (wad, "FDOOM1.WAD")]
    if dll:
        files.append((dll, "EUSER.DLL"))
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

    listing = sh("mdir", "-i", f"{DISK}@@{OFFSET}", "::", check=True,
                 capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    for want in ("doome32", "fdoom1wad"):
        if want not in norm:
            sys.exit(f"mdir missing {want}:\n{listing}")
    print(f"disk OK: {DISK} ({PART_SECTORS} sectors @ LBA {PART_LBA}, "
          f"{len(files)} files)")


def build_iso():
    """Rebuild the GRUB ISO with the staged set (needs mkgrubiso.py updates)."""
    script = os.path.join(REPO, "tools/mkgrubiso.py")
    if not os.path.isfile(script):
        sys.exit(f"missing {script}")
    sh("python3", script, ISO, cwd=REPO, check=True,
       stdout=subprocess.DEVNULL)
    print(f"ISO OK: {ISO}")


def wait_for_bytes(log_path, needles, timeout, start=0):
    """Poll a growing log for any of several byte strings; never raises."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            if any(n in f.read() for n in needles):
                return True
        time.sleep(0.2)
    return False


def qmp_screendump(sock_path, png, tries=16):
    """Best-effort QMP PNG screendump; return True on success."""
    for _ in range(tries):
        if os.path.exists(sock_path):
            break
        time.sleep(0.25)
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(sock_path)
        f = s.makefile("rwb")
        f.readline()                                  # QMP greeting
        f.write(b'{"execute":"qmp_capabilities"}\n')
        f.flush()
        f.readline()                                  # capabilities reply
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
        print(f"[WARN] QMP screendump failed: {exc}")
        return False
    for _ in range(20):                               # PNG write is async
        if os.path.exists(png) and os.path.getsize(png) > 1000:
            print(f"[INFO] screendump {png}: {os.path.getsize(png)} bytes")
            return True
        time.sleep(0.15)
    return False


def run_qemu(log_path, png_path, use_iso):
    """Boot once, launch DOOM, feed keys, grab the title-screen PNG."""
    if os.path.exists(QMP):
        os.remove(QMP)
    qemu = [
        "timeout", "75", "qemu-system-i386", "-M", "q35", "-m", "128",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-qmp", f"unix:{QMP},server,nowait", "-no-reboot",
    ]
    if use_iso:
        qemu += ["-cdrom", ISO, "-boot", "d"]
    else:
        qemu += ["-kernel", KERNEL,
                 "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
                 "-device", "ich9-ahci,id=sata",
                 "-device", "ide-hd,drive=d0,bus=sata.0"]
    print("== QEMU boot (75 s cap), launching DOOM ==")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            wait_for_bytes(log_path, PROMPTS, 70)
            time.sleep(0.5)
            vm.stdin.write(b"DOOM\n")
            vm.stdin.flush()
            wait_for_bytes(log_path, [b"DOOM: TITLE", b"DOOM: WAD OK"], 30)
            time.sleep(2.0)                            # let the title paint
            shot = qmp_screendump(QMP, png_path)
            vm.stdin.write(GAME_KEYS)
            vm.stdin.flush()
            wait_for_bytes(log_path, [b"DOOM: DONE", b"DOOM: FRAME"], 20)
            time.sleep(1.0)
        finally:
            try:
                vm.terminate()
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return shot


def check_log(text):
    ok = True
    for marker in MARKERS:
        good = marker in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")
    for marker in BOOT_MARKERS:
        good = marker in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] boot: {marker}")
    bound = any("e32run: bind slot" in ln and "ord 915" in ln
                for ln in text.splitlines())
    ok &= bound
    print(f"  [{'PASS' if bound else 'FAIL'}] EUSER ord 915 bound")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    ok &= not exc
    print(f"  [{'PASS' if not exc else 'FAIL'}] no Exc/Panic/FAULT/#PF")
    for ln in exc:
        print("      ", ln[:200])
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--no-build-iso", action="store_true")
    ap.add_argument("--log", default=LOG)
    ap.add_argument("--png", default=PNG)
    ap.add_argument("--disk-boot", action="store_true",
                    help="boot -kernel + AHCI disk instead of the GRUB ISO")
    args = ap.parse_args()

    assets = check_assets()
    if assets is None:
        print("\nRESULT: SKIP")
        return 0
    e32, wad, dll = assets
    check_tools()
    os.makedirs(TMP, exist_ok=True)

    use_iso = not args.disk_boot
    if not (args.no_rebuild_disk and os.path.exists(DISK)):
        build_disk(e32, wad, dll)
    if use_iso and not (args.no_build_iso and os.path.exists(ISO)):
        build_iso()

    shot = run_qemu(args.log, args.png, use_iso)
    with open(args.log, "r", errors="replace") as f:
        text = f.read()
    print(f"\n== results (serial log: {args.log})")
    ok = check_log(text)
    if shot:
        print(f"  [PASS] screendump {args.png} (verify title screen manually)")
    else:
        print(f"  [INFO] screendump unavailable ({args.png})")
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_genuine_fullchain_test.py -- ONE boot, ONE disk, the complete genuine chain.

Earlier gates (r1_genuine_retire_test.py, r1_genuine_{efsrv,ewsrv,eshell}_test.py)
prove the genuine components individually, on separate staged disks.  This test
stages ALL of them on a single FAT32 boot volume and boots the CURRENT GRUB ISO
(unchanged, no `make isogrub`), asserting that the boot chain launches them in
genuine order inside one machine session:

    EUSER.DLL resident -> EFILE.E32 (!FileServer) -> ESTART.E32 ->
    EWSRV.E32 -> ESHELL.E32 (banner + prompt)

The four E32 images are auto-launched by GenuineBootLaunch() during Init3, so
all markers are searched from byte 0 of the serial log; they must never be
spawned from the shell (a second instance dies with a spurious FAULT).  The
`genuine file server up, svcmgr fallback skipped` line is emitted by the kernel
AFTER the whole boot list, so the expected log order below is the real code
order (variant/e32load.cpp::GenuineBootLaunch), not the conceptual list order.

Exit 0 only if every component marker is present in order, the genuine path is
taken (no svcmgr/portfilesrv fallback), the ESHELL banner AND prompt appear,
and there are no Exc / PanicKernExec / FAULT lines.  A screendump is always
attempted in `finally` as visual evidence.

Usage:
    python3 tools/r1_genuine_fullchain_test.py [--dry-run]
    python3 tools/r1_genuine_fullchain_test.py --log-dir /tmp/opencode
"""
import argparse
import hashlib
import shlex
import socket
import subprocess
import sys
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
PORT = TOOLS.parent
ISO = PORT / "build/symbian-grub.iso"
KERNEL = PORT / "build/kernfull.elf"
GENUINE = PORT / "genuine/build"
EUSER = PORT / "EUSER.DLL"

# One disk, all four genuine components (plus EUSER and the genuine FS driver).
# (label, source path, FAT name on C:)
COMPONENTS = [
    ("EUSER", EUSER, "EUSER.DLL"),
    ("EFILE", GENUINE / "EFILE.E32", "EFILE.E32"),
    ("EFAT32", GENUINE / "EFAT32.FSY", "EFAT32.FSY"),
    ("ESTART", GENUINE / "estart_.E32", "ESTART.E32"),
    ("EWSRV", GENUINE / "EWSRV.E32", "EWSRV.E32"),
    ("ESHELL", GENUINE / "etshell_.E32", "ESHELL.E32"),
]

# Ordered evidence.  The banner of a component is printed by the component
# itself *before* the kernel's `GenuineBoot: <name> r=0` line for that component.
BOOT_ANCHOR = b"KERN spawning genuine boot DThread"
CHAIN = [
    ("kernel", "boot DThread spawned", BOOT_ANCHOR),
    ("kernel", "probing boot volume", b"GenuineBoot: probing boot volume"),
    ("EUSER", "EUSER.DLL resident (2503 exports)", b"e32run: EUSER.DLL resident"),
    ("EUSER", "exports=2503", b"exports=2503"),
    ("EFILE", "EFILE.E32 launched", b"GenuineBoot: EFILE.E32 r=0"),
    ("ESTART", "ESTART.E32 launched", b"GenuineBoot: ESTART.E32 r=0"),
    ("EWSRV", "EWSRV.E32 launched", b"GenuineBoot: EWSRV.E32 r=0"),
    ("ESHELL", "genuine ESHELL banner", b"ESHELL 0.01(4011)"),
    ("ESHELL", "ESHELL.E32 launched", b"GenuineBoot: ESHELL.E32 r=0"),
    ("kernel", "genuine file server up (no fallback)",
     b"GenuineBoot: file server up, svcmgr fallback skipped"),
    ("ESHELL", "genuine C:\\> prompt (shell uses C:/>)", b"C:/>"),
]
# Any of these means the shim path ran instead of the genuine chain.
FALLBACK_MARKERS = [b"SvcMgr: SVCMGR.E32 not found", b"SvcMgr: boot launch r=0",
                    b"EFILE: port-shim", b"SvcMgr: booting portfilesrv"]
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def wait_for(path, needle, pos=0, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, "rb") as f:
            f.seek(pos)
            data = f.read()
        i = data.find(needle)
        if i >= 0:
            return pos + i + len(needle)
        time.sleep(0.2)
    return None


def qmp_screendump(sock_path, png):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline and not sock_path.exists():
        time.sleep(0.2)
    if not sock_path.exists():
        return False
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(str(sock_path))
        f = s.makefile("rwb")
        f.readline()                      # QMP greeting
        f.write(b'{"execute":"qmp_capabilities"}\n'); f.flush(); f.readline()
        if png.exists():
            png.unlink()
        f.write((f'{{"execute":"screendump","arguments":{{"filename":"{png}",'
                 f'"format":"png"}}}}\n').encode())
        f.flush()
        for _ in range(10):
            line = f.readline()
            if not line or b"return" in line or b"error" in line.lower():
                break
        f.close(); s.close()
    except OSError as exc:
        print(f"  [WARN] QMP screendump: {exc}", flush=True)
        return False
    for _ in range(20):
        if png.exists() and png.stat().st_size > 1000:
            return True
        time.sleep(0.15)
    return False


def stage_disk(logdir):
    disk, fs = logdir / "r1_fullchain.img", logdir / "r1_fullchain_fs.img"
    for p in (disk, fs):
        if p.exists():
            p.unlink()
    with open(disk, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(fs, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", fs, check=True)
    for _, src, name in COMPONENTS:
        sh("mcopy", "-i", fs, src, f"::{name}", check=True)
    with open(fs, "rb") as f:
        data = f.read()
    if len(data) > FS_SIZE:
        raise RuntimeError("FAT image larger than partition")
    entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
             + LBA.to_bytes(4, "little") + SECTORS.to_bytes(4, "little"))
    mbr = bytearray(512)
    mbr[446:462] = entry
    mbr[510], mbr[511] = 0x55, 0xAA
    with open(disk, "r+b") as f:
        f.seek(OFFSET)
        f.write(data)
        f.seek(0)
        f.write(mbr)
    listing = sh("mdir", "-i", f"{disk}@@{OFFSET}", "::", check=True,
                 capture_output=True, text=True).stdout
    norm = "".join(listing.lower().split()).replace(".", "")
    for _, _, name in COMPONENTS:
        if name.lower().replace(".", "") not in norm:
            raise RuntimeError(f"staged disk missing {name}")
    print(f"  [PASS] one FAT32 disk staged with all {len(COMPONENTS)} genuine files",
          flush=True)
    return disk


def boot_and_collect(disk, logdir, direct_kernel=False):
    """Boot the current GRUB ISO with the single staged disk attached as C:.
    Returns (log_text, screendump_ok, marker_offsets)."""
    log, png, sock = (logdir / "r1_fullchain.log", logdir / "r1_fullchain.png",
                      logdir / "r1_fullchain-qmp.sock")
    for p in (log, png, sock):
        if p.exists():
            p.unlink()
    qemu = ["qemu-system-i386", "-M", "q35", "-m", "128", "-display", "none",
            "-serial", "stdio", "-vga", "std",
            "-drive", f"file={disk},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot",
            "-qmp", f"unix:{sock},server,nowait"]
    if direct_kernel:
        qemu += ["-kernel", str(KERNEL)]
    else:
        qemu += ["-cdrom", str(ISO), "-boot", "d"]
    print(f"== booting {'current kernfull.elf (--kernel)' if direct_kernel else 'CURRENT GRUB ISO'} "
          f"with the full-chain disk", flush=True)
    offsets = {}
    pos = 0
    shot = False
    with open(log, "wb") as lf:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=lf,
                              stderr=subprocess.STDOUT)
        try:
            for label, desc, needle in CHAIN:
                # Every marker is anchored at boot start: components launch
                # during Init3, before the console prompt. Sequential search
                # also validates relative order. On a miss keep `pos` so later
                # markers can still be found; out-of-order hits are reported
                # by the post-run whole-log scan below.
                end = wait_for(log, needle, pos, 120 if b"E32 r=0" in needle else 60)
                if end is not None:
                    offsets[needle] = end - len(needle)
                    pos = end
                    print(f"  [MARK] {label:7s} {desc} @ byte {end - len(needle)}",
                          flush=True)
                else:
                    print(f"  [MARK] {label:7s} {desc} MISSING", flush=True)
        finally:
            try:
                time.sleep(3.0)            # let the genuine desktop paint
                shot = qmp_screendump(sock, png)
            except Exception as exc:
                print(f"  [WARN] screendump failed: {exc}", flush=True)
            vm.terminate()
            try:
                vm.wait(timeout=5)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return log.read_text(errors="replace"), shot, offsets


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true",
                    help="verify inputs and print the expected chain; no QEMU")
    ap.add_argument("--log-dir", default="/tmp/opencode",
                    help="output directory (default: /tmp/opencode)")
    ap.add_argument("--kernel", action="store_true",
                    help="diagnostic: boot build/kernfull.elf instead of the ISO")
    args = ap.parse_args()

    logdir = Path(args.log_dir)
    logdir.mkdir(parents=True, exist_ok=True)
    print("r1_genuine_fullchain_test -- all four genuine components in ONE boot",
          flush=True)
    print(f"workspace: {PORT}   log-dir: {logdir}   dry-run: {args.dry_run}",
          flush=True)

    missing = []
    for label, src, _ in COMPONENTS:
        print(f"  {label:7s} {src}" + ("" if src.is_file() else "  [MISSING]"),
              flush=True)
        if not src.is_file():
            missing.append(str(src))
    if not ISO.is_file():
        missing.append(str(ISO))
    if args.kernel and not KERNEL.is_file():
        missing.append(str(KERNEL))
    if missing:
        print("[FAIL] missing inputs: " + ", ".join(missing), flush=True)
        return 1
    if args.dry_run:
        print(f"ISO: {ISO}  ({ISO.stat().st_size} B, "
              f"sha256={sha256(ISO)[:16]}...)", flush=True)
        print("Expected in-order boot markers:", flush=True)
        for label, desc, needle in CHAIN:
            print(f"  {label:7s} {desc:42s} {needle.decode()}", flush=True)
        print("\n[DRY-RUN] no QEMU boot; verdict rows = SKIP", flush=True)
        rows = [(label, desc, "SKIP") for label, desc, _ in CHAIN]
        print_table(rows + [("screendump", "QMP PNG", "SKIP"),
                            ("faults", "no Exc/PanicKernExec/FAULT", "SKIP"),
                            ("fallback", "svcmgr/portfilesrv not used", "SKIP")])
        return 0

    disk = stage_disk(logdir)
    text, shot, offsets = boot_and_collect(disk, logdir, args.kernel)
    raw = text.encode("utf-8", "replace")

    rows, fail = [], 0
    for label, desc, needle in CHAIN:
        if needle in offsets:
            verdict = "PASS"
        else:
            elsewhere = raw.find(needle)
            verdict = "FAIL" if elsewhere < 0 else f"FAIL (out of order, byte {elsewhere})"
            fail += 1
        rows.append((label, desc, verdict))

    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    rows.append(("faults", f"no Exc/PanicKernExec/FAULT ({len(exc)} lines)",
                 "PASS" if not exc else "FAIL"))
    if exc:
        fail += 1
        for ln in exc[:6]:
            print("    " + ln, flush=True)
    hits = [m.decode() for m in FALLBACK_MARKERS if m in raw]
    rows.append(("fallback", "svcmgr/portfilesrv not used" +
                 (" (saw: " + "; ".join(hits) + ")" if hits else ""),
                 "PASS" if not hits else "FAIL"))
    if hits:
        fail += 1
    png = logdir / "r1_fullchain.png"
    rows.append(("screendump", str(png), "PASS" if shot else "FAIL"))
    if not shot:
        fail += 1

    print_table(rows)
    print(f"log: {logdir / 'r1_fullchain.log'}", flush=True)
    print(f"RESULT: {'PASS' if not fail else 'FAIL'}  ({len(rows) - fail}/{len(rows)} rows PASS)",
          flush=True)
    print("note: kernel logs `genuine file server up` AFTER the boot list, "
          "and a component's banner BEFORE its `r=0` line; order above is code order.",
          flush=True)
    return 0 if not fail else 1


def print_table(rows):
    print("\n----- full-chain evidence -----", flush=True)
    print(f"  {'component':12s} {'marker':42s} verdict", flush=True)
    for label, desc, verdict in rows:
        print(f"  {label:12s} {desc:42s} {verdict}", flush=True)


if __name__ == "__main__":
    sys.exit(main())

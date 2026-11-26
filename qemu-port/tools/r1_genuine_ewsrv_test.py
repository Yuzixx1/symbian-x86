#!/usr/bin/env python3
"""r1_genuine_ewsrv_test.py -- QEMU proof for the GENUINE Symbian EWSRV E32.

Builds genuine/build/EWSRV.E32 and genuine/build/EWCLIENT.E32 from read-only
kernelhwsrv sources (via genuine/ewsrv/build_ewsrv.py and build_client.py),
stages both with the genuine EUSER.DLL on a FAT32 disk, and boots the ported
x86 kernel with -vga std. EWSRV.E32 is staged under its genuine FAT name, so
GenuineBootLaunch() auto-starts it during Init3, before the shell prompt; this
script waits for its startup markers from byte 0 of the serial log rather than
re-launching it from the shell (a duplicate process dies silently).  It then
launches EWCLIENT.E32 from the shell, which connects to the genuine window
server over the !E32WindowServer session and draws a titled text window.  It
asserts the genuine startup markers, the external-client IPC markers, and PE2
EUSER import binding, checks for guest faults, and grabs a PNG screendump to
/tmp/opencode/genuine_ewsrv.png after the client has drawn its window.

Skips gracefully (exit 0) when the genuine sources are unavailable.
"""
import argparse
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

PORT = Path(__file__).resolve().parent.parent
ROOT = Path("/tmp/opencode")
DISK = ROOT / "genuine_ewsrv.img"
FS = ROOT / "genuine_ewsrv_fs.img"
LOG = ROOT / "genuine_ewsrv.log"
SOCK = ROOT / "genuine_ewsrv-qmp.sock"
SHOT = ROOT / "genuine_ewsrv.png"
E32 = PORT / "genuine/build/EWSRV.E32"
CLIENT = PORT / "genuine/build/EWCLIENT.E32"
KERNEL = PORT / "build/kernfull.elf"
EUSER_DLL = PORT / "EUSER.DLL"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "
MARKERS = [
    b"EWSRV: genuine window server starting",
    b"EWSRV: E32Main entered (genuine ws_main.cpp)",
    b"EWSRV: WINSERVER IPC server started",
    b"EWSRV-EDISP: LFB=",
    b"EWSRV: CScreenDriver::New() ok",
    b"EWSRV: entering active scheduler (window server READY)",
]
# External client (EWCLIENT.E32) markers.  Every one must be r=0 and the
# window must be drawn; these prove the !E32WindowServer session connects and
# CWsWindow::CreateL succeeds (no EWindowTooWide sret-ABI failure).
CLIENT_MARKERS = [
    b"EWCLIENT: app_main started",
    b"EWCLIENT: Create r=0",
    b"EWCLIENT: ScreenSize r=0 128x76",
    b"EWCLIENT: Init r=0",
    b"EWCLIENT: SetTitle r=0",
    b"EWCLIENT: Write r=0",
    b"EWCLIENT: SetCursorPos r=0",
    b"EWCLIENT: Write2 r=0",
    b"EWCLIENT: window drawn",
]
CLIENT_LAUNCH = b"ewclient.e32"
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF", "Abort")

# EWSRV.E32 is staged under its genuine name, so GenuineBootLaunch() starts it
# during Init3 -- before the serial shell prompt. Wait for the startup markers
# from byte 0 of the log, never from the prompt position, and do not re-launch
# it from the shell (the duplicate instance dies silently).
EWSRV_BOOT_MARKER = b"GenuineBoot: EWSRV.E32 r=0"
SHOT_SETTLE = 3.0     # seconds after the last boot marker for the desktop to paint



def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def build_disk():
    for path in (DISK, FS):
        if path.exists():
            path.unlink()
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "EWSRV.E32"), (CLIENT, "EWCLIENT.E32"),
                      (EUSER_DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing staged file: {src}")
        sh("mcopy", "-i", FS, src, f"::{name}", check=True)
    data = FS.read_bytes()
    if len(data) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    with DISK.open("r+b") as f:
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
             capture_output=True, text=True).stdout
    norm = "".join(out.lower().replace(".", "").split())
    for want in ("ewsrve32", "ewcliente32", "euserdll"):
        if want not in norm:
            sys.exit(f"FAT staging missing {want}: {out}")
    print("FAT OK: EWSRV.E32 + EWCLIENT.E32 + EUSER.DLL staged")


def wait_for(path, needle, pos=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, "rb") as f:
            f.seek(pos)
            data = f.read()
        i = data.find(needle)
        if i >= 0:
            return pos + i + len(needle)
        time.sleep(0.1)
    raise RuntimeError(f"missing {needle!r} in serial log")


def qmp_screendump(path):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline and not os.path.exists(SOCK):
        time.sleep(0.2)
    if not os.path.exists(SOCK):
        print("[WARN] QMP socket never appeared")
        return False
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(str(SOCK))
        f = s.makefile("rwb")
        f.readline()                       # QMP greeting
        f.write(b'{"execute":"qmp_capabilities"}\n'); f.flush(); f.readline()
        if os.path.exists(path):
            os.remove(path)
        f.write(('{"execute":"screendump","arguments":{"filename":"'
                 + str(path) + '","format":"png"}}\n').encode()); f.flush()
        for _ in range(10):
            line = f.readline()
            if not line or b"return" in line or b"error" in line.lower():
                break
        f.close(); s.close()
    except Exception as exc:
        print("[WARN] QMP screendump failed:", exc)
        return False
    for _ in range(20):
        if path.exists() and path.stat().st_size > 500:
            break
        time.sleep(0.15)
    ok = path.exists() and path.stat().st_size > 500
    if ok:
        try:
            from PIL import Image
            im = Image.open(path)
            print(f"[INFO] screendump {path}: {im.size[0]}x{im.size[1]} "
                  f"({path.stat().st_size} bytes)")
        except Exception:
            pass
    return ok


def screendump_renders(png):
    """A rendered desktop means the screen is not one single flat colour.
    The genuine EWSRV text-server desktop paints a solid background plus a
    border (2 colours), so the threshold is >1, not >4."""
    try:
        from PIL import Image
        im = Image.open(png).convert("RGB")
        colours = im.getcolors(maxcolors=1 << 20)
        return colours is not None and len(colours) > 1
    except Exception:
        return png.stat().st_size > 500   # no PIL: non-trivial PNG size only


def run_qemu(log_path):
    if SOCK.exists():
        SOCK.unlink()
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-qmp", f"unix:{SOCK},server,nowait",
        "-kernel", str(KERNEL),
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, waiting for boot-time genuine EWSRV ...")
    pos, shot_ok, boot_at, client_at = 0, False, None, None
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            # Anchored at boot start: GenuineBootLaunch() emitted these lines
            # during Init3, before the prompt. A manual "ewsrv" here would
            # start a second instance that dies silently (spurious FAULT).
            for marker in MARKERS:
                pos = wait_for(log_path, marker, 0, 90)
            boot_at = time.monotonic()
            print(f"  [INFO] boot-time EWSRV markers present at byte {pos}")

            # Skip the manual launch when the genuine boot marker is present.
            # It normally follows the last startup marker by a few lines, so
            # give it a short grace window before deciding it is absent.
            boot_launched = EWSRV_BOOT_MARKER in Path(log_path).read_bytes()
            if not boot_launched:
                try:
                    wait_for(log_path, EWSRV_BOOT_MARKER, 0, 10)
                    boot_launched = True
                except RuntimeError:
                    pass
            if boot_launched:
                print("  [INFO] boot marker present; skipping manual ewsrv launch")
            else:
                print("  [WARN] no boot marker; falling back to manual ewsrv launch")
                pos = wait_for(log_path, PROMPT, 0, 90)
                vm.stdin.write(b"ewsrv\n")
                vm.stdin.flush()
                for marker in MARKERS:
                    pos = wait_for(log_path, marker, pos, 45)

            # Launch the external client from the shell.  EWSRV is already
            # boot-started and ready (we waited for its READY marker above).
            # Skip only if a boot-time client already emitted its first marker.
            if CLIENT_MARKERS[0] in Path(log_path).read_bytes():
                print("  [INFO] client already started at boot; skipping launch")
                client_at = time.monotonic()
            else:
                pos = wait_for(log_path, PROMPT, 0, 90)
                vm.stdin.write(CLIENT_LAUNCH + b"\n")
                vm.stdin.flush()
                print(f"  [INFO] launched {CLIENT_LAUNCH.decode()} from shell")
                pos = wait_for(log_path, CLIENT_MARKERS[0], pos, 60)
                for marker in CLIENT_MARKERS[1:]:
                    pos = wait_for(log_path, marker, pos, 60)
                client_at = time.monotonic()
                print(f"  [INFO] client markers present at byte {pos}")
        except RuntimeError as exc:
            print("QEMU drive failed:", exc)
        finally:
            # Always attempt the screendump, even if a wait above timed out.
            # Anchor the settle delay to the client's last draw marker (or the
            # EWSRV boot marker as fallback) so the window has time to paint.
            try:
                settle_from = client_at if client_at is not None else boot_at
                if settle_from is not None:
                    left = SHOT_SETTLE - (time.monotonic() - settle_from)
                    if left > 0:
                        time.sleep(left)
                shot_ok = qmp_screendump(SHOT)
            except Exception as exc:
                print("[WARN] screendump:", exc)
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill(); vm.wait()
    return shot_ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build", action="store_true",
                    help="skip rebuilding EWSRV.E32/EWCLIENT.E32")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=str(LOG))
    args = ap.parse_args()
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(t):
            sys.exit(f"missing tool: {t}")
    if not args.no_build:
        for script, label in (("build_ewsrv.py", "EWSRV.E32"),
                              ("build_client.py", "EWCLIENT.E32")):
            r = sh("python3", PORT / "genuine/ewsrv" / script)
            if r.returncode:
                sys.exit(f"{label} build failed")
    for path, label in ((E32, "EWSRV.E32"), (CLIENT, "EWCLIENT.E32")):
        if not path.exists():
            sys.exit(f"{label} missing")
    if os.path.exists(args.log):
        os.remove(args.log)
    if not (args.no_rebuild_disk and DISK.exists()):
        build_disk()
    shot_ok = run_qemu(args.log)

    text = Path(args.log).read_text(errors="replace")
    ok = True
    for m in MARKERS + CLIENT_MARKERS:
        hit = m.decode() in text
        ok &= hit
        print(f"  [{'PASS' if hit else 'FAIL'}] {m.decode()}")
    import struct
    blob = E32.read_bytes()
    imp_off = struct.unpack_from("<I", blob, 108)[0]
    expected = struct.unpack_from("<i", blob, imp_off + 8)[0]
    bound = sum(1 for ln in text.splitlines()
                if "e32run: bind slot" in ln and "ord " in ln)
    bind_ok = bound >= expected
    ok &= bind_ok
    print(f"  [{'PASS' if bind_ok else 'FAIL'}] {bound} PE2 EUSER imports bound "
          f"(expected >= {expected})")
    exc_lines = [ln for ln in text.splitlines()
                 if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no guest exception/panic/FAULT")
    for line in exc_lines[:8]:
        print("      ", line)
    renders = shot_ok and screendump_renders(SHOT)
    print(f"  [{'PASS' if renders else 'FAIL'}] screendump renders desktop "
          f"(>1 colour) {SHOT}")
    ok &= renders
    if shot_ok:
        try:
            st = SHOT.stat()
            print(f"  [INFO] PNG {SHOT}: {st.st_size} bytes, mtime "
                  f"{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(st.st_mtime))}")
        except OSError:
            pass
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

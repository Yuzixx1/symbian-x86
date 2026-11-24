#!/usr/bin/env python3
"""r1_genuine_pointer_test.py -- close the last EUSER sret-ABI gap.

The genuine EWSRV has five calls to EUSER ord 1868 (TRawEvent::Pos, an 8-byte
TPoint returned in EDX:EAX after the -freg-struct-return rebuild).  They live in
CEvent::RunL()'s TRawEvent::EPointerMove branch, which QEMU never reached:
there is no pointing device and `mouse test`/`press` feed only the hand-written
wsrv ring, not the genuine EKA2 kernel event queue.

This test stages \\PTRINJ.TXT on the boot FAT volume.  The port bridge
(variant/qemu_display.cpp: GenuinePointerBridge) reads it and calls the public
Kern::AddEvent API with a genuine TRawEvent(EPointerMove) at known coordinates.
Genuine EWSRV is already the kernel's event-hook owner, so its pending
UserSvr::RequestEvent completes, CEvent::RunL() dispatches EPointerMove and
calls TRawEvent::Pos() on the delivered event.

Asserts:
  * bridge marker shows an accepted AddEvent (add=0) with the injected coords,
  * bridge verdict shows the event was consumed off the kernel event queue,
  * genuine EWSRV boot + external EWCLIENT window markers are present,
  * no Exc/PanicKernExec/FAULT/MMU #PF/Abort in the guest log,
  * QMP screendump renders the client window (>1 colour).

Skips gracefully (exit 0) when the genuine sources are unavailable.
"""
import argparse
import os
import re
import shlex
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

PORT = Path(__file__).resolve().parent.parent
ROOT = Path("/tmp/opencode")
DISK = ROOT / "genuine_pointer.img"
FS = ROOT / "genuine_pointer_fs.img"
LOG = ROOT / "genuine_pointer.log"
SOCK = ROOT / "genuine_pointer-qmp.sock"
SHOT = ROOT / "genuine_pointer.png"
E32 = PORT / "genuine/build/EWSRV.E32"
CLIENT = PORT / "genuine/build/EWCLIENT.E32"
KERNEL = PORT / "build/kernfull.elf"
PORTFILESrv = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "

# Coordinates injected into the genuine kernel event queue (pixels).  EWSRV's
# MouseMove divides by the text font and clamps to the 128x76 text grid, so any
# small positive value is both plausible and on-screen.
PTR_X, PTR_Y = 80, 40
PTRINJ = ROOT / "genuine_pointer_PTRINJ.TXT"

MARKERS = [
    b"EWSRV: genuine window server starting",
    b"EWSRV: E32Main entered (genuine ws_main.cpp)",
    b"EWSRV: WINSERVER IPC server started",
    b"EWSRV-EDISP: LFB=",
    b"EWSRV: CScreenDriver::New() ok",
    b"EWSRV: entering active scheduler (window server READY)",
]
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

EWSRV_BOOT_MARKER = b"GenuineBoot: EWSRV.E32 r=0"
SHOT_SETTLE = 3.0


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def build_disk():
    if not (E32.exists() and CLIENT.exists()):
        sys.exit("genuine EWSRV/EWCLIENT images missing (build first)")
    PTRINJ.write_text(f"{PTR_X} {PTR_Y}\n")
    for path in (DISK, FS):
        if path.exists():
            path.unlink()
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "EWSRV.E32"), (CLIENT, "EWCLIENT.E32"),
                      (EUSER_DLL, "EUSER.DLL"),
                      (PORTFILESrv, "portfilesrv"),
                      (PTRINJ, "PTRINJ.TXT")):
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
    for want in ("ewsrve32", "ewcliente32", "euserdll", "portfilesrv", "ptrinjt"):
        if want not in norm:
            sys.exit(f"FAT staging missing {want}: {out}")
    print(f"FAT OK: EWSRV + EWCLIENT + EUSER + portfilesrv + "
          f"PTRINJ.TXT({PTR_X},{PTR_Y}) staged")


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
        f.readline()
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
    try:
        from PIL import Image
        im = Image.open(png).convert("RGB")
        colours = im.getcolors(maxcolors=1 << 20)
        return colours is not None and len(colours) > 1
    except Exception:
        return png.stat().st_size > 500


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
    print("== booting QEMU (genuine EWSRV + injected pointer event) ...")
    pos, shot_ok, boot_at, client_at = 0, False, None, None
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            for marker in MARKERS:
                pos = wait_for(log_path, marker, 0, 90)
            boot_at = time.monotonic()
            print(f"  [INFO] boot-time EWSRV markers present at byte {pos}")

            # Opt-in pointer bridge emits these during GenuineBootLaunch.
            for marker in (b"PTRBRIDGE:", b"PTRBRIDGE: verdict"):
                pos = wait_for(log_path, marker, 0, 30)
            print(f"  [INFO] pointer bridge markers present at byte {pos}")

            boot_launched = EWSRV_BOOT_MARKER in Path(log_path).read_bytes()
            if not boot_launched:
                try:
                    wait_for(log_path, EWSRV_BOOT_MARKER, 0, 10)
                    boot_launched = True
                except RuntimeError:
                    pass
            if not boot_launched:
                pos = wait_for(log_path, PROMPT, 0, 90)
                vm.stdin.write(b"ewsrv\n")
                vm.stdin.flush()
                for marker in MARKERS:
                    pos = wait_for(log_path, marker, pos, 45)

            if CLIENT_MARKERS[0] in Path(log_path).read_bytes():
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

    # Pointer evidence: the bridge accepted the event with our coords and the
    # kernel event queue advanced (EWSRV's RequestEvent got it -> RunL calls
    # EUSER ord 1868 TRawEvent::Pos on the delivered 8-byte TPoint).
    inject = re.search(r"PTRBRIDGE: injected EPointerMove x=(\d+) y=(\d+) add=(\-?\d+)",
                       text)
    inj_ok = bool(inject) and (int(inject.group(1)), int(inject.group(2))) == (PTR_X, PTR_Y) \
        and int(inject.group(3)) == 0
    ok &= inj_ok
    print(f"  [{'PASS' if inj_ok else 'FAIL'}] pointer bridge injected "
          f"EPointerMove({PTR_X},{PTR_Y}) via Kern::AddEvent (add=0)")

    verdict = re.search(r"PTRBRIDGE: verdict head=[0-9a-f]+ tail=[0-9a-f]+ consumed=(\d+)",
                        text)
    consume_ok = bool(verdict) and int(verdict.group(1)) == 1
    ok &= consume_ok
    print(f"  [{'PASS' if consume_ok else 'FAIL'}] genuine EWSRV consumed the "
          f"raw pointer event (TRawEvent::Pos path reached)")

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
    print(f"  [{'PASS' if renders else 'FAIL'}] screendump renders client window "
          f"(>1 colour) {SHOT}")
    ok &= renders
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""QEMU proof: full genuine EFILE.EXE publishes FileServer and serves IPC.

Stages the name-patched EFILE (unprotected RFileServer, see
patch_efile_name.py) under the alias EFILERFS.E32 so the boot-time GenuineBoot
FAT probe cannot auto-launch it during Init3, then launches it and the patched
F32 probe client from the serial shell.
"""
import os
import subprocess
import sys
import time
from pathlib import Path

PORT = Path(__file__).resolve().parent.parent
ROOT = Path("/tmp/opencode")
DISK = ROOT / "fallback_efile.img"
FS = ROOT / "fallback_efile_fs.img"
LOG = ROOT / "fallback_efile.log"
SERVER = PORT / "genuine/build/EFILE_RFS_NOPANIC.E32"
CLIENT = PORT / "genuine/build/F32CLIENT_RFS.E32"
KERNEL = PORT / "build/kernfull.elf"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF", "Abort")


def sh(*args, **kw):
    return subprocess.run([str(a) for a in args], **kw)


def build_disk():
    for p in (DISK, FS):
        if p.exists():
            p.unlink()
    DISK.write_bytes(b"\0" * (MB * 1024 * 1024))
    FS.write_bytes(b"\0" * FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True, stdout=subprocess.DEVNULL)
    for src, name in ((SERVER, "EFILERFS.E32"), (CLIENT, "F32CRFS.E32"),
                      (EUSER_DLL, "EUSER.DLL")):
        if not Path(src).exists():
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", FS, src, f"::{name}", check=True)
    data = FS.read_bytes()
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


def wait_for(path, needle, pos=0, timeout=120):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        data = Path(path).read_bytes()
        i = data.find(needle, pos)
        if i >= 0:
            return i + len(needle)
        time.sleep(0.2)
    raise RuntimeError(f"missing {needle!r} (see {path})")


def main():
    build_disk()
    if LOG.exists():
        LOG.unlink()
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-kernel", str(KERNEL),
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    with LOG.open("wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=log,
                              stderr=subprocess.STDOUT)
        try:
            pos = wait_for(LOG, PROMPT, timeout=120)
            print("PASS: serial shell prompt", flush=True)
            vm.stdin.write(b"efilerfs\n")
            vm.stdin.flush()
            for marker in (b"EFILE: genuine file server starting",
                           b"E32Main entered", b"FileServer E32Main"):
                try:
                    pos = wait_for(LOG, marker, pos, 30)
                    print(f"PASS: {marker.decode()}", flush=True)
                except RuntimeError:
                    print(f"n/a: {marker.decode()}", flush=True)
            pos = wait_for(LOG, PROMPT, pos, 30)
            vm.stdin.write(b"f32crfs\n")
            vm.stdin.flush()
            pos = wait_for(LOG, b"F32CLIENT: IPC round trip OK", pos, 90)
            print("PASS: F32CLIENT: IPC round trip OK", flush=True)
            time.sleep(1)
        except RuntimeError as exc:
            print("FAIL:", exc, flush=True)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=5)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    text = LOG.read_text(errors="replace")
    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    print(f"[{'PASS' if len(binds) >= 463 else 'FAIL'}] "
          f"{len(binds)} PE2 EUSER imports bound (EFILE needs 463)")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    print(f"[{'PASS' if not exc else 'FAIL'}] no guest fault")
    for ln in exc[:10]:
        print("      ", ln)
    ok = ("EFILE: genuine file server starting" in text
          and "F32CLIENT: IPC round trip OK" in text
          and len(binds) >= 463 and not exc)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""QEMU proof for the genuine (name-unpatched) EFILE.EXE server.

Stages any E32 given by EFILE_E32 (default genuine/build/EFILE.E32) under an
alias so GenuineBoot cannot auto-launch it, plus an IPC client whose connect
name is patched to the genuine "!FileServer".  Reports import binds, the
kernel's file-server-registration line and guest faults.
"""
import os
import subprocess
import sys
import time
from pathlib import Path

PORT = Path(__file__).resolve().parent.parent
ROOT = Path("/tmp/opencode")
DISK = ROOT / "fallback_efile_genuine.img"
FS = ROOT / "fallback_efile_genuine_fs.img"
LOG = Path(os.environ.get("EFILE_LOG", str(ROOT / "fallback_efile_genuine.log")))
SERVER = Path(os.environ.get("EFILE_E32", str(PORT / "genuine/build/EFILE.E32")))
CLIENT = Path(os.environ.get("EFILE_CLIENT", str(ROOT / "efile_client_bang.E32")))
KERNEL = PORT / "build/kernfull.elf"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "
# e32run logs one "bind slot" line per PE2 import bound to EUSER.  The full
# genuine EFILE.E32 binds 463 ordinals (kernel: "imports bound dll (463
# ordinals)"); the patched F32CLI.E32 client binds 1 ordinal ("imports bound
# dll (1 ordinals)").  The single QEMU log covers both processes, so the
# exact expected total is 464 -- not 463.  Keep this an exact check: lowering
# it to >=463 would mask a silently broken EFILE or client import table.
EXPECTED_BINDS = 463 + 1  # 463 EFILE + 1 F32CLI = 464
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
    for src, name in ((SERVER, "EFILEX.E32"), (CLIENT, "F32CLI.E32"),
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
        i = Path(path).read_bytes().find(needle, pos)
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
            vm.stdin.write(b"efilex\n")
            vm.stdin.flush()
            pos = wait_for(LOG, b"EFILE: genuine file server starting", pos, 30)
            print("PASS: EFILE entered", flush=True)
            time.sleep(6)
            vm.stdin.write(b"f32cli\n")
            vm.stdin.flush()
            time.sleep(4)
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
    print(f"[{'PASS' if len(binds) == EXPECTED_BINDS else 'FAIL'}] "
          f"{len(binds)} PE2 imports bound (expected {EXPECTED_BINDS} "
          f"= 463 EFILE + 1 F32CLI)")
    for marker in ("File server thread registered", "E32Main returned",
                   "F32CLIENT: IPC round trip OK", "EFILE: port-shim"):
        print(f"  {marker!r}: {text.count(marker)}")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    print(f"[{'PASS' if not exc else 'FAIL'}] no guest fault")
    for ln in exc[:6]:
        print("      ", ln.strip())
    print("LOG:", LOG)
    return 0 if len(binds) == EXPECTED_BINDS and not exc else 1


if __name__ == "__main__":
    sys.exit(main())

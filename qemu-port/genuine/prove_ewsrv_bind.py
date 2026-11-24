#!/usr/bin/env python3
"""Prove genuine EWSRV.E32 binds all 212 PE2 EUSER imports in QEMU.

Stages EWSRV under the alias EWSRVX.E32 so the new GenuineBoot FAT-probe
chain (which auto-launches \\EWSRV.E32 during Init3) cannot race the shell
launch; the shell's RunImage then loads the same bytes via "ewsrvx".
"""
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

PORT = Path(__file__).resolve().parent.parent
ROOT = Path("/tmp/opencode")
DISK = ROOT / "fallback_ewsrv_bind.img"
FS = ROOT / "fallback_ewsrv_bind_fs.img"
LOG = ROOT / "fallback_ewsrv_bind.log"
E32 = PORT / "genuine/build/EWSRV.E32"
KERNEL = PORT / "build/kernfull.elf"
PORTFILESrv = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"
EUSER_DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
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
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF", "Abort")


def sh(*args, **kw):
    print("+", " ".join(str(a) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def build_disk():
    for p in (DISK, FS):
        if p.exists():
            p.unlink()
    DISK.write_bytes(b"\0" * (MB * 1024 * 1024))
    FS.write_bytes(b"\0" * FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True, stdout=subprocess.DEVNULL)
    for src, name in ((E32, "EWSRVX.E32"), (EUSER_DLL, "EUSER.DLL"),
                      (PORTFILESrv, "portfilesrv")):
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
    raise RuntimeError(f"missing {needle!r}")


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
            pos = wait_for(LOG, PROMPT)
            print("PASS: reached serial shell prompt", flush=True)
            vm.stdin.write(b"ewsrvx\n")
            vm.stdin.flush()
            for m in MARKERS:
                pos = wait_for(LOG, m, pos, 60)
                print(f"PASS: {m.decode()}", flush=True)
            time.sleep(3)
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
    bound = [ln.strip() for ln in text.splitlines()
             if "e32run: bind slot" in ln]
    print(f"[{'PASS' if len(bound) == 212 else 'FAIL'}] "
          f"{len(bound)} PE2 EUSER imports bound (expected 212)")
    print("  first:", bound[0] if bound else "<none>")
    print("  last: ", bound[-1] if bound else "<none>")
    exc_lines = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    print(f"[{'PASS' if not exc_lines else 'FAIL'}] no guest fault")
    for ln in exc_lines[:8]:
        print("      ", ln)
    ok = len(bound) == 212 and not exc_lines and all(m.decode() in text for m in MARKERS)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

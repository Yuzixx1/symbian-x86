#!/usr/bin/env python3
"""r1_genuine_hal_test.py -- the designed HAL probe: genuine UserSvr::HalFunction
reaching the port kernel's display HAL entry.

Registered in r1_final_verify.py's DEFAULT_TESTS as row 15 (`hal_probe`).  It
needs a hal_smoke.E32 built with -DHAL_PROBE_SCREENINFO and proves one narrow
thing (the HAL veneer), not a boot-required behaviour -- so when hal_smoke.E32
or EUSER.DLL is absent the probe prints `RESULT: SKIP` and exits 0 without
booting QEMU.  Only a missing artifact skips; a boot or assertion failure is
still FAIL.  r1_final_verify.py reports SKIP as non-fatal by default but fatal
under --strict-skip, so the 15-row gate cannot go green on a missing artifact.

Because the pipeline imports a single EUSER symbol (ord 915, RDebug::Printf),
everything the probe prints comes through that PE2 IAT slot, so the probe
result and the loader's bind line are asserted together.

hal_smoke.E32 (genuine hal_main.cpp + userhal.cpp + hal_glue.cpp as an EXE,
genuine/hal/build_hal.sh, exe twin only) is staged with the genuine EUSER.DLL
on a FAT32 disk, build/kernfull.elf is booted in QEMU, and `hal_smoke` is typed
at the serial shell.  The probe block in hal_smoke_main calls

    UserSvr::HalFunction(EHalGroupDisplay, EDisplayHalScreenInfo, &pckg, NULL)

which is the genuine 4-argument path: UserSvr::HalFunction body -> Exec::
HalFunction (exec 4, int 0x21) -> the port kernel's EHalGroupDisplay entry
(variant/qemu_display.cpp:496), which enables the Bochs VBE LFB and copies a
24-byte TScreenInfoV01 back with Kern::InfoCopy.

Asserted:
  * the smoke markers still print (HAL::Get/Set/GetAll exercised),
  * "HALPROBE: r=0 addr=fd000000 w=1024 h=768"  -- i.e. r == KErrNone,
    iScreenAddress == 0xFD000000 (the q35 Bochs VBE LFB BAR), and
    iScreenSize == 1024x768,
  * EUSER ord 915 bound by the loader,
  * no "Exc "/"PanicKernExec"/"FAULT:" lines.

Usage:
  python3 tools/r1_genuine_hal_test.py [--log PATH] [--no-rebuild-disk]

Exit code 0 = PASS or SKIP (SKIP never boots QEMU), 1 = FAIL.
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK = ROOT + "/r1_genuine_hal.img"
FSIMG = ROOT + "/r1_genuine_hal_fs.img"
LOG_DEFAULT = ROOT + "/r1_genuine_hal.log"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KERNEL = REPO + "/build/kernfull.elf"
HAL_SMOKE = REPO + "/genuine/build/hal_smoke.E32"
EUSER = REPO + "/EUSER.DLL"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")

# Smoke path markers that must survive the probe addition (the probe sits
# between HAL::GetAll and the final "done" line of hal_smoke_main).
SMOKE_MARKERS = [
    b"HALU: genuine hal_main.cpp+userhal.cpp loaded",
    b"HALU: HAL::Get(ECPU) -> r=0 value=2 (ECPU_X86=2)",
    b"HALU: HAL::GetAll -> r=0 entries=",
    b"HALU SMOKE: done, exiting cleanly",
]

# The probe line, exact as hal_glue.cpp prints it.
PROBE_LINE = b"HALPROBE: r=0 addr=fd000000 w=1024 h=768"
PROBE_RE = re.compile(r"HALPROBE: r=(-?\d+) addr=([0-9a-fA-F]{8}) w=(\d+) h=(\d+)")
EXPECT_ADDR = 0xFD000000
EXPECT_W, EXPECT_H = 1024, 768


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def skip_reason():
    """Why the probe cannot run, or None when its artifacts are present.

    Both inputs come from outside build/kernfull.elf: hal_smoke.E32 from
    genuine/hal/build_hal.sh and EUSER.DLL, tracked at qemu-port/EUSER.DLL.
    When either is absent there is nothing to probe, so the harness reports
    SKIP instead of failing a full-gate run on a missing artifact.
    """
    parts = []
    if not os.path.isfile(HAL_SMOKE):
        parts.append(f"{HAL_SMOKE} (build with: sh genuine/hal/build_hal.sh)")
    if not os.path.isfile(EUSER):
        parts.append(EUSER)
    if not parts:
        return None
    return "missing " + ", ".join(parts)


def validate_inputs():
    ok = True
    for path in (KERNEL, HAL_SMOKE, EUSER):
        if not os.path.isfile(path):
            print(f"[FAIL] missing {path}")
            ok = False
    if not os.path.isfile(HAL_SMOKE):
        print("       build it with: sh genuine/hal/build_hal.sh")
    if not ok:
        sys.exit(1)
    r = sh("python3", REPO + "/tools/e32read.py", HAL_SMOKE, "--quiet")
    good = r.returncode == 0
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] E32 valid: hal_smoke.E32")
    # Fail fast if the exe twin was rebuilt without -DHAL_PROBE_SCREENINFO:
    # the marker literal lives in the image's data section, so its absence
    # means the probe block was compiled out and QEMU would prove nothing.
    with open(HAL_SMOKE, "rb") as f:
        image = f.read()
    good = b"HALPROBE:" in image
    ok &= good
    print(f"[{'PASS' if good else 'FAIL'}] hal_smoke.E32 contains HALPROBE "
          f"(built with -DHAL_PROBE_SCREENINFO)")
    return ok


def build_disk():
    for path in (DISK, FSIMG):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FSIMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FSIMG, check=True)
    # HAL_SMOKE.E32 upper-case so the shell's `hal_smoke` command resolves it;
    # EUSER.DLL supplies the ord-915 RDebug::Printf the markers print through.
    for src, name in ((HAL_SMOKE, "HAL_SMOKE.E32"), (EUSER, "EUSER.DLL")):
        sh("mcopy", "-i", FSIMG, src, f"::{name}", check=True)
    with open(FSIMG, "rb") as f:
        data = f.read()
    if len(data) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
             + LBA.to_bytes(4, "little") + SECTORS.to_bytes(4, "little"))
    mbr = bytearray(512)
    mbr[446:462] = entry
    mbr[510], mbr[511] = 0x55, 0xAA
    with open(DISK, "r+b") as f:
        f.seek(OFFSET)
        f.write(data)
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK}@@{OFFSET}", "::", check=True,
             capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for want in ("hal_smokee32", "euserdll"):
        if want not in norm:
            sys.exit(f"mdir missing {want}:\n{out.stdout}")
    print("mdir OK: HAL_SMOKE.E32, EUSER.DLL staged")


def wait_for(path, needle, start=0, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, "rb") as f:
            f.seek(start)
            data = f.read()
        i = data.find(needle)
        if i >= 0:
            return start + i + len(needle)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {needle!r} (see {path})")


def run_qemu(log_path):
    qemu = ["qemu-system-i386", "-M", "q35", "-m", "128",
            "-vga", "std", "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    print("== booting QEMU, launching hal_smoke (HAL_PROBE_SCREENINFO) ...", flush=True)
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = wait_for(log_path, PROMPT, 0, 90)
            vm.stdin.write(b"hal_smoke\n")
            vm.stdin.flush()
            pos = wait_for(log_path, PROBE_LINE, pos, 45)
            pos = wait_for(log_path, b"HALU SMOKE: done", pos, 45)
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


def check(text):
    ok = True
    for marker in SMOKE_MARKERS:
        hit = marker.decode() in text
        ok &= hit
        print(f"  [{'PASS' if hit else 'FAIL'}] {marker.decode()}")
    m = PROBE_RE.search(text)
    hit = m is not None
    ok &= hit
    print(f"  [{'PASS' if hit else 'FAIL'}] HALPROBE line printed")
    if m:
        r_val, addr, width, height = m.groups()
        for label, good, got in (
                ("r == 0 (KErrNone)", int(r_val) == 0, r_val),
                (f"addr == 0x{EXPECT_ADDR:08X}", int(addr, 16) == EXPECT_ADDR, addr),
                (f"w == {EXPECT_W}", int(width) == EXPECT_W, width),
                (f"h == {EXPECT_H}", int(height) == EXPECT_H, height)):
            ok &= good
            print(f"  [{'PASS' if good else 'FAIL'}] {label} (got {got})")
    bound915 = any("ord 915" in ln for ln in text.splitlines()
                   if "e32run: bind slot" in ln)
    ok &= bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound by loader")
    exc_lines = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no 'Exc '/'PanicKernExec'/'FAULT:' lines")
    for line in exc_lines[:10]:
        print("   ", line[:200])
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    # A missing probe artifact is a SKIP: checked before the tool probe and
    # before any disk or QEMU work, so a host without qemu still means SKIP.
    reason = skip_reason()
    if reason is not None:
        print(f"[SKIP] {reason}")
        print("RESULT: SKIP")
        return 0
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")
    os.makedirs(ROOT, exist_ok=True)
    if os.path.exists(args.log):
        os.remove(args.log)
    ok = validate_inputs()
    if not (args.no_rebuild_disk and os.path.exists(DISK)):
        build_disk()
    try:
        run_qemu(args.log)
    except RuntimeError as exc:
        print("QEMU drive failed:", exc)
        ok = False
    text = open(args.log, "r", errors="replace").read()
    ok &= check(text)
    if not ok:
        print("\n== relevant serial lines:")
        for line in text.splitlines():
            if any(k in line.lower() for k in ("halu", "halprobe", "e32run", "loaded r")):
                print("   ", line[:220])
    print(f"serial log: {args.log}")
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

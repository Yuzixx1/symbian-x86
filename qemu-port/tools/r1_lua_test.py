#!/usr/bin/env python3
"""r1_lua_test.py -- QEMU R1 test for the genuine Lua 5.4.6 E32 interpreter.

Stages build/user/LUA.E32 and EUSER.DLL on a FAT32 disk, boots the R1 kernel,
launches `lua` from the serial shell, drives the interactive REPL with real Lua
statements and verifies the results, PE2 EUSER ordinal 915 binding and a clean
(no guest exception / kernel panic) log.  Exits 0 only when every check passes.

Usage: python3 tools/r1_lua_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISK = "/tmp/opencode/r1_lua.img"
FS = "/tmp/opencode/r1_lua_fs.img"
LOG = "/tmp/opencode/r1_lua.log"
KERNEL = os.path.join(ROOT, "build/kernfull.elf")
E32 = os.path.join(ROOT, "build/user/LUA.E32")
BOOT = os.path.join(ROOT, "build/user/LUABOOT.E32")
DLL = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

# Ordered markers.  The first two come from the embedded demo script; the rest
# are emitted by the commands fed to the REPL below.
MARKERS = [
    b"LUA: LUA 5.4.6 READY",
    b"LUA: fib(10)=55",
    b"LUA: table t[2]=20",
    b"LUA: T1=5",
    b"LUA: LOOP=1",
    b"LUA: LOOP=2",
    b"LUA: LOOP=3",
    b"LUA: T3=2",
    b"LUA: T4=14",
    b"LUA: DONE",
]

# (statement fed to the REPL, marker it must produce)
COMMANDS = [
    ("print(2+3)", None),
    ('print("LUA: T1="..(2+3))', b"LUA: T1=5"),
    ("for i=1,3 do print(i) end", None),
    ('for i=1,3 do print("LUA: LOOP="..i) end', b"LUA: LOOP=3"),
    ("t={1,2,3}; print(t[2])", None),
    ('print("LUA: T3="..t[2])', b"LUA: T3=2"),
    ("function f(x) return x*2 end; print(f(7))", None),
    ('print("LUA: T4="..f(7))', b"LUA: T4=14"),
]

EXC_KEYS = ("Exc ", "PanicKernExec")


def sh(*args, **kw):
    print("+", " ".join(args))
    return subprocess.run(args, **kw)


def build_disk():
    for path in (DISK, FS):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FS, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS, check=True)
    for src, name in ((E32, "LUA.E32"), (BOOT, "LUABOOT.E32"),
                      (DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src}")
        sh("mcopy", "-i", FS, src, f"::{name}", check=True)
    with open(FS, "rb") as f:
        data = f.read()
    with open(DISK, "r+b") as f:
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
    norm = "".join(out.lower().split()).replace(".", "")
    for wanted in ("luae32", "luaboot", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out}")
    print("mdir OK: LUA.E32, LUABOOT.E32 and EUSER.DLL staged")


def tail_from(path, pos):
    with open(path, "rb") as f:
        f.seek(pos)
        return f.read()


def wait_any(log_path, needles, start=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        data = tail_from(log_path, start)
        found = None
        best = None
        for needle in needles:
            i = data.find(needle)
            if i >= 0 and (best is None or i < best):
                best, found = i, needle
        if found is not None:
            return found, start + best + len(found)
        time.sleep(0.1)
    raise RuntimeError(f"serial log missing one of {needles!r} (see {log_path})")


def run_qemu(log_path):
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, driving Lua REPL via serial console ...")
    vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                          stdout=open(log_path, "wb"), stderr=subprocess.STDOUT)
    try:
        pos = wait_any(log_path, [b"C:/> "], 0, 120)[1]
        vm.stdin.write(b"luaboot\n")
        vm.stdin.flush()
        # The Lua child's banner (and its demo output) can be emitted before
        # both the shell prompt we just consumed and the launcher's own
        # "launched" status line.  Search from byte 0 so an early READY is
        # never skipped, then walk the demo markers forward from there.
        _, pos = wait_any(log_path, [b"LUA: LUA 5.4.6 READY"], 0, 120)
        _, pos = wait_any(log_path, [b"LUA: fib(10)=55"], pos, 60)
        _, pos = wait_any(log_path, [b"LUA: table t[2]=20"], pos, 60)
        _, pos = wait_any(log_path, [b"lua> "], pos, 60)
        # The shell needs a couple of ticks to notice the console lease after
        # the service-manager launch; feeding keystrokes earlier races it.
        time.sleep(2.0)
        for statement, marker in COMMANDS:
            vm.stdin.write(statement.encode() + b"\n")
            vm.stdin.flush()
            if marker is not None:
                _, pos = wait_any(log_path, [marker], pos, 60)
            _, pos = wait_any(log_path, [b"lua> "], pos, 60)
            time.sleep(0.2)
        vm.stdin.write(b"quit\n")
        vm.stdin.flush()
        wait_any(log_path, [b"LUA: DONE"], pos, 60)
        time.sleep(1.0)
    finally:
        vm.terminate()
        try:
            vm.wait(timeout=3)
        except subprocess.TimeoutExpired:
            vm.kill()
            vm.wait()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(tool):
            sys.exit(f"missing tool: {tool}")
    if not (args.no_rebuild_disk and os.path.exists(DISK)):
        build_disk()

    error = None
    try:
        run_qemu(args.log)
    except RuntimeError as exc:
        error = exc
        print("QEMU drive failed:", exc)
    with open(args.log, errors="replace") as f:
        text = f.read()

    ok = True
    offsets = [text.find(m.decode()) for m in MARKERS]
    for marker, p in zip(MARKERS, offsets):
        good = p >= 0
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker.decode()}")
    ordered = (all(p >= 0 for p in offsets)
               and all(offsets[i] < offsets[i + 1] for i in range(len(offsets) - 1)))
    ok = ok and ordered
    print(f"  [{'PASS' if ordered else 'FAIL'}] marker order")

    boot_ok = "LUABOOT: launched LUA.E32" in text
    ok = ok and boot_ok
    print(f"  [{'PASS' if boot_ok else 'FAIL'}] LUABOOT service exec 268")

    bound915 = any("ord 915" in ln for ln in text.splitlines() if "bind" in ln)
    ok = ok and bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    exc_lines = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines and error is None
    ok = ok and clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc or PanicKernExec in log")
    for line in exc_lines[:10]:
        print("   ", line[:220])

    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        seen = set()
        for line in text.splitlines():
            low = line.lower()
            if ("lua" in low or "e32run" in low or "exc " in low) and line not in seen:
                print("   ", line[:220])
                seen.add(line)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

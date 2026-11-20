#!/usr/bin/env python3
"""r1_maze_test.py -- QEMU test for the MAZE generator/solver E32.

Stages build/user/MAZE.E32 (a PE2 image importing EUSER.DLL ordinal 915 =
RDebug::Printf) plus the genuine EUSER.DLL on a FAT32 disk, boots the R1
kernel, launches `maze` from the serial shell, drives `selftest` and `quit`
through prompt-synchronised PortCon input, then asserts the MZ markers:
startup banner, the nine deterministic grid rows, the solved '*' path, the
path-length marker, selftest OK and DONE.  A small Python reference mirrors the
C fixed-seed DFS/BFS so the exact expected rows and path length are known.
Also checks E32 validation, ordinal-915 binding and absence of guest faults.

Usage: python3 tools/r1_maze_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from collections import deque

ROOT = "/tmp/opencode"
DISK = ROOT + "/r1_maze.img"
FS = ROOT + "/r1_maze_fs.img"
LOG = ROOT + "/r1_maze.log"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
E32 = os.path.join(REPO, "build/user/MAZE.E32")
KERNEL = os.path.join(REPO, "build/kernfull.elf")
DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
       "euser.dll")

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE = SECTORS * 512
OFFSET = LBA * 512

EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")
PROMPT = b"MZ> "
ROW = re.compile(r"^MZ: [#.*]{9}$")
STAR_ROW = re.compile(r"^MZ: [#.*]*\*[#.*]*$")

# ------------------------------------------------ Python mirror of maze.c ---
N = 9
BASE_SEED = 0x12345678
CY = (-2, 0, 2, 0)
CX = (0, 2, 0, -2)
_rnd_state = 0


def rnd():
    global _rnd_state
    _rnd_state = (_rnd_state * 1103515245 + 12345) & 0xFFFFFFFF
    return (_rnd_state >> 16) & 0x7FFF


def reference_maze(seed):
    global _rnd_state
    maze = [[1] * N for _ in range(N)]
    _rnd_state = seed
    maze[1][1] = 0

    def carve(y, x):
        r = rnd()
        d0 = r % 4
        for k in range(4):
            d = (d0 + k) % 4
            ny, nx = y + CY[d], x + CX[d]
            if not (1 <= ny <= N - 2 and 1 <= nx <= N - 2):
                continue
            if maze[ny][nx] != 1:
                continue
            maze[(y + ny) // 2][(x + nx) // 2] = 0
            maze[ny][nx] = 0
            carve(ny, nx)

    carve(1, 1)
    maze[N - 2][N - 2] = 0
    return maze


def reference_solve(maze):
    q = deque([(1, 1)])
    prev = {(1, 1): None}
    while q:
        y, x = q.popleft()
        if (y, x) == (N - 2, N - 2):
            break
        for dy, dx in ((-1, 0), (1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if (0 <= ny < N and 0 <= nx < N and maze[ny][nx] == 0
                    and (ny, nx) not in prev):
                prev[(ny, nx)] = (y, x)
                q.append((ny, nx))
    if (N - 2, N - 2) not in prev:
        return []
    path = []
    cur = (N - 2, N - 2)
    while cur is not None:
        path.append(cur)
        cur = prev[cur]
    return path


_REF_MAZE = reference_maze(BASE_SEED)
SHOW_ROWS = ["MZ: " + "".join("#" if c else "." for c in row)
             for row in _REF_MAZE]
PATH_LEN = len(reference_solve(_REF_MAZE))

# Each marker goes through EUSER ord 915 *and* the DebugPrint fallback, so
# duplicates in the log are expected; only presence and first-seen order matter.
MARKERS = ["MZ: MAZE GENERATOR 9x9"] + SHOW_ROWS + [
    "MZ: SELFTEST begin",
    "MZ: path length=%d" % PATH_LEN,
    "MZ: SELFTEST OK",
    "MZ: DONE",
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
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
    for src, name in ((E32, "MAZE.E32"), (DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src} (build it first)")
        sh("mcopy", "-i", FS, src, f"::{name}", check=True)
    with open(FS, "rb") as f:
        data = f.read()
    if len(data) > FS_SIZE:
        sys.exit("FAT image larger than partition")
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
             capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for wanted in ("mazee32", "euserdll"):
        if wanted not in norm:
            sys.exit(f"mdir missing {wanted}:\n{out.stdout}")
    print("mdir OK: MAZE.E32 and EUSER.DLL staged")


def wait_for(log_path, marker, start=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log_path, "rb") as f:
            f.seek(start)
            data = f.read()
        if marker in data:
            return start + data.index(marker) + len(marker)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {marker!r} (see {log_path})")


def run_qemu(log_path):
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
        "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata",
        "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print("== booting QEMU, driving MAZE via serial console ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            # Asic::Init3 has already mounted the FAT partition at boot.
            pos = wait_for(log_path, b"C:/> ", timeout=60)

            def send(text, expect, timeout=30):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("maze", PROMPT, 60)       # launch + banner + maze board
            send("selftest", PROMPT)       # gen/show/solve/path length
            send("quit", b"MZ: DONE")      # exit the tool
            time.sleep(1.0)                # let the final marker flush
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

    try:
        run_qemu(args.log)
    except RuntimeError as exc:
        print("QEMU drive failed:", exc)
    with open(args.log, errors="replace") as f:
        text = f.read()

    print("reference path length:", PATH_LEN)
    ok = True
    for marker in MARKERS:
        good = marker in text
        ok = ok and good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")
    # Sequential search from a forward cursor: duplicate row strings must each
    # be found again after the previous marker, proving the real output order.
    cursor = 0
    ordered = True
    for marker in MARKERS:
        p = text.find(marker, cursor)
        if p < 0:
            ordered = False
            break
        cursor = p + len(marker)
    ok = ok and ordered
    print(f"  [{'PASS' if ordered else 'FAIL'}] marker order")

    rows = [ln.strip() for ln in text.splitlines() if ROW.match(ln.strip())]
    enough = len(rows) >= 9
    ok = ok and enough
    print(f"  [{'PASS' if enough else 'FAIL'}] rendered 9-char rows (>= 9): {len(rows)}")

    stars = [ln.strip() for ln in text.splitlines() if STAR_ROW.match(ln.strip())]
    has_path = len(stars) >= 1
    ok = ok and has_path
    print(f"  [{'PASS' if has_path else 'FAIL'}] solved '*' path rows: {len(stars)}")

    val = "e32run: validate r=0" in text
    ok = ok and val
    print(f"  [{'PASS' if val else 'FAIL'}] E32 validates (r=0)")

    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    bound915 = any("ord 915" in ln for ln in binds)
    ok = ok and bound915
    print(f"  [{'PASS' if bound915 else 'FAIL'}] EUSER ord 915 bound")

    exc_lines = [ln for ln in text.splitlines()
                 if any(k in ln for k in EXC_KEYS)]
    clean = not exc_lines
    ok = ok and clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT in log")
    for line in exc_lines:
        print("   ", line[:220])

    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        shown = set()
        for line in text.splitlines():
            if "mz:" in line.lower() and line not in shown:
                print("   ", line[:220])
                shown.add(line)
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

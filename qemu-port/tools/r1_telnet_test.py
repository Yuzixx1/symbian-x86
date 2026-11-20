#!/usr/bin/env python3
"""r1_telnet_test.py -- end-to-end TELNET-style interactive TCP client test.

Boots build/kernfull.elf with QEMU user-mode SLIRP networking, stages
TELNET.E32 (a PE2 E32 that does a genuine TCP 3-way handshake and then sends a
greeting line / polls for echoed text through the kernel<->userland e1000 frame
bridge) plus the real EUSER.DLL on a FAT32 disk, then drives the serial shell:

  * an initial short `netbridge` pump,
  * `telnet`       -- launches the client (posts SYN, waits on the RX ring),
  * `netbridge ...`-- the kernel poll loop drains TX via e1000_tx() and feeds
                      received frames (SYN-ACK / RST / echo) into RX.

The default target is the SLIRP gateway 10.0.2.2:80, where no listener may
exist; a graceful "TNET: connect timeout" outcome is therefore acceptable.
The test always requires the "TNET: connecting to ..." marker, a graceful
"TNET: DONE", the connected-path markers when a handshake did complete, and no
guest exception/panic.

Usage: python3 tools/r1_telnet_test.py [--kernel KERN] [--e32 E32] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/r1_telnet_test.img"
FS_IMG = "/tmp/opencode/r1_telnet_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_telnet_test.log"
E32_DEFAULT = "build/user/TELNET.E32"
EUSER_DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
              "euser.dll")
KERNEL_DEFAULT = "build/kernfull.elf"
MBR_LBA = 2048
PART_SECTORS = 16384
FS_SIZE = PART_SECTORS * 512

BOOT_MARKERS = [
    "e1000: BAR0=",                       # NIC discovered at boot
    "netbridge: base=",                   # bridge buffer exposed via execs
    "ord 915",                            # EUSER RDebug::Printf bound
    "telnet started (user process)",      # user E32 launched
    "netbridge: polling",                 # kernel poll loop running
]
REQUIRED_MARKERS = [
    "TNET: connecting to 10.0.2.2:80",
    "TNET: DONE",
]
CONNECTED_MARKERS = [
    "TNET: SYN sent",
    "TNET: SYN-ACK received",
    "TNET: connected",
    "TNET: ACK sent",
    "TNET: sent ",
]
TIMEOUT_MARKERS = [
    "TNET: connect timeout",
    "TNET: no data (timeout)",
]
DATA_MARKERS = [
    "TNET: recv ",
]
BAD_MARKERS = [
    "TNET: FAIL",
]
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(t):
            sys.exit(f"required tool missing: {t}")


def build_disk(e32):
    for p in (DISK_IMG, FS_IMG):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK_IMG, "wb") as f:
        f.truncate(32 * 1024 * 1024)
    with open(FS_IMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)
    # The shell's RunFromDisk tries the bare name first, then "<name>.E32".
    for src, name in ((e32, "telnet"), (e32, "TELNET.E32"),
                      (EUSER_DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src} (build it first)")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)
    with open(FS_IMG, "rb") as f:
        fat = f.read()
    with open(DISK_IMG, "r+b") as f:
        f.seek(MBR_LBA * 512)
        f.write(fat)
        entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                 + MBR_LBA.to_bytes(4, "little")
                 + PART_SECTORS.to_bytes(4, "little"))
        mbr = bytearray(512)
        mbr[446:462] = entry
        mbr[510] = 0x55
        mbr[511] = 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK_IMG}@@{MBR_LBA * 512}", "::",
             check=True, capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    assert "telnet" in norm and "euserdll" in norm, out.stdout


def run_qemu(kernel, log, target=None):
    qemu = [
        "timeout", "90", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    # The shell thread blocks inside `netbridge` (the poll loop), while the
    # E32 runs on its own thread. Short pump, launch, then a long pump that
    # drives the whole SYN/SYN-ACK/ACK/HELLO/data exchange (or its timeout).
    launch = "telnet" + ((" " + target) if target else "")
    feeder = ("sleep 1.5; printf '\\n'; "
              "sleep 2.0; printf 'netbridge 1500\\n'; "
              f"sleep 2.0; printf '{launch}\\n'; "
              "sleep 1.5; printf 'netbridge 40000\\n'; "
              "sleep 25.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text, connecting_marker=None, expect_connected=False):
    results = {}
    required = list(REQUIRED_MARKERS)
    if connecting_marker:
        required = [m for m in required if not m.startswith("TNET: connecting to ")]
        required.append(connecting_marker)
    for m in BOOT_MARKERS + required:
        results[m] = m in text
    for m in BAD_MARKERS:
        results["absent: " + m] = m not in text

    connected = "TNET: connected" in text
    # Either a real connection was established (require the full path) or the
    # handshake timed out gracefully (require the connect-timeout marker).
    if connected or expect_connected:
        for m in CONNECTED_MARKERS:
            results["connected: " + m] = m in text
        results["connected: recv or no-data"] = (
            any(m in text for m in DATA_MARKERS)
            or "TNET: no data (timeout)" in text
        )
    else:
        results["timeout path: TNET: connect timeout"] = (
            "TNET: connect timeout" in text
        )

    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    return results, exc, connected


def main():
    ap = argparse.ArgumentParser(description="R1 TELNET-style TCP client QEMU test")
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--e32", default=E32_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--target", default=None,
                    help='optional "host port" fed to the telnet prompt')
    ap.add_argument("--expect-connected", action="store_true",
                    help="require a real TCP handshake (use with --target)")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()
    connecting_marker = None
    if args.target:
        parts = args.target.split()
        if len(parts) == 2:
            connecting_marker = f"TNET: connecting to {parts[0]}:{parts[1]}"
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk(args.e32)
    text = ""
    for _attempt in range(2):                 # busy CI host: retry a cold boot
        text = run_qemu(args.kernel, args.log, args.target)
        res, _, _ = check(text, connecting_marker, args.expect_connected)
        if all(res.values()):
            break
    results, exc, connected = check(text, connecting_marker, args.expect_connected)
    print("== telnet test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print("state: " + ("REAL TCP CONNECTION ESTABLISHED" if connected
                       else "graceful connect timeout (no listener)"))
    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("TNET:", "netbridge", "telnet", "e1000",
                                     "fault", "panic", "Exc ")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

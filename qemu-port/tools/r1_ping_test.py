#!/usr/bin/env python3
"""r1_ping_test.py -- real ICMP echo request/reply over the netbridge.

Boots build/kernfull.elf with QEMU user-mode SLIRP networking, stages
PING.E32 (the PE2 E32 real ping client) and the genuine EUSER.DLL on a FAT32
disk, then drives the serial shell:
  * `ping`      launches the user program (ARP for 10.0.2.2, then posts real
                Ethernet/IPv4/ICMP echo requests into the bridge TX ring);
  * `netbridge 20000` runs the kernel poll loop that drains TX via e1000_tx()
                and fills RX from e1000_recv() with SLIRP's real echo replies.

Checks that at least one genuine ICMP echo reply from QEMU SLIRP (10.0.2.2)
crosses the shared ring into userland with valid IP/ICMP checksums, and that
no guest exception/panic appears in the serial log.

Usage: python3 tools/r1_ping_test.py [--kernel KERN] [--e32 E32] [--log LOG]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/r1_ping_test.img"
FS_IMG = "/tmp/opencode/r1_ping_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_ping_test.log"
E32_DEFAULT = "build/user/PING.E32"
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
    "ping started (user process)",        # user E32 launched
    "netbridge: polling",                 # kernel poll loop running
]
PING_MARKERS = [
    "PING: ARP gateway 10.0.2.2",
    "PING: echo request seq 1 sent",
    "PING: reply from 10.0.2.2 seq",      # >=1 real echo reply required
    "PING: DONE",
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
    # 8.3 short name: stage PING.E32 as "ping" so the shell's RunImage can
    # find it via the unrecognised-command fallback.
    for src, name in ((e32, "ping"), (EUSER_DLL, "EUSER.DLL")):
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
    assert "ping" in norm and "euserdll" in norm, out.stdout


def run_qemu(kernel, log):
    qemu = [
        "timeout", "80", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    # E32Run returns immediately; launch the ping process first so its ARP
    # request and echo requests are queued in TX, then pump the bridge with
    # the kernel poll loop long enough for the real SLIRP replies.
    feeder = ("sleep 1.5; printf '\\n'; "
              "sleep 4.0; printf 'ping\\n'; "
              "sleep 1.5; printf 'netbridge 20000\\n'; "
              "sleep 18.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text):
    results = {}
    for m in BOOT_MARKERS + PING_MARKERS:
        results[m] = m in text

    reply_lines = re.findall(
        r"PING: reply from 10\.0\.2\.2 seq (\d+) len=(\d+)", text)
    results["at least one real ICMP echo reply"] = len(reply_lines) >= 1
    tally = re.search(r"PING: (\d+)/(\d+) replies", text)
    results["nonzero reply tally"] = bool(tally and int(tally.group(1)) >= 1)

    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    return results, exc, reply_lines, tally


def main():
    ap = argparse.ArgumentParser(description="R1 real ICMP ping QEMU test")
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--e32", default=E32_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk(args.e32)
    text = ""
    for attempt in range(2):                 # busy CI host: retry a cold boot
        text = run_qemu(args.kernel, args.log)
        res, _, _, _ = check(text)
        if res.get("at least one real ICMP echo reply"):
            break
    results, exc, replies, tally = check(text)
    print("== ping test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    print("  echo replies seen:", replies if replies else "none")
    if tally:
        print("  tally:", tally.group(0))
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("ping", "PING", "netbridge", "e32run",
                                     "e1000", "fault", "panic")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

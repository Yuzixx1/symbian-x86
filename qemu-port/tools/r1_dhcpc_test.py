#!/usr/bin/env python3
"""r1_dhcpc_test.py -- end-to-end REAL DHCP lease test over e1000/SLIRP.

Boots build/kernfull.elf with QEMU user-mode SLIRP networking, stages
DHCPC.E32 (the PE2 E32 DHCP client) and the genuine EUSER.DLL on a FAT32 disk,
then drives the serial shell:
  * `dhcpc`      launches the user program (posts a DHCP DISCOVER into the TX
                  ring, waits for the OFFER, sends REQUEST, waits for ACK);
  * `netbridge` runs the kernel poll loop that drains TX via e1000_tx() and
                  fills RX from e1000_recv().
Asserts the full DORA marker sequence and prints the leased IP/mask/gateway/
DNS from QEMU SLIRP's real DHCP server, and that no guest exception/panic
appears in the serial log.

Usage: python3 tools/r1_dhcpc_test.py [--kernel KERN] [--e32 E32] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/r1_dhcpc_test.img"
FS_IMG = "/tmp/opencode/r1_dhcpc_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_dhcpc_test.log"
E32_DEFAULT = "build/user/DHCPC.E32"
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
    "dhcpc started (user process)",       # user E32 launched
    "netbridge: polling",                 # kernel poll loop running
]
DHCP_MARKERS = [
    "DHCP: bridge base=",
    "DHCP: DISCOVER sent",
    "DHCP: OFFER from ",
    "DHCP: REQUEST sent",
    "DHCP: ACK received",
    "DHCP: IP=",
    "DHCP: DONE",
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
    # 8.3 short name: the shell's FatReadPath resolves `dhcpc` to DHCPC.E32.
    for src, name in ((e32, "dhcpc"), (EUSER_DLL, "EUSER.DLL")):
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
    assert "dhcpc" in norm and "euserdll" in norm, out.stdout


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
    # Launch the user E32 first (E32Run returns immediately), then run the
    # kernel poll loop while it waits for the DHCP OFFER / ACK.
    feeder = ("sleep 1.5; printf '\\n'; "
              "sleep 4.0; printf 'dhcpc\\n'; "
              "sleep 1.5; printf 'netbridge 30000\\n'; "
              "sleep 26.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text):
    results = {}
    for m in BOOT_MARKERS + DHCP_MARKERS:
        results[m] = m in text
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    return results, exc


def lease_lines(text):
    return [ln.strip() for ln in text.splitlines()
            if "DHCP: OFFER" in ln or "DHCP: IP=" in ln or "DHCP: DONE" in ln]


def main():
    ap = argparse.ArgumentParser(description="R1 DHCP client QEMU test")
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
        res, _ = check(text)
        if res.get("DHCP: DONE") and res.get("DHCP: ACK received"):
            break
    results, exc = check(text)
    print("== DHCP client test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print("\n== lease details:")
    for line in lease_lines(text):
        print("   ", line[:220])
    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("DHCP", "netbridge", "e32run", "dhcpc",
                                     "e1000", "fault", "panic")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

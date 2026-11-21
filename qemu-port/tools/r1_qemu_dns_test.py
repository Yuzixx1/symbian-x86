#!/usr/bin/env python3
"""r1_qemu_dns_test.py -- end-to-end QEMU test for DNSRES.E32.

DNSRES.E32 is now the REAL RFC1035 DNS client (user/dnsnet.c), not the old
loopback-responder fake.  This test builds a 32 MB raw disk with a bootable
FAT32 partition, stages DNSRES.E32 and the genuine EUSER.DLL, boots
build/kernfull.elf with QEMU user-mode networking, and drives the serial shell:
  * `dnsres`        launches the client (ARP 10.0.2.2, then an A/IN query for
                    example.com posted into the netbridge TX ring);
  * `netbridge 25000` pumps the kernel<->userland e1000 frame bridge so the
                    query reaches SLIRP and the genuine reply lands in RX.

It checks that a real DNS reply (from SLIRP's 10.0.2.3 proxy) crossed the
bridge, that example.com resolves to a real public IP (NOT the fake hardcoded
93.184.216.34), and that no loopback-responder marker or guest fault appears.

Usage: python3 tools/r1_qemu_dns_test.py [--e32 E32] [--kernel KERN] [--log LOG]
"""
import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys

DISK_IMG = "/tmp/opencode/r1_dns_test.img"
FS_IMG = "/tmp/opencode/r1_dns_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_dns_test.log"
E32_DEFAULT = "build/user/DNSRES.E32"
EUSER_DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
             "euser.dll")
KERNEL_DEFAULT = "build/kernfull.elf"
MBR_LBA = 2048
PART_SECTORS = 16384
FS_SIZE = PART_SECTORS * 512

BOOT_MARKERS = [
    "BAR0=",                              # NIC discovered at boot (the leading
                                          # 'e' of e1000 is often eaten by serial)
    "netbridge: base=",                   # bridge buffer exposed via execs
    "ord 915",                            # EUSER RDebug::Printf bound
]
DNS_MARKERS = [
    "DNSNET: DNS client over netbridge start",
    "DNSNET: ARP request for gateway 10.0.2.2 posted",
    "DNSNET: query sent example.com",
    "DNSNET: reply from 10.0.2.3",        # REAL reply from SLIRP DNS proxy
    "DNSNET: example.com -> ",
    "DNSNET: DONE",
]
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF")
FAKE_KEYS = ("loopback responder", "93.184.216.34")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "timeout"):
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def build_disk(e32):
    for path in (DISK_IMG, FS_IMG):
        if os.path.exists(path):
            os.remove(path)
    with open(DISK_IMG, "wb") as f:
        f.truncate(32 * 1024 * 1024)
    with open(FS_IMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FS_IMG, check=True)
    # 8.3 short name: stage as "dnsres" so the shell's FatReadPath finds the
    # program under the familiar `dnsres` command name.
    for src, name in ((e32, "dnsres"), (EUSER_DLL, "EUSER.DLL")):
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
    assert "dnsres" in norm and "euserdll" in norm, out.stdout


def run_qemu(kernel, log):
    qemu = [
        "timeout", "110", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    # E32Run returns immediately; pump the bridge while the client waits.
    feeder = ("sleep 1.5; printf '\\n'; "
              "sleep 4.0; printf 'dnsres\\n'; "
              "sleep 1.5; printf 'netbridge 25000\\n'; "
              "sleep 30.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def resolved_ip(text):
    m = re.search(r"DNSNET: example\.com -> (\d+\.\d+\.\d+\.\d+)", text)
    return m.group(1) if m else None


def check(text):
    results = {}
    for m in BOOT_MARKERS + DNS_MARKERS:
        results[m] = m in text
    results["E32 validates (r=0)"] = "e32run: validate r=0" in text
    results["no timeout marker"] = "DNSNET: no reply (timeout)" not in text
    ip = resolved_ip(text)
    results["real public IP resolved"] = bool(
        ip and ip not in ("93.184.216.34", "127.0.0.1")
        and not ip.startswith("10.") and not ip.startswith("0."))
    results["no loopback/fake answer"] = not any(k in text for k in FAKE_KEYS)
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    return results, exc, ip


def main():
    ap = argparse.ArgumentParser(description="R1 DNS resolver QEMU test")
    ap.add_argument("--e32", default=E32_DEFAULT)
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk(args.e32)
    text = ""
    for attempt in range(2):                 # busy CI host: retry a cold boot
        text = run_qemu(args.kernel, args.log)
        res, _, _ = check(text)
        if res.get("DNSNET: reply from 10.0.2.3"):
            break
    results, exc, ip = check(text)
    print("== DNS test results")
    ok = True
    for name, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {name}")
    print(f"resolved example.com -> {ip}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])
    print(f"serial log: {args.log}")
    if not ok:
        for ln in text.splitlines():
            if any(k in ln.lower() for k in
                   ("dnsnet", "dnsres", "loopback", "93.184.216.34",
                    "e32run", "fault", "panic")):
                print("   ", ln[:200])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

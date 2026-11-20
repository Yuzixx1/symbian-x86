#!/usr/bin/env python3
"""r1_qemu_nic_test.py — end-to-end D-06 e1000 NIC bring-up test.

Boots the full Kern image with QEMU user-mode SLIRP networking, drives the
`nic` shell command over the serial console, and checks that:
  * e1000_init() runs at boot (BAR0/MAC/link banner),
  * e1000_link_up() reports link,
  * ARP requests sent with e1000_tx() get ARP replies back via e1000_recv()
    repeatedly (proves sustained TX/RX, not just one packet).

Usage: python3 tools/r1_qemu_nic_test.py [--kernel KERN] [--log LOG]
"""
import argparse
import shlex
import subprocess
import sys

DEFAULT_KERNEL = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"
DISK_IMG = "/tmp/opencode/r1_test.img"
LOG_DEFAULT = "/tmp/opencode/r1_qemu_nic_test.log"

BOOT_MARKERS = ["e1000: BAR0=", "MAC=", "link=1"]
NIC_MARKERS = ["nic: e1000 MAC=", "link=1", "nic arp: try 1 tx=0 (OK)",
               "nic arp: try 2 tx=0 (OK)", "nic arp: try 3 tx=0 (OK)",
               "RX reply", "nic arp: RESULT PASS (3/3 ARP replies)"]


def main():
    ap = argparse.ArgumentParser(description="D-06 e1000 NIC bring-up test")
    ap.add_argument("--kernel", default=DEFAULT_KERNEL)
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()

    qemu = [
        "timeout", "70", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-display", "none", "-serial", "stdio",
        "-kernel", args.kernel,
        "-drive", f"file={DISK_IMG},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    # Shell prompt appears ~4s into boot. Send the NIC test and let it finish.
    feeder = "sleep 4; printf '\\n'; sleep 10; printf 'nic arp 3\\n'; sleep 45"
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(args.log)} 2>&1"
    print("+", cmd)
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(args.log, "r", errors="replace") as f:
        text = f.read()

    print("== NIC test markers")
    all_ok = True
    for m in BOOT_MARKERS + NIC_MARKERS:
        ok = m in text
        all_ok = all_ok and ok
        print(f"  [{'PASS' if ok else 'FAIL'}] {m}")
    print(f"serial log: {args.log}")
    print("RESULT: PASS" if all_ok else "RESULT: FAIL")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())

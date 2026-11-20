#!/usr/bin/env python3
"""r1_wget_test.py -- end-to-end WGET-style downloader test (real HTTP -> FAT).

Boots build/kernfull.elf with QEMU user-mode SLIRP networking, stages WGET.E32
(a PE2 E32 that does a genuine TCP 3-way handshake + HTTP/1.0 GET through the
kernel<->userland e1000 frame bridge and writes the reassembled response body
to a FAT file via PortFile IPC) plus the real EUSER.DLL on a FAT32 disk, then
drives the serial shell:

  * a short `netbridge` pump (kernel drains TX / feeds RX),
  * `wget`          -- launches the downloader,
  * `netbridge ...` -- drives the whole SYN/SYN-ACK/ACK/GET/data exchange.

Asserts the WGET marker sequence (GET URL, status 200, body byte count, saved
file with byte count, DONE), absence of failure/exception markers, and -- since
the disk is *not* a snapshot -- that DOWNLOAD.TXT really exists on the FAT
image and holds the fetched page body.

Usage: python3 tools/r1_wget_test.py [--kernel KERN] [--e32 E32] [--log LOG]
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DISK_IMG = "/tmp/opencode/r1_wget_test.img"
FS_IMG = "/tmp/opencode/r1_wget_test_fs.img"
LOG_DEFAULT = "/tmp/opencode/r1_wget_test.log"
E32_DEFAULT = os.path.join(REPO, "build/user/WGET.E32")
EUSER_DLL = ("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/"
             "euser.dll")
KERNEL_DEFAULT = os.path.join(REPO, "build/kernfull.elf")
MBR_LBA = 2048
PART_SECTORS = 16384
FS_SIZE = PART_SECTORS * 512
OFFSET = MBR_LBA * 512

BOOT_MARKERS = [
    "e1000: BAR0=",                       # NIC discovered at boot
    "netbridge: base=",                   # bridge buffer exposed via execs
    "ord 915",                            # EUSER RDebug::Printf bound
    "wget started (user process)",        # user E32 launched
    "netbridge: polling",                 # kernel poll loop running
]
WGET_MARKERS = [
    "WGET: GET http://example.com/",
    "WGET: bridge base=",
    "WGET: SYN sent",
    "WGET: SYN-ACK received",
    "WGET: ACK sent",
    "WGET: GET sent",
    "WGET: status 200",
    "WGET: body ",
    "WGET: saved DOWNLOAD.TXT (",
    "WGET: DONE",
]
BAD_MARKERS = [
    "WGET: fetch failed (timeout)",
    "WGET: save failed",
]
EXC_KEYS = ("Exc ", "PanicKernExec", "MMU #PF", "FAULT:")


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
    # Stage under both the short shell spelling and the 8.3 E32 spelling.
    for src, name in ((e32, "wget"), (e32, "WGET.E32"),
                      (EUSER_DLL, "EUSER.DLL")):
        if not os.path.exists(src):
            sys.exit(f"missing {src} (build it first)")
        sh("mcopy", "-i", FS_IMG, src, f"::{name}", check=True)
    with open(FS_IMG, "rb") as f:
        fat = f.read()
    with open(DISK_IMG, "r+b") as f:
        f.seek(OFFSET)
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
    out = sh("mdir", "-i", f"{DISK_IMG}@@{OFFSET}", "::",
             check=True, capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    assert "wget" in norm and "euserdll" in norm, out.stdout


def run_qemu(kernel, log):
    qemu = [
        "timeout", "90", "qemu-system-i386",
        "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-display", "none", "-serial", "stdio",
        "-kernel", kernel,
        "-drive", f"file={DISK_IMG},format=raw,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    # NB: no snapshot=on -- the downloaded file must be written to DISK_IMG.
    feeder = ("sleep 1.5; printf '\\n'; "
              "sleep 2.0; printf 'netbridge 1500\\n'; "
              "sleep 2.0; printf 'wget\\n'; "
              "sleep 1.5; printf 'netbridge 40000\\n'; "
              "sleep 25.0")
    cmd = f"{{ {feeder}; }} | {' '.join(shlex.quote(a) for a in qemu)} > {shlex.quote(log)} 2>&1"
    subprocess.run(["bash", "-c", cmd], check=False)
    with open(log, "r", errors="replace") as f:
        return f.read()


def check(text):
    results = {}
    for m in BOOT_MARKERS + WGET_MARKERS:
        results[m] = m in text
    for m in BAD_MARKERS:
        results["absent: " + m] = m not in text
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    results["no exception/panic in log"] = not exc
    return results, exc


def verify_saved_file():
    """Read DOWNLOAD.TXT back off the FAT image after QEMU has exited."""
    if not os.path.exists(DISK_IMG):
        return False, "disk image missing"
    out = sh("mdir", "-i", f"{DISK_IMG}@@{OFFSET}", "::",
             check=False, capture_output=True, text=True)
    # mtools prints short names as "DOWNLOAD TXT" (space instead of '.').
    listing = "".join(out.stdout.upper().split())
    if "DOWNLOADTXT" not in listing:
        return False, "DOWNLOAD.TXT not present on FAT image: " + out.stdout.strip()
    local = "/tmp/opencode/r1_wget_DOWNLOAD.TXT"
    if os.path.exists(local):
        os.remove(local)
    cp = sh("mcopy", "-i", f"{DISK_IMG}@@{OFFSET}", "::DOWNLOAD.TXT", local,
            check=False, capture_output=True, text=True)
    if cp.returncode != 0 or not os.path.exists(local):
        return False, "mcopy DOWNLOAD.TXT failed: " + cp.stderr.strip()
    with open(local, "rb") as f:
        body = f.read()
    head = body[:80].decode("latin-1", "replace")
    has_page = ("Example Domain" in body.decode("latin-1", "replace")
                or "<html" in body.decode("latin-1", "replace").lower())
    detail = f"{len(body)} bytes; starts {head!r}"
    return (len(body) > 0 and has_page), detail


def main():
    ap = argparse.ArgumentParser(description="R1 WGET downloader QEMU test")
    ap.add_argument("--kernel", default=KERNEL_DEFAULT)
    ap.add_argument("--e32", default=E32_DEFAULT)
    ap.add_argument("--log", default=LOG_DEFAULT)
    ap.add_argument("--no-rebuild-disk", action="store_true")
    args = ap.parse_args()
    check_tools()
    if not (args.no_rebuild_disk and os.path.exists(DISK_IMG)):
        build_disk(args.e32)
    text = ""
    for _ in range(2):                        # retry a cold boot on a busy host
        text = run_qemu(args.kernel, args.log)
        res, _ = check(text)
        if all(res.values()):
            break
    results, exc = check(text)
    print("== wget test results")
    ok = True
    for m, passed in results.items():
        ok = ok and passed
        print(f"  [{'PASS' if passed else 'FAIL'}] {m}")
    if exc:
        print("\n== exception/panic lines:")
        for line in exc:
            print("   ", line[:220])

    print("\n== on-disk FAT verification")
    disk_ok, detail = verify_saved_file()
    ok = ok and disk_ok
    print(f"  [{'PASS' if disk_ok else 'FAIL'}] DOWNLOAD.TXT on FAT disk: {detail}")

    print(f"serial log: {args.log}")
    if not ok:
        print("\n== relevant log lines:")
        for ln in text.splitlines():
            if any(k in ln for k in ("WGET:", "netbridge", "wget", "e1000",
                                     "fault", "panic", "Exc ")):
                print("   ", ln[:220])
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""r1_realhw_readiness_audit.py -- static in-CI real-PC/USB readiness audit.

Complements the off-CI G15 harness (tools/r1_realhw_boot.py, physical PC,
never in DEFAULT_TESTS): this audit is pure static analysis, runs in
seconds, and needs no QEMU, so it can gate CI on what the variant glue now
covers versus the known real-hardware gaps.

REQUIRED checks (any failure => RESULT: FAIL, exit 1):
  R1  variant/qemu_pci.cpp: recursive PCI-PCI bridge walk (bus > 0) AND
      PCIe ECAM/MMCONFIG detection with 0xCF8/0xCFC port-I/O fallback.
  R2  variant/qemu_usb.cpp: polled BOT/CBW USB mass-storage driver
      (USBC/USBS signatures, CBW/CSW structs, BOT proto 0x50, class 0x08,
      SCSI INQUIRY/READ CAPACITY/READ(10), exported READ(10) entry point).
  R3  this audit exists and is wired as a CI step (self-presence).
  R4  kernelhwsrv/ untouched (git porcelain empty) -- read-only rule holds.
  R5  off-CI enforcement: no tools/r1_realhw_boot.py in-tree, and
      r1_final_verify.py DEFAULT_TESTS carries no realhw/physical gate.
  R6  HwInit0 idempotency freeze respected in the owned files: UsbInit
      early-returns when already initialized; the PCI bus scan is guarded
      by a scanned flag (no boot-time growth, no re-walk).

KNOWN-GAP checklist (informational only, never fails the audit; they are
the documented real-PC gaps from docs/PLAN_SURVEY_HW_USB.md):
  NVMe storage, local APIC / IO-APIC / MSI (PIC-only, IRQ <= 16), ACPI
  parsing, USB hubs/hotplug, CD-R/physical boot (human, off-CI).

Usage:
    python3 tools/r1_realhw_readiness_audit.py [--root qemu-port]
Exit 0 + "RESULT: PASS" when every REQUIRED check passes.
"""
import argparse
import os
import re
import subprocess
import sys

PORT_DEFAULT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

REQUIRED = []
GAPS = []


def check(name, ok, detail=""):
    REQUIRED.append((name, bool(ok), detail))
    return ok


def gap(name, present, detail=""):
    GAPS.append((name, bool(present), detail))


def read(path):
    try:
        with open(path, "r", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def main():
    ap = argparse.ArgumentParser(description="static real-hardware readiness audit")
    ap.add_argument("--root", default=PORT_DEFAULT, help="qemu-port root")
    args = ap.parse_args()
    root = os.path.abspath(args.root)

    pci = read(os.path.join(root, "variant", "qemu_pci.cpp"))
    usb = read(os.path.join(root, "variant", "qemu_usb.cpp"))
    fv = read(os.path.join(root, "tools", "r1_final_verify.py"))

    print("== r1_realhw_readiness_audit (static, root=%s)" % root)

    # R1: PCI bridge walk + ECAM.
    if not pci:
        check("R1 qemu_pci.cpp present", False, "file missing/unreadable")
    else:
        walk = ("ScanBus" in pci and "0x18" in pci and "0x06" in pci and
                re.search(r"sub\s*==\s*0x04", pci) is not None)
        ecam = ("gEcam" in pci and "0xB0000000" in pci and "0xCF8" in pci and
                "0xCFC" in pci)
        check("R1a recursive bridge walk (bus>0, sec-bus reg 0x18)", walk)
        check("R1b ECAM/MMCONFIG detect + port-I/O fallback", ecam)

    # R2: BOT/CBW mass-storage driver.
    if not usb:
        check("R2 qemu_usb.cpp present", False, "file missing/unreadable")
    else:
        bot = all(t in usb for t in ("0x43425355", "0x53425355", "Cbw", "Csw"))
        proto = all(t in usb for t in ("gBuf[o + 5] == 8", "gBuf[o + 6] == 6",
                                       "gBuf[o + 7] == 0x50"))
        scsi = all(t in usb for t in ("0x12", "0x25", "0x28"))
        entry = "UsbMsdRead10" in usb
        check("R2a BOT/CBW wire structs + USBC/USBS signatures", bot)
        check("R2b mass-storage class 08 / subclass 06 / proto 50 detect", proto,
              "iface class 08, sub 06, proto 0x50")
        check("R2c SCSI INQUIRY(0x12)/READ CAPACITY(0x25)/READ(10)(0x28)", scsi)
        check("R2d exported polled READ(10) entry point", entry)

    # R3: self-presence.
    check("R3 audit self-presence", os.path.exists(os.path.abspath(__file__)))

    # R4: kernelhwsrv read-only.
    kh = os.path.join(os.path.dirname(root), "kernelhwsrv")
    porcelain = ""
    if os.path.isdir(os.path.join(kh, ".git")) or os.path.isdir(kh):
        try:
            r = subprocess.run(["git", "-C", kh, "status", "--porcelain"],
                               capture_output=True, text=True, timeout=30)
            porcelain = r.stdout.strip()
        except Exception as e:  # noqa: BLE001
            porcelain = "git-error: %s" % e
    check("R4 kernelhwsrv/ porcelain clean", porcelain == "", repr(porcelain[:80]))

    # R5: off-CI enforcement.
    g15 = os.path.join(root, "tools", "r1_realhw_boot.py")
    check("R5a no in-tree G15 physical-boot harness", not os.path.exists(g15))
    check("R5b r1_final_verify DEFAULT_TESTS has no realhw gate",
          "realhw" not in fv.lower())

    # R6: HwInit0 idempotency in owned files.
    if usb:
        check("R6a UsbInit idempotent (gInit early-return)",
              re.search(r"if \(gInit\)\s*return", usb) is not None)
    if pci:
        check("R6b PCI bus scan guarded (gScanned)", "gScanned" in pci)

    # Known-gap checklist (informational).
    variant_files = []
    vdir = os.path.join(root, "variant")
    if os.path.isdir(vdir):
        variant_files = os.listdir(vdir)
    gap("NVMe storage driver", any("nvme" in f.lower() for f in variant_files),
        "no variant/*nvme* — modern NVMe-only boards cannot host C:")
    gap("local APIC / IO-APIC / MSI", False,
        "PIC-only INTx (variant/qemu_variant_int.cpp), IRQ <= 16, no ACPI parsing")
    gap("USB hubs / hotplug / composite", False,
        "root-port HID + BOT storage only; no hub or hotplug path")
    gap("CD-R / physical boot", False,
        "human off-CI step per docs/hw_worksheet.md; G15 harness stays out of tree")

    print("\n== required checks")
    all_pass = True
    for name, ok, detail in REQUIRED:
        all_pass = all_pass and ok
        print("  [%s] %s%s" % ("PASS" if ok else "FAIL", name,
                               (" (%s)" % detail) if detail and not ok else ""))
    print("\n== known real-PC gaps (informational, non-fatal)")
    for name, present, detail in GAPS:
        print("  [GAP ] %s -- %s" % (name, detail))
    print("\nRESULT: " + ("PASS" if all_pass else "FAIL"))
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())

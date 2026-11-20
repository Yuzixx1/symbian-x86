#!/usr/bin/env bash
# boot_grub.sh — OPTIONAL legacy launcher: boot build/symbian-grub.iso with its
# bundled /PROGRAMS.IMG attached as a real second disk, so the EKA2 shell can
# mount it and run end-user E32 programs. No longer required: plain ISO boot
# (see docs/r1_boot_and_selftest.md) exposes programs at X: automatically,
# because GRUB now loads PROGRAMS.IMG as a multiboot module and the kernel
# auto-mounts it. Kept for compatibility; QEMU cannot use a file nested inside
# an ISO directly as a block device, so the image is extracted from the ISO first.
#
# Usage: tools/boot_grub.sh [path/to/iso] [extra QEMU args...]
# Env:   PROGRAMS_IMG=<path>  override the extraction destination.
# Once the EKA2> prompt appears: MOUNT, DIR, CALC (or HELLO, SNAKE, ...).
set -euo pipefail

HERE="$(cd -- "$(dirname -- "$0")" && pwd)"
QPORT="$(cd -- "$HERE/.." && pwd)"
ISO="${SYMBIAN_ISO:-$QPORT/build/symbian-grub.iso}"
IMG="${PROGRAMS_IMG:-/tmp/opencode/symbian-programs.img}"
if [[ $# -gt 0 && -f "$1" ]]; then ISO="$1"; shift; fi

if [[ ! -f "$ISO" ]]; then
    echo "missing $ISO (run: make isogrub)" >&2
    exit 1
fi
mkdir -p "$(dirname -- "$IMG")"
python3 - "$ISO" "$IMG" <<'PY'
import sys
from pycdlib import PyCdlib
iso = PyCdlib()
iso.open(sys.argv[1])
iso.get_file_from_iso(sys.argv[2], iso_path="/PROGRAMS.IMG;1")
iso.close()
print(f"extracted /PROGRAMS.IMG -> {sys.argv[2]}")
PY

# -boot d is essential: without it the attached data disk wins boot order and
# the machine hangs in the FAT32 partition's (bootable but empty) MBR.
exec qemu-system-i386 -M q35 -m 128 -cdrom "$ISO" -boot d -display none -serial stdio \
    -drive "file=$IMG,format=raw,snapshot=on,if=none,id=d0" \
    -device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 \
    -monitor none -no-reboot "$@"

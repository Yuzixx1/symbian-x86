#!/usr/bin/env python3
"""Set the ProtServ capability bit in an elf2e32-produced E32 header.

elf2e32.py emits a zero capability set; genuine Symbian file servers need
ECapabilityProtServ (bit 8 of caps0) to create the protected '!FileServer'
name (ExecHandler::ServerCreateWithOptions, kernel/eka/kernel/sipc.cpp).
Recomputes the Symbian header CRC after patching.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent.parent / "tools"))
from mke32 import crc32_symbian, KImageCrcInitialiser

CAPS0_OFF = 136   # E32ImageHeaderV::iS at 132: SID@132 VID@136? see below
CRC_OFF = 20


def patch(path):
    blob = bytearray(Path(path).read_bytes())
    hdr_size = struct.unpack_from("<I", blob, 100)[0]  # iCodeOffset
    if hdr_size != 156:
        raise SystemExit(f"unexpected header size {hdr_size}")
    # This packer's E32ImageHeaderV: 124 uncompressedSize, 128 SID, 132 VID,
    # 136 caps[0], 140 caps[1].
    caps_off = 136
    caps = struct.unpack_from("<I", blob, caps_off)[0]
    struct.pack_into("<I", blob, caps_off, caps | (1 << 8))  # ECapabilityProtServ
    struct.pack_into("<I", blob, CRC_OFF, KImageCrcInitialiser)
    struct.pack_into("<I", blob, CRC_OFF, crc32_symbian(bytes(blob[:156]), 0))
    Path(path).write_bytes(blob)
    print(f"{path}: caps0 {caps:#x} -> {caps | (1<<8):#x}, header CRC recomputed")


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        patch(arg)

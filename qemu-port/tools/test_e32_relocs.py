#!/usr/bin/env python3
"""Host-side positive/negative relocation-section checks for E32 images."""
import os
import struct
import subprocess
import sys

from mke32 import build_datarel_e32, crc32_symbian, KImageCrcInitialiser

OUT = "/tmp/opencode"


def recalc_header(blob):
    struct.pack_into("<I", blob, 20, KImageCrcInitialiser)
    struct.pack_into("<I", blob, 20, crc32_symbian(bytes(blob[:156])))


def valid(blob):
    name = os.path.join(OUT, "reloc-validate.e32")
    with open(name, "wb") as dst:
        dst.write(blob)
    return subprocess.run([sys.executable, "tools/e32read.py", name, "--quiet"],
                          stdout=subprocess.DEVNULL).returncode == 0


def main():
    os.makedirs(OUT, exist_ok=True)
    good = build_datarel_e32(0x10000001, 0x10000002, 0x10000002)
    assert valid(good)
    offset = struct.unpack_from("<I", good, 116)[0]
    cases = {}

    bad = bytearray(good)
    struct.pack_into("<I", bad, offset + 16, 0x2010)  # outside 16B data
    cases["outside initialized data"] = bad

    bad = bytearray(good)
    struct.pack_into("<I", bad, offset + 4, 2)  # one real fixup only
    cases["wrong relocation count"] = bad

    bad = bytearray(good)
    struct.pack_into("<I", bad, offset + 16, 0x4008)  # unsupported type
    cases["invalid relocation type"] = bad

    bad = bytearray(good)
    struct.pack_into("<I", bad, 116, 0)    # no relocations, but data exists
    # This is a valid image: absence of relocations is fine.
    recalc_header(bad)
    assert valid(bad)

    bad = bytearray(good)
    struct.pack_into("<I", bad, 104, 0)    # no data to relocate
    recalc_header(bad)
    cases["data relocation without data"] = bad

    for description, image in cases.items():
        if valid(image):
            raise AssertionError(f"accepted: {description}")
        print("rejected:", description)
    print("valid data relocations accepted")


if __name__ == "__main__":
    main()

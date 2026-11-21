#!/usr/bin/env python3
"""Pack the Lua E32 with a higher PE2 IAT offset.

The shared tools/elf2e32.py hard-codes IAT_OFF = 0x8000 (32 KiB of code).
A full Lua 5.4 VM plus parser and libraries needs ~124 KiB of text, so this
wrapper reuses the *same* vetted packer with IAT_OFF = 0x20000.  The R1 loader
reads the IAT offset from the image header (e32load.cpp: slot = textSize + 4k)
and accepts any in-range value; e32read.py validates the same way.  The emitted
image is still a genuine PE2 E32 importing EUSER.DLL ordinal 915.
"""
import os
import sys
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import elf2e32  # noqa: E402

IAT_OFF = 0x20000


def main():
    if len(sys.argv) != 4 or sys.argv[3] != "--euser-import-915":
        sys.exit("usage: pack_lua_e32.py INPUT OUTPUT --euser-import-915")
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    # pack() binds iat_off as a *default argument* at def time, so mutating
    # elf2e32.IAT_OFF here has no effect -- pass it explicitly instead.
    result = elf2e32.pack(src.read_bytes(), 915, iat_off=IAT_OFF)
    dst.write_bytes(result)
    print(f"wrote {dst} ({len(result)} bytes, PE2 EUSER ord 915 IAT @ {IAT_OFF:#x})")


if __name__ == "__main__":
    main()

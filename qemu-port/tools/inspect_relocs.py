#!/usr/bin/env python3
"""Dump code relocation bytes of an E32 image."""
import struct, sys

path = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv"
d = open(path, "rb").read()
creloc = struct.unpack_from("<I", d, 112)[0]
code_size = struct.unpack_from("<i", d, 48)[0]
print("codeRelocOff:", creloc, "codeSize:", code_size, "fileSize:", len(d))
if not creloc:
    print("no code relocs")
    sys.exit(0)
r = d[creloc:]
print("reloc bytes (%d):" % len(r), " ".join("%02x" % b for b in r[:80]))
# Try interpret as sequence of (page:u16, block:u16) pairs then u16 entry offsets
print("as u16 words:", [struct.unpack_from("<H", r, 2*i)[0]
                        for i in range(min(len(r)//2, 40))])

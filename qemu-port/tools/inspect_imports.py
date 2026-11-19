#!/usr/bin/env python3
"""Inspect the import section of an E32 EXE to determine DLL ordinals."""
import struct, sys

path = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv"
d = open(path, "rb").read()
flags = struct.unpack_from("<I", d, 44)[0]
imp = struct.unpack_from("<I", d, 108)[0]
print("file size:", len(d))
print("flags: %08x  PE2=%s" % (flags, bool(flags & 0x20000000)))
print("dllRefs:", struct.unpack_from("<i", d, 84)[0])
print("impOff:", imp)
print("textSize:", struct.unpack_from("<i", d, 96)[0],
      "codeSize:", struct.unpack_from("<i", d, 48)[0])
if not imp:
    print("no imports")
    sys.exit(0)
isz = struct.unpack_from("<I", d, imp)[0]
sec = imp + 4
name_off = struct.unpack_from("<I", d, sec)[0]
n = struct.unpack_from("<i", d, sec + 4)[0]
print("import section size:", isz, " nameOff:", name_off, " n:", n)
nm = d[imp + name_off:imp + name_off + 80]
print("dll name bytes:", nm[:48])
print("sec[8:48] hex:", " ".join("%02x" % b for b in d[sec+8:sec+48]))
# Non-PE2: slot list at sec+8, each 4 bytes. Ordinals are positional (1..n).
print("slot words:", [struct.unpack_from("<I", d, sec + 8 + 4*k)[0]
                      for k in range(min(n, 8))])

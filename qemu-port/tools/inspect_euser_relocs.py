#!/usr/bin/env python3
"""Inspect EUSER.DLL code relocation section."""
import struct

p = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
d = open(p, "rb").read()
rel = struct.unpack_from("<I", d, 112)[0]
code_size = struct.unpack_from("<i", d, 48)[0]
print("relocOff:", rel, "codeSize:", code_size, "fileSize:", len(d))
size = struct.unpack_from("<I", d, rel)[0]
print("reloc section size field:", size, "bytes available:", len(d)-rel)
data = d[rel:]
p2 = rel + 8
end = rel + min(size, len(d)-rel)
blocks = 0
maxoff = 0
while p2 + 8 <= end:
    page, bs = struct.unpack_from("<II", d, p2)
    print("block @%x: page=%08x bs=%08x (page&fff=%x)" % (p2, page, bs, page & 0xfff))
    if (page & 0xfff) or bs < 8 or (bs & 3):
        print("  -> invalid block, stop")
        break
    if p2 + bs > end:
        bs = end - p2
    for q in range(p2+8, p2+bs, 2):
        if q+2 > len(d): break
        e = struct.unpack_from("<H", d, q)[0]
        if e:
            off = page + (e & 0xfff)
            if off > maxoff: maxoff = off
    blocks += 1
    p2 += bs
print("blocks:", blocks, "max offset written:", hex(maxoff), "vs codeSize:", hex(code_size))

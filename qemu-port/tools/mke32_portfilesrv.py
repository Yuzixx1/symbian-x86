#!/usr/bin/env python3
"""mke32_portfilesrv.py -- rebuild the flat, reloc-fixed PE2 portfilesrv E32.

Same class of builder as mke32_ca_test.py, but for portfilesrv: it flattens the
genuine portfilesrv.exe PE image (.text/.rdata/.idata at their real RVAs based
at 0x400000) into a single E32 code section and emits genuine E32 page/block
relocations for *every* absolute dword in [codeBase, codeBase+codeSize).

This is the fix for the "Exc 06, EIP=0x203" class of fault: when svcmgr launches
portfilesrv as a second user process, the loader copies it to a different run
address.  Absolute operands linked for 0x400000 (0x402000, 0x40303c, 0x403040,
...) must be fixed up by the reloc section; a stale/absent reloc section leaves
them pointing at the linked base, and a `call/jmp DWORD PTR [abs]` then loads a
garbage EIP.  Running directly from the shell happened to reuse the first run
address, masking the bug.

Entry is the PE .text RVA (0x1000), not 0 -- the flattened code begins with the
real _start at RVA 0x1000.

Usage:
    python3 tools/mke32_portfilesrv.py [--output PATH]
"""
import os
import struct
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mke32 import crc32_symbian, KImageCrcInitialiser

SRC_PE = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-build-v1/bin/portfilesrv.exe"
OLD_E32 = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv"
DEFAULT_OUT = ("/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/"
               "output/portfilesrv")

CODEBASE = 0x400000
CODEOFF = 156             # NoHoles V-header size
ENT_RVA = 0x1000          # PE .text entry RVA
TEXTSIZE = 0x303C         # PE2 IAT slots at 0x40303c/40/44
ORDS = (1221, 1953, 1)    # _memcpy, FsRegisterThread, User::Exit


def rd32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def build():
    with open(SRC_PE, "rb") as f:
        pe = f.read()
    assert pe[:2] == b"MZ", "not MZ"
    peoff = rd32(pe, 0x3C)
    assert pe[peoff:peoff + 4] == b"PE\0\0", "no PE signature"
    mach, nsec, ts, psym, nsym, optsz, chars = struct.unpack_from(
        "<HHIIIHH", pe, peoff + 4)
    opt = peoff + 24
    assert struct.unpack_from("<H", pe, opt)[0] == 0x10B, "not PE32"
    imagebase = rd32(pe, opt + 28)
    assert imagebase == CODEBASE, "unexpected PE image base"
    secoff = opt + optsz

    sections = []
    for i in range(nsec):
        o = secoff + 40 * i
        name = pe[o:o + 8].rstrip(b"\0").decode()
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", pe, o + 8)
        sections.append((name, vaddr, vsize, rawsize,
                         pe[rawptr:rawptr + rawsize]))

    codesize = max(v + sz for _, v, sz, _, _ in sections)
    image = bytearray(codesize)
    for _name, vaddr, vsize, rawsize, raw in sections:
        n = min(vsize, rawsize)
        image[vaddr:vaddr + n] = raw[:n]

    # Every absolute dword in the code span gets a TEXT relocation.  Scanning
    # every byte offset catches unaligned immediates (mov eax, imm32 etc.).
    reloc_sites = []
    for o in range(codesize - 3):
        if CODEBASE <= struct.unpack_from("<I", image, o)[0] < CODEBASE + codesize:
            reloc_sites.append(o)

    # PE2 IAT: slots live at textSize+4*k; the code reads them via absolute
    # addresses (0x40303c/40/44), so those sites are in reloc_sites already.
    for k, ordinal in enumerate(ORDS):
        struct.pack_into("<I", image, TEXTSIZE + 4 * k, ordinal)
    assert [rd32(image, TEXTSIZE + 4 * k) for k in range(len(ORDS))] == list(ORDS)

    # Patch the real entry epilogue's `ret` into User::Exit(0) via IAT slot 2.
    exit_seq = b"\x6a\x00\xb8" + struct.pack("<I", CODEBASE + TEXTSIZE + 8) + b"\xff\x10"
    entry_end = ENT_RVA + 0x47
    image[entry_end:entry_end + len(exit_seq)] = exit_seq
    imm_site = entry_end + 3
    if imm_site not in reloc_sites:
        reloc_sites.append(imm_site)
    reloc_sites = sorted(set(reloc_sites))

    # E32 page/block relocations: page is section-relative; entries are
    # 0x1000 (TEXT) | offset-within-page; pad with a zero entry for 4B align.
    pages = defaultdict(list)
    for o in reloc_sites:
        pages[o & ~0xFFF].append(o & 0xFFF)
    blocks = bytearray()
    for page in sorted(pages):
        offs = sorted(set(pages[page]))
        ents = [0x1000 | off for off in offs]
        if len(ents) % 2:
            ents.append(0)
        blocks += struct.pack("<2I", page, 8 + 2 * len(ents))
        blocks += b"".join(struct.pack("<H", e) for e in ents)
    reloc_size = 8 + len(blocks)
    reloc_count = len(reloc_sites)

    # PE2 import section: DLL name only; IAT ordinals are in the code section.
    dll_name = b"euser{000a0000}[100039e5].dll\0"
    isize = 8 + ((len(dll_name) + 3) & ~3)
    imp_sec = struct.pack("<II", 12, len(ORDS)) + dll_name
    imp_sec += b"\0" * (isize - len(imp_sec))
    impoff = CODEOFF + codesize
    relocoff = impoff + 4 + isize

    with open(OLD_E32, "rb") as f:
        orig = f.read()
    header = bytearray(CODEOFF)
    struct.pack_into("<4I", header, 0, *(rd32(orig, o) for o in (0, 4, 8, 12)))
    struct.pack_into("<I", header, 16, 0x434F5045)       # 'EPOC'
    struct.pack_into("<I", header, 20, KImageCrcInitialiser)
    struct.pack_into("<I", header, 24, 0)                # module version
    struct.pack_into("<I", header, 28, 0)                # uncompressed
    struct.pack_into("<I", header, 44, 0x22000000)       # V header + PE2 imports
    struct.pack_into("<6i", header, 48, codesize, 0,
                     struct.unpack_from("<i", orig, 56)[0],
                     struct.unpack_from("<i", orig, 60)[0],
                     struct.unpack_from("<i", orig, 64)[0], 4096)
    struct.pack_into("<I", header, 72, ENT_RVA)          # entry: real _start
    struct.pack_into("<2I", header, 76, CODEBASE, 0)
    struct.pack_into("<i", header, 84, 1)                # one DLL ref (EUSER)
    struct.pack_into("<I", header, 88, 0)
    struct.pack_into("<i", header, 92, 0)
    struct.pack_into("<i", header, 96, TEXTSIZE)
    struct.pack_into("<5I", header, 100, CODEOFF, 0, impoff, relocoff, 0)
    struct.pack_into("<2H", header, 120,
                     struct.unpack_from("<H", orig, 120)[0],
                     struct.unpack_from("<H", orig, 122)[0])
    struct.pack_into("<I", header, 20, crc32_symbian(bytes(header), 0))

    data = (bytes(header) + bytes(image) + struct.pack("<I", isize) + imp_sec
            + struct.pack("<2I", reloc_size, reloc_count) + bytes(blocks))
    assert len(data) == relocoff + reloc_size
    return data, codesize, reloc_sites, relocoff


if __name__ == "__main__":
    out = DEFAULT_OUT
    for i, a in enumerate(sys.argv):
        if a == "--output" and i + 1 < len(sys.argv):
            out = sys.argv[i + 1]
    data, csize, sites, roff = build()
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(data)
    print("WROTE %s (%d bytes)" % (out, len(data)))
    print("  codeSize=%#x entry=%#x textSize=%#x relocs=%d @%#x" %
          (csize, ENT_RVA, TEXTSIZE, len(sites), roff))
    print("  reloc sites: %s" % " ".join("%#x" % s for s in sites))

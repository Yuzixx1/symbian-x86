#!/usr/bin/env python3
"""mke32_netstack_elf.py -- pack netstack.elf into NETSTACK.E32.

Self-contained ELF32/i386 -> PE2-import E32 packer.  The .iat output section
from user/link_netstack.ld is appended to .text; its 1-based ordinal (915,
RDebug::Printf) is bound by the E32Run loader before the process resumes.
Import-format bits are cleared before PE2 is set so the E32 validator accepts
flags=0x22000000 rather than an invalid 0x30000000 impFmt.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mke32 import (build_args_e32, crc32_symbian, KImageCrcInitialiser,
                   KImageHdrFmt_V, EPOC_SIG, ECpuX86,
                   KExecutableImageUidValue, uid_checksum)

IAT_SECTION = ".iat"
DLL_NAME = b"euser{000a0000}[100039e5].dll\0"
PE2 = 0x20000000
HEADER_SIZE = 156


def pack(blob):
    if blob[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("expected little-endian ELF32")
    hdr = struct.unpack_from("<HHIIIIIHHHHHH", blob, 16)
    kind, machine, _, entry, _, shoff, _, _, _, _, shsize, shnum, shstr = hdr
    if kind != 2 or machine != 3 or shsize != 40:
        raise ValueError("expected linked i386 ELF with section headers")
    sections = [struct.unpack_from("<10I", blob, shoff + i * shsize)
                for i in range(shnum)]

    def contents(s):
        data = blob[s[4]:s[4] + s[5]]
        if len(data) != s[5]:
            raise ValueError("truncated section")
        return data

    names = contents(sections[shstr])
    named = {names[s[0]:].split(b"\0", 1)[0].decode(): (i, s)
             for i, s in enumerate(sections)}
    ti, text = named[".text"]
    ii, iat = named[IAT_SECTION]
    di, data = named.get(".data", (-1, (0, 1, 3, 0x500000, 0, 0, 0, 0, 1, 0)))
    bi, bss = named.get(".bss", (-1, (0, 8, 3, data[3] + data[5], 0, 0, 0, 0, 1, 0)))
    if text[3] != 0x400000 or data[3] != 0x500000 or not text[3] <= entry < text[3] + text[5]:
        raise ValueError("unexpected linked bases or entry point")
    if iat[3] != text[3] + text[5]:
        raise ValueError(".iat must immediately follow .text")
    if not iat[5] or iat[5] % 4 or iat[5] // 4 > 16:
        raise ValueError(".iat must hold 1..16 32-bit ordinals")
    text_size = text[5]
    nimp = iat[5] // 4
    for ordinal in struct.unpack("<" + "I" * nimp, contents(iat)):
        if not 1 <= ordinal <= 65535:
            raise ValueError(f"bad PE2 ordinal {ordinal}")
    for i, s in enumerate(sections):
        if s[2] & 2 and s[5] and i not in (ti, di, bi, ii):
            raise ValueError("unsupported allocated section")
    bss_size = max(0, bss[3] + bss[5] - (data[3] + data[5])) if bss[5] else 0

    regions = {ti: 1, di: 2, bi: 2, ii: 1}
    relocations = {ti: [], di: [], ii: []}
    for s in sections:
        if s[1] != 9 or s[7] not in relocations:
            continue
        target = sections[s[7]]
        symbols = contents(sections[s[6]])
        raw = contents(s)
        if len(raw) % 8:
            raise ValueError("malformed ELF relocation section")
        for pos in range(0, len(raw), 8):
            address, info = struct.unpack_from("<II", raw, pos)
            symbol = struct.unpack_from("<IIIBBH", symbols, (info >> 8) * 16)
            region = regions.get(symbol[5])
            typ = info & 255
            offset = address - target[3]
            if not 0 <= offset <= target[5] - 4:
                raise ValueError("relocation outside target section")
            if typ == 2 and region == regions[s[7]]:
                continue
            if typ != 1 or region is None:
                raise ValueError(f"unsupported relocation type/target: {typ}/{symbol[5]}")
            relocations[s[7]].append((offset, region))
    for offset, region in relocations[ii]:
        relocations[ti].append((text_size + offset, region))

    def reloc_section(entries):
        if not entries:
            return b""
        pages = {}
        for offset, region in sorted(entries):
            pages.setdefault(offset & ~4095, []).append((region << 12) | (offset & 4095))
        body = bytearray()
        for page, words in pages.items():
            if len(words) & 1:
                words.append(0)
            body += struct.pack("<II", page, 8 + len(words) * 2)
            body += struct.pack("<" + "H" * len(words), *words)
        return struct.pack("<II", 8 + len(body), len(entries)) + body

    header = bytearray(build_args_e32(0x10000001, 0x10000002, 0x10000002)[:HEADER_SIZE])
    code = contents(text) + contents(iat)
    initialized = contents(data) if data[5] else b""
    cr = reloc_section(relocations[ti])
    dr = reloc_section(relocations[di])
    data_off = HEADER_SIZE + len(code)
    cr_off = data_off + len(initialized)
    dr_off = cr_off + len(cr)
    sec = struct.pack("<II", 12, nimp) + DLL_NAME
    sec += b"\0" * ((-len(sec)) & 3)
    isize = len(sec)
    imp_off = dr_off + len(dr)

    struct.pack_into("<6I", header, 48, len(code), len(initialized),
                     4096, 1048576, 8192, bss_size)
    struct.pack_into("<I", header, 72, entry - text[3])
    struct.pack_into("<I", header, 44,
                     (struct.unpack_from("<I", header, 44)[0] & ~0xF0000000) | PE2)
    struct.pack_into("<I", header, 84, 1)
    struct.pack_into("<I", header, 96, text_size)
    struct.pack_into("<5I", header, 100, HEADER_SIZE,
                     data_off if initialized else 0, imp_off,
                     cr_off if cr else 0, dr_off if dr else 0)
    struct.pack_into("<I", header, 20, KImageCrcInitialiser)
    struct.pack_into("<I", header, 20, crc32_symbian(bytes(header)))
    return (bytes(header) + code + initialized + cr + dr
            + struct.pack("<I", isize) + sec), nimp


def main():
    if len(sys.argv) != 3:
        print("usage: mke32_netstack_elf.py <input.elf> <output.e32>",
              file=sys.stderr)
        return 2
    blob, nimp = pack(Path(sys.argv[1]).read_bytes())
    Path(sys.argv[2]).write_bytes(blob)
    print(f"wrote {sys.argv[2]} ({len(blob)} bytes, {nimp} PE2 import, ord 915)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

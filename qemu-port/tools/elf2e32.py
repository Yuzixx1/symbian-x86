#!/usr/bin/env python3
"""Pack a freestanding, statically linked ELF32/i386 into a relocatable E32.

Link with user/link.ld and ld --emit-relocs. Supports R_386_32 and
same-region R_386_PC32; rejects everything else rather than losing fixups.
No host dependencies beyond Python. This is a port build pipeline, not
an implementation of the Symbian SDK's general elf2e32 utility.
"""
import argparse
import struct
from pathlib import Path
from mke32 import build_args_e32, crc32_symbian, KImageCrcInitialiser


def pack(blob):
    if blob[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('expected little-endian ELF32')
    hdr = struct.unpack_from('<HHIIIIIHHHHHH', blob, 16)
    kind, machine, _, entry, _, shoff, _, _, _, _, shsize, shnum, shstr = hdr
    if kind != 2 or machine != 3 or shsize != 40:
        raise ValueError('expected linked i386 ELF with section headers')
    sections = [struct.unpack_from('<10I', blob, shoff + i * 40) for i in range(shnum)]

    def contents(s):
        data = blob[s[4]:s[4] + s[5]]
        if len(data) != s[5]:
            raise ValueError('truncated section')
        return data

    names = contents(sections[shstr])
    named = {names[s[0]:].split(b'\0', 1)[0].decode(): (i, s)
             for i, s in enumerate(sections)}
    ti, text = named['.text']
    di, data = named.get('.data', (-1, (0, 1, 3, 0x500000, 0, 0, 0, 0, 1, 0)))
    bi, bss = named.get('.bss', (-1, (0, 8, 3, data[3] + data[5], 0, 0, 0, 0, 1, 0)))
    if text[3] != 0x400000 or data[3] != 0x500000 or not text[3] <= entry < text[3] + text[5]:
        raise ValueError('unexpected linked bases or entry point')
    for i, s in enumerate(sections):
        if s[2] & 2 and s[5] and i not in (ti, di, bi):
            raise ValueError('unsupported allocated section')
    # Include linker alignment padding between initialized data and BSS.
    bss_size = max(0, bss[3] + bss[5] - (data[3] + data[5])) if bss[5] else 0
    regions = {ti: 1, di: 2, bi: 2}
    relocations = {ti: [], di: []}
    for s in sections:
        if s[1] != 9 or s[7] not in relocations:
            continue
        target = sections[s[7]]
        symbols = contents(sections[s[6]])
        raw = contents(s)
        if len(raw) % 8:
            raise ValueError('malformed ELF relocation section')
        for pos in range(0, len(raw), 8):
            address, info = struct.unpack_from('<II', raw, pos)
            symbol = struct.unpack_from('<IIIBBH', symbols, (info >> 8) * 16)
            region = regions.get(symbol[5])
            typ = info & 255
            offset = address - target[3]
            if not 0 <= offset <= target[5] - 4:
                raise ValueError('relocation outside target section')
            if typ == 2 and region == regions[s[7]]:
                continue  # PC-relative references within one allocation survive.
            if typ != 1 or region is None:
                raise ValueError(f'unsupported relocation type/target: {typ}/{symbol[5]}')
            relocations[s[7]].append((offset, region))

    def reloc_section(entries):
        if not entries:
            return b''
        pages = {}
        for offset, region in sorted(entries):
            pages.setdefault(offset & ~4095, []).append((region << 12) | (offset & 4095))
        body = bytearray()
        for page, words in pages.items():
            if len(words) & 1:
                words.append(0)
            body += struct.pack('<II', page, 8 + len(words) * 2)
            body += struct.pack('<' + 'H' * len(words), *words)
        return struct.pack('<II', 8 + len(body), len(entries)) + body

    header = bytearray(build_args_e32(0x10000001, 0x10000002, 0x10000002)[:156])
    code = contents(text)
    initialized = contents(data) if data[5] else b''
    cr = reloc_section(relocations[ti])
    dr = reloc_section(relocations[di])
    data_off = 156 + len(code)
    cr_off = data_off + len(initialized)
    dr_off = cr_off + len(cr)
    struct.pack_into('<6I', header, 48, len(code), len(initialized), 4096, 1048576, 8192, bss_size)
    struct.pack_into('<I', header, 72, entry - text[3])
    struct.pack_into('<I', header, 84, 0)  # no DLL dependencies
    struct.pack_into('<I', header, 96, len(code))
    struct.pack_into('<5I', header, 100, 156, data_off if initialized else 0,
                     0, cr_off if cr else 0, dr_off if dr else 0)
    struct.pack_into('<I', header, 20, KImageCrcInitialiser)
    struct.pack_into('<I', header, 20, crc32_symbian(header))
    return bytes(header) + code + initialized + cr + dr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input')
    parser.add_argument('output')
    args = parser.parse_args()
    try:
        result = pack(Path(args.input).read_bytes())
    except (ValueError, KeyError, IndexError, struct.error) as error:
        parser.error(str(error))
    Path(args.output).write_bytes(result)
    print(f'wrote {args.output} ({len(result)} bytes)')


if __name__ == '__main__':
    main()

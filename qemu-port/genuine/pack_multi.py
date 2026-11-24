#!/usr/bin/env python3
"""pack_multi.py -- PE2 E32 packer with N EUSER.DLL imports (for genuine EWSRV).

Like tools/elf2e32.py but supports *many* PE2 IAT slots instead of one.
The ELF must be linked (ld -m elf_i386 --emit-relocs -T user/link.ld) with an
absolute symbol at codeBase+iat_start+4*k for each imported function.  Pass the
1-based EUSER ordinal for slot k with --ordinals (comma separated), or an
ordinal file with --ordinals-file.  Slot k's 1-based ordinal is written into the
IAT; the kernel loader overwrites it with the genuine export address.

The image UID3 defaults to the genuine EFILE UID (0x100000bf) for compatibility
with existing callers.  Pass --uid3 (CLI) or the uid3= argument (direct import)
to stamp a different image's genuine UID3, e.g. ESTART 0x10272C04 or ESHELL
0x101F7AE7; build_args_e32() recomputes the UID checksum (+12) and header CRC
(+20) from it.
"""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
from mke32 import build_args_e32, crc32_symbian, KImageCrcInitialiser  # noqa: E402

K_IMAGE_HDR_FMT_V = 0x02000000
K_IMAGE_IMP_FMT_PE2 = 0x20000000
EUSER_PE2_DLL = b'euser{000a0000}[100039e5].dll\0'
CODE_BASE = 0x400000


def pack(blob: bytes, ordinals, iat_start: int, uid3: int = 0x100000bf):
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
    if text[3] != CODE_BASE or data[3] != 0x500000 or not text[3] <= entry < text[3] + text[5]:
        raise ValueError('unexpected linked bases or entry point')
    for i, s in enumerate(sections):
        if s[2] & 2 and s[5] and i not in (ti, di, bi):
            raise ValueError('unsupported allocated section')
    bss_size = max(0, bss[3] + bss[5] - (data[3] + data[5])) if bss[5] else 0

    n = len(ordinals)
    iat_end = iat_start + 4 * n
    if len(text[4:]) and text[5] != len(contents(text)):
        raise ValueError('bad text size')
    text_len = text[5]
    if iat_start < text_len or iat_start & 3:
        raise ValueError(f'IAT start {iat_start:#x} must be 4-aligned and >= text {text_len:#x}')
    code_size = iat_end
    # Relocations: SHN_ABS symbols in the IAT range become TEXT fixups.
    regions = {ti: 1, di: 2, bi: 2}
    relocations = {ti: [], di: []}
    seen_iat = set()
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
            if symbol[5] == 0xfff1 and typ == 1:          # SHN_ABS IAT slot
                addr = symbol[1] - CODE_BASE    # ELF ABS symbols are absolute
                if not (iat_start <= addr < iat_end):
                    raise ValueError(f'unexpected ABS reloc target {addr:#x}')
                if addr not in seen_iat:
                    seen_iat.add(addr)
                relocations[s[7]].append((offset, 1))
                continue
            if typ == 2 and region == regions[s[7]]:
                continue
            if typ != 1 or region is None:
                raise ValueError(f'unsupported relocation type/target: {typ}/{symbol[5]}')
            relocations[s[7]].append((offset, region))
    if seen_iat != {iat_start + 4 * k for k in range(n)}:
        missing = [hex(iat_start + 4 * k) for k in range(n)
                   if iat_start + 4 * k not in seen_iat]
        raise ValueError(f'no relocation for IAT slots: {missing[:8]}')

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

    # build_args_e32() stamps uid1/uid2/uid3, recomputes the UID checksum at +12
    # and the header CRC at +20 from uid3, so no separate CRC patch is needed.
    header = bytearray(build_args_e32(0x1000008c, uid3, 0)[:156])
    code = bytearray(contents(text))
    code.extend(b'\x00' * (iat_end - len(code)))
    for k, ordinal in enumerate(ordinals):
        struct.pack_into('<I', code, iat_start + 4 * k, ordinal)
    code = bytes(code)
    initialized = contents(data) if data[5] else b''
    cr = reloc_section(relocations[ti])
    dr = reloc_section(relocations[di])
    data_off = 156 + len(code)
    cr_off = data_off + len(initialized)
    dr_off = cr_off + len(cr)
    imp_off = dr_off + len(dr)
    isize = 8 + ((len(EUSER_PE2_DLL) + 3) & ~3)
    # Section layout (offsets from impOff): [0:4]=isize, [4:8]=nameOff=12,
    # [8:12]=ordinal count n, [12:]=DLL name.  The count MUST be packed as a
    # third header word -- writing it at imp+8 over a name that starts at
    # imp+8 clobbers the first 4 name bytes ("euse" -> binary), which only
    # passed validation because just the ".dll\0" suffix is checked.
    imp = bytearray(struct.pack('<III', isize, 12, n) + EUSER_PE2_DLL)  # nameOff=12
    imp += b'\x00' * (isize + 4 - len(imp))   # 4-byte size field + isize payload
    struct.pack_into('<6i', header, 48, code_size, len(initialized),
                     4096, 1048576, 8192, bss_size)
    struct.pack_into('<I', header, 72, entry - CODE_BASE)
    struct.pack_into('<I', header, 84, 1)                 # one DLL dep: EUSER
    struct.pack_into('<I', header, 88, 0)                 # no exports
    struct.pack_into('<I', header, 92, 0)
    struct.pack_into('<I', header, 96, iat_start)         # textSize
    struct.pack_into('<5I', header, 100, 156,
                     data_off if initialized else 0, imp_off,
                     cr_off if cr else 0, dr_off if dr else 0)
    struct.pack_into('<I', header, 44, K_IMAGE_HDR_FMT_V | K_IMAGE_IMP_FMT_PE2)
    struct.pack_into('<I', header, 20, KImageCrcInitialiser)
    struct.pack_into('<I', header, 20, crc32_symbian(header))
    return bytes(header) + code + initialized + cr + dr + imp


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input')
    ap.add_argument('output')
    ap.add_argument('--ordinals', help='comma-separated 1-based EUSER ordinals')
    ap.add_argument('--ordinals-file', help='file with one ordinal per line')
    ap.add_argument('--iat-start', type=lambda s: int(s, 0), required=True,
                    help='IAT start offset inside .text (4-aligned, >= code length)')
    ap.add_argument('--uid3', type=lambda s: int(s, 0), default=0x100000bf,
                    help='image UID3 (default 0x100000bf, the genuine EFILE UID; '
                         'ESTART=0x10272C04, ESHELL=0x101F7AE7). UID checksum and '
                         'header CRC are recomputed automatically')
    a = ap.parse_args()
    if a.ordinals_file:
        ords = [int(x.strip()) for x in Path(a.ordinals_file).read_text().split()
                if x.strip() and not x.startswith('#')]
    else:
        ords = [int(x) for x in a.ordinals.split(',')]
    if not ords:
        ap.error('no ordinals given')
    out = pack(Path(a.input).read_bytes(), ords, a.iat_start, a.uid3)
    Path(a.output).write_bytes(out)
    print(f'wrote {a.output} ({len(out)} bytes, {len(ords)} imports, '
          f'IAT {a.iat_start:#x}..{a.iat_start + 4*len(ords):#x})')


if __name__ == '__main__':
    main()

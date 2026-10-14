#!/usr/bin/env python3
"""e32read.py -- validate + dump an E32 image (mirrors genuine checks).

Implements the header-subset of
  E32ImageHeaderV::ValidateHeader / ValidateWholeImage
from kernelhwsrv/userlibandfileserver/fileserver/inc/f32image.h,
using the genuine CRC32/UID-checksum algorithms from
kernelhwsrv/kernel/eka/euser/us_func.cpp.

Usage:
  python3 tools/e32read.py <file.e32> [--quiet]
Exit 0 = valid, 1 = corrupt/unsupported.
"""
import struct
import sys

from mke32 import (
    crc16_ccitt,
    crc32_symbian,
    uid_checksum,
    KImageCrcInitialiser,
    KExecutableImageUidValue,
    EPOC_SIG,
    ECpuX86,
    KImageHdrFmt_V,
    KImageImpFmt_ELF,
    KCodeSegIdOffset,
)
KImageImpFmt_PE = 0x00000000
KImageImpFmt_PE2 = 0x20000000

KDynamicLibraryUidValue = 0x10000079
KImageDll = 0x00000001
KImageHdrFmtMask = 0x0F000000
KImageABIMask = 0x00000018
KImageEptMask = 0x000000E0
KImageImpFmtMask = 0xF0000000


def validate_relocs(blob: bytes, offset: int, section_size: int) -> str | None:
    """Mirror the port's page/block relocation checks for code or data."""
    if not offset:
        return None
    if offset + 8 > len(blob):
        return "relocation header outside file"
    size, count = struct.unpack_from("<Ii", blob, offset)
    if size < 8 or size & 3 or count < 0 or offset + size > len(blob):
        return "relocation section size/count corrupt"
    pos, end, actual = offset + 8, offset + size, 0
    while pos < end:
        if pos + 8 > end:
            return "relocation block header truncated"
        page, block_size = struct.unpack_from("<2I", blob, pos)
        if page & 0xFFF or block_size < 8 or block_size & 3 or pos + block_size > end:
            return "relocation page/block invalid"
        for site in range(pos + 8, pos + block_size, 2):
            (entry,) = struct.unpack_from("<H", blob, site)
            if not entry:
                continue
            if entry & 0xF000 not in (0x1000, 0x2000, 0x3000):
                return "relocation type invalid"
            if page + (entry & 0xFFF) + 4 > section_size:
                return "relocation target outside section"
            actual += 1
        pos += block_size
    if actual != count:
        return "relocation count mismatch"
    return None


def fail(msg):
    print(f"E32 INVALID: {msg}")
    return 1


def main() -> int:
    quiet = "--quiet" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--quiet"]
    if len(args) != 1:
        print("usage: e32read.py <file.e32> [--quiet]", file=sys.stderr)
        return 2
    path = args[0]
    with open(path, "rb") as f:
        blob = f.read()
    size = len(blob)
    if size < 156:
        return fail(f"file too small ({size} < min V header 156)")

    (uid1, uid2, uid3, uid_chk) = struct.unpack_from("<4I", blob, 0)
    (sig,) = struct.unpack_from("<I", blob, 16)
    (hdr_crc,) = struct.unpack_from("<I", blob, 20)
    (modver,) = struct.unpack_from("<I", blob, 24)
    (comp,) = struct.unpack_from("<I", blob, 28)
    (flags,) = struct.unpack_from("<I", blob, 44)
    (code_size, data_size, heap_min, heap_max, stack_size, bss_size) = struct.unpack_from("<6i", blob, 48)
    (entry,) = struct.unpack_from("<I", blob, 72)
    (code_base, data_base) = struct.unpack_from("<2I", blob, 76)
    (dll_refs,) = struct.unpack_from("<i", blob, 84)
    (exp_off,) = struct.unpack_from("<I", blob, 88)
    (exp_count,) = struct.unpack_from("<i", blob, 92)
    (text_size,) = struct.unpack_from("<i", blob, 96)
    (code_off, data_off, imp_off, code_reloc_off, data_reloc_off) = struct.unpack_from("<5I", blob, 100)
    (priority, cpu) = struct.unpack_from("<2H", blob, 120)
    header_size = code_off

    # header format + size
    if (flags & KImageHdrFmtMask) != KImageHdrFmt_V:
        return fail(f"header format {flags & KImageHdrFmtMask:#x} != V ({KImageHdrFmt_V:#x})")
    if header_size > size:
        return fail(f"header size {header_size} > file {size}")
    if header_size < 156:
        return fail(f"header size {header_size} < V minimum 156")

    # cpu (x86 port accepts X86 only)
    if cpu != ECpuX86:
        return fail(f"cpu id {cpu:#x} != ECpuX86 ({ECpuX86:#x})")

    # uids
    if uid_checksum(uid1, uid2, uid3) != uid_chk:
        return fail("uid checksum mismatch")
    if sig != EPOC_SIG:
        return fail(f"signature {sig:#x} != EPOC ({EPOC_SIG:#x})")
    want_uid1 = KDynamicLibraryUidValue if (flags & KImageDll) else KExecutableImageUidValue
    if uid1 != want_uid1:
        return fail(f"uid1 {uid1:#x} inconsistent with DLL flag (want {want_uid1:#x})")

    # header crc: zero the field to KImageCrcInitialiser then CRC32 from 0
    tmp = bytearray(blob[:header_size])
    struct.pack_into("<I", tmp, 20, KImageCrcInitialiser)
    if crc32_symbian(bytes(tmp), 0) != hdr_crc:
        return fail("header CRC mismatch")

    # version / compression
    if modver >= 0x80000000 or (modver & 0xFFFF) > 0x8000:
        return fail(f"module version {modver:#x} not supported")
    if comp != 0 and comp not in (0x101F7AFC, 0x102822AA):
        return fail(f"compression {comp:#x} not supported")
    uncompressed = size

    # heap/stack/bss signs
    if heap_min < 0:
        return fail("heap min negative")
    if heap_max < heap_min:
        return fail("heap max < min")
    if stack_size < 0 or bss_size < 0:
        return fail("stack/bss negative")

    # x86 ABI/EPT rules (f32image.h:713-719): no bits allowed
    if flags & KImageEptMask:
        return fail("entry-point-type bits set (x86 must be 0)")
    if flags & KImageABIMask:
        return fail("ABI bits set (x86 must be 0)")
    imp_fmt = flags & KImageImpFmtMask
    if imp_fmt not in (0, KImageImpFmt_ELF, KImageImpFmt_PE, KImageImpFmt_PE2):
        return fail(f"import format {imp_fmt:#x} not supported")

    # entry point
    if not (entry < code_size):
        return fail(f"entry {entry:#x} outside code size {code_size}")
    if entry + KCodeSegIdOffset + 4 > code_size:
        return fail("entry + codeseg-id slot outside code")
    if code_base & 3 or data_base & 3:
        return fail("code/data base misaligned")

    # code / data regions
    code_end = code_off + code_size
    if code_end < code_off or code_end > uncompressed:
        return fail("code region outside file")
    if data_off == 0:
        if data_size != 0:
            return fail("data offset 0 but size nonzero")
    else:
        data_end = data_off + data_size
        if data_end < data_off or data_off < code_end or data_end > uncompressed:
            return fail("data region outside file")

    # dll refs need imports (pilot: 0=import-free, 1=EUSER below)
    if dll_refs < 0 or dll_refs > 1:
        return fail("dll ref count out of pilot range")
    if dll_refs and not imp_off:
        return fail("dll refs but no import section")
    if imp_off and not dll_refs:
        return fail("import section but no dll refs")

    # exports
    if exp_count < 0 or exp_count > 65535:
        return fail("export count out of range")
    if exp_count:
        exp_end = exp_off + exp_count * 4
        if exp_end < exp_off or exp_off < code_off or exp_end > code_end:
            return fail("export dir outside code")
        for i in range(exp_count):
            (target,) = struct.unpack_from("<I", blob, exp_off + 4 * i)
            if target >= code_size:
                return fail(f"export {i + 1} target outside code")

    # text size
    if not (0 <= text_size <= code_size):
        return fail("text size outside code")

    # import / reloc section headers must at least fit (full walk = WholeImage)
    for name, off, min_size in (
        ("import", imp_off, 4),
        ("code-reloc", code_reloc_off, 8),
        ("data-reloc", data_reloc_off, 8),
    ):
        if off:
            if off < code_end or off + min_size > uncompressed:
                return fail(f"{name} section outside rest-of-data")

    if data_reloc_off and (not data_off or not data_size):
        return fail("data relocations without initialized data")
    for name, offset, section_size in (("code", code_reloc_off, code_size),
                                       ("data", data_reloc_off, data_size)):
        reason = validate_relocs(blob, offset, section_size)
        if reason:
            return fail(f"{name} {reason}")

    # pilot import section walk (must agree with E32Validate twin)
    if imp_off:
        (isize,) = struct.unpack_from("<I", blob, imp_off)
        if isize < 22 or imp_off + 4 + isize > uncompressed:
            return fail("import section size corrupt")
        sec = imp_off + 4
        (name_off,) = struct.unpack_from("<I", blob, sec)
        (nimp,) = struct.unpack_from("<i", blob, sec + 4)
        if nimp < 2 or nimp > 4:
            return fail("pilot serves ordinals 1-4 (EUSER Exit/PutC/CmdLen/+)")
        if isize < 14 + 4 * nimp:
            return fail("import section size too small for import count")
        if name_off + 6 > isize + 4:
            return fail("import dll name outside section")
        if blob[imp_off + name_off:imp_off + name_off + 6] != b"EUSER\x00":
            return fail("unknown provider DLL (pilot: EUSER only)")
        seen = set()
        for k in range(nimp):
            (s,) = struct.unpack_from("<I", blob, sec + 8 + 4 * k)
            if (s & 3) or not (text_size <= s and s + 4 <= code_size):
                return fail("IAT slot outside code text gap")
            if s in seen:
                return fail("IAT slots alias")
            seen.add(s)

    # export description tail (f32image.h ValidateExportDescription)
    # packed offsetof(iExportDescSize) = 152; NoHoles tail = 2+1+1 -> 4
    (exp_desc_size,) = struct.unpack_from("<H", blob, 152)
    exp_desc_type = blob[154]
    ed_size = (exp_desc_size + 2 + 1 + 3) & ~3
    if 152 + ed_size != header_size:
        return fail(f"export-desc tail mismatch (size {exp_desc_size} type {exp_desc_type:#x})")
    if exp_desc_type == 0x00:
        pass  # NoHoles
    elif exp_desc_type == 0x01:
        if ((exp_count + 7) >> 3) != exp_desc_size:
            return fail("full-bitmap size mismatch")
    elif exp_desc_type == 0x02:
        # sparse-bitmap8: meta scan (f32image.h:918-947)
        bitmap_size = (exp_count + 7) >> 3
        meta_size = (bitmap_size + 7) >> 3
        if meta_size > exp_desc_size:
            return fail("sparse bitmap does not fit")
        total = meta_size
        meta = blob[155:155 + meta_size]
        if len(meta) < meta_size:
            return fail("sparse bitmap truncated")
        for byte in meta:
            b = byte
            while True:
                if b & 1:
                    total += 1
                b >>= 1
                if not b:
                    break
        if total != exp_desc_size:
            return fail("sparse bitmap size mismatch")
    else:
        return fail(f"export-desc type {exp_desc_type:#x} not supported")

    # data+ bss cap (f32image.h:799)
    if data_size + bss_size > 0x7FFF0000:
        return fail("data+bss too large")

    if not quiet:
        kind = "DLL" if (flags & KImageDll) else "EXE"
        print(f"E32 OK: {kind} cpu=x86 entry={entry:#x} code={code_size} data={data_size} bss={bss_size}")
        print(f"  uids: {uid1:#010x} {uid2:#010x} {uid3:#010x} chk={uid_chk:#010x}")
        print(f"  bases: code={code_base:#010x} data={data_base:#010x} heap={heap_min}/{heap_max} stack={stack_size}")
        print(f"  header: size={header_size} crc={hdr_crc:#010x} flags={flags:#010x} dllrefs={dll_refs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

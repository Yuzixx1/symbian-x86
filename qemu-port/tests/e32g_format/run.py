#!/usr/bin/env python3
"""Compile and test the actual parser; fixtures and scratch are explicit inputs."""
import argparse
import hashlib
import json
import random
from pathlib import Path
import struct
import subprocess


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def put(b, o, n):
    struct.pack_into('<I', b, o, n)


# Independent table construction, not copied from parser or pilot generator.
TABLE = []
for value in range(256):
    for _ in range(8):
        value = (value >> 1) ^ (0xEDB88320 if value & 1 else 0)
    TABLE.append(value)
assert TABLE[205] == 0xE5D5BE0D


def crc(b, table=TABLE):
    c = 0
    for x in b:
        c = (c >> 8) ^ table[(c ^ x) & 255]
    return c


def seal(b):
    put(b, 20, 0xC90FDAA2)
    put(b, 20, crc(b[:u32(b, 100)]))
    return b


def checked_uid(b):
    def ccitt(seq):
        c = 0
        for x in seq:
            c ^= x << 8
            for _ in range(8):
                c = ((c << 1) ^ (0x1021 if c & 0x8000 else 0)) & 65535
        return c
    return ccitt(b[:12:2]) | (ccitt(b[1:12:2]) << 16)


def synthetic(kind=0x3000, desc_type=0, desc=b'', name=b'euser{000a0000}[100039e5].dll'):
    """Independently laid-out V header, EXE, 128-byte code, 1 export/import.

    Relocation iSize EXCLUDES its section header; imports size INCLUDES it.
    Values derive from f32image.h, not tools/mke32.py.
    """
    hs = 152 + ((len(desc) + 6) & ~3)
    code = bytearray(128)
    code[:2] = b'\xeb\x0e'
    put(code, 32, 0x400020)  # general code relocation site
    put(code, 64, 85)  # one ordinal at textSize
    put(code, 80, 0 if desc else 32)  # zero is permitted, not absent
    if desc:
        put(code, 80, 0)  # absent marker is entry offset (zero)
    imp = bytearray(struct.pack('<III', 12 + len(name) + 1, 12, 1) + name + b'\0')
    while len(imp) % 4:
        imp.append(0)
    put(imp, 0, len(imp))
    reloc = struct.pack('<IIIIHH', 12, 1, 0, 12, kind | 32, 0)
    b = bytearray(hs) + code + imp + reloc
    for off, value in [(0,0x1000007A),(16,0x434F5045),(24,0x000A0000),
                       (44,0x22004000),(48,128),(56,4096),(60,1048576),
                       (64,8192),(76,0x400000),(84,1),(88,hs+80),(92,1),
                       (96,64),(100,hs),(108,hs+128),(112,hs+128+len(imp)),
                       (128,0x12345678),(132,0x70000001),(136,0x123),(140,0x456)]:
        put(b, off, value)
    struct.pack_into('<HH', b, 120, 350, 0x1000)
    struct.pack_into('<H', b, 152, len(desc))
    b[154] = desc_type
    b[155:155+len(desc)] = desc
    put(b, 12, checked_uid(b))
    return seal(b)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--fixtures', type=Path, required=True)
    ap.add_argument('--scratch', type=Path, required=True)
    ap.add_argument('--cxx', default='g++')
    ap.add_argument('--sanitize', action='store_true')
    args = ap.parse_args()
    # Unique directory: never overwrite previous test evidence.
    args.scratch.mkdir(parents=True, exist_ok=False)
    here = Path(__file__).resolve().parent
    module = here.parents[1] / 'loader'
    command = [args.cxx, '-std=c++98', '-Wall', '-Wextra', '-Werror', '-pedantic',
               '-fno-exceptions', '-fno-rtti', '-I'+str(module),
               str(module/'e32g_format.cpp'), str(here/'probe.cpp'),
               '-o', str(args.scratch/'probe')]
    if args.sanitize:
        command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g', '-no-pie']
    def compile_logged(argv, label):
        done = subprocess.run(argv, capture_output=True, text=True)
        (args.scratch/(label+'.json')).write_text(json.dumps(dict(
            argv=argv, returncode=done.returncode, stdout=done.stdout, stderr=done.stderr),indent=2)+'\n')
        if done.returncode:
            raise RuntimeError(done.stderr)
    compile_logged(command, 'compile_probe')
    api_command = [str(here/'api.cpp') if x == str(here/'probe.cpp') else
                   str(args.scratch/'api') if x == str(args.scratch/'probe') else x
                   for x in command]
    compile_logged(api_command, 'compile_api')
    subprocess.run([str(args.scratch/'api')], check=True)
    results = []

    def test(name, b, expected=0, issue=None, fields=None):
        p = args.scratch / (name+'.e32')
        p.write_bytes(b)
        done = subprocess.run([str(args.scratch/'probe'), str(p)], capture_output=True, text=True)
        assert done.returncode == 0, (name, done.returncode, done.stdout, done.stderr)
        got = list(map(int, done.stdout.split()))
        assert got[0] == expected, (name, got, expected)
        if issue is not None:
            issues = dict(Buffer=1, Header=2, Profile=3, Uid=4, Signature=5,
                          Crc=6, Version=7, Sizes=8, Entry=9, Exports=10,
                          ExportDescription=11, Imports=12, Name=13, Relocations=14)
            assert got[1] == issues[issue], (name, got, issue)
        if fields is not None:
            assert got[3:] == fields, (name, got[3:], fields)
        results.append(dict(name=name, output=got, sha256=hashlib.sha256(b).hexdigest()))
        print('PASS', name, done.stdout.strip())

    # Identity manifest is pinned from CLEAN-BUILD, not learned from fixtures.
    manifest = json.loads((here/'v1_manifest.json').read_text())
    before = {}
    for name, record in manifest.items():
        b = (args.fixtures/name).read_bytes()
        h = hashlib.sha256(b).hexdigest()
        assert h == record['sha256'], (name,h)
        before[name] = h
        test('v1_'+name, b, fields=record['fields'])
    base = synthetic()
    for typ in [0x1000,0x2000,0x3000]:
        test('reloc_type_%04x'%typ, synthetic(typ))
    test('full_bitmap_hole', synthetic(desc_type=1,desc=b'\x00'))
    test('sparse_bitmap_hole', synthetic(desc_type=2,desc=b'\x01\x00'))
    test('sparse_implicit_present', seal_change_export(synthetic(desc_type=2,desc=b'\x00'),32))

    def mutate(name, off, value, status=1, issue=None):
        b = bytearray(base); put(b,off,value); seal(b); test(name,b,status,issue)

    mutate('code_overflow',48,0xFFFFFFF0,issue='Sizes')
    mutate('data_overflow',52,0xFFFFFFF0,issue='Sizes')
    mutate('text_overrun',96,129,issue='Sizes')
    mutate('entry_overrun',72,113,issue='Entry')
    mutate('entry_overflow',72,0xFFFFFFFC,issue='Entry')
    mutate('export_count_overflow',92,0xFFFFFFFF,issue='Exports')
    mutate('export_outside_code',88,len(base)-4,issue='Exports')
    mutate('import_offset_overflow',108,0xFFFFFFFC,issue='Imports')
    mutate('dependency_overflow',84,0x80000000,issue='Imports')
    mutate('missing_import_section',108,0,issue='Imports')
    mutate('reloc_offset_overflow',112,0xFFFFFFFC,issue='Relocations')
    mutate('compression_unsupported',28,0x101F7AFC,2,'Profile')
    mutate('elf_unsupported',44,0x12004000,2,'Profile')
    mutate('paged_unsupported',44,0x22004200,2,'Profile')
    mutate('negative_heap',56,0x80000000,issue='Sizes')
    mutate('heap_order',60,1,issue='Sizes')
    mutate('exception_descriptor',144,0xFFFFFFFD,issue='Entry')
    mutate('version_unsupported',24,0x80000000,2,'Version')
    mutate('uid_mismatch',8,0x98765432,issue='Uid')
    mutate('signature_bad',16,0,issue='Signature')
    b = bytearray(base); put(b,0,0x10000079); put(b,12,checked_uid(b)); seal(b)
    test('uid_flag_disagreement',b,2,'Uid')
    mutate('no_relocs',112,0,status=0)
    b = bytearray(base); put(b,108,0); put(b,84,0); seal(b)
    test('no_imports',b)
    b = bytearray(base); put(b,88,0); put(b,92,0); seal(b)
    test('no_exports',b)
    # Unaligned x86 relocation sites are legitimate; parser uses byte reads.
    b = bytearray(base); ro0 = u32(b,112)
    struct.pack_into('<H',b,ro0+16,0x3021); test('unaligned_x86_site',b)
    b = bytearray(base); ro0 = u32(b,112)
    struct.pack_into('<H',b,ro0+16,0); put(b,ro0+4,0); test('reloc_padding_only',b)
    for name, pos in [('crc_bad',130),('crc_index205',131)]:
        b = bytearray(base); b[pos] ^= 1; test(name,b,1,'Crc')
    # Force a header whose checksum differs under the legacy bad table.
    bad_table = TABLE.copy(); bad_table[205] = 0xE5D5C06D
    for n in range(10000):
        b = bytearray(base); put(b,132,n); seal(b)
        h = bytearray(b[:u32(b,100)]); put(h,20,0xC90FDAA2)
        if crc(h,bad_table) != u32(b,20):
            test('crc205_standard',b)
            put(b,20,crc(h,bad_table)); test('crc205_legacy_rejected',b,1,'Crc')
            break
    else:
        raise AssertionError('failed to construct CRC205 regression')
    ro = u32(base,112); io = u32(base,108)
    mutate('reloc_size_includes_header',ro,20,issue='Relocations')
    mutate('reloc_size_short',ro,4,issue='Relocations')
    mutate('reloc_size_overflow',ro,0xFFFFFFFC,issue='Relocations')
    mutate('reloc_count_mismatch',ro+4,2,issue='Relocations')
    mutate('reloc_count_negative',ro+4,0xFFFFFFFF,issue='Relocations')
    mutate('reloc_block_short',ro+12,4,issue='Relocations')
    mutate('reloc_block_overrun',ro+12,16,issue='Relocations')
    mutate('reloc_unaligned_page',ro+8,1,issue='Relocations')
    mutate('reloc_destination_overflow',ro+8,0xFFFFF000,issue='Relocations')
    for label, entry in [('reloc_type_invalid',0x4000),('reloc_site_overrun',0x307F)]:
        b = bytearray(base); struct.pack_into('<H',b,ro+16,entry); test(label,b,1,'Relocations')
    mutate('import_size_overflow',io,0xFFFFFFFC,issue='Imports')
    mutate('import_count_overflow',io+8,0x40000000,issue='Imports')
    mutate('iat_overrun',io+8,100,issue='Imports')
    mutate('name_offset_overflow',io+4,0xFFFFFFFF,issue='Name')
    b = bytearray(base); b[io+12:ro] = b'x'*(ro-io-12); test('name_unterminated',b,1,'Name')
    for label, name in [('bad_uid',b'euser[100039eg].dll'),
                        ('bad_version',b'euser{000a000z}[100039e5].dll'),
                        ('bad_brackets',b'eu[ser].dll'),('path',b'..\\euser.dll'),
                        ('empty',b''),('long',b'x'*81)]:
        test('name_'+label,synthetic(name=name),2 if label=='long' else 1,'Name')
    test('name_plain',synthetic(name=b'euser.dll'))
    test('name_uid_zero',synthetic(name=b'euser[00000000].dll'))
    b = synthetic(desc_type=1,desc=b'\x01'); test('bitmap_marker_disagreement',b,1,'ExportDescription')
    b = synthetic(desc_type=2,desc=b'\x01'); test('sparse_payload_missing',b,1,'ExportDescription')
    b = synthetic(desc_type=3,desc=b''); test('bitmap_unknown',b,2,'ExportDescription')
    b = synthetic(desc_type=1,desc=b'\x00\x00'); test('full_bitmap_wrong_size',b,1,'ExportDescription')
    mutate('header_size_mismatch',100,160,issue='ExportDescription')
    for n in [0,1,12,100,155,156,len(base)-1]:
        test('truncated_%d'%n,base[:n],1)
    # Data relocations and EXE data/BSS are independent of code relocs.
    b = bytearray(base); dataoff = len(b); b += b'\0'*8
    put(b,52,8); put(b,68,16); put(b,104,dataoff)
    dro = len(b); b += struct.pack('<IIIIHH',12,1,0,12,0x2000,0)
    put(b,116,dro); seal(b); test('data_relocation',b)
    put(b,0,0x10000079); put(b,44,u32(b,44)|1); put(b,12,checked_uid(b)); seal(b)
    test('writable_dll_unsupported',b,2,'Profile')
    # Deterministic bounded mutation smoke: statuses may vary, but the compiled
    # reader must never write input, crash, hang, or expose a partial result.
    rng = random.Random(0xE32)
    for i in range(200):
        b = bytearray(base)
        for _ in range(rng.randrange(1,6)):
            b[rng.randrange(len(b))] = rng.randrange(256)
        if u32(b,100) <= len(b):
            seal(b)
        if i % 3 == 0:
            b = b[:rng.randrange(len(b))]
        p = args.scratch/('mutation_%03d.e32'%i); p.write_bytes(b)
        done = subprocess.run([str(args.scratch/'probe'),str(p)],capture_output=True,text=True)
        assert done.returncode == 0, (i,done.returncode,done.stderr)
    for name, digest in before.items():
        assert hashlib.sha256((args.fixtures/name).read_bytes()).hexdigest() == digest
    (args.scratch/'results.json').write_text(json.dumps(dict(command=command,api_command=api_command,
        mutation_smoke_count=200,results=results),indent=2)+'\n')
    print('PASS total:',len(results),'vectors + API checks + 200 mutation smoke cases; immutable fixtures preserved')


def seal_change_export(b, value):
    put(b,u32(b,88),value)
    return seal(b)


if __name__ == '__main__':
    main()

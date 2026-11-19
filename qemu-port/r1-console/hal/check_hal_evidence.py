#!/usr/bin/env python3
"""Independent byte audit of preserved HAL v2 and accepted EKDATA v3 evidence.

Reads existing target artifacts only. It does not compile, link, execute, or
modify target images. The genuine upstream generator is run into a temporary
directory solely to confirm that the preserved generated tables are
reproducible from the adopted recipes.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

HAL_SHA256 = '4a95dbe01d201b8a0b347e0e5c4cc39f798a7d89d6e163d7f63d71d3a04cb150'
EKDATA_SHA256 = '2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f'
EXPECTED = (
    ('_ZN3HAL3GetEN7HALData10TAttributeERi', 1),
    ('_ZN3HAL3SetEN7HALData10TAttributeEi', 2),
    ('_ZN3HAL6GetAllERiRPNS_6SEntryE', 3),
    ('_ZN3HAL3GetEiN7HALData10TAttributeERi', 4),
    ('_ZN3HAL3SetEiN7HALData10TAttributeEi', 5),
    ('_ZN11HalInternal12InitialValueE', 6),
)
VALID = {
    'ECPU', 'ECPUABI', 'ESystemTickPeriod', 'ENanoTickPeriod',
    'EMemoryPageSize', 'EKeyboardIndex', 'EDisplayMode',
    'EDisplayNumberOfScreens',
}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def pe_layout(path):
    data = Path(path).read_bytes()
    pe = u32(data, 0x3c)
    require(data[pe:pe + 4] == b'PE\0\0', 'not a PE image: ' + str(path))
    machine, count, _, symbols_ptr, symbols_count, optional_size = \
        struct.unpack_from('<HHIIIH', data, pe + 4)
    require(machine == 0x14c, 'unexpected PE machine')
    sections = []
    for index in range(count):
        offset = pe + 24 + optional_size + 40 * index
        name = data[offset:offset + 8].split(b'\0')[0].decode('ascii')
        virtual_size, virtual_address, raw_size, raw_pointer = \
            struct.unpack_from('<IIII', data, offset + 8)
        sections.append({
            'name': name, 'virtual_size': virtual_size,
            'virtual_address': virtual_address, 'raw_size': raw_size,
            'raw_pointer': raw_pointer,
        })
    directories = pe + 24 + 96
    export_rva, export_size = struct.unpack_from('<II', data, directories)

    def raw_offset(rva):
        for section in sections:
            start = section['virtual_address']
            extent = max(section['virtual_size'], section['raw_size'])
            if start <= rva < start + extent:
                return section['raw_pointer'] + rva - start
        raise AssertionError('RVA outside PE sections: 0x%x' % rva)

    export_offset = raw_offset(export_rva)
    base = u32(data, export_offset + 16)
    function_count = u32(data, export_offset + 20)
    name_count = u32(data, export_offset + 24)
    functions_offset = raw_offset(u32(data, export_offset + 28))
    functions = [u32(data, functions_offset + 4 * index)
                 for index in range(function_count)]

    string_base = symbols_ptr + symbols_count * 18
    string_size = u32(data, string_base)

    def symbol_name(entry_offset):
        field = data[entry_offset:entry_offset + 8]
        if field[:4] != b'\0\0\0\0':
            return field.split(b'\0')[0].decode('latin1')
        string_offset = u32(field, 4)
        if string_offset < 4 or string_offset >= string_size:
            return ''
        end = data.index(b'\0', string_base + string_offset)
        return data[string_base + string_offset:end].decode('latin1')

    symbols = {}
    index = 0
    while index < symbols_count:
        entry = symbols_ptr + index * 18
        name = symbol_name(entry)
        value, section, _, _, aux = struct.unpack_from('<IHHBB', data, entry + 8)
        if name and section != 0 and name not in symbols:
            symbols[name] = (section, value)
        index += 1 + aux
    return {
        'data': data, 'sections': sections, 'base': base,
        'function_count': function_count, 'name_count': name_count,
        'functions': functions, 'symbols': symbols,
    }


def e32_header(data):
    return {
        'uid1': u32(data, 0), 'uid2': u32(data, 4), 'uid3': u32(data, 8),
        'flags': u32(data, 44), 'code_size': u32(data, 48),
        'data_size': u32(data, 52), 'bss': u32(data, 68),
        'code_base': u32(data, 76), 'exports': u32(data, 88),
        'export_count': u32(data, 92), 'text_size': u32(data, 96),
        'code': u32(data, 100), 'imports': u32(data, 108),
        'code_reloc': u32(data, 112), 'data_reloc': u32(data, 116),
        'sid': u32(data, 128), 'vid': u32(data, 132),
        'caps0': u32(data, 136), 'caps1': u32(data, 140),
    }


def generated_values(path):
    values = []
    names = []
    for value, name in re.findall(
            r'^\s*(-?\d+),?\s*// (\w+)', Path(path).read_text(), re.M):
        values.append(int(value))
        names.append(name)
    require(len(values) == 121, 'unexpected generated InitialValue length')
    require(len(set(names)) == len(names), 'duplicate generated attribute name')
    return values, names


def generated_body(path):
    # halcfg.pl writes the output path on its first comment line.
    return Path(path).read_text().split('\n', 1)[1]


def def_entries(path):
    entries = []
    for symbol, ordinal, declared in re.findall(
            r'^\s*(\S+)\s+@\s*(\d+)\s+NONAME'
            r'(?:\s+DATA(?:\s+(\d+))?)?\s*(?:;.*)?$',
            Path(path).read_text(), re.M):
        entries.append((symbol, int(ordinal),
                        int(declared) if declared else None))
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--v2-root', type=Path, required=True,
                        help='surviving HAL v2 build root')
    parser.add_argument('--ekdata-root', type=Path, required=True,
                        help='accepted EKDATA v3 build root')
    parser.add_argument('--upstream', type=Path, required=True,
                        help='genuine kernelhwsrv reference root')
    parser.add_argument('--port', type=Path,
                        default=Path(__file__).resolve().parents[2],
                        help='live qemu-port directory')
    parser.add_argument('--json', type=Path,
                        help='optional JSON evidence output path')
    args = parser.parse_args()

    root = args.v2_root.resolve()
    ekdata_root = args.ekdata_root.resolve()
    upstream = args.upstream.resolve()
    port = args.port.resolve()
    release = root / 'epoc32/release/x86/urel'
    hal_path = release / 'hal.dll'
    hal_pe_path = release / 'hal.dll.sym'
    hal = hal_path.read_bytes()
    require(sha256(hal_path) == HAL_SHA256, 'HAL image hash changed')
    require(sha256(root / 'staged/sys/bin/HAL.DLL') == HAL_SHA256,
            'staged HAL differs from audited image')

    header = e32_header(hal)
    require(header['export_count'] == 6, 'HAL export count is not six')
    require(header['data_size'] == 0 and header['bss'] == 0 and
            header['data_reloc'] == 0, 'HAL has writable DLL data')
    require(header['uid1'] == 0x10000079 and
            header['uid2'] == 0x1000008d and
            header['sid'] == header['uid3'] == 0x100039e8,
            'HAL UID/SID mismatch')

    pe = pe_layout(hal_pe_path)
    require(pe['base'] == 1 and pe['function_count'] == 6,
            'HAL PE export table is not six entries at base 1')
    require(pe['name_count'] == 0, 'HAL exports unexpectedly named')

    actual_e32 = [u32(hal, header['exports'] + 4 * index)
                  for index in range(header['export_count'])]
    packed = [section for section in pe['sections']
              if section['name'] in ('.text', '.rdata')]
    require(sum(section['virtual_size'] for section in packed) ==
            header['text_size'], 'packed HAL text/rdata extent mismatch')

    rows = []
    for symbol, ordinal in EXPECTED:
        require(ordinal == len(rows) + 1, 'non-contiguous HAL ordinals')
        coff_name = '_' + symbol
        require(coff_name in pe['symbols'], 'missing linked symbol ' + symbol)
        section_number, value = pe['symbols'][coff_name]
        require(1 <= section_number <= len(pe['sections']),
                'bad symbol section for ' + symbol)
        section = pe['sections'][section_number - 1]
        expected_rva = section['virtual_address'] + value
        expected_e32 = sum(item['virtual_size'] for item in packed
                           if item['name'] != section['name'] and
                           packed.index(item) < packed.index(section)) + value
        require(pe['functions'][ordinal - 1] == expected_rva,
                'PE export mismatch at HAL ordinal %d' % ordinal)
        require(actual_e32[ordinal - 1] == expected_e32,
                'E32 export mismatch at HAL ordinal %d' % ordinal)
        rows.append({
            'ordinal': ordinal, 'symbol': symbol,
            'section': section['name'],
            'symbol_offset': hex(value), 'PE_rva': hex(expected_rva),
            'E32_offset': hex(expected_e32),
        })

    values, names = generated_values(
        root / 'workspace/qemu-port/r1-console/hal/generated/values.cpp')
    initial = struct.pack('<121i', *values)
    require(len(initial) == 484, 'InitialValue is not 484 bytes')
    initial_symbol = pe['symbols']['_' + EXPECTED[-1][0]]
    initial_section = pe['sections'][initial_symbol[0] - 1]
    initial_pe_offset = initial_section['raw_pointer'] + initial_symbol[1]
    require(initial_pe_offset + len(initial) <=
            initial_section['raw_pointer'] + initial_section['raw_size'],
            'InitialValue exceeds PE .rdata')
    initial_e32_offset = header['code'] + actual_e32[5]
    require(initial_e32_offset + len(initial) <= len(hal),
            'InitialValue exceeds E32 image')
    pe_bytes = pe['data'][initial_pe_offset:initial_pe_offset + len(initial)]
    e32_bytes = hal[initial_e32_offset:initial_e32_offset + len(initial)]
    require(pe_bytes == initial and e32_bytes == initial,
            'full 484-byte InitialValue does not match generated values')

    valid = [name for name, value in zip(names, values) if name in VALID]
    require(set(valid) == VALID, 'unexpected valid HAL attribute set')
    require(all(value == 0 for name, value in zip(names, values)
                if name not in VALID),
            'unexpected non-zero unsupported HAL value')
    require(values[names.index('ECPU')] == 2 and
            values[names.index('ECPUABI')] == 6 and
            values[names.index('ESystemTickPeriod')] == 15625 and
            values[names.index('ENanoTickPeriod')] == 1000 and
            values[names.index('EMemoryPageSize')] == 4096,
            'HAL initial value profile mismatch')

    def_entries_list = def_entries(
        upstream / 'halservices/hal/eabi/halu.def')
    require([(symbol, ordinal) for symbol, ordinal, _ in def_entries_list] ==
            list(EXPECTED), 'genuine halu.def ordinal contract changed')
    require(def_entries_list[-1][2] == 428,
            'genuine InitialValue DATA annotation changed')
    prep_path = (root / 'epoc32/build/hal/c_6787c3d59c7ec372/'
                       'hal_dll/x86/urel/hal.prep')
    prepared = [re.sub(r'\s+DATA(?:\s+\d+)?\s*$', ' DATA', line.strip())
                for line in prep_path.read_text().splitlines() if line.strip()]
    declared = [re.sub(r'\s+DATA(?:\s+\d+)?\s*$', ' DATA', line.strip())
                for line in (upstream / 'halservices/hal/eabi/halu.def')
                .read_text().splitlines() if line.strip()]
    require(prepared == declared,
            'prepdef output is not the genuine def with size metadata removed')
    require('DATA 428' not in prep_path.read_text() and
            prepared[-1].endswith(' DATA'),
            'prepdef retained the 428-byte size annotation')

    build_xml = (root / 'logs/build_hal.xml').read_text()
    require(re.search(r'Resolved def file:\s+.*/halu\.def\s*$', build_xml, re.M),
            'HAL build did not resolve halu.def')
    require(re.search(r'prepdef\.pl\s+.*/halu\.def\s+.*/hal\.prep'
                      r'\s+nodatasizes\s+_E32Dll', build_xml),
            'HAL prep did not use nodatasizes')

    generated = root / 'workspace/qemu-port/r1-console/hal/generated'
    hal_source = port / 'r1-console/hal'
    with tempfile.TemporaryDirectory(prefix='r1-hal-evidence-') as temp:
        config = Path(temp) / 'config.cpp'
        values_cpp = Path(temp) / 'values.cpp'
        subprocess.run([
            'perl', str(upstream / 'halservices/hal/halcfg.pl'), '-x',
            str(upstream / 'halservices/hal/inc/hal_data.h'),
            str(hal_source / 'config.hcf'), str(config),
        ], check=True, stdout=subprocess.DEVNULL)
        subprocess.run([
            'perl', str(upstream / 'halservices/hal/halcfg.pl'),
            str(upstream / 'halservices/hal/inc/hal_data.h'),
            str(hal_source / 'values.hda'), str(values_cpp),
        ], check=True, stdout=subprocess.DEVNULL)
        require(generated_body(config) ==
                generated_body(generated / 'config.cpp'),
                'preserved config.cpp not reproducible from current recipe')
        require(generated_body(values_cpp) ==
                generated_body(generated / 'values.cpp'),
                'preserved values.cpp not reproducible from current recipe')

    ekdata_path = ekdata_root / 'epoc32/release/x86/urel/ekdata.dll'
    require(sha256(ekdata_path) == EKDATA_SHA256,
            'accepted EKDATA image hash changed')
    for alias in ('EKDATA.DLL', 'EKDATA.00'):
        require(sha256(ekdata_root / 'staged/sys/bin' / alias) ==
                EKDATA_SHA256, 'EKDATA staged alias mismatch: ' + alias)
    ekdata = ekdata_path.read_bytes()
    ekdata_header = e32_header(ekdata)
    require(ekdata_header['export_count'] == 3 and
            ekdata_header['data_size'] == 0 and
            ekdata_header['bss'] == 0 and
            ekdata_header['data_reloc'] == 0,
            'EKDATA v3 export/data expectations changed')
    require(ekdata_header['uid1'] == 0x10000079 and
            ekdata_header['uid2'] == 0x1000008d and
            ekdata_header['sid'] == ekdata_header['uid3'] == 0x100039e0,
            'EKDATA UID/SID mismatch')

    hal_mmp = (hal_source / 'hal.mmp').read_text()
    ekdata_mmp = (port / 'r1-console/keymap/ekdata.mmp').read_text()
    require(re.search(r'^DEFFILE\s+.*halservices/hal/eabi/hal\.def\s*$',
                      hal_mmp, re.M),
            'main HAL recipe does not use the strict hal.def stem')
    require(re.search(r'^DEFFILE\s+\./ekdata\.def\s*$', ekdata_mmp, re.M),
            'main EKDATA recipe does not use ./ekdata.def')
    require((port / 'r1-console/keymap/ekdatau.def').read_bytes() ==
            (upstream / 'kernel/eka/eabi/ekdata.def').read_bytes(),
            'main local EKDATA def is not byte-identical to upstream')

    result = {
        'status': 'PASS',
        'scope': 'existing HAL v2 and accepted EKDATA v3 bytes; no build or execution',
        'hal': {
            'sha256': HAL_SHA256,
            'size': len(hal),
            'header': header,
            'ordinals': rows,
            'initial_value': {
                'bytes': len(initial),
                'attributes': len(values),
                'pe_offset': hex(initial_pe_offset),
                'e32_offset': hex(initial_e32_offset),
                'pe_match': True,
                'e32_match': True,
                'def_declared_size': 428,
                'prep_size_metadata_removed': True,
            },
            'valid_attributes': valid,
            'writable_dll_data': False,
        },
        'ekdata': {
            'sha256': EKDATA_SHA256,
            'size': len(ekdata),
            'export_count': ekdata_header['export_count'],
            'data_size': ekdata_header['data_size'],
            'bss': ekdata_header['bss'],
            'data_reloc': ekdata_header['data_reloc'],
            'aliases_identical': True,
        },
        'recipes': {
            'hal_mmp': sha256(hal_source / 'hal.mmp'),
            'hal_checker': sha256(hal_source / 'check_source.py'),
            'ekdata_mmp': sha256(port / 'r1-console/keymap/ekdata.mmp'),
            'ekdata_def': sha256(port / 'r1-console/keymap/ekdatau.def'),
        },
    }
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS: HAL six ordinals and 484-byte InitialValue match linked PE/E32 bytes')
    print('      EKDATA v3 hash/exports/data and byte-identical aliases verified')
    print('      HAL prep nodatasizes and current recipe files verified')
    print('HOST-ONLY: no target build, link, execution or console acceptance')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Host-only source/generator checks; never compiles or executes target code."""
import argparse
import ast
from pathlib import Path
import re
import subprocess
import tempfile


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def assignments(path):
    result = {}
    for line in path.read_text().splitlines():
        line = line.split('//', 1)[0].strip()
        if not line:
            continue
        key, value = (s.strip() for s in line.split('=', 1))
        require(key not in result, f'duplicate attribute {key}')
        result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', type=Path, required=True)
    parser.add_argument('--port', type=Path, required=True,
                        help='live port reference, not old-HEAD-only worktree')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    keymap = here.parent / 'keymap'
    up = args.upstream.resolve()
    expected = {
        'ECPU': 'x86', 'ECPUABI': 'x86', 'ESystemTickPeriod': '15625',
        'ENanoTickPeriod': '1000', 'EMemoryPageSize': '4096',
        'EKeyboardIndex': '0', 'EDisplayMode': '0',
        'EDisplayNumberOfScreens': '1',
    }
    require(assignments(here / 'values.hda') == expected, 'HAL value profile')
    require(assignments(here / 'config.hcf') == dict.fromkeys(expected, '0'),
            'HAL immutable-only config; no settable/property profile')
    asic = (args.port / 'variant/qemu_variant_asic.cpp').read_text()
    require(re.search(r'MsTickPeriod\(\).*?return\s+1000\s*;', asic, re.S),
            'port nano tick changed')
    timer = (up / 'kernel/eka/kernel/stimer.cpp').read_text()
    require('pQ->iTickPeriod=15625;' in timer, 'system tick changed')
    page = (up / 'kernel/eka/include/memmodel/epoc/direct/x86/mmboot.h').read_text()
    require('KPageShift=12;' in page and 'KPageSize=1<<KPageShift;' in page,
            'direct x86 page geometry changed')
    make = (args.port / 'Makefile').read_text()
    require('-D__X86__' in make and ' stimer ' in make, 'kernel selection changed')

    source = (keymap / 'keymap.cpp').read_text()
    header = (up / 'kernel/eka/include/e32keys.h').read_text()
    # Required singleton scan constants have explicit numeric definitions upstream.
    scans = {name: int(value, 16) for name, value in
             re.findall(r'(EStdKey\w+)\s*=\s*(0x[0-9a-fA-F]+)', header)}

    def scan_value(token):
        token = token.strip()
        return scans[token] if token in scans else ord(ast.literal_eval(token))

    def rows(name, end):
        section = source.split('#define ' + name + '(K)', 1)[1].split(end, 1)[0]
        # C character literals may themselves contain comma, quote or parentheses.
        char = r"'(?:\\.|[^'\\])'"
        token = rf'(?:{char}|\w+)'
        return re.findall(rf'K\(\s*({token})\s*,\s*({token})'
                          + (rf'\s*,\s*({token})' if name == 'SYMBOL_KEYS' else '')
                          + r'\s*\)', section)

    fixed = rows('FIXED_KEYS', '// Explicit ordinary')
    symbols = rows('SYMBOL_KEYS', '#define FIXED_BLOCK')
    require(len(fixed) == 22 and len(symbols) == 21, 'key row counts')
    all_scans = [scan_value(r[0]) for r in fixed + symbols] + list(range(65, 91))
    require(len(set(all_scans)) == len(all_scans), 'duplicate/overlapping scan blocks')
    require(all(1 <= s <= scans['EStdKeyEquals'] for s in all_scans), 'scan bounds')
    require(set(range(65, 91)).isdisjoint({scan_value(r[0]) for r in fixed + symbols}),
            'letter overlap')
    mapping = {r[0]: r[1] for r in fixed}
    for key in ('Backspace', 'Tab', 'Enter', 'Escape', 'LeftArrow', 'RightArrow',
                'UpArrow', 'DownArrow', 'LeftShift', 'RightShift', 'LeftAlt',
                'RightAlt', 'LeftCtrl', 'RightCtrl', 'CapsLock'):
        require(mapping.get('EStdKey' + key) == 'EKey' + key, 'required key ' + key)
    actual_symbols = {scan_value(s): (ast.literal_eval(b), ast.literal_eval(u))
                      for s, b, u in symbols}
    for digit, shift in zip('0123456789', ')!@#$%^&*('):
        require(actual_symbols[ord(digit)] == (digit, shift), 'US digit ' + digit)
    require(actual_symbols[scans['EStdKeyBackSlash']] == ('\\', '|'), 'US backslash')
    require(actual_symbols[scans['EStdKeySingleQuote']] == ("'", '"'), 'US quote')
    for array, expected_values in (('lowerLetters', list(range(97, 123))),
                                   ('upperLetters', list(range(65, 91))),
                                   ('ctrlLetters', list(range(1, 27)))):
        body = re.search(r'\b' + array + r'\[\]\s*=\s*\{(.*?)\}', source, re.S)[1]
        values = [ast.literal_eval(v.strip()) for v in body.split(',') if v.strip()]
        values = [ord(v) if isinstance(v, str) else v for v in values]
        require(values == expected_values, 'letter array ' + array)
    require('states[13]' in source and source.count('{0,NULL}') == 13, '13 empty states')
    for side in ('LeftShift', 'RightShift', 'LeftCtrl', 'RightCtrl', 'LeftAlt', 'RightAlt'):
        require('MODIFIER(EKey' + side + ', EModifier' + side in source,
                'side-specific modifier rule ' + side)
    require('ETurnOffModifier,side}' in source.replace(' ', ''), 'release clears side only')
    require('EStateNormal,EPassKeyThru,0' in source, 'default pass-through catch-all')
    require('EStateCtrlDigits,' not in source and 'EPassSpecialKeyThru' not in source,
            'excluded special-state actions')
    require(source.count('EXPORT_C void KeyData') == 3, 'three exports')
    definition = (up / 'kernel/eka/eabi/ekdata.def').read_text()
    for ordinal, symbol in enumerate(('KeyDataConvTable', 'KeyDataFuncTable', 'KeyDataSettings'), 1):
        require(re.search(symbol + r'.*?@\s*' + str(ordinal) + r'\s+NONAME', definition),
                'fixed upstream export ' + symbol)
    mmp = (keymap / 'ekdata.mmp').read_text()
    require('UID             0x1000008d 0x100039e0' in mmp, 'UID2/UID3')
    # Strict Unicode resolution appends u; ./ keeps the local def in this
    # directory instead of the platform-default def directory.
    require(re.search(r'^DEFFILE\s+\./ekdata\.def\s*$', mmp, re.M),
            'local EKDATA stem with explicit ./')
    require((keymap / 'ekdatau.def').read_bytes() ==
            (up / 'kernel/eka/eabi/ekdata.def').read_bytes(),
            'byte-identical local EKDATA frozen def')
    require(not re.search(r'^\s*NOSTRICTDEF\b', mmp, re.M | re.I),
            'no relaxed EKDATA exports')

    # Run the real upstream generator, not a substitute parser. Generated C++ is
    # temporary inside this owned HAL directory; no SDK/build-tree writes.
    hal = up / 'halservices/hal'
    hal_mmp = (here / 'hal.mmp').read_text()
    # Strict Unicode resolution appends u, so the genuine source stem is
    # hal.def (resolving halu.def), not a literal halu.def.
    require(re.search(r'^DEFFILE\s+.*halservices/hal/eabi/hal\.def\s*$',
                     hal_mmp, re.M), 'strict HAL stem resolves genuine halu.def')
    require(not re.search(r'^\s*NOSTRICTDEF\b', hal_mmp, re.M | re.I),
            'no relaxed HAL exports')
    require((hal / 'eabi/halu.def').is_file(), 'HAL fixed export source exists')
    with tempfile.TemporaryDirectory(prefix='.source-check-', dir=here) as temp:
        config = Path(temp) / 'config.cpp'
        values = Path(temp) / 'values.cpp'
        for options, input_name, output in ((['-x'], 'config.hcf', config),
                                             ([], 'values.hda', values)):
            subprocess.run(['perl', str(hal / 'halcfg.pl'), *options,
                            str(hal / 'inc/hal_data.h'), str(here / input_name), str(output)],
                           check=True)
        generated = config.read_text()
        props = generated.split('HalInternal::Properties[]=')[1].split('};')[0]
        implementations = generated.split('HalInternal::Implementation[]=')[1].split('};')[0]
        for name in expected:
            require(re.search(r'HAL::EValid,?\s*// ' + name + r'\s*$', props, re.M),
                    'generated valid property ' + name)
            require(re.search(r'NULL,?\s*// ' + name + r'\s*$', implementations, re.M),
                    'generated immutable implementation ' + name)
        require('HAL::ESettable' not in props, 'unexpected settable HAL property')
        for name in ('EDisplayContrast', 'ECPUArch', 'ECPUSpeed', 'EDisplayXPixels'):
            require(re.search(r'\b0,?\s*// ' + name + r'\s*$', props, re.M),
                    'unsupported attribute ' + name)
        initial = values.read_text()
        for name, value in {'ECPU': 2, 'ECPUABI': 6, **{
                n: int(v) for n, v in expected.items() if v.isdecimal()}}.items():
            require(re.search(r'\b' + str(value) + r',?\s*// ' + name + r'\s*$', initial, re.M),
                    'generated initial value ' + name)
    print('PASS: HAL source constants + genuine generator; key rows/bounds/duplicates;')
    print('      required keys, letter/digit maps, fixed export declarations, adopted')
    print('      DEFFILE recipes and UID metadata. HOST-ONLY: this checker does not')
    print('      compile, link or execute target images; see the closure report for')
    print('      separate historical HAL v2 and EKDATA v3 artifact evidence.')


if __name__ == '__main__':
    main()

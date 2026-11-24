#!/usr/bin/env python3
"""Build the GENUINE Symbian F32 / EFSRV sources from kernelhwsrv.

Everything in kernelhwsrv/ is READ-ONLY: this script only reads from it and
writes object files, archives and a manifest into qemu-port/genuine/.

Sources / defines / include order come from the genuine .mmp/.mmh files:
  fileserver/sfsrv/efsrv.mmp + efsrv.mmh   -> EFSRV client library (efsrv.dll)
  fileserver/sfile/efile.mmp + efile.mmh   -> EFSRV server EXE (efile.exe)
  fileserver/sfat32/efat32.mmp             -> F32 FAT32 file system (efat32.fsy)
  fileserver/fs_utils/                     -> shared F32 utilities

The host gcc 16 toolchain is used with the genuine exported EKA2 SDK headers
from the cached Raptor build root (preinclude gcc.h + variant .hrh), matching
the defines used by the genuine X86GCC UREL build.  -D_UNICODE is required:
SDK typedef TDes is TDes16 only in the genuine Unicode product build, and the
sfat32 sources use TDes where they expect 16-bit descriptors.

Usage: python3 genuine/build.py [--rebuild]
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KH = Path('/home/yuzi/Documents/Sybian REAL/kernelhwsrv')
FS = KH / 'userlibandfileserver' / 'fileserver'
SDK = Path('/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/include')
OUT = Path(__file__).resolve().parent
OBJ = OUT / 'obj'

CDEFS = ['__GCC32__', '__X86__', '__EPOC32__', '__GENERIC_X86__', '_UNICODE']
CFLAGS = ['-m32', '-Os', '-ffreestanding', '-fno-exceptions', '-fno-rtti',
          '-fpermissive', '-w', '-fno-pic', '-fno-pie', '-fno-stack-protector',
          # Genuine EUSER.DLL (prebuilt, MSVC/Symbian x86 ABI) returns small
          # structs (TThreadId from RThread::Id()) in EDX:EAX; gcc -m32's
          # default Itanium sret shifts the EUSER arg slots and faults KERN 4.
          '-freg-struct-return',
          # Genuine _L()/L"..." wchar_t literals are 16-bit under Symbian;
          # Linux gcc defaults to 32-bit, yielding UTF-32 _LIT strings that
          # EUSER's 16-bit CServer2 name compare rejects (KErrBadName -28).
          '-fshort-wchar']
PREINCLUDE = [str(SDK / 'gcc.h'), str(SDK / 'variant' / 'port_x86_rfile64_v1.hrh')]
INCS = [str(SDK / 'variant'), str(SDK), str(SDK / 'platform'),
        str(FS / 'group'), str(FS / 'inc'), str(FS / 'sfat32'),
        str(FS / 'sfat32' / 'inc'), str(FS / 'sfile'), str(FS / 'sfile' / 'traces'),
        str(FS / 'sfsrv'), str(FS / 'sfsrv' / 'traces'), str(FS / 'ftrace'),
        str(FS / 'fs_utils'), str(FS / 'srom'),
        str(KH / 'kernel' / 'eka' / 'include'), str(KH / 'kernel' / 'eka' / 'kernel')]


def cmd(args):
    r = subprocess.run(args, cwd=str(ROOT))
    if r.returncode:
        raise SystemExit(f'FAILED ({r.returncode}): {" ".join(map(str,args))}')


def compile_one(src, dst, extra_defs=()):
    dst.parent.mkdir(parents=True, exist_ok=True)
    args = ['gcc', *CFLAGS, *[f'-D{d}' for d in (*CDEFS, *extra_defs)],
            *[f'-include{p}' for p in PREINCLUDE],
            *[f'-I{p}' for p in INCS], '-c', str(src), '-o', str(dst)]
    cmd(args)


def group(name, spec):
    """spec: (srcdir, [filenames], extra defines)"""
    src_dir, files, extra = spec
    objs = []
    for f in files:
        # The server reuses three client-side translation units from sfsrv and
        # two support files from elsewhere in the kernel tree (see efile.mmh).
        candidates = [src_dir / f, FS / 'sfsrv' / f,
                      KH / 'kernel/eka/kernel' / f, FS / 'srom' / f]
        src = next((p for p in candidates if p.is_file()), None)
        if src is None:
            raise SystemExit(f'missing genuine source {src_dir / f}')
        o = OBJ / name / (src.stem + '.o')
        compile_one(src, o, extra)
        objs.append(o)
    archive = OUT / f'{name}.A'
    if archive.exists():
        archive.unlink()
    cmd(['ar', 'rcs', str(archive), *map(str, objs)])
    return objs, archive


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rebuild', action='store_true', help='delete obj/ first')
    args = ap.parse_args()
    if args.rebuild and OBJ.exists():
        shutil.rmtree(OBJ)
    OBJ.mkdir(parents=True, exist_ok=True)

    # efat32.mmp SOURCE list (sfat32) + fs_utils sources.
    f32 = sorted(p.name for p in (FS / 'sfat32').glob('*.cpp'))
    f32 += ['filesystem_utils.cpp', 'bit_vector.cpp']
    # efsrv.mmh SOURCE list (sfsrv = client-side F32 library).
    efsrv = ['cl_cdir.cpp', 'cl_cli.cpp', 'cl_debug.cpp', 'cl_dir.cpp',
             'cl_drive.cpp', 'cl_entry.cpp', 'cl_file.cpp', 'cl_find.cpp',
             'cl_fman.cpp', 'cl_fmt.cpp', 'cl_fraw.cpp', 'cl_ftext.cpp',
             'cl_main.cpp', 'cl_parse.cpp', 'cl_scan.cpp', 'cl_insecure.cpp',
             'cl_plugin.cpp', 'cl_blockmap.cpp', 'cl_clamp.cpp', 'cl_shim.cpp',
             'cl_notification.cpp']
    # efile.mmh SOURCE list (sfile = the real !FileServer EXE) plus the three
    # shared client translation units and the two non-WINS support files.
    efile = sorted(p.name for p in (FS / 'sfile').glob('sf_*.cpp')
                   if p.name != 'sf_lwins.cpp')
    efile += ['cl_file.cpp', 'cl_cli.cpp', 'cl_dir.cpp', 'byte_pair.cpp', 'sr_rom.cpp']

    specs = {
        'F32': (FS / 'sfat32',
                [f for f in f32 if (FS / 'sfat32' / f).is_file()]
                + [f for f in f32 if (FS / 'fs_utils' / f).is_file()], []),
        'EFSRV': (FS / 'sfsrv', efsrv, []),
        'EFILE': (FS / 'sfile', efile, ['SYMBIAN_PRIVATE_EFSRV']),
    }
    manifest = {'toolchain': subprocess.check_output(['gcc', '--version']).decode().splitlines()[0],
                'cflags': CFLAGS, 'defines': CDEFS,
                'preinclude': PREINCLUDE, 'includes': INCS, 'groups': {}}
    for name, (src_dir, files, extra) in specs.items():
        # Sources may live in two directories (sfat32 + fs_utils); compile
        # each from the directory that contains it.
        if name == 'F32':
            objs = []
            for f in files:
                d = FS / 'sfat32' if (FS / 'sfat32' / f).is_file() else FS / 'fs_utils'
                o = OBJ / name / (Path(f).stem + '.o')
                compile_one(d / f, o)
                objs.append(o)
            archive = OUT / 'F32.A'
            if archive.exists():
                archive.unlink()
            cmd(['ar', 'rcs', str(archive), *map(str, objs)])
        else:
            objs, archive = group(name, (src_dir, files, extra))
        manifest['groups'][name] = {
            'sources': files,
            'source_dir': str(src_dir),
            'objects': len(objs),
            'archive': archive.name,
            'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
        }
        print(f'{name}: {len(objs)} objects -> {archive.name} '
              f'({archive.stat().st_size} bytes)')

    # Precise account of the link blocker: EUSER ordinals required by each
    # archive, resolved against the genuine x86 euseru.def.
    defp = KH / 'kernel/eka/bx86gcc/euseru.def'
    ords = {}
    if defp.is_file():
        for line in defp.read_text(errors='replace').splitlines():
            import re
            m = re.match(r'\s*(\S+)\s+@\s+(\d+)', line)
            if m:
                ords[m.group(1)] = int(m.group(2))
    for name in specs:
        undef = subprocess.run(['nm', '-u', *map(str, OBJ.glob(f'{name}/*.o'))],
                               capture_output=True, text=True).stdout
        syms = {ln.split()[-1] for ln in undef.splitlines() if ' U ' in ln}
        need = sorted({ords[s] for s in syms if s in ords})
        manifest['groups'][name]['euser_ordinals'] = len(need)
        manifest['groups'][name]['euser_max_ordinal'] = max(need, default=0)

    (OUT / 'BUILD_MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('manifest: BUILD_MANIFEST.json')


if __name__ == '__main__':
    main()

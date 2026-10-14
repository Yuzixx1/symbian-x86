#!/usr/bin/env python3
"""Build one SDK application into APP.E32.

All toolchain paths stay inside Python, so workspace paths with spaces
(which GNU make word-splitting cannot survive) work fine. The app's
Makefile is a thin wrapper around this script.

Usage: buildapp.py --sdk SDKDIR --app NAME --src main.c [--src ...]
       [--out DIR] [--cc gcc]
"""
import argparse
import os
import subprocess
import sys
from pathlib import Path

USER_FLAGS = ['-m32', '-march=i486', '-Os', '-ffreestanding', '-fno-pic',
              '-fno-pie', '-fno-builtin', '-fno-stack-protector',
              '-fno-asynchronous-unwind-tables', '-fno-unwind-tables',
              '-mpreferred-stack-boundary=2', '-Wall', '-Wextra']
RUNTIME_SRCS = ['start.S', 'runtime.c', 'malloc.c']


def run(command, **kwargs):
    print('+', ' '.join(str(c) for c in command))
    subprocess.run(command, check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sdk', required=True)
    parser.add_argument('--app', required=True)
    parser.add_argument('--src', action='append', default=[])
    parser.add_argument('--out', default='.')
    parser.add_argument('--cc', default=os.environ.get('CC', 'gcc'))
    args = parser.parse_args()
    sdk = Path(args.sdk)
    port = sdk / '../qemu-port'
    port = port.resolve()
    out = Path(args.out)
    build = out / 'build'
    build.mkdir(parents=True, exist_ok=True)
    user = port / 'user'
    tools = port / 'tools'
    libgcc = subprocess.run([args.cc, '-m32', '-print-libgcc-file-name'],
                            check=True, capture_output=True,
                            text=True).stdout.strip()
    objs = []
    for src in RUNTIME_SRCS + args.src:
        src_path = Path(src)
        if not src_path.is_absolute():
            base = user if src in RUNTIME_SRCS else out
            src_path = base / src
        obj = build / (src_path.stem + '.o')
        command = [args.cc]
        if src_path.suffix == '.S':
            command += ['-m32']
        else:
            command += USER_FLAGS + ['-I' + str(user)]
        command += ['-c', str(src_path), '-o', str(obj)]
        if (not obj.exists() or
                src_path.stat().st_mtime > obj.stat().st_mtime):
            run(command)
        objs.append(str(obj))
    elf = build / (args.app + '.elf')
    run(['ld', '-m', 'elf_i386', '--emit-relocs', '-T', str(user / 'link.ld'),
         '-o', str(elf)] + objs + [libgcc])
    e32 = out / (args.app + '.E32')
    env = dict(os.environ)
    env['PYTHONPATH'] = str(tools) + os.pathsep + env.get('PYTHONPATH', '')
    run([sys.executable, str(tools / 'elf2e32.py'), str(elf), str(e32)],
        env=env)
    run([sys.executable, str(tools / 'e32read.py'), str(e32), '--quiet'],
        env=env)
    print(f'built {e32}')
    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""Chunks: private create/read/adjust plus cross-process global sharing."""
import re
import subprocess
from pathlib import Path
import test_cli

test_cli.DISK = '/tmp/opencode/chunk-test.img'
test_cli.LOG = '/tmp/opencode/chunk-test.log'


def main():
    subprocess.run(['make', 'build/kernfull.elf', 'userland'], check=True)
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'], check=True)
    for name in ('CHUNK', 'CHKOPEN'):
        subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                        f'build/user/{name}.E32', f'::{name}.E32'], check=True)
    # 15-char filename: LFN exec must resolve via the long-name matcher.
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/CHKOPEN.E32', '::VERYLONGNAME.E32'], check=True)
    with open(test_cli.LOG, 'wb') as log:
        vm = subprocess.Popen([
            'qemu-system-i386', '-M', 'q35', '-m', '128', '-display', 'none',
            '-serial', 'stdio', '-kernel', 'build/kernfull.elf',
            '-drive', f'file={test_cli.DISK},format=raw,if=none,id=d0',
            '-device', 'ich9-ahci,id=sata', '-device', 'ide-hd,drive=d0,bus=sata.0',
            '-monitor', 'none', '-no-reboot'], stdin=subprocess.PIPE,
            stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = test_cli.wait_for(log, b'EKA2> ', timeout=30)
            vm.stdin.write(b'mount\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'C:/> ', pos)
            # Creator stays parked (holding the global chunk) while the
            # opener runs, so the two programs genuinely share memory.
            pos = test_cli.run_command(vm, log, pos, 'CHUNK.E32',
                                       b'CHUNK: global ready', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'CHKOPEN.E32',
                                       b'CHUNKOPEN: shared ok', b'C:/> ')
            # Same program under a 15-char (LFN) name must exec too.
            pos = test_cli.run_command(vm, log, pos, 'VERYLONGNAME.E32',
                                       b'CHUNKOPEN: shared ok', b'C:/> ')
            before = pos
            pos = test_cli.run_command(vm, log, pos, 'ps', b'CHUNK', b'C:/> ')
            with open(test_cli.LOG, 'rb') as source:
                source.seek(before)
                listing = source.read(pos - before).decode('utf-8', 'replace')
            chunk_id = re.search(r'(\S*CHUNK\S*).*?id=(\d+)', listing).group(2)
            pos = test_cli.run_command(vm, log, pos, f'kill {chunk_id}',
                                       b'killed', b'C:/> ')
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'FAULT:' not in output and 'Exc ' not in output, output
            assert 'CHUNK: private ok' in output, output
            assert output.count('CHUNKOPEN: shared ok') == 2, output
            print('PASS: private chunk, adjust, cross-process global share, LFN exec')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

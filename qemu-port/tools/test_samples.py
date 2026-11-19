#!/usr/bin/env python3
"""Sample programs: build them, then run each on the OS."""
import subprocess
import sys
import time
from pathlib import Path
import test_cli

HERE = Path(__file__).resolve().parent
PORT = HERE.parent
SDK = PORT.parent / 'sdk'
test_cli.DISK = '/tmp/opencode/samples-test.img'
test_cli.LOG = '/tmp/opencode/samples-test.log'


def main():
    for app in ('calc', 'guess', 'wc', 'rev', 'fib', 'mem'):
        subprocess.run(['make', '-C', str(SDK / 'samples' / app)], check=True)
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'],
                   check=True, cwd=PORT, stdout=subprocess.DEVNULL)
    for app in ('CALC', 'GUESS', 'WC', 'REV', 'FIB', 'MEM'):
        subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                        str(SDK / 'samples' / app.lower() / f'{app}.E32'),
                        f'::{app}.E32'], check=True)
    with open(test_cli.LOG, 'wb') as log:
        vm = subprocess.Popen([
            'qemu-system-i386', '-M', 'q35', '-m', '128', '-display', 'none',
            '-serial', 'stdio', '-kernel', str(PORT / 'build/kernfull.elf'),
            '-drive', f'file={test_cli.DISK},format=raw,if=none,id=d0',
            '-device', 'ich9-ahci,id=sata', '-device', 'ide-hd,drive=d0,bus=sata.0',
            '-monitor', 'none', '-no-reboot'], stdin=subprocess.PIPE,
            stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = test_cli.wait_for(log, b'EKA2> ', timeout=30)
            vm.stdin.write(b'mount\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'C:/> ', pos)
            pos = test_cli.run_command(vm, log, pos, 'CALC 12 * 7',
                                       b'CALC: 84', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'CALC 20 - 5',
                                       b'CALC: 15', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'REV hello',
                                       b'REV: olleh', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'FIB 10',
                                       b'FIB: 0 1 1 2 3 5 8 13 21 34', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'MEM',
                                       b'MEM: 24 blocks', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'WC HELLO.TXT',
                                       b'WC:', b'C:/> ')
            # Interactive sample: lease, guess, quit.
            pos = test_cli.run_command(vm, log, pos, 'GUESS',
                                       b'GUESS:', b'C:/> ')
            vm.stdin.write(b'50\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'GUESS:', pos, timeout=20)
            vm.stdin.write(b'quit\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'GUESS: bye', pos, timeout=20)
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'FAULT:' not in output and 'Exc ' not in output, output
            print('PASS: calc, rev, fib, mem, wc, guess')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

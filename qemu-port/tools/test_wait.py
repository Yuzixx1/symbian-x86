#!/usr/bin/env python3
"""Blocking wait: user-mode logon API plus the kernel exit-ring shell command."""
import re
import subprocess
import time
from pathlib import Path
import test_cli

test_cli.DISK = '/tmp/opencode/wait-test.img'
test_cli.LOG = '/tmp/opencode/wait-test.log'


def wait_for_n(marker, start, count, timeout=15):
    """Wait until `count` copies of marker appear at/after `start`."""
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        with open(test_cli.LOG, 'rb') as source:
            source.seek(start)
            if source.read().count(marker) >= count:
                return
        time.sleep(0.1)
    raise RuntimeError('only %d/%d %r' % (
        Path(test_cli.LOG).read_bytes()[start:].count(marker), count, marker))


def no_output(marker, start, duration):
    """Assert marker does not appear within `duration` seconds."""
    until = time.monotonic() + duration
    while time.monotonic() < until:
        with open(test_cli.LOG, 'rb') as source:
            source.seek(start)
            if marker in source.read():
                raise AssertionError('premature ' + repr(marker))
        time.sleep(0.1)


def main():
    subprocess.run(['make', 'build/kernfull.elf', 'build/user/IPCSRV.E32',
                    'build/user/ECHO.E32', 'build/user/WAITFOR.E32'], check=True)
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'],
                   check=True)
    for name in ('IPCSRV', 'ECHO', 'WAITFOR'):
        subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                        f'build/user/{name}.E32', f'::{name}.E32'], check=True)
    with open(test_cli.LOG, 'wb') as log:
        vm = subprocess.Popen([
            'qemu-system-i386', '-M', 'q35', '-m', '128', '-display', 'none',
            '-serial', 'stdio', '-kernel', 'build/kernfull.elf',
            '-drive', f'file={test_cli.DISK},format=raw,if=none,id=d0',
            '-device', 'ich9-ahci,id=sata', '-device', 'ide-hd,drive=d0,bus=sata.0',
            '-monitor', 'none', '-no-reboot'], stdin=subprocess.PIPE,
            stdout=log, stderr=subprocess.STDOUT)
        try:
            # Boot prompt is EKA2> on some variants, C:/> on others.
            try:
                pos = test_cli.wait_for(log, b'EKA2> ', timeout=30)
            except RuntimeError:
                pos = test_cli.wait_for(log, b'C:/> ', timeout=30)
            if Path(test_cli.LOG).read_bytes().rfind(b'EKA2> ') > pos - 7:
                vm.stdin.write(b'mount\n'); vm.stdin.flush()
                pos = test_cli.wait_for(log, b'C:/> ', pos)
            pos = test_cli.run_command(vm, log, pos, 'IPCSRV.E32',
                                       b'IPCSRV: ready', b'C:/> ')
            before = pos
            pos = test_cli.run_command(vm, log, pos, 'ps', b'IPCSRV', b'C:/> ')
            with open(test_cli.LOG, 'rb') as source:
                source.seek(before)
                listing = source.read(pos - before).decode('utf-8', 'replace')
            server_id = re.search(r'(\S*IPCSRV\S*).*?id=(\d+)', listing).group(2)

            # User API: live process times out and reports KErrTimedOut (-33).
            marker = f'WAITFOR: result pid={server_id} r=-33'.encode()
            t0 = pos
            pos = test_cli.run_command(vm, log, pos, f'waitfor.e32 {server_id} 1000',
                                       b'started', b'C:/> ')
            wait_for_n(marker, t0, 1)
            pos = Path(test_cli.LOG).stat().st_size

            # User API: two waiters post ProcessLogon before the target dies,
            # then both are released by the same last-thread death completion.
            done = f'WAITFOR: result pid={server_id} r=0 t=0'.encode()
            for _ in range(2):
                before = pos
                pos = test_cli.run_command(vm, log, pos,
                                           f'waitfor.e32 {server_id} 8000',
                                           b'started', b'C:/> ')
                no_output(done, before, 1)
            pos = test_cli.run_command(vm, log, pos, f'kill {server_id}',
                                       b'killed', b'C:/> ')
            wait_for_n(done, before, 2)
            pos = Path(test_cli.LOG).stat().st_size

            # Kernel shell wait still reports post-mortem records.
            pos = test_cli.run_command(vm, log, pos, 'wait 999999 1',
                                       b'no record', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'ECHO.E32 bye',
                                       b'ECHO: bye', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'ECHO.E32 again',
                                       b'ECHO: again', b'C:/> ')
            before = pos
            pos = test_cli.run_command(vm, log, pos, 'exits', b'ECHO', b'C:/> ')
            with open(test_cli.LOG, 'rb') as source:
                source.seek(before)
                exits = source.read(pos - before).decode('utf-8', 'replace')
            echo_id = re.search(r'ECHO.*?id=(\d+)', exits).group(1)
            pos = test_cli.run_command(vm, log, pos, f'wait {echo_id}',
                                       b'reason=0', b'C:/> ')
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'FAULT:' not in output and 'Exc ' not in output, output
            print('PASS: user wait timeout, blocking + concurrent waiters, exit records')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

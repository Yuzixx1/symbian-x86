#!/usr/bin/env python3
"""Fault attribution: a crashing user program must name itself in the
reaper's exit line while the shell and later programs survive."""
import subprocess
from pathlib import Path
import test_cli

test_cli.DISK = '/tmp/opencode/crash-test.img'
test_cli.LOG = '/tmp/opencode/crash-test.log'


def main():
    subprocess.run(['make', 'build/kernfull.elf', 'userland'], check=True)
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'], check=True)
    for name in ('CRASH', 'ECHO'):
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
            pos = test_cli.wait_for(log, b'EKA2> ', timeout=30)
            vm.stdin.write(b'mount\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'C:/> ', pos)
            pos = test_cli.run_command(vm, log, pos, 'exec CRASH.E32',
                                       b'CRASH: about to fault', b'C:/> ')
            # Shell survives the fault and runs the next program.
            pos = test_cli.run_command(vm, log, pos, 'exec ECHO.E32 alive',
                                       b'ECHO: alive', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'ps',
                                       b'PortFile channels: 0', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exits',
                                       b'CRASH.E32', b'C:/> ')
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'FAULT:' not in output, output
            assert 'CRASH: survived' not in output, output
            assert 'prog=CRASH.E32' in output, output
            print('PASS: fault attributed, shell survived')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

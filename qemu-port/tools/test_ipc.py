#!/usr/bin/env python3
"""Real Symbian kernel IPC across separately loaded ring-3 processes."""
import subprocess
from pathlib import Path
import test_cli

test_cli.DISK = '/tmp/opencode/ipc-test.img'
test_cli.LOG = '/tmp/opencode/ipc-test.log'


def main():
    subprocess.run(['make', 'build/kernfull.elf', 'userland'], check=True)
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'], check=True)
    for name in ('IPCSRV', 'IPCCLI'):
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
            def command(text, expected):
                nonlocal pos
                pos = test_cli.run_command(vm, log, pos, text, expected, b'C:/> ')
            command('exec IPCCLI.E32', b'IPCCLI: connect failed')
            for cycle in range(2):
                command('exec IPCSRV.E32', b'IPCSRV: ready')
                for client in range(3):
                    command('exec IPCCLI.E32', b'IPCCLI: 32 replies and unsupported request verified')
                command('exec IPCCLI.E32 stop', b'IPCCLI: stop acknowledged')
                command('exec IPCCLI.E32', b'IPCCLI: connect failed')
            command('exec IPCSRV.E32', b'IPCSRV: ready')
            command('exec IPCCLI.E32 die', b'IPCCLI: pending request received KErrServerTerminated')
            command('exec IPCCLI.E32', b'IPCCLI: connect failed')
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'FAULT:' not in output and 'Exc ' not in output, output
            assert output.count('IPCCLI: binary descriptors, offsets, overflow and type checks verified') == 6, output
            assert output.count('IPCCLI: 16-bit descriptors verified') == 6, output
            assert output.count('IPCSRV: receive cancellation verified') == 3, output
            print('PASS: connect, repeated request/reply, disconnect, shutdown, restart')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Concurrency: server + clients + shell diagnostics alive together; killing
a session-owning server recovers cleanly and the exit ring records it."""
import re
import subprocess
from pathlib import Path
import test_cli

test_cli.DISK = '/tmp/opencode/conc-test.img'
test_cli.LOG = '/tmp/opencode/conc-test.log'


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
            # Server blocks in genuine receive while shell + clients run.
            pos = test_cli.run_command(vm, log, pos, 'exec IPCSRV.E32',
                                       b'IPCSRV: ready', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec IPCCLI.E32',
                                       b'unsupported request verified', b'C:/> ')
            # Shell diagnostics stay responsive under load.
            pos = test_cli.run_command(vm, log, pos, 'date', b'20', b'C:/> ')
            before = pos
            pos = test_cli.run_command(vm, log, pos, 'ps', b'IPCSRV', b'C:/> ')
            with open(test_cli.LOG, 'rb') as source:
                source.seek(before)
                listing = source.read(pos - before).decode('utf-8', 'replace')
            match = re.search(r'(\S*IPCSRV\S*).*?id=(\d+)', listing)
            assert match, listing
            server_id = match.group(2)
            # Kill the session-owning server; clients must fail to connect.
            pos = test_cli.run_command(vm, log, pos, f'kill {server_id}',
                                       b'killed', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec IPCCLI.E32',
                                       b'IPCCLI: connect failed', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exits',
                                       b'IPCSRV', b'C:/> ')
            # Restart and prove full recovery.
            pos = test_cli.run_command(vm, log, pos, 'exec IPCSRV.E32',
                                       b'IPCSRV: ready', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec IPCCLI.E32',
                                       b'unsupported request verified', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec IPCCLI.E32 stop',
                                       b'stop acknowledged', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'ps',
                                       b'PortFile channels: 0', b'C:/> ')
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'FAULT:' not in output and 'Exc ' not in output, output
            print('PASS: concurrent server/clients/shell, kill recovery, exit ring')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

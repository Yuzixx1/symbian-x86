#!/usr/bin/env python3
"""Interactive program: console lease, typed input, sleep, clock, random."""
import subprocess
from pathlib import Path
import test_cli

test_cli.DISK = '/tmp/opencode/interact-test.img'
test_cli.LOG = '/tmp/opencode/interact-test.log'


def main():
    subprocess.run(['make', 'build/kernfull.elf', 'userland'], check=True)
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/INTERACT.E32', '::INTERACT.E32'], check=True)
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
            # Start the program; it leases the console before prompting.
            start = pos
            vm.stdin.write(b'INTERACT.E32\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'Name? ', pos, timeout=30)
            # Only now type: anything earlier would be a shell command.
            vm.stdin.write(b'Ada\n')
            vm.stdin.flush()
            # The program output and the shell's post-exec prints interleave
            # nondeterministically (real sleeps changed the old ordering), so
            # sync on the announce from the run start, then the next prompt.
            pos = test_cli.wait_for(log, b'INTERACT.E32 started', start, timeout=30)
            pos = test_cli.wait_for(log, b'C:/> ', pos, timeout=30)
            # The prompt announces an asynchronous process, not its completion.
            # Never inspect the log or terminate QEMU until the user program
            # has consumed the input and finished.
            pos = test_cli.wait_for(log, b'Hello Ada!', pos, timeout=30)
            pos = test_cli.wait_for(log, b'INTERACT: done', pos, timeout=30)
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'Hello Ada!' in output, output
            assert 'INTERACT: done' in output, output
            assert 'INTERACT: ready 20' in output, output
            assert 'random=' in output, output
            assert 'FAULT:' not in output and 'Exc ' not in output, output
            pos = test_cli.run_command(vm, log, pos, 'ps',
                                       b'PortFile channels: 0', b'C:/> ')
            print('PASS: lease, typed input, sleep, clock, random, release')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""SDK end-to-end: scaffold a new app, build it and all examples through
only SDK tooling, then boot the OS and run the SDK-built binaries."""
import shutil
import subprocess
import sys
import time
from pathlib import Path
import test_cli

HERE = Path(__file__).resolve().parent
PORT = HERE.parent
SDK = PORT.parent / 'sdk'
TWO = '/tmp/opencode'
test_cli.DISK = TWO + '/sdk-test.img'
test_cli.LOG = TWO + '/sdk-test.log'


def main():
    # Scaffolder produces a buildable app outside the workspace tree.
    shutil.rmtree(TWO + '/SdkTmp', ignore_errors=True)
    subprocess.run([sys.executable, str(SDK / 'tools/newapp.py'),
                    'SdkTmp', TWO], check=True)
    subprocess.run(['make', '-C', TWO + '/SdkTmp'], check=True)
    assert (Path(TWO) / 'SdkTmp/SDKTMP.E32').exists()
    # Every shipped example builds through the SDK alone.
    for app in ('hello', 'args', 'fileview', 'prompt', 'asleep'):
        subprocess.run(['make', '-C', str(SDK / 'examples' / app)], check=True)
    # And the SDK-built binaries actually run on the OS.
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'],
                   check=True, cwd=PORT, stdout=subprocess.DEVNULL)
    (Path(TWO) / 'sdkhello.txt').write_text('sdk fileview works\n')
    for example, short in [('hello/HELLO', 'HELLO'), ('asleep/ASLEEP', 'ASLEEP'),
                           ('fileview/FILEVIEW', 'FILEVIEW')]:
        subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                        str(SDK / 'examples' / (example + '.E32')),
                        f'::{short}.E32'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    TWO + '/sdkhello.txt', '::SDKHELLO.TXT'], check=True)
    kern = PORT / 'build/kernfull.elf'
    with open(test_cli.LOG, 'wb') as log:
        vm = subprocess.Popen([
            'qemu-system-i386', '-M', 'q35', '-m', '128', '-display', 'none',
            '-serial', 'stdio', '-kernel', str(kern),
            '-drive', f'file={test_cli.DISK},format=raw,if=none,id=d0',
            '-device', 'ich9-ahci,id=sata', '-device', 'ide-hd,drive=d0,bus=sata.0',
            '-monitor', 'none', '-no-reboot'], stdin=subprocess.PIPE,
            stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = test_cli.wait_for(log, b'EKA2> ', timeout=30)
            time.sleep(0.3)
            vm.stdin.write(b'mount\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'C:/> ', pos)
            time.sleep(0.2)
            vm.stdin.write(b'exec HELLO.E32\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'Hello from an SDK app!', pos, timeout=20)
            vm.stdin.write(b'exec FILEVIEW.E32 SDKHELLO.TXT\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'sdk fileview works', pos, timeout=20)
            vm.stdin.write(b'exec ASLEEP.E32\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'yielded ok', pos, timeout=20)
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'FAULT:' not in output and 'Exc ' not in output, output
            print('PASS: scaffold, example builds, SDK binaries run')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

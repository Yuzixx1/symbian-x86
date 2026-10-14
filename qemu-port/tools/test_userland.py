#!/usr/bin/env python3
"""Boot compiled ECHO and verify genuine user-side command-line copying."""
import subprocess
from pathlib import Path
import test_cli

ROOT = Path('/tmp/opencode')
test_cli.DISK = str(ROOT / 'userland-test.img')
test_cli.LOG = str(ROOT / 'userland-test.log')


def main():
    subprocess.run(['make', 'userland'], check=True)
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/ECHO.E32', '::ECHO.E32'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/TYPE.E32', '::TYPE.E32'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/CP.E32', '::CP.E32'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/FILECHK.E32', '::FILECHK.E32'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/CWD.E32', '::CWD.E32'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/MALLOC.E32', '::MALLOC.E32'], check=True)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    'build/user/SLEEP.E32', '::SLEEP.E32'], check=True)
    (ROOT / 'too-large.txt').write_bytes(b'Z' * 66560)
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    str(ROOT / 'too-large.txt'), '::LARGE.TXT'], check=True)
    (ROOT / 'mid-size.txt').write_bytes(b'START-' + b'A' * 5000 + b'-END-MID-OK')
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    str(ROOT / 'mid-size.txt'), '::MID.TXT'], check=True)
    (ROOT / 'type-content.txt').write_text('Hello from user file!\nSecond line.\n')
    subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                    str(ROOT / 'type-content.txt'), '::HELLO.TXT'], check=True)
    with open(test_cli.LOG, 'wb') as log:
        vm = subprocess.Popen([
            'qemu-system-i386', '-M', 'q35', '-m', '128', '-display', 'none',
            '-serial', 'stdio', '-kernel', 'build/kernfull.elf',
            '-drive', f'file={test_cli.DISK},format=raw,if=none,id=d0',
            '-device', 'ich9-ahci,id=sata', '-device', 'ide-hd,drive=d0,bus=sata.0',
            '-monitor', 'none', '-no-reboot'], stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT)
        try:
            pos = test_cli.wait_for(log, b'EKA2> ', timeout=30)
            vm.stdin.write(b'mount\n')
            vm.stdin.flush()
            pos = test_cli.wait_for(log, b'C:/> ', pos)
            cases = ['', 'hello world', '"two words"  three',
                     'one two three four five six seven eight nine ten', 'x' * 100]
            for tail in cases:
                command = 'exec C:ECHO.E32' + (' ' + tail if tail else '')
                start = pos
                pos = test_cli.run_command(vm, log, pos, command,
                                           b'ECHO.E32 started', b'C:/> ')
                test_cli.wait_for(log, ('ECHO: ' + tail).encode(), start)
                print('verified user output:', repr(tail))
            # User-mode file access over genuine logical channels.
            pos = test_cli.run_command(vm, log, pos, 'exec C:TYPE.E32 C:\\HELLO.TXT',
                                       b'TYPE.E32 started', b'C:/> ')
            test_cli.wait_for(log, b'Hello from user file!', pos - 800)
            test_cli.wait_for(log, b'TYPE: EOF', pos - 800)
            print('verified user file read')
            pos = test_cli.run_command(vm, log, pos, 'exec C:TYPE.E32 C:\\MISSING.TXT',
                                       b'open failed', b'C:/> ')
            print('verified user open-fail path')
            # User-mode write: CP copies via PortFile read+write, shell reads back.
            pos = test_cli.run_command(vm, log, pos, 'exec C:CP.E32 C:\\HELLO.TXT C:\\COPY.TXT',
                                       b'CP: copied', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'type C:\\COPY.TXT',
                                       b'Hello from user file!', b'C:/> ')
            print('verified user file write')
            pos = test_cli.run_command(vm, log, pos, 'exec CP.E32 "HELLO.TXT" "QUOTE.TXT"',
                                       b'CP: copied', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'type QUOTE.TXT',
                                       b'Hello from user file!', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec CP.E32 HELLO.TXT QUOTE.TXT extra',
                                       b'CP: use two paths', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec CP.E32 "HELLO.TXT QUOTE.TXT',
                                       b'CP: use two paths', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec CP.E32 LARGE.TXT COPY.TXT',
                                       b'open source failed', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'type COPY.TXT',
                                       b'Hello from user file!', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec CP.E32 MID.TXT MIDCOPY.TXT',
                                       b'CP: copied', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec TYPE.E32 MIDCOPY.TXT',
                                       b'END-MID-OK', b'C:/> ')
            for unused in range(3):
                pos = test_cli.run_command(vm, log, pos, 'exec FILECHK.E32',
                                           b'FILECHECK: independent handles', b'C:/> ')
                pos = test_cli.run_command(vm, log, pos, 'ps',
                                           b'PortFile channels: 0', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'md WORK', b'', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'copy HELLO.TXT WORK\\NOTE.TXT', b'', b'C:/> ')
            pos = test_cli.run_command(vm, log, pos, 'cd WORK', b'', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec C:\\TYPE.E32 NOTE.TXT',
                                       b'Hello from user file!', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec C:\\CP.E32 NOTE.TXT NEW.TXT',
                                       b'CP: copied', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'type NEW.TXT',
                                       b'Hello from user file!', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec C:\\TYPE.E32 ..\\HELLO.TXT',
                                       b'Hello from user file!', b'C:/WORK/> ')
            print('verified inherited CWD, relative writes, and parent traversal')
            pos = test_cli.run_command(vm, log, pos, 'md SUB', b'', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'copy NOTE.TXT SUB\\NOTE.TXT', b'', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec C:\\CWD.E32',
                                       b'CWD: relative open ok', b'C:/WORK/> ')
            test_cli.wait_for(log, b'C:\\WORK', pos - 1500)
            pos = test_cli.run_command(vm, log, pos, 'exec C:\\CWD.E32 SUB',
                                       b'CWD-NEW:', b'C:/WORK/> ')
            test_cli.wait_for(log, b'C:\\WORK\\SUB', pos - 1500)
            pos = test_cli.run_command(vm, log, pos, 'exec C:\\CWD.E32 \\NOPE',
                                       b'CWD: chdir failed', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'ps',
                                       b'PortFile channels: 0', b'C:/WORK/> ')
            print('verified mutable per-process CWD, shell isolation, and bad-dir rejection')
            pos = test_cli.run_command(vm, log, pos, 'exec C:\\MALLOC.E32',
                                       b'MALLOC: allocator verified', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'reap',
                                       b'reaped 1', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'exits',
                                       b'MALLOC.E32', b'C:/WORK/> ')
            pos = test_cli.run_command(vm, log, pos, 'exec C:\\SLEEP.E32',
                                       b'SLEEP: done', b'C:/WORK/> ')
            output = Path(test_cli.LOG).read_text(errors='replace')
            assert 'Exc ' not in output, output
            assert 'FAULT:' not in output, output
            assert 'reason=0' in output, 'missing retained-process exit status'
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Diagnostic: stop the genuine EFILE boot in QEMU with GDB and dump panics."""
import os
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import prove_efile_genuine as pg  # noqa: E402

KERNEL = pg.KERNEL
LOG = Path('/tmp/opencode/gdb_efile_serial.log')
GDBLOG = Path('/tmp/opencode/gdb_efile_gdb.log')
BPS = (
    ('0x14e980', 'K::PanicCurrentThread'),
    ('0x14e950', 'K::PanicKernExec'),
    ('0x159960', 'K::Fault'),
)


def main():
    pg.build_disk()
    for p in (LOG, GDBLOG):
        if p.exists():
            p.unlink()
    qemu = [
        'qemu-system-i386', '-M', 'q35', '-m', '128',
        '-netdev', 'user,id=n0', '-device', 'e1000,netdev=n0',
        '-audiodev', 'none,id=a0', '-device', 'AC97,audiodev=a0',
        '-vga', 'std', '-display', 'none', '-serial', 'stdio',
        '-kernel', str(KERNEL),
        '-drive', f'file={pg.DISK},format=raw,snapshot=on,if=none,id=d0',
        '-device', 'ich9-ahci,id=sata', '-device', 'ide-hd,drive=d0,bus=sata.0',
        '-monitor', 'none', '-no-reboot', '-s', '-S',
    ]
    with LOG.open('wb') as log, GDBLOG.open('wb') as gdbout:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=log,
                              stderr=subprocess.STDOUT)
        gd = subprocess.Popen(['gdb', '-q', str(KERNEL)], stdin=subprocess.PIPE,
                              stdout=gdbout, stderr=subprocess.STDOUT, text=True,
                              bufsize=1)
        try:
            def g(line, wait=0.7):
                gd.stdin.write(line + '\n')
                gd.stdin.flush()
                time.sleep(wait)
            g('set pagination off')
            g('target remote :1234', 2)
            for addr, name in BPS:
                g(f'break *{addr}')
            g('c', 3)
            pos = pg.wait_for(LOG, pg.PROMPT, timeout=120)
            print('prompt; launching EFILEX', flush=True)
            vm.stdin.write(b'efilex\n')
            vm.stdin.flush()
            pg.wait_for(LOG, b'EFILE: genuine file server starting', pos, 60)
            print('EFILE entered; waiting for GDB stop', flush=True)
            # keep feeding 'c' while the kernel runs; a breakpoint stops it
            for _ in range(40):
                time.sleep(0.5)
                txt = GDBLOG.read_text(errors='replace')
                if 'Breakpoint' in txt or 'Program received signal' in txt:
                    break
            for line in ('bt 16', 'info registers eax ecx edx eip esp ebp',
                         'x/12wx $esp', 'c'):
                g(line, 1)
            time.sleep(2)
        finally:
            gd.stdin.write('quit\n')
            gd.stdin.flush()
            gd.terminate()
            vm.terminate()
            try:
                vm.wait(timeout=5)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    print('GDB:', GDBLOG.read_text(errors='replace')[-6000:])
    print('SERIAL tail:', LOG.read_text(errors='replace')[-1200:])
    return 0


if __name__ == '__main__':
    sys.exit(main())

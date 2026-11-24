#!/usr/bin/env python3
"""Diagnostic: log fast-exec handle arguments issued by genuine EFILE."""
import signal
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import prove_efile_genuine as pg  # noqa: E402

LOG = Path('/tmp/opencode/gdb_handles_serial.log')
GDBLOG = Path('/tmp/opencode/gdb_handles_gdb.log')


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
        '-kernel', str(pg.KERNEL),
        '-drive', f'file={pg.DISK},format=raw,snapshot=on,if=none,id=d0',
        '-device', 'ich9-ahci,id=sata', '-device', 'ide-hd,drive=d0,bus=sata.0',
        '-monitor', 'none', '-no-reboot', '-s', '-S',
    ]
    with LOG.open('wb') as log, GDBLOG.open('wb') as gdbout:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=log,
                              stderr=subprocess.STDOUT)
        gd = subprocess.Popen(['gdb', '-q', str(pg.KERNEL)], stdin=subprocess.PIPE,
                              stdout=gdbout, stderr=subprocess.STDOUT, text=True,
                              bufsize=1)
        def g(line, wait=0.25):
            gd.stdin.write(line + '\n')
            gd.stdin.flush()
            time.sleep(wait)
        def drain(n=20, start=0):
            for _ in range(n):
                time.sleep(0.2)
                txt = GDBLOG.read_text(errors='replace')
                if 'Breakpoint 1,' in txt[start:]:
                    g('printf "HIT handle=%x ebx=%x\\n", *(unsigned*)($esp+4), $ebx', 0.2)
                    g('bt 5', 0.3)
                    g('c', 0.4)
                else:
                    break
        try:
            g('set pagination off')
            g('target remote :1234', 2)
            g('break *0x12b6f0', 0.5)  # PreprocessHandler entry
            g('c', 3)
            pos = pg.wait_for(LOG, pg.PROMPT, timeout=120)
            drain()   # boot-time handle execs
            off = len(GDBLOG.read_text(errors='replace'))
            print('prompt; launching EFILEX', flush=True)
            vm.stdin.write(b'efilex\n')
            vm.stdin.flush()
            pg.wait_for(LOG, b'EFILE: genuine file server starting', pos, 60)
            for _ in range(25):
                drain(start=off)
                off = len(GDBLOG.read_text(errors='replace'))
                if b'FAULT:' in LOG.read_bytes():
                    break
            gd.send_signal(signal.SIGINT)
            time.sleep(1)
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
    txt = GDBLOG.read_text(errors='replace')
    print(txt[txt.find('prompt') if 'prompt' in txt else 0:][-9000:])
    return 0


if __name__ == '__main__':
    sys.exit(main())

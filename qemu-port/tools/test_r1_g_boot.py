#!/usr/bin/env python3
"""DRAFT guest regression; explicit authorization flag required. Not run on card20."""
import argparse
from pathlib import Path
import re
import subprocess
import time

def main():
    p=argparse.ArgumentParser();p.add_argument('--allow-qemu',action='store_true');p.add_argument('--kernel',type=Path,required=True);p.add_argument('--disk',type=Path,required=True);p.add_argument('--scratch',type=Path,required=True)
    a=p.parse_args()
    if not a.allow_qemu:p.error('draft: separately authorized QEMU slot and --allow-qemu required')
    a.scratch.mkdir(parents=True,exist_ok=False);path=a.scratch/'serial.log'
    with path.open('wb') as log:
        vm=subprocess.Popen(['qemu-system-i386','-M','q35','-m','128','-display','none','-serial','stdio','-kernel',str(a.kernel.resolve()),'-drive','file='+str(a.disk.resolve())+',format=raw,snapshot=on,if=none,id=d0','-device','ich9-ahci,id=sata','-device','ide-hd,drive=d0,bus=sata.0','-monitor','none','-no-reboot'],stdin=subprocess.PIPE,stdout=log,stderr=subprocess.STDOUT)
        pos=0
        def wait(token):
            nonlocal pos
            end=time.monotonic()+40
            while time.monotonic()<end:
                data=path.read_bytes();at=data.find(token,pos)
                if at>=0:pos=at+len(token);return
                if vm.poll() is not None:raise AssertionError('guest exited')
                time.sleep(.05)
            raise AssertionError('timeout '+repr(token))
        def command(text,prompt=b'EKA2> '):
            start=pos;vm.stdin.write(text.encode()+b'\n');vm.stdin.flush();wait(prompt)
            return path.read_bytes()[start:pos].decode(errors='replace')
        try:
            wait(b'EKA2> ')
            for cmd in ['check','abandon','reject-manifest','reject-import','heap','sched','heap','sched']:
                out=command('gboot '+cmd);assert 'gboot: result=0' in out,out
                if cmd in ('heap','sched'):
                    assert 'R1A:'+cmd+':done:PASS' in out,out
                    assert ('heap-cbase-leave-trap' if cmd=='heap' else 'timer-runl')+':PASS' in out,out
                    assert re.search(r'G:exit id=\d+ type=0 reason=0',out),out
                if cmd in ('check','abandon'):assert 'G:resume' not in out and 'G:abandon requested' in out,out
                if cmd.startswith('reject'):assert 'G:rejection '+cmd in out and 'G:resume' not in out,out
            assert 'G:reap count=0' in command('gboot reap')
            # Separately provisioned disposable legacy disk must contain ECHO.E32.
            command('mount',b'C:/> ')
            assert 'ECHO: legacy-after-g' in command('ECHO.E32 legacy-after-g',b'C:/> ')
            command('reap',b'C:/> ')
            data=path.read_text(errors='replace');assert 'FAULT:' not in data and 'Exc ' not in data
        finally:
            vm.terminate()
            try:vm.wait(timeout=3)
            except subprocess.TimeoutExpired:vm.kill();vm.wait()
    print('PASS genuine heap/sched/repeat/reject/abandon and legacy survival')

if __name__=='__main__':main()

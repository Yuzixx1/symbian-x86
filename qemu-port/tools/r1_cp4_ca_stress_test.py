#!/usr/bin/env python3
"""P-08 concurrency stress: multiple simultaneous IPC client processes doing
async requests against one server, a mid-test kill, over repeated cycles.
Checks: no lost request-semaphore completions, no Exc/fault, exit-ring record
per death, and zero leaked channels/segs (vs. a post-mount baseline).
"""
import re
import subprocess
import sys
import time
from pathlib import Path

import test_cli

test_cli.DISK = '/tmp/opencode/cp4_stress.img'
test_cli.LOG = '/tmp/opencode/cp4_stress.log'
LOG = test_cli.LOG

CLIENTS = 4          # simultaneous request-issuing processes (3-5)
CYCLES = 3           # repeated cycles
VICTIM = 2           # which client instance is killed mid-test
DONE = 'CPSTRESS: done'

FAIL_MARKERS = (
    'CPSTRESS: connect failed', 'CPSTRESS: connect2 failed',
    'CPSTRESS: send1 failed', 'CPSTRESS: send2 failed',
    'CPSTRESS: waitA failed', 'CPSTRESS: waitB failed',
    'IPCCLI: reply mismatch', 'IPCCLI: async check failed',
)
FAULT_KEYS = ('FAULT:', 'Exc ', 'PanicKernExec')

LOG_FH = None  # open serial log handle, required by test_cli.wait_for/run_command


def total_done():
    return Path(LOG).read_text(errors='replace').count(DONE)


def done_tail_ids(n):
    """Instance ids of the last n 'CPSTRESS: done <id>' lines in the log."""
    text = Path(LOG).read_text(errors='replace')
    return [int(m) for m in re.findall(r'CPSTRESS: done(\d+) \d+', text)][-n:]


def wait_for_done(target, timeout=30):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        seen = total_done()
        if seen >= target:
            return seen
        time.sleep(0.15)
    raise AssertionError(f'expected >= {target} clean completions, saw {seen}')


def read_range(pos, end):
    with open(LOG, 'rb') as src:
        src.seek(pos)
        return src.read(end - pos).decode('utf-8', 'replace')


def boot_prompt(timeout=30):
    """Return (offset-after-prompt, needs_mount). Some kernels auto-mount and
    boot straight to C:/>, others start at EKA2> and require `mount`."""
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        data = Path(LOG).read_bytes()
        if b'C:/> ' in data:
            return data.index(b'C:/> ') + len(b'C:/> '), False
        if b'EKA2> ' in data:
            return data.index(b'EKA2> ') + len(b'EKA2> '), True
        time.sleep(0.15)
    raise AssertionError('no shell prompt in serial log')


def run(vm, pos, cmd, expected=b''):
    return test_cli.run_command(vm, LOG_FH, pos, cmd, expected, b'C:/> ')


def ps(vm, pos, pattern):
    before = pos
    pos = run(vm, pos, 'ps', b'PortFile channels:')
    text = read_range(before, pos)
    return pos, re.findall(pattern, text)


def ps_ids(vm, pos, name):
    return ps(vm, pos, r'\b' + re.escape(name) + r'\S*\s+id=(\d+)')


def channels(vm, pos):
    pos, m = ps(vm, pos, r'PortFile channels:\s*(\d+)')
    assert m, 'no channel count in ps'
    return pos, int(m[-1])


def seg_count(vm, pos):
    before = pos
    pos = run(vm, pos, 'segs', b'code segs')
    text = read_range(before, pos)
    m = re.findall(r'(\d+) code segs', text)
    assert m, text
    return pos, int(m[-1])


def main():
    global LOG_FH
    subprocess.run(['make', 'build/kernfull.elf', 'userland'], check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', test_cli.DISK, '64'],
                   check=True, stdout=subprocess.DEVNULL)
    for name in ('IPCSRV', 'IPCCLI', 'CPSTRESS'):
        subprocess.run(['mcopy', '-o', '-i', test_cli.DISK + '@@2048S',
                        f'build/user/{name}.E32', f'::{name}.E32'], check=True)

    with open(LOG, 'wb') as log:
        LOG_FH = log
        vm = subprocess.Popen([
            'qemu-system-i386', '-M', 'q35', '-m', '128', '-display', 'none',
            '-serial', 'stdio', '-kernel', 'build/kernfull.elf',
            '-drive', f'file={test_cli.DISK},format=raw,if=none,id=d0',
            '-device', 'ich9-ahci,id=sata', '-device', 'ide-hd,drive=d0,bus=sata.0',
            '-monitor', 'none', '-no-reboot'], stdin=subprocess.PIPE,
            stdout=log, stderr=subprocess.STDOUT)
        try:
            pos, needs_mount = boot_prompt()
            if needs_mount:
                pos = run(vm, pos, 'mount')
            pos, base_segs = seg_count(vm, pos)
            pos, base_chan = channels(vm, pos)
            print(f'baseline: {base_segs} code segs, {base_chan} channels')

            for cycle in range(1, CYCLES + 1):
                done_prior = total_done()
                # One server plus CLIENTS concurrent request-issuing processes.
                pos = run(vm, pos, 'IPCSRV.E32', b'IPCSRV: ready')
                pos, srv = ps_ids(vm, pos, 'IPCSRV')
                assert len(srv) == 1, f'cycle {cycle}: server ids {srv}'
                server = srv[0]
                victim_launch = pos
                for i in range(1, CLIENTS + 1):
                    pos = run(vm, pos, f'CPSTRESS.E32 {i}', b'CPSTRESS: start')
                    if i == VICTIM:
                        victim_launch = pos
                pos, clients = ps_ids(vm, pos, 'CPSTRESS')
                assert len(clients) == CLIENTS, f'cycle {cycle}: client ids {clients}'

                # Kill one client mid-test; the others must still complete
                # every async request (no lost request-semaphore completions).
                # Prove the victim is still in-flight: it made progress but
                # has not emitted its completion marker yet.
                victim = clients[VICTIM - 1]
                in_flight = read_range(victim_launch, pos)
                assert f'CPSTRESS: tick{VICTIM} ' in in_flight, (
                    f'cycle {cycle}: victim made no progress before kill')
                assert f'CPSTRESS: done{VICTIM} ' not in in_flight, (
                    f'cycle {cycle}: victim finished before kill (not mid-test)')
                pos = run(vm, pos, f'kill {victim}', f'killed {victim}'.encode())
                wait_for_done(done_prior + CLIENTS - 1)
                # Every surviving client must have drained all its async
                # requests: a lost request-semaphore completion would hang its
                # ipc_wait_for and leave it without a 'done' marker.
                tail = done_tail_ids(CLIENTS - 1)
                survivors = [i for i in range(1, CLIENTS + 1) if i != VICTIM]
                assert sorted(tail) == sorted(survivors), (
                    f'cycle {cycle}: done ids {tail} != survivors {survivors}')

                # Kill the session-owning server; a fresh client must fail to connect.
                pos = run(vm, pos, f'kill {server}', f'killed {server}'.encode())
                pos = run(vm, pos, 'IPCCLI.E32', b'IPCCLI: connect failed')

                # The exit ring must record both the killed client and server.
                before = pos
                pos = run(vm, pos, 'exits', b'IPCSRV')
                ring = read_range(before, pos)
                assert f'id={victim} ' in ring, f'cycle {cycle}: victim {victim} not in exits'
                assert f'id={server} ' in ring, f'cycle {cycle}: server {server} not in exits'
                print(f'cycle {cycle}: clients={clients} victim={victim} '
                      f'server={server} recorded in exit ring')

            # Recovery: fresh server completes a full client run, then stops cleanly.
            pos = run(vm, pos, 'IPCSRV.E32', b'IPCSRV: ready')
            target = total_done() + 1
            pos = run(vm, pos, 'CPSTRESS.E32 99', b'CPSTRESS: start')
            wait_for_done(target)
            pos = run(vm, pos, 'IPCCLI.E32 stop', b'stop acknowledged')

            # Leak checks: reap dead image segs, then compare to baseline.
            pos = run(vm, pos, 'reap')
            pos, final_chan = channels(vm, pos)
            assert final_chan == base_chan, f'channels {base_chan}->{final_chan}'
            pos, final_segs = seg_count(vm, pos)
            assert final_segs == base_segs, f'segs {base_segs}->{final_segs}'

            out = Path(LOG).read_text(errors='replace')
            for f in FAULT_KEYS:
                assert f not in out, f'kernel fault marker {f!r} in log'
            done = total_done()
            assert done >= (CLIENTS - 1) * CYCLES + 1, f'only {done} clean completions'
            for marker in FAIL_MARKERS:
                assert marker not in out, f'failure marker {marker!r} in log'
            print(f'PASS: {CYCLES} cycles x {CLIENTS} clients, {done} clean '
                  f'completions, no faults, exit ring, channels={final_chan} '
                  f'segs={final_segs}')
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'FAIL: {exc}', file=sys.stderr)
        raise

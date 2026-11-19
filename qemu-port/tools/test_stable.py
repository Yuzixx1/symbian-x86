#!/usr/bin/env python3
"""Sequential stabilization suite; preserves each check's actual exit status."""
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOGS = Path('/tmp/opencode/stabilization')
CHECKS = (
    'test_e32_relocs.py', 'test_args.py', 'test_dll.py',
    'test_data_reloc.py', 'test_userland.py', 'test_ipc.py',
    'test_cli.py', 'test_cli_diskfull.py', 'test_crash.py', 'test_conc.py',
    'test_interact.py', 'test_sdk.py', 'test_samples.py', 'test_wait.py',
    'test_chunk.py',
)
FILE_REGRESSION = 'tests/r1_file_regression/test_r1_file_regression.py'


def run_command(name, command, log):
    try:
        with log.open('wb') as handle:
            result = subprocess.run(command, cwd=ROOT, stdout=handle,
                                    stderr=subprocess.STDOUT, timeout=300)
        passed = result.returncode == 0
    except subprocess.TimeoutExpired:
        passed = False
    print(f'{"PASS" if passed else "FAIL"} {name}: {log}', flush=True)
    return passed


def g_boot_available():
    asset_dir = os.environ.get('R1_G_ASSET_DIR')
    if not asset_dir:
        return None, 'R1_G_ASSET_DIR not set'
    path = Path(asset_dir)
    if not (path / 'r1_g_assets.inc').is_file():
        return None, 'R1_G_ASSET_DIR lacks r1_g_assets.inc'
    return path, None


def run_g_boot(log_dir):
    asset_dir, reason = g_boot_available()
    if asset_dir is None:
        print(f'SKIP test_r1_g_boot: {reason}', flush=True)
        return None
    # Provision a disposable seeded disk with ECHO.E32, matching the driver's
    # legacy-survival assertion. Never mutate the committed tree.
    disk = log_dir / 'r1_g_boot.img'
    scratch = log_dir / 'r1_g_boot_scratch'
    if disk.exists():
        disk.unlink()
    if scratch.exists():
        shutil.rmtree(scratch)
    try:
        subprocess.run(['python3', 'tools/mkdisk.py', 'seeded', str(disk), '64'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
        subprocess.run(['mcopy', '-o', '-i', str(disk) + '@@2048S',
                        'build/user/ECHO.E32', '::ECHO.E32'],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        print(f'FAIL test_r1_g_boot: disk provisioning failed: {exc}', flush=True)
        return False
    elf = ROOT / 'build' / 'kernfull.elf'
    if not elf.is_file():
        print('FAIL test_r1_g_boot: build/kernfull.elf missing', flush=True)
        return False
    command = [sys.executable, 'tools/test_r1_g_boot.py', '--allow-qemu',
               '--kernel', str(elf), '--disk', str(disk),
               '--scratch', str(scratch)]
    log = log_dir / 'test_r1_g_boot.log'
    return run_command('test_r1_g_boot', command, log)


def main():
    LOGS.mkdir(parents=True, exist_ok=True)
    asset_dir, _ = g_boot_available()
    build_command = ['make']
    if asset_dir is not None:
        build_command = ['make', 'R1_G_ASSET_DIR=' + str(asset_dir)]
    commands = [('build', build_command)]
    commands += [(name[:-3], [sys.executable, 'tools/' + name]) for name in CHECKS]

    failures = []
    for name, command in commands:
        path = LOGS / (name + '.log')
        passed = run_command(name, command, path)
        if not passed:
            failures.append(name)
            if name == 'build':
                break

    g_result = run_g_boot(LOGS)
    if g_result is False:
        failures.append('test_r1_g_boot')
    elif g_result is None:
        # Explicit skip is not a pass and not a failure; record it visibly.
        print('SKIP-RECORDED test_r1_g_boot (not counted as a pass)', flush=True)

    # File-service regression: QEMU guest test using card 36's portfilesrv
    # E32 image as the registered !FileServer host. Runs serially in the
    # exclusive QEMU slot; no R1_G_ASSET_DIR dependency.
    fr_log = LOGS / 'test_r1_file_regression.log'
    fr_passed = run_command('test_r1_file_regression',
                            [sys.executable, 'tests/r1_file_regression/'
                             'test_r1_file_regression.py'], fr_log)
    if not fr_passed:
        failures.append('test_r1_file_regression')

    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())

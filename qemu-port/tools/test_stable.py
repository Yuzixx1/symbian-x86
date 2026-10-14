#!/usr/bin/env python3
"""Sequential stabilization suite; preserves each check's actual exit status."""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOGS = Path('/tmp/opencode/stabilization')
CHECKS = (
    'test_e32_relocs.py', 'test_args.py', 'test_dll.py',
    'test_data_reloc.py', 'test_userland.py', 'test_ipc.py',
    'test_cli.py', 'test_cli_diskfull.py', 'test_crash.py', 'test_conc.py',
    'test_interact.py', 'test_sdk.py',
)


def main():
    LOGS.mkdir(parents=True, exist_ok=True)
    commands = [('build', ['make'])]
    commands += [(name[:-3], [sys.executable, 'tools/' + name]) for name in CHECKS]
    failures = []
    for name, command in commands:
        path = LOGS / (name + '.log')
        try:
            with path.open('wb') as log:
                result = subprocess.run(command, cwd=ROOT, stdout=log,
                                        stderr=subprocess.STDOUT, timeout=300)
            passed = result.returncode == 0
        except subprocess.TimeoutExpired:
            passed = False
        print(f'{"PASS" if passed else "FAIL"} {name}: {path}', flush=True)
        if not passed:
            failures.append(name)
            if name == 'build':
                break
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())

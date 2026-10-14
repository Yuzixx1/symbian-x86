#!/usr/bin/env python3
"""Scaffold a new SDK application.

Usage: python3 tools/newapp.py Name [dest-dir]

Creates dest-dir/Name/{main.c, Makefile, README.md} from an annotated
template. Build with `make` inside the app dir (needs gcc -m32 multilib).
"""
import re
import sys
from pathlib import Path

MAIN_C = '''#include "runtime.h"

/* {name}: SDK starter. Returning from app_main exits the process with
   the return value as the exit reason (see `exits` in the shell). */
static U16 arguments[64];

int app_main(void)
{{
    int length = command_line(arguments, 64);
    if (length < 0)
        {{
        print("{NAME}: command line too long");
        return 1;
        }}
    print("Hello from {NAME}!");
    if (length > 0)
        print("(run `args` in the shell to see your command line)");
    return 0;
}}
'''

MAKEFILE = '''APP := {NAME}
SRCS := main.c
SDK := {sdk}

all:
\tpython3 "$(SDK)/tools/buildapp.py" --sdk "$(SDK)" --app $(APP) $(SRCS:%=--src %)

clean:
\trm -rf build $(APP).E32

.PHONY: all clean
'''

README = '''# {name}

Built with the SDK: run `make` here (requires gcc with -m32 multilib).

Copy `{NAME}.E32` onto a FAT32 test volume, boot the OS, then:

```
mount
exec {NAME}.E32 your args here
```

The process exit reason is the return value of `app_main`.
'''


def main():
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print(__doc__)
        return 1
    name = sys.argv[1]
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', name):
        print('Name must start with a letter and hold only letters/digits/_')
        return 1
    dest = Path(sys.argv[2]) if len(sys.argv) == 3 else Path.cwd()
    appdir = dest / name
    if appdir.exists():
        print(f'{appdir} already exists')
        return 1
    sdk_dir = Path(__file__).resolve().parent.parent
    appdir.mkdir(parents=True)
    sdk_path = str(sdk_dir)
    upper = name.upper()
    (appdir / 'main.c').write_text(MAIN_C.format(name=name, NAME=upper))
    (appdir / 'Makefile').write_text(MAKEFILE.format(NAME=upper, sdk=sdk_path))
    (appdir / 'README.md').write_text(README.format(name=name, NAME=upper))
    print(f'created {appdir}/ (main.c, Makefile, README.md)')
    print('build with: make -C ' + str(appdir))
    return 0


if __name__ == '__main__':
    sys.exit(main())

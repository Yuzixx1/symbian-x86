# SDK — application development for this OS

Write freestanding C programs, build them into E32 images, run them with
the shell `exec` command. No Symbian toolchain needed: stock `gcc -m32`,
`ld`, and Python 3.

Layout:

```
sdk/
  README.md            this file
  docs/
    getting-started.md step-by-step: first app, deploy, run, debug
    api.md             runtime + IPC function reference
    limits.md          honest constraints (read before shipping)
    ipc.md             client/server guide and roadmap
  tools/
    newapp.py          scaffold a new app: newapp.py Name [dest]
    buildapp.py        the actual build (called by app Makefiles)
  examples/
    hello/             minimal print + exit code
    args/              command-line echo
    fileview           read and print a file
    prompt/            leased console input
    asleep/            sleep, clock, random
```

Quick start:

```
python3 tools/newapp.py MyApp
make -C MyApp            # produces MyApp/MYAPP.E32
```

Then copy `MYAPP.E32` onto a FAT32 test volume and `exec MYAPP.E32`.
Full walkthrough: `docs/getting-started.md`.

Relationship to the trees beside this one: the OS port lives in
`../qemu-port` (compiler flags, `user/` runtime sources, `link.ld`,
`tools/elf2e32.py`, `tools/e32read.py`). The SDK compiles against those
sources but never modifies them; the upstream Symbian sources in
`../kernelhwsrv` are read-only and not needed to build apps.

Status: single-process C apps are fully supported (console, files,
directories, heap, sleep, clock, random, IPC client/server via `ipc.h`).
C++ exceptions/RTTI, a C library, and a windowed UI do not exist yet —
see `docs/limits.md`.

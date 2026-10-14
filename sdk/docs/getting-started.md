# Getting started

## Prerequisites

- `gcc` with 32-bit multilib (`gcc -m32` must work), binutils `ld`, Python 3.
- `mtools` (`mcopy`) to put programs onto a test disk.
- QEMU (`qemu-system-i386`) to run the OS, or real hardware/VM (see the
  port's `docs/hw_tests.md`).
- A bootable OS image: build `../qemu-port` with default `make`
  (produces `build/symbian-grub.iso`).

## Your first app

```
python3 tools/newapp.py Greet
make -C Greet
```

This yields `Greet/GREET.E32`. The template prints a greeting and returns 0.

## Deploy it

Make a disposable FAT32 disk and copy the program on (same recipe the
port's own tests use):

```
python3 ../qemu-port/tools/mkdisk.py seeded /tmp/my.img 64
mcopy -o -i /tmp/my.img@@2048S Greet/GREET.E32 ::GREET.E32
```

Boot the OS with that disk attached (AHCI example):

```
qemu-system-i386 -M q35 -m 128 -display none -serial stdio \
  -kernel ../qemu-port/build/kernfull.elf \
  -drive file=/tmp/my.img,format=raw,if=none,id=d0 \
  -device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 \
  -monitor none -no-reboot
```

## Run it

```
mount
exec GREET.E32 world
```

Expect `Hello from Greet!`. The trailing words become the program's command
line (see `command_line` in `api.md`). The return value of `app_main`
becomes the process exit reason — check it with the shell `exits` command.

## Read input

Only one program at a time may own the keyboard: call `con_acquire()`,
use `read_line()`, then `con_release()` before returning. While leased,
the shell stays quiet. See `examples/prompt` and `api.md`.

## Debug

- Serial log is your debugger: every `print` appears there. Note each
  `print` call starts a new line on the wire (DebugPrint transport).
- A fault prints `Exc <n>` plus registers, and the reaper names your
  program (`prog=FOO.E32`); the shell survives. Fix, rebuild, rerun.
- `ps` lists processes, `kill <id>` ends one, `reap`/`exits` inspect deaths.
- If the shell ignores keys after your program exits, your program died
  holding the console lease — always `con_release()` on every path
  (the shell reclaims dead leases, but clean code releases).

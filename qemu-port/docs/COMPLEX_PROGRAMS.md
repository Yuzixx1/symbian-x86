# Running complex programs on the R1 QEMU port

This guide summarizes what is actually required to get a non-trivial program
running as a bundled E32 on this OS. It is based on the userland build rules in
`user/user.mk`, the runtime in `user/runtime.c`/`user/malloc.c`, the loader
contract and the programs already proven in `docs/USER_APPS_CATALOG.md`.
For the graphics-specific details see `docs/GUI_PROGRAMMING.md`.

## 1. Hard constraints

Every user program is a **freestanding 32-bit PE2-style E32 image** running at
ring 3 under the genuine EKA2 kernel. There is no hosted environment:

- **No hosted libc.** No `stdio.h`, `stdlib.h`, `string.h`, `math.h` or CRT
  startup. `USER_FLAGS` uses `-ffreestanding -fno-builtin -fno-pic -fno-pie
  -nostdlib`-style linking; only ` LIBGCC` and the port's own `runtime.o`
  (plus optional `malloc.o`/`ipc.o`) are linked in.
- **Tiny heap.** `user/malloc.c` is a first-fit, chunk-backed allocator: the
  initial kernel chunk is **16 KiB**, grown in 16 KiB steps up to a **64 KiB
  (MAX_CHUNK 65536) cap**. Large buffers must be static/global or backed by an
  explicit `chunk_create`/`chunk_adjust` (`RChunk`) executive — not malloc.
- **No FPU dependency.** Programs use `int`/`unsigned` and fixed-point
  arithmetic only (e.g. audio `audio_tone` uses 16.16 phase accumulation). No
  `float`/`double`, no `libm`, no floating-point exception support.
- **PE2 import of EUSER ordinal 915.** Serial markers use the genuine
  `RDebug::Printf` import. `elf2e32.py --euser-import 915` patches ordinal 915
  into the PE2 IAT at `0x408000`; each program declares its own
  `<name>_printf_slot` at that address and a `marker()` helper that calls the
  bound function, falling back to `print()` (DebugPrint exec 86) when unbound.
- **Single entry point.** Every program exposes `int app_main(void)` and reads
  its command line with `command_line()`; `build/user/start.o` is the ELF entry
  that sets up the stack/heap and calls it.
- **No threads API beyond execs.** There is `sleep_ms`, `thread_yield`,
  `thread_park`, timers (133/52/53) and process logon (116/123), but no
  pthreads/C++ threads and no true concurrent GUI event loop.
- **No real filesystem beyond PortFile IPC.** `file_open/read/write/save` are
  logical PortFile channels with per-channel snapshots and a per-process CWD,
  not POSIX file descriptors. `X:` is a read-only snapshot of `PROGRAMS.IMG`.

## 2. What has been proven

These are not sketches — each is a bundled E32 with a passing host test:

- **Recursive-descent parser — `CALC`** (`user/calc.c`): tokenizes and parses
  `+ - * / ()` with precedence and error recovery; `r1_calc_test.py`.
- **Full TCP/HTTP stack in one E32 — `WBROWSER`** (`user/wbrowser.c` plus the
  netstack): DNS, TCP SYN handshake, HTTP/1.0 GET and HTML tag stripping over
  the e1000 frame bridge (execs 247/248) or the `!SocketServer` IPC path;
  `r1_wbrowser_test.py`. `HTTPGET`, `PING`, `DNSNET`, `NTPC` prove the layers
  independently.
- **BASIC interpreter — `BASIC`** (being built by another agent): a line
  interpreter is a natural fit for the 64 KiB heap and freestanding runtime;
  track its build status in `docs/USER_APPS_CATALOG.md` before relying on it.
- **Real-time graphical game — `BREAKOUT`** (being built): frame-based
  painting through exec 251 plus keyboard polling via `con_tryread()`, the same
  pattern already proven by the six-frame `SAVER` animation and by `GUISHELL`.
- **Rich text applications** — `NOTEPAD`, `FILEMGR`, `ADDRBOOK`, `PWVAULT`,
  `TEXTEDIT` — prove interactive line editing, search/replace and file save
  entirely through the runtime/PortFile layer.

## 3. Porting checklist

1. **Rewrite to freestanding C.** Use `-m32 -march=i486 -Os -ffreestanding`;
   eliminate libc headers, `printf`, dynamic allocation heuristics and any
   FPU use. Replace big heap allocations with globals or explicit chunks.
2. **Replace stdio with `runtime.h`.** `print`/`print_n` for output,
   `command_line` for argv, `file_*` for storage, `sleep_ms`/`time_now`/
   `random32` for time and randomness, `con_*`/`read_line` for input. Provide
   `int app_main(void)`.
3. **Add markers via ordinal 915.** Declare `<name>_printf_slot` at `0x408000`
   with a `marker()`/`NbPrintf` helper exactly as in `user/calc.c` or
   `user/guishell.c`. Choose a short prefix (e.g. `BASIC:`) the test can grep.
4. **Add a build rule** in `user/user.mk`: compile `<name>.o` with
   `USER_FLAGS`, link `<name>.elf` from `start.o + runtime.o` (+ `malloc.o`/
   `ipc.o`/`guishost.h` as needed) using `ld -m elf_i386 --emit-relocs
   --defsym=<name>_printf_slot=0x408000 -T user/link.ld`, then run
   `elf2e32.py $< $@ --euser-import 915` and `e32read.py --quiet`.
5. **Write a test harness.** Model on `tools/r1_calc_test.py`: stage the E32
   and `EUSER.DLL` on a 32 MiB FAT32 disk at LBA 2048, boot
   `build/kernfull.elf` with QEMU (`-vga std` for GUI programs), feed the
   program name at the `EKA2>` prompt, assert markers in order and assert no
   `Exc `/`PanicKernExec`/MMU #PF. Exit 0 only on full pass.
6. **Bundle in `PROGRAMS.IMG`.** Every `build/user/*.E32` is packed into the
   GRUB module mounted as `X:` so the program is launchable by name. Confirm
   the image is rebuilt and the shell lists/launches it (`X:<NAME>`).
7. **Verify** with `python3 tools/r1_<name>_test.py` and update
   `docs/USER_APPS_CATALOG.md` (markers, build rule, test name).

## 4. Honest gaps

- **No libc / no C++ runtime.** No `new`/`delete`, exceptions, RTTI, locale,
  regex, math or standard containers. Everything must be hand-rolled or ported
  into freestanding C.
- **No threads API beyond execs.** No mutexes, condition variables or async
  callbacks; cooperative polling and `sleep_ms` are the concurrency model.
- **No real filesystem beyond PortFile IPC.** No mmap, no directories in the
  POSIX sense, no device files, no stdio streams, no durable cross-process
  locking.
- **No dynamic linking of new DLLs.** Only the existing EUSER import (ordinal
  915) is bound by the loader. You cannot ship a new `.DLL` and link against
  it; extra functionality must be compiled into the E32 or exposed as a slow
  exec / IPC server.
- **No compositor or input toolkit.** GUI programs repaint a shared
  framebuffer sequentially; see `docs/GUI_PROGRAMMING.md` for the stable
  display-list protocol and the in-progress `user/guilib.h` toolkit.

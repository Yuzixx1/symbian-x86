# E32 Developer Guide — writing a user application for this OS

This guide walks through adding a new end-user application to the EKA2 PC port:
writing freestanding C, wiring a genuine PE2 EUSER.DLL import, adding `user.mk`
rules, launching it from the shell / bundling it into `PROGRAMS.IMG`, and writing
a QEMU test. It is based on the working examples `user/calc.c`,
`user/selftest.c`, `user/start.S`, `user/runtime.h`, `user/ipc.h`,
`user/link.ld`, `tools/elf2e32.py` and `user/user.mk`.

An "E32" here is **not** a Symbian SDK build. It is a freestanding 32-bit ELF
(`-m32 -ffreestanding`, no hosted libc) that `tools/elf2e32.py` repacks into a
relocatable E32 image with an optional PE2-style import of `EUSER.DLL`. The only
host tools in the pipeline are the cross-ish `gcc`/`ld` flags, Python, and
`e32read.py` for validation.

---

## 1. Step 1 — write C against `runtime.h`

Every app defines `int app_main(void)`. The crt is `user/start.S`: `_start`
calls `app_main`, pushes its return value, calls `process_exit`, then `ud2`.
You do not write a `main`; you do not link a hosted libc.

`user/runtime.h` is the entire supported user API surface. The groups you will
normally use:

* **Console / lifecycle:** `print(const char*)`, `print_n(text, len)`,
  `process_exit(int)` (noreturn), `app_main(void)`,
  `command_line(U16* buffer, unsigned capacity)`.
* **Heap:** `malloc(unsigned)`, `free(void*)` — a chunk-backed user heap.
* **Files** (via PortFile / `portfilesrv`): `file_open`, `file_read`,
  `file_close`, `file_create_write`, `file_append`, `file_commit`, `file_save`,
  plus `change_dir` / `get_cwd`.
* **Chunks (RChunk executives):** `chunk_create(name, initial, max)`,
  `chunk_open(name)`, `chunk_base(h)`, `chunk_size(h)`, `chunk_adjust(h, n)`.
* **Executives / time:** `sleep_us`, `sleep_ms`, `thread_yield`, `thread_park`,
  `time_now`, `random32`, and the `QE_EXEC_*` constants / `process_open_by_id`,
  `process_logon`, `process_wait_exit`, etc.
* **Console input:** `con_acquire`, `con_release`, `con_tryread`, `read_line`.
* **IPC** (include `user/ipc.h` separately): `ipc_server_create`, `ipc_receive`,
  `ipc_complete`, `ipc_accept`, `ipc_connect`, `ipc_call`, `ipc_call_args`,
  `ipc_read8/16`, `ipc_write8/16`, `ipc_wait`, `ipc_call_async`, `ipc_close`,
  and the `struct IpcMessage` / `IpcArgs` ABIs.
* **Arbitrary executives:** `extern int exec0(unsigned n);`,
  `extern int exec2(unsigned n, unsigned a1, unsigned a2);`,
  `extern int exec4(...);` implemented in `start.S` (int 0x21, preserving
  callee-saved registers).

A minimal skeleton (adapted from `calc.c` / `selftest.c`):

```c
#include "runtime.h"

int app_main(void)
{
    print("MYAPP: READY");          /* DebugPrint fallback: always visible */
    void* p = malloc(64);
    if (p) { p[0] = 'A'; free(p); }
    print("MYAPP: DONE");
    return 0;                        /* start.S calls process_exit for us */
}
```

Build strings directly into a `char buf[]`: there is no `sprintf`. Both
`calc.c` and `selftest.c` contain tiny `append`/`dec`/`append_int` helpers you
can copy. Values are plain unsigned/signed 32-bit; add/sub/mul wrap naturally
and division truncates toward zero (guard `INT_MIN / -1` and zero divisors).

---

## 2. Step 2 — wire the PE2 EUSER.DLL import (ordinal 915)

Ordinal **915** is `RDebug::Printf(char const*, ...)`. Linking against it is how
markers get onto the real EUSER path instead of only the DebugPrint fallback.
The mechanism is one absolute IAT slot in the text region:

* `user/link.ld` places `.text` at `0x00400000` and `.data`/`.bss` at
  `0x00500000`.
* `tools/elf2e32.py` defines `IAT_OFF = 0x8000`; with `--euser-import 915` it
  pads `.text` to that offset and writes the 1-based ordinal there. The slot
  address is therefore `codeBase + 0x8000 = 0x408000` (the `textSize` field in
  the E32 header). **The IAT slot must be defined at exactly `0x408000`** —
  `elf2e32.py` rejects absolute relocations outside `[0x400000, 0x410000)`.
* The E32 header gains one DLL dependency, the decorated name
  `euser{000a0000}[100039e5].dll`, and the PE2 import format flag.
* At load, `E32Run` resolves ordinal 915 and overwrites the slot with the
  function address. The serial log then contains
  `e32run: bind slot <n> ord 915`.

Three edits are required:

**(a) Declare an external slot and a marker shim** in your C file (pattern from
`calc.c` and `selftest.c`):

```c
extern unsigned int my_printf_slot;          /* --defsym, not a real variable */
typedef void (*PrintfFn)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&my_printf_slot;
    if (fn != 915u && fn != 0u)              /* resolved by E32Run */
        ((PrintfFn)fn)(text);                 /* genuine EUSER.DLL ord 915 */
    else
        print(text);                          /* DebugPrint fallback */
}
```

`915` is the *unresolved* seed value; `0` means no import was emitted. Only a
resolved address is callable. (In `calc.c` the extern is declared as a function
pointer and dereferenced as `*(unsigned*)(void*)&calc_printf_slot` — same idea.)

**(b) Define the slot at link time** with `--defsym` (see §3):

```
ld -m elf_i386 --emit-relocs --defsym=my_printf_slot=0x408000 -T user/link.ld ...
```

`--emit-relocs` is mandatory: `elf2e32.py` converts R_386_32 relocations into
E32 fixups. It specially recognises an `SHN_ABS` symbol in `[0x400000,
0x410000)` as the PE2 IAT slot; without `--defsym` the linker never creates that
symbol and the import will not bind.

**(c) Pack with the import** and validate:

```
python3 tools/elf2e32.py build/user/myapp.elf build/user/MYAPP.E32 --euser-import 915
python3 tools/e32read.py build/user/MYAPP.E32 --quiet
```

**Marker string rule:** keep marker strings free of bare `%` characters. EUSER
`RDebug::Printf` is variadic; the shim passes exactly one argument, so a `%d` or
even a stray `%` in the string makes printf read a non-existent argument and
can fault in EUSER. Format numbers into the buffer first, then pass the whole
line as a single plain string (see `append_int` in `calc.c`). If you truly need
a percent sign, escape it as `%%` — but the simplest safe rule is "no `%` at all
in marker strings."

---

## 3. Step 3 — add `user.mk` rules

`user/user.mk` is included by the top-level Makefile. `USER_FLAGS` is already
defined:

```
USER_FLAGS := -m32 -march=i486 -Os -ffreestanding -fno-pic -fno-pie \
  -fno-builtin -fno-stack-protector -fno-exceptions \
  -fno-asynchronous-unwind-tables -fno-unwind-tables \
  -mpreferred-stack-boundary=2 -Wall -Wextra -Iuser -fno-rtti
```

The generic compile rule is `build/user/%.o: user/%.c user/runtime.h` → `$(CC)
$(USER_FLAGS) -c $< -o $@`; `.cpp` uses `$(CXX)` with the same flags; `start.o`
is assembled separately as `$(CC) -m32 -c`. You normally do not add a compile
rule — just add the app's `.o` as a prerequisite of its `.elf`.

For `user/myapp.c`, add a block modelled on the `CALC.E32` rules (lines
76–85 of `user.mk`):

```make
# Short comment describing what the app does and which ordinal it imports.
build/user/myapp.elf: build/user/start.o build/user/runtime.o \
                      build/user/myapp.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=my_printf_slot=0x408000 \
	   -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/MYAPP.E32: build/user/myapp.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
```

Link-object guidance:

* `start.o` — **always** (provides `_start`, `process_exit`, `exec0/2/4`,
  `ipc_wait_any`).
* `runtime.o` — **always** (print, malloc, file, chunk, time functions).
* `malloc.o` — add if you call `malloc`/`free` and they are not already folded
  into `runtime.o` for your target; `selftest.elf` links it explicitly.
* `ipc.o` — add only if you call the `ipc_*` API (`selftest.elf`, `ipcsrv.elf`,
  `cpstress.elf` do).
* Your own `.o` plus `user/link.ld`.
* `$(LIBGCC)` supplies any compiler runtime helpers.

If your app does **not** import EUSER, omit `--defsym` and `--euser-import`
(e.g. `WAITFOR.E32`, `IPCSRV.E32`) — but then markers only reach the DebugPrint
fallback and `e32run: bind slot ... ord 915` will not appear, which matters for
automated tests.

Finally, add the E32 to the `userland` target (near the top of `user.mk`):

```make
userland: build/user/MYAPP.E32
```

Then build with `make build/user/MYAPP.E32` (or `make userland`). A successful
`e32read.py --quiet` run means the header, relocations and import were accepted.

---

## 4. Step 4 — launch from the shell; bundling into `PROGRAMS.IMG`

The shell launches programs by **bare name** (see `docs/SHELL_REFERENCE.md`,
"Programs / implicit execution"). It looks on the active drive first, then
searches **all mounted drives** `A:`–`Z:`, so a program bundled in
`PROGRAMS.IMG` (mounted on `X:`) is found even when the active drive is `C:`.
If the typed name has no `.` in it, `RunImage` retries with **`.E32` appended**;
`MYAPP`, `MYAPP.E32` and `X:MYAPP` all launch the same image. Before launching,
dead user images are reaped. Arguments after the program name are passed to
`app_main` as a UTF-16 command line via `command_line()` (see `calc.c`).

`tools/mkgrubiso.py` builds `PROGRAMS.IMG` automatically as part of `make
isogrub`. Its `curated_programs()` scans `build/user/` and includes **every
`*.E32` whose size is at least `E32_MIN_SIZE = 30000` bytes** (~30 KB), plus
`EUSER.DLL`. It stages them into a 32 MB MBR/FAT32 disk using the same
`mkfs.fat` + `mcopy` recipe as the test harnesses (bootable `0x0C` partition at
LBA 2048), verifies the listing with `mdir`, writes `build/programs.img`, and
adds it to the ISO as `/PROGRAMS.IMG;1` via a GRUB `module` line. `NOTES.TXT` and
`README.TXT` are added for the guest. Consequence for you:

* Build your app with `make userland` (or at least ensure the `.E32` exists in
  `build/user/`) **before** `make isogrub`, or it will not be bundled.
* If your E32 is suspiciously small (< 30 KB), it will be silently skipped.
  Real apps with markers/EUSER imports are well over this threshold; a tiny
  import-free stub may not be.
* Keep `EUSER.DLL` next to the images — the loader resolves the decorated PE2
  dependency by bare name.
* Prefer 8.3-compatible staged names where possible; the guest FAT driver's
  `FatReadPath` cannot match some long names reliably, which is why the test
  harnesses `mcopy` apps as short names (e.g. `calc`, `netbr`, `dnsres`).

At the boot prompt, a typical session is:

```
EKA2> MYAPP
MYAPP: READY
MYAPP: DONE
```

---

## 5. Step 5 — write a QEMU test for your app

Copy `tools/r1_calc_test.py` — it is the canonical app harness. The essential
pieces:

1. **Paths/constants** — `DISK = /tmp/opencode/r1_myapp.img`, `FS = ..._fs.img`,
   `LOG = /tmp/opencode/r1_myapp.log`, `KERNEL = build/kernfull.elf`,
   `E32 = build/user/MYAPP.E32`, `DLL` = the cached genuine `euser.dll`. Use
   `MB, LBA, SECTORS = 32, 2048, 16384`, `OFFSET = LBA * 512`.
2. **`MARKERS`** — the exact strings your app emits, in order, e.g.
   `"MYAPP: READY"`, `"MYAPP: DONE"`. With a PE2 import, also assert an
   `e32run: bind slot ... ord 915` line exists.
3. **`EXC_KEYS = ("Exc ", "PanicKernExec")`** and a `scan_exceptions()` helper;
   any match fails the run even if all markers printed.
4. **`build_disk()`** — remove old images, `truncate` the raw disk, create and
   `mkfs.fat -F 32` the FS image, `mcopy` `MYAPP.E32` (under an 8.3 name) and
   `EUSER.DLL` into it, embed at `OFFSET`, write the MBR (`0x80/0x0C`, LBA
   2048, 16384 sectors, `55 AA`), then verify with `mdir -i <disk>@@<OFFSET>`.
5. **`run_qemu()`** — `timeout 60 qemu-system-i386 ... -kernel KERNEL -drive
   file=DISK,format=raw,snapshot=on ... -display none -serial stdio`, with a
   bash feeder `sleep 1.5; printf '\n'; sleep 3.0; printf 'myapp\n'; sleep 5.0`.
   Sleep ~1 s after the run (or poll log size) so `timeout`'s SIGTERM does not
   lose the last markers. Exit 124 is expected.
6. **`main()`** — check required tools, build the disk (unless
   `--no-rebuild-disk`), read the log, check each marker and their order, check
   the ord-915 bind line, scan exceptions, print `[PASS]`/`[FAIL]` lines and a
   final `RESULT: PASS/FAIL`, and exit 0/1. Optionally add a `--log` argument.

The wrapper `tools/r1_os_selfcheck.py` only runs the four core tests, so your
new harness is invoked directly (`python3 tools/r1_myapp_test.py`) until someone
adds it to `STEPS`. Give network-class apps a ~150 s outer timeout (see
`docs/TESTING_QA.md`, §6).

---

## 6. Common pitfalls

* **8.3 filenames.** The guest FAT driver cannot reliably match long names;
  stage files with short names (`calc`, `myapp`, `netbr`) and normalise `mdir`
  output when verifying. Names longer than 8.3 get mtools LFN aliases that are
  not always resolvable in-guest.
* **`-fno-rtti` warnings are benign.** `USER_FLAGS` includes `-fno-rtti` and
  `-fno-exceptions`; C++ support files (`portfilesrv.cpp`, `svcmgr.cpp`) may
  emit warnings about missing RTTI/exception tables. They are expected for this
  freestanding build — do not "fix" them by removing the flags.
* **No hosted libc.** No `printf`, `sprintf`, `memcpy` from the host, no
  `<stdio.h>`. Build output strings by hand (`append`, `dec`, `append_int` in
  `calc.c`/`selftest.c`; a tiny `memcmp` in `selftest.c`).
* **Bare `%` in printf strings faults EUSER.** The ord-915 shim passes one
  argument. Pre-format numbers and pass a plain string; never hand a format
  specifier with no matching variadic argument.
* **IAT slot must be `0x408000`.** Use `--defsym=<your_slot>=0x408000` at link
  and `--euser-import 915` at pack time. Slot at a different address is
  rejected or never binds; forgetting `915` means no PE2 import at all.
* **Don't call `ipc_connect` without a server loop.** It blocks waiting on a
  session that will never answer unless a matching `ipc_server_create` /
  `ipc_receive` loop is running (`selftest.c` deliberately only does
  create/close). For async work use `ipc_call_async` + `ipc_wait_for`, as
  `cpstress.c` / `ipccli.c` do.
* **Don't run `make clean` while other agents/builds are active.** It wipes the
  shared `build/` tree (all `.o`, `.elf`, `.E32`) that other builds and QEMU
  harnesses are using, manufacturing widespread spurious failures.
* **Stale E32 in the ISO.** Adding a `userland` rule is not enough if `make
  isogrub` ran first; rebuild the ISO after adding a new app so `PROGRAMS.IMG`
  contains it.

For the testing side of your new harness, see `docs/TESTING_QA.md`, especially
§5 (disk staging / markers / exception scanning) and §7 (false failures).

# Ported software — running *real* programs on the EKA2 PC port

This document is about porting **existing, third-party C programs** onto this
OS, as distinct from writing another small application from scratch. It records
what a port actually has to give up, which pieces of the port runtime replace a
hosted libc, how each of the four current ports (Lua, a Z-machine interpreter,
miniz/ ZIP32 and bc) is built, staged and tested, what licensing still needs
verifying, and a repeatable recipe for the next port.

> **Status snapshot.** The four ports are being implemented by sibling agents
> concurrently with this write-up. Marker prefixes and source layouts below are
> the *contract* the integration test expects; an entry is "shipped" only when
> `build/user/<NAME>.E32` exists, `tools/e32read.py` accepts it and
> `tools/r1_ported_software_test.py` passes. Re-check `build/user/` and
> `docs/USER_APPS_CATALOG.md` before quoting any status as final. The test
> deliberately skips images that are not built yet, so it is safe to run at any
> point during the effort.

---

## 1. Why real ports matter

Hand-written apps (`CALC`, `SNAKE`, `DTCALC`, …) prove the *platform*: execs,
IPC, the FAT/PortFile layer, GUI display lists. They are written to fit the
runtime from the first line, so they never stress its edges. A real port is a
different proof:

* **It brings behaviour nobody designed for this OS.** A Lua interpreter, a
  Z-machine with a virtual 64 KiB address space, a DEFLATE inflater and a
  arbitrary-precision calculator exercise recursion, large static buffers,
  pointer-heavy data structures and long input streams in combinations no
  hand-written app uses.
* **It is an honest compatibility claim.** "This OS can run Lua/Zork/zip/bc"
  means upstream semantics survived the rewrite — not that a lookalike demo
  printed the same string. If a port only covers a subset, the docs must say so
  (§7).
* **It exposes runtime gaps.** Every port has found the same wall: no hosted
  libc, a 64 KiB malloc cap, no stdio streams and no dynamic linking. Fixing
  those gaps *inside a port* via a small shim layer is what makes the next port
  cheaper.

A port is **not** a genuine Symbian SDK build. An "E32" here is a freestanding
ELF32/i386 repacked by `tools/elf2e32.py` into a relocatable image with an
optional PE2-style `EUSER.DLL` import (see `docs/E32_DEVELOPER_GUIDE.md`).
Everything below refers to that pipeline.

---

## 2. Definition of done for a port

1. Upstream source (or a clearly delimited subset) is present as freestanding
   C under `user/`, builds with `USER_FLAGS` and links only `start.o`,
   `runtime.o` and optionally `malloc.o`/`ipc.o` plus `$(LIBGCC)`.
2. A `user/user.mk` rule links with
   `ld -m elf_i386 --emit-relocs --defsym=<name>_printf_slot=0x408000 -T user/link.ld`
   and packs with `python3 tools/elf2e32.py ... --euser-import 915`, then
   validates with `tools/e32read.py --quiet`.
3. The image is bundled into `PROGRAMS.IMG` by `make isogrub` and launchable by
   bare name from the `X:/>` shell.
4. A host test stages the image with `EUSER.DLL`, boots the kernel, feeds a
   basic command and asserts markers, the ord-915 bind and a clean serial log.
5. The `docs/USER_APPS_CATALOG.md` row, this document's status table and the
   licence note match what actually runs — including the subset limitations.

---

## 3. Freestanding toolchain constraints

From `docs/COMPLEX_PROGRAMS.md`, `user/user.mk` and `user/link.ld`:

* **No hosted libc or CRT.** `-m32 -march=i486 -Os -ffreestanding -fno-pic
  -fno-pie -fno-builtin -fno-stack-protector -fno-exceptions -fno-rtti`, no
  `<stdio.h>`/`<stdlib.h>`/`<string.h>`/`<math.h>`, no `main`. Every program
  exposes `int app_main(void)`; `user/start.S` calls it, then `process_exit`.
* **Fixed memory map.** `.text` at `0x00400000`, `.data`/`.bss` at
  `0x00500000`. `elf2e32.py` only accepts R_386_32 (and same-region R_386_PC32)
  relocations and rejects unsupported sections rather than dropping fixups.
* **One imported symbol.** The PE2 import is `RDebug::Printf`, ordinal 915,
  written at `codeBase + 0x8000 = 0x408000`. The IAT slot must be defined there
  with `--defsym`; the unresolved seed is `915`, `0` means no import.
* **No FPU.** `int`/`unsigned` and fixed point only; no `double`, no `libm`,
  no floating-point exception support.
* **No threads, no dynamic DLLs, no POSIX filesystem.** Cooperative
  `sleep_ms`/`thread_yield` plus IPC; only the EUSER ord-915 import is bound;
  storage is PortFile channels, not file descriptors.

---

## 4. The libc-shim strategy

Ports do not get libc back. They get a thin vocabulary implemented on top of
`user/runtime.h` (the whole supported user API) plus a few local helpers.

### 4.1 `printf` -> EUSER ordinal 915

Each port declares its own absolute slot and a marker shim (pattern from
`user/calc.c` and `user/basic.c`):

```c
extern void (*<name>_printf_slot)(const char*);
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&<name>_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* genuine EUSER.DLL */
    else
        print(text);                         /* DebugPrint fallback */
}
```

Ord 915 is variadic and the shim passes exactly **one** argument, so marker
strings must contain **no bare `%`**. Format numbers into a stack buffer first
(hand-rolled `append_int`/`dec`, as in `calc.c`), then pass the finished line.
This is why the tests grep plain prefixes like `LUA:` or `BC:` rather than
printf-style output.

### 4.2 `malloc`/`free` -> `user/malloc.c`

`malloc.c` is a first-fit, address-ordered allocator over a genuine RChunk:

* initial chunk **16 KiB**, grown in **16 KiB** steps, hard cap
  **64 KiB (`MAX_CHUNK` 65536)**;
* 8-byte alignment, 8-byte block header, coalescing `free`;
* no locks (user-thread isolation is an exec-level property), no `realloc`
  unless the port implements it locally with copy+free;
* a request that cannot ever fit fails *before* creating a chunk.

Large buffers (a Z-machine story image, miniz dictionaries, Lua string tables)
must therefore be **static/global** or backed by an explicit
`chunk_create`/`chunk_adjust`, not by `malloc`. This is the single most common
cause of a port that "works on Linux, faults on EKA2".

### 4.3 String and numeric helpers

`memcpy`/`memset`/`strlen`/`strcmp` and integer-to-decimal come from tiny
local static helpers in the port source (see `user/selftest.c`'s `memcmp` and
`calc.c`'s `append` helpers). No `strtol`, no `snprintf`, no locale. bc's
arbitrary-precision digits and Lua's number formatting are both hand-rolled on
fixed-size unsigned arrays.

### 4.4 File I/O via PortFile

`file_open`, `file_read`, `file_close`, `file_create_write`, `file_append`,
`file_commit`, `file_save`, `change_dir`/`get_cwd` in `runtime.h` are logical
PortFile channels served by `portfilesrv` — **not** POSIX file descriptors and
not stdio streams. Consequences for ports:

* open a story/zip file by absolute path, read in bounded chunks into a static
  or chunk-backed buffer; no `fseek`/`mmap`;
* `X:` is a read-only snapshot of `PROGRAMS.IMG`, so bundled test data must be
  staged alongside the E32 (the harness can `mcopy` a small fixture);
* saving (bc sessions, Lua scripts edited in-app) needs the PortFile server
  running, exactly as `TEXTEDIT`/`DTEDIT` require;
* use `command_line(U16*, cap)` for arguments — the kernel passes UTF-16, so
  convert to bytes before handing them to upstream code.

---

## 5. How each port is built, staged and tested

All four share one flow:

```
edit user/<port>.c (+ local shim/helpers)
  -> make build/user/<NAME>.E32          # USER_FLAGS, --emit-relocs, --defsym
  -> python3 tools/e32read.py build/user/<NAME>.E32 --quiet
  -> make isogrub                        # curates build/user/*.E32 + EUSER.DLL
  -> python3 tools/r1_ported_software_test.py
```

`tools/mkgrubiso.py` curates every `*.E32` of at least `E32_MIN_SIZE = 30000`
bytes plus `EUSER.DLL` into `build/programs.img`, mounted as `X:` in the guest;
the harness verifies the staging with `mdir` before booting with `-vga std`
and `-cdrom`. A suspiciously small image (< 30 KB) is silently skipped by the
curator — another reason to keep the interpreter core in the E32.

Per-port notes:

* **LUA.E32** — Lua-style scripting core: expression evaluation, variables and
  `print` over the ord-915 shim. Input is a one-line program fed at the shell or
  through `command_line`; markers `LUA:` (start), `LUA: RESULT`, `LUA: DONE`.
  The io/os/math libraries are out of scope; `math` would need libm.
* **ZMACHINE.E32** — a Z-machine interpreter driving a small story file. The
  virtual machine's memory image is allocated as an explicit chunk (or static
  buffer), story data is read through `file_read`, and input is a line of
  commands (`look`, `quit`). Markers `ZMAC:` / `ZORK:` family.
* **ZIP32.E32** — miniz's inflate path wrapped as a zip member lister/extractor:
  reads a small staged `.zip`, walks the central directory and checks a CRC.
  Only decompression (`tinfl`) is ported; the compressor is not needed.
  Markers `ZIP:` family.
* **BC32.E32** — a desk-calculator style subset of bc: `+ - * /`, precedence,
  a handful of variables and `quit`. Big numbers are fixed-capacity digit
  arrays; there is no `libm`, no scripts and no GNU extensions. Markers `BC:`
  family.

`tools/r1_ported_software_test.py` probes for the four E32s, **skips missing
ones**, stages the present ones plus `EUSER.DLL`, boots one GRUB ISO per
available port, feeds its basic command, checks start/result/done markers, the
`e32run: bind slot ... ord 915` line and the absence of `Exc`,
`PanicKernExec` and `FAULT`, and writes one log per program under
`/tmp/opencode/`. Exit 0 means every available port passed (or all were
skipped).

---

## 6. Licensing notes to verify before claiming a port

This is a porting effort, so upstream licences travel with the code. Verify
each of these against the *actual* source file that lands in `user/`:

| Port | Likely upstream licence | What to verify / risk |
|------|-------------------------|-----------------------|
| Lua | MIT (permissive) | Keep the copyright notice; a vanilla Lua tree is too large — record which files were actually vendored. |
| Z-machine interpreter | Varies (often MIT/GPL) | The **interpreter** licence and the **game data** licence are separate; Infocom ZORK story files are copyrighted. Ship only a freely redistributable story fixture or a synthetic Z3. |
| miniz | Public domain / MIT (verify the exact release) | Attribute the author if the chosen release asks for it; note that only the inflate subset was copied. |
| bc | GNU bc is GPL (GPLv3 in current releases) | Copying GNU bc source into the tree obliges GPL compliance (licence + source availability). A clean-room subset avoids that but must be described as *not* GNU bc. |

Do not assume "small single-file library" means unlicensed. Record the exact
version, URL, licence text and the list of modified files in the port's source
header and in this table before merge.

---

## 7. Honest status table (snapshot — verify before relying on it)

| Port | E32 | What is real today | Status | Notes |
|------|-----|--------------------|--------|-------|
| Lua | `LUA.E32` | Lua-style expression/statement core rewritten freestanding; no io/os/math libs | **Subset / in progress** | Genuine interpreter semantics for a small language, not a full Lua 5.x tree. |
| Z-machine | `ZMACHINE.E32` | Interpreter core + chunk-backed VM memory; story read via PortFile | **Subset / in progress** | Blocked on a redistributable story file for a full Zork demo; synthetic fixture is the proof. |
| miniz | `ZIP32.E32` | Inflate-only zip member reader with CRC check | **Subset / in progress** | Compressor and most of miniz.c intentionally omitted. |
| bc | `BC32.E32` | Fixed-precision 32-bit-style subset with precedence and variables | **Blocked/subset** | Arbitrary precision and GNU bc compatibility out of reach; licence must be settled first. |

Statuses are intentionally conservative. "In progress" means the source and/or
build rule is not yet merged; "blocked" means a licence or memory constraint is
unresolved. When a port lands, replace the status with **Full source** or
**Subset (shipped)** and add the catalogue row + passing log path. Never claim
*Full source* for a vendored subset.

---

## 8. Recipe: porting another real C program

1. **Pick a bounded upstream program.** Prefer one C file, integer-only, small
   working set. Read its licence (§6) and copy the exact version into
   `user/<name>_port.c` with a header naming upstream, version, licence and
   what you removed.
2. **Audit libc usage.** List every libc call. Map `printf` to the ord-915
   marker shim, `malloc/free` to `user/malloc.c`, string functions to local
   helpers, file I/O to PortFile, `argv` to `command_line`. Delete anything you
   cannot map (`fseek`, `mmap`, `libm`, threads).
3. **Bound the memory.** Replace large heap allocations with static arrays or
   `chunk_create`; stay under the 64 KiB malloc cap; keep `.text` below
   `IAT_OFF` (0x8000) so the import can be patched.
4. **Add `app_main` and markers.** Wrap upstream `main` as `app_main(void)`,
   convert `argv`, emit `<NAME>: READY`, a result line and `<NAME>: DONE` with
   no bare `%`.
5. **Wire the build.** Add the `.o` prerequisites, link with `--emit-relocs
   --defsym=<name>_printf_slot=0x408000 -T user/link.ld`, pack with
   `elf2e32.py --euser-import 915`, validate with `e32read.py --quiet`, and add
   the E32 to the `userland` target.
6. **Stage and boot.** `make isogrub` (or `mcopy` the E32 + `EUSER.DLL` to a
   FAT32 disk at LBA 2048), boot `build/kernfull.elf`, feed a basic command at
   `EKA2>`, and confirm `e32run: bind slot ... ord 915`.
7. **Write the test.** Copy `tools/r1_calc_test.py`, assert markers in order,
   ord-915 bind and no `Exc`/`PanicKernExec`/`FAULT`; skip gracefully if the
   image is not built. Keep logs under `/tmp/opencode/`.
8. **Document honestly.** Add the row to `USER_APPS_CATALOG.md`, update the
   status table here with the *real* scope, and append the doc/test to
   `DOCS_INDEX.md`.
9. **Rebuild the ISO last.** `PROGRAMS.IMG` is a snapshot; an app added after
   `make isogrub` will not be on the disk until the ISO is rebuilt.

---

## 9. References

* `docs/E32_DEVELOPER_GUIDE.md` — the five-step app build/launch/test guide.
* `docs/COMPLEX_PROGRAMS.md` — hard constraints and the porting checklist.
* `docs/USER_APPS_CATALOG.md` — the authoritative per-image status and markers.
* `docs/TESTING_QA.md` — disk staging, marker and exception-scanning rules.
* `user/runtime.h`, `user/malloc.c`, `user/calc.c`, `user/basic.c`,
  `user/link.ld`, `user/user.mk` — the working code this write-up derives from.
* `tools/elf2e32.py`, `tools/mkgrubiso.py`, `tools/r1_calc_test.py`,
  `tools/r1_gui_apps_test.py`, `tools/r1_ported_software_test.py` — pipeline.

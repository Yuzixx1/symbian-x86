# Genuine Symbian userspace -> our PE2 E32 pipeline

Foundation for replacing hand-written `user/*.c` servers with **unmodified
sources from `kernelhwsrv/`**, compiled with the existing `-m32` toolchain and
packed by the existing `tools/elf2e32.py`. `kernelhwsrv/` is never modified.

## What was proven (2026-11-23)

`genuine/build/GENUINE.E32` was built from:
* our tiny harness `genuine/hello/hello.cpp` (includes **genuine** `e32std.h`
  / `e32panic.h`, calls genuine `EmptyFunction()`, prints through the PE2 IAT), **and**
* the **unmodified genuine EUSER source**
  `kernelhwsrv/kernel/eka/euser/epoc/x86/uc_utl.cpp` (810 bytes; provides
  `EmptyFunction` and `__cxa_pure_virtual`; only its `Panic(TCdtPanic)` stub is
  supplied by the harness).

`python3 genuine/prove_qemu.py` staged it with the genuine `EUSER.DLL`, booted
`build/kernfull.elf`, launched `genuine`, and got:

```
e32run: EUSER.DLL resident code=3d9948 data=0 exports=2503
e32run: bind slot 32768 ord 915 -> 003e7898
e32run: imports bound dll (1 ordinals)
e32run: Loaded r=0 / Resume()...
GENUINE: e32std.h compiled with gcc -m32 -ffreestanding
GENUINE: EUSER uc_utl.cpp compiled + called
GENUINE HELLO: printf via EUSER.DLL ordinal 915
genuine started (user process, pid=8)
```

RESULT: PASS (markers + ord-915 binding, no Exc/PanicKernExec/FAULT).

## Build command (exact)

From `qemu-port/`:

```bash
python3 tools/genuine_build.py \
  genuine/hello/hello.cpp \
  ../kernelhwsrv/kernel/eka/euser/epoc/x86/uc_utl.cpp \
  --name GENUINE --type exe --iat-sym genuine_printf_slot --euser-import 915
python3 tools/e32read.py genuine/build/GENUINE.E32
python3 genuine/prove_qemu.py
```

Output: `genuine/build/GENUINE.E32` (33,008 bytes). The tool compiles each
source, adds `user/start.S` + `user/runtime.c` for EXEs/servers, links with
`ld -m elf_i386 --emit-relocs`, calls `elf2e32.py` and validates with
`e32read.py --quiet`. No `make clean`, no writes outside `genuine/` and
`tools/genuine_build.py`.

### Define set that actually compiles the genuine x86 headers

Tested with gcc/g++ 16.2.1, `-m32 -ffreestanding -Os`, on `e32std.h`,
`e32cmn.h`, `e32base.h`, `e32svr.h`, `u32std.h`, `e32math.h`, `e32panic.h`:

```
-D__GCC32__ -D__X86__ -D__EPOC32__ -D_UNICODE
-DIMPORT_C= -DEXPORT_C= -DIMPORT_D= -DEXPORT_D=
-D__NO_THROW=
-DTEMPLATE_SPECIALIZATION=template<>
-D__cdecl=__attribute__((cdecl)) -D__fastcall=__attribute__((fastcall))
-D__stdcall=__attribute__((stdcall))
-DNONSHARABLE_CLASS(x)=class\ x -DNONSHARABLE_STRUCT(x)=struct\ x
-D_FOFF(c,f)=((TInt)__builtin_offsetof(c,f))
```

Without these, the headers fail on `TEMPLATE_SPECIALIZATION`, `__NO_THROW`,
`IMPORT_C`, `NONSHARABLE_CLASS` and `_FOFF`. `__GCCE__`/`__WINSCW__` are the
ARM/EVCB toolchain defines and are **not** the right choice here; the genuine
GCC/x86 branch is selected by `__GCC32__ + __X86__`. Compiler flags also use
`-mpreferred-stack-boundary=2` (MSVC `__cdecl` 4-byte stack alignment), and
`-fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-builtin`.

### Include paths that worked (automatic defaults)

```
<repo>/kernelhwsrv/kernel/eka/include        # e32std.h e32cmn.h e32base.h e32svr.h
<repo>/kernelhwsrv/kernel/eka/euser           # us_std.h, uc_*.h
<repo>/kernelhwsrv/kernel/eka/euser/epoc      # u32std.h
<repo>/kernelhwsrv/kernel/eka/euser/maths     # um_std.h
<repo>/qemu-port/build/export                 # generated exec_enum.h (u32exec.h)
<repo>/qemu-port/inc                          # port prefix shims (harmless here)
```

`qemu-port/user` is deliberately **not** on the path: its hand-written
`e32base.h` / `e32cmn.h` would shadow the genuine headers.

## Link types and exports

* `--type exe` — uses `genuine/link_exe.ld`, adds port startup, ordinary
  process. This is the proven path.
* `--type server` — `genuine/link_server.ld`; loader-visible layout is the
  same as EXE (the port has no separate server UID1 path yet). **Do not
  combine with `--def`**: the kernel loader hard-rejects non-DLL images
  that carry exports (`variant/e32load.cpp:407`: `!isDll && expCount!=0 ->
  KErrNotSupported`), so `genuine_build.py` refuses `--def` unless
  `--type dll` (override with `--allow-non-dll-exports`, which only warns
  loudly and produces an e32read-valid but unloadable image).
* `--type dll` — `genuine/link_dll.ld`; flips UID1 to `0x10000079` and
  `KImageDll` in flags, patches UID checksum/header CRC. Built and validated
  (`GENUINEDLL.E32`, `EXPORTPROBE.E32`), not yet load-tested in QEMU. Provide
  `_start` (or your own entry) in the sources: DLL builds skip the port
  startup by default.

`elf2e32.py` emits `exp_off=exp_count=0`, so `genuine_build.py` post-processes
the E32 when `--def` is given: it links a zero-filled `.genuine_exp` pad at
the end of `.text` (see the link scripts), resolves each `.def` export to its
ELF symbol, writes the export directory into that pad, then recomputes the
Symbian header CRC via `mke32.crc32_symbian`. `e32read.py` accepts the result.

Genuine `.def` files come in two flavours and both are parsed: plain /
GCC-mangled names (`bx86gcc/euseru.def`: `_ZN6RDebug6PrintfEPKcz @ 915`) and
MSVC-decorated C++ names (`bx86/*.def`: `?Printf@RDebug@@SAXPBDZZ @ 1345`).
The name is everything before the final ` @ <ordinal>`. **Any non-empty,
non-comment EXPORTS line that cannot be parsed, or any export that cannot be
mapped to an ELF symbol, aborts the build with a nonzero exit and a
file:line message — entries are never silently dropped** (a dropped entry
would shift every later export ordinal and mis-bind the directory).

MSVC-decorated names never equal gcc's Itanium symbols, so map them
explicitly:

```bash
--def genuine/hello/export_probe.def \
--export-map '?Printf@RDebug@@SAXPBDZZ=_Z13EmptyFunctionv'
```

Use `nm genuine/build/<NAME>.elf | c++filt` to discover the GCC symbol.
Self-check: `python3 tools/genuine_build.py --selftest` parses the genuine
defs read-only (`fileserver/bx86/efsrvu.def`, `eka/bx86/euseru.def`,
`eka/bx86gcc/euseru.def`) and synthetic plain/decorated/alias/garbage lines,
reporting parsed/mapped/failed counts (2026-11-23: 5105 entries, 2432
MSVC-decorated, 0 failed).

## EUSER imports / IAT

`--euser-import 915 --iat-sym <symbol>` does what `user.mk` does by hand:
`ld --defsym=<symbol>=0x408000` puts an absolute IAT slot at `codeBase+0x8000`;
`elf2e32.py` emits the PE2 import section and seeds the slot with the ordinal;
the loader overwrites it with the resolved EUSER export before resume. Our
code reads the slot with `*(unsigned*)(void*)&<symbol>` and calls it.
Ordinal 915 is `RDebug::Printf(char const*, ...)` in the **staged** genuine
EUSER.DLL used by the QEMU tests (bind line: `bind slot 32768 ord 915`).
The current in-tree `kernelhwsrv/kernel/eka/bx86/euseru.def` lists `Printf` at
a different ordinal (1345) — always pass the ordinal explicitly.

## Known blockers / limits

* Only one DLL dependency (EUSER) and only PE2-style imports are supported by
  `elf2e32.py` / the kernel loader; multiple DLLs need pipeline work.
* Code must fit below `iat_off` (0x8000 by default) because the IAT sits at
  `codeBase+0x8000`; use `--iat-off` for bigger components. Export targets
  are conservatively held to the same bound even when no import exists.
* Stack size is fixed at **8192** bytes in every image, hardcoded by
  `tools/elf2e32.py:109` — not configurable via `genuine_build.py` or the
  link scripts.
* `--name` must be a single safe path component (`^[A-Za-z0-9][A-Za-z0-9._-]*$`);
  anything with `/`, `..` or an absolute path is rejected, so all outputs are
  confined to `genuine/build/` (kernelhwsrv/ can never be written via `--name`).
* `--def` is only allowed with `--type dll` (see above).
* No hosted libc, exceptions, RTTI or thread-safe statics. Genuine EUSER
  sources often need small stubs (`Panic`, `TRealX`, `__cxa_pure_virtual`,
  allocator) supplied by harness code.
* `.def` exports for DLL images validate with `e32read.py` and bind by
  ordinal (checked for `EXPORTPROBE.E32`), but are not yet exercised by the
  kernel's DLL loader in QEMU.
* The harness `genuine/hello/hello.cpp` is build glue, not a port of Symbian
  logic; the genuine logic comes from the unmodified `uc_utl.cpp`.

## Files

```
tools/genuine_build.py          new builder (compile/link/pack/validate)
genuine/link_{exe,server,dll}.ld link scripts
genuine/hello/hello.cpp         IAT + genuine-header harness
genuine/hello/genuine_exports.def example export list (single export)
genuine/hello/export_probe.def  3-export probe .def (plain + MSVC-decorated)
genuine/prove_qemu.py           FAT staging + QEMU proof runner
genuine/build/                  generated .o/.elf/.E32 outputs
genuine/README.md               this file
```

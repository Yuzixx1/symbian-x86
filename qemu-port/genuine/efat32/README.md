# Genuine F32 DLL / FSY images (`EFSRV.DLL`, `EFAT32.FSY`)

`build_fsy_dll.py` turns the genuine `kernelhwsrv/` F32 objects (built by
`genuine/f32_efsrv_build.py` into `genuine/obj/{EFSRV,F32}`) into the two
E32 images the EFSRV retire gate looks for under `genuine/build/`:

| image | genuine sources | exports | EUSER PE2 imports |
|---|---|---|---|
| `EFSRV.DLL` | `fileserver/sfsrv/cl_*.cpp` (21 objects) | 421 ordinals from `fileserver/eabi/efsrvu.def` (GCC-mangled EABI .def) | 131 |
| `EFAT32.FSY` | `fileserver/sfat32/*.cpp` + `fs_utils/` (24 objects) | ordinal 1 = `CreateFileSystem` (`sfat32/sl_main.cpp`) | 173 |

`kernelhwsrv/` is never written. Both images are real DLL-shaped E32 files:
UID1 `0x10000079` (`KDynamicLibraryUidValue`), `KImageDll` flag, PE2 IAT,
export directory inside `.text`, NoHoles export-desc tail. `tools/e32read.py
--quiet` passes for both (exports, relocs, CRC, UID checksum all validated).

## Build

```
python3 genuine/f32_efsrv_build.py --rebuild   # if obj/ is stale
python3 genuine/efat32/build_fsy_dll.py
```

Build strategy: pass-1 link discovers undefined symbols; EUSER symbols become
`jmp *slot` thunks bound to absolute PE2 IAT slots at `0x440000..` (kept below
the `.data` load base `0x500000`); `genuine/pack_multi.py` writes the multi-DLL
import E32; `tools/genuine_build.py:patch_exports(make_dll=True)` flips the DLL
UID/flags and writes the ordinal export directory.

## Port glue and stubs (documented, never genuine logic)

- `start.S` — `_start: ret`. E32 libraries are never started as processes; the
  loader only needs a valid in-code entry slot.
- `dll_glue.cpp` — freestanding `operator new/delete` (64 KiB static bump
  heap), libc helpers (`memcpy/memmove/memset/memcmp/memclr/memcompare`),
  `__cxa_pure_virtual`/`__cxa_atexit`, and `efat32_export_stub` (fallback
  export target for `.def` ordinals with no emitted symbol). No kernelhwsrv
  source is replaced.
- `EFAT32.FSY` generated ret-0 stubs (70): cross-DLL symbols the FSY imports
  from the genuine `!FileServer` engine (`CFileSystem`, `CMountCB`, `CFileCB`,
  `F32Properties`, `HAL`, `TDrive`, code-page/notify helpers, …). Our PE2
  packer supports one provider (EUSER), so these cannot be expressed as real
  imports yet. They are listed verbatim in the build log and are the main
  reason the FSY is **not yet functionally loadable** by the real EFILE;
  wiring FSY→EFILE exports is follow-up work owned by the EFILE agent.
- Genuine EFSRV exports: 410/421 `eabi/efsrvu.def` ordinals resolve to real
  GCC symbols in the linked ELF. The remaining 11 (10 RTTI `_ZTI*` slots that
  `-fno-rtti` does not emit, plus `RRawDisk::Caps`, which is not defined by
  this client-only object set) point to the documented `efat32_export_stub`
  so the ordinal layout stays exact. A missing/undefined export is never
  silently dropped: it is logged by ordinal and name at build time.

## Gate status

`tools/r1_genuine_retire_test.py` reports `BUILT: YES` for EFSRV/F32 once
these two files exist. A full `PASS` still requires the genuine `!FileServer`
(`EFILE.E32`) to actually start and answer the `F32CLIENT` probe — that fix
is in flight and is not faked here.

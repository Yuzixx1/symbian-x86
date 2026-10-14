# User-mode processes — plan (EKA2 Direct model, x86)

## Current foundation follow-up

- Loader captures each launched process's selected drive and CWD before
  resume; relative PortFile reads/writes and `..` traversal use that snapshot.
  Reaping removes the state; the 16-entry limit fails before process creation.
  Cross-drive relative paths currently use the other drive's root.
- Genuine logical-channel handles support independent opens and automatic
  close on thread exit. `FILECHK.E32` deliberately leaves two handles open;
  the QEMU test checks independent read positions and a zero channel census
  after each of three runs. No parallel handle table was added.
- CP streams in 256-byte pieces to the full 64 KiB PortFile range; oversize
  sources fail at open, preserving an existing destination. This boundary is
  covered by the integration test.
- Exit history (16-entry ring + `EXITS` verb), mutable validated per-process
  CWD (`CWD.E32`), and a 16 KiB first-fit user heap (`MALLOC.E32`) are done;
  blocking `wait`, chunk-backed heap growth, and replacement of the snapshot
  PortFile transport with a filesystem server remain pending.
- `python3 tools/test_userland.py` verifies the above on q35/AHCI.

Status: user processes RUN (load, ring-3 run, draw, files, IPC, exit; shell
survives). See README §20. The stage list below is the original plan; items
marked [x] later in this file are done.

Goal: run genuine Symbian user EXEs (E32 image format) as user-mode
processes under our kernel. Genuine tree stays read-only; new code in
`variant/` + `tools/`.

## Format (read-only reference)
- `kernelhwsrv/userlibandfileserver/fileserver/inc/f32image.h`:
  `E32ImageHeader[V]` (uids, `EPOC` sig, header CRC, flags→ABI/Ept/HdrFmt/
  ImpFmt, code/data/bss/heap/stack sizes, entry offset, code/data bases,
  dll refs, export dir, text size, file offsets, reloc/import sections,
  CPU id, priority). Layout: header, code (.text+IAT+exports), then data,
  imports, code relocs, data relocs. Validators exist
  (`ValidateHeader/Imports/Relocations/WholeImage`).
- Kernel side: `kernel/eka/kernel/sprocess.cpp` (`DProcess`, uses
  `M::NewCodeSeg`), `scodeseg.cpp` (`DCodeSeg`), `sexec.cpp` (exec model).

## Stages
1. **Reader** (`tools/e32read.py`, host-side): parse + validate an E32 file
   (header/CRC, sections, imports, relocs), dump summary. Needs a TEST
   BINARY first (see 2) — no Symbian toolchain here.
2. **Test binary problem**: no `elf2e32`/gcce available. Options, in order:
   (a) hand-assemble a minimal valid E32 EXE with a Python builder
   (`tools/mke32.py`: header + one code section + entry stub + relocs);
   (b) cross-build a real Symbian toolchain (weeks — no).
   Go with (a): the format is fully described in-tree.
3. **In-kernel loader** (`variant/e32load.cpp`): read file from FAT32 into
   a kernel buffer, validate, allocate code/data chunks (`RChunk`,
   user addresses per Direct model: code linked for its `iCodeBase` —
   MUST be <2GB user side; our test binary will link accordingly),
   copy sections, apply code+data relocs, resolve imports (only kernel
   exports at first: `User::`/`RThread` subset via ordinal IAT fill —
   needs an export-name table for `ekern.exe`... our kernel has no export
   table; stage 3a runs import-free binaries, i.e. pure-syscall test
   exes using `int`-gate? NO — syscalls go via ordinal calls into ekern;
   import-free means no external calls at all except a Hut-style exit
   trap we define (e.g. SVC 0 / illegal-opcode exit convention)).
4. **Process/thread bring-up**: `Kern::ProcessCreate` + first
   `RThread` with user PC/SP, user stack pages, heap reserve; `ESTART`
   equivalent: our loader IS estart (no separate estart.exe yet).
5. **Shell verbs**: `exec <file> [args]` (load+run+report exit),
   `ps` already lists processes.
6. **Later**: DLLs + IAT import resolution against kernel exports,
   `RProcess` args/env, heaps, file-server session for user processes,
   capabilities (SSecurityInfo present in header — enforce later),
   compression (Uid/inflate — tools emit uncompressed first).

## First unblocked step
`tools/mke32.py` minimal-EXE builder + `tools/e32read.py` validator
round-trip on Linux (no kernel changes, no toolchain). Then the
in-kernel loader against the known-good file.

## Status (done)
- [x] `tools/mke32.py` emits a 220-byte import-free uncompressed x86 EXE
  (E32ImageHeaderV, NoHoles, `ECpuX86`, codeBase `0x400000`) that passes
  the genuine `ValidateHeader` rules; `tools/e32read.py` validates it
  (exit 0) and rejects corrupt/truncated/non-E32 inputs (exit 1).
- [x] In-kernel twin `variant/e32load.cpp` (`E32Validate`, same genuine
  CRC32/UID algorithms) + `FatRead` + shell `exec <file>`: verified live
  in QEMU (`exec HELLO.E32` validates, `exec HELLO.TXT` rejected,
  corrupt `BAD.E32` prints `corrupt` — host and kernel agree).
- [x] In-kernel loader (`variant/e32load.cpp` cont.): data copy + BSS
  zero + TEXT/DATA/INFERRED relocs (genuine `svRelocateSection` mirror);
  `Kern::ProcessCreate` → copy → `Loaded` → `Resume`; ring-3 execution
  via lazy TSS + DPL3 exec gates (genuine `__X86Vector20/21`).
- [x] Initialized-data relocation sites validated against `.data` bounds
  and applied after the data copy; host negative tests and a QEMU
  process-data readback live in `tools/test_e32_relocs.py` and
  `tools/test_data_reloc.py`.
- [x] Process/thread bring-up + `exec` runs binaries (VGA proofs);
  exit via `ThreadKill` exec; `kill <id>`; priority passthrough.
- [x] User heap live: private chunk (`ChunkCreate`) + readback, mutex
  probe, handle closes before exit.
- [x] Pilot imports: EUSER ord 1–2 trampolines, ELF import section +
  IAT, kernel/host twins agree incl. negatives; `build/imp.e32`.
- [x] Teardown: `DProcess::Release` stall root-caused (seg creator ref
  + attach-open pin; global delete/free asymmetry) and repaired via
  post-mortem reaper + heap-routed free — see README §22. Steady-state
  verified (heap/RAM flat, no zombies, kill path clean).
- [x] Real DLL step 1: `EUSER.DLL` as a file (export dir of code offsets);
  kernel validates DLL headers + export targets, loads it resident once,
  resolves EXE ordinals from the table (`tools/test_dll.py`); trampoline
  fallback kept when the DLL is absent.
- [x] Program command lines: genuine `HBuf` in `DProcess::iCommandLine`,
  `exec` tail-join, `args` verb, user-side CmdLen export (ord 3);
  `tools/test_args.py` round-trips shell -> process -> user thread.
- [x] Exit history: 16-entry ring in `E32Reap` (name/id/type/reason, orphan
  path marked) + shell `EXITS` verb; `test_crash.py`/`test_conc.py` cover
  fault attribution and kill recovery. Blocking `wait` still out.
- [x] User allocator: `user/malloc.c` first-fit over 16 KiB (alignment, reuse,
  OOM-NULL, coalescing); `MALLOC.E32` in `test_userland.py`. Chunk-backed
  growth still out.
- [x] Program time/sleep/random: `sleep_ms` (fresh RTimer per call),
  `thread_yield`, `time_now` (TimeNow + libgcc), `random32` (MathRandom);
  `SLEEP.E32` proves repeat sleeps. Thread-embedded `User::After` hangs on
  reuse in this port — runtime avoids it.
- [x] Console input: `PortCon` lease (`variant/usercon.cpp`) + `read_line`;
  shell yields while leased and reclaims on owner death; `INTERACT.E32` in
  `test_interact.py`. Real console server still out.
- [x] Build hygiene: `build/user/runtime.o` now depends on `user/runtime.c`
  (a missing dep silently shipped stale user binaries during this work).
- [x] Compiled userland: freestanding GCC + `user/link.ld` +
  `tools/elf2e32.py` (R_386_32/PC32, rejects the rest) builds
  `build/user/ECHO.E32` (`make userland`); genuine `ProcessCommandLine`
  TDes8 copy + `DebugPrint` console; `tools/test_userland.py` covers
  empty/quoted/long tails end to end in QEMU.
- [x] User-mode files step 1: `PortFile` logical device (`variant/userfiles.cpp`)
  over genuine `ChannelCreate`/`ChannelRequest` executives; open absolute
  `C:\...` snapshot + chunked read; `build/user/TYPE.E32` prints a file;
  missing-file and no-arg paths tested in `tools/test_userland.py`.
- [x] User-mode files step 2: write path (open-write + 256B chunk appends +
  commit via `FatWritePath`, 64 KiB cap, 8.3 names); `file_save()` in the
  port runtime; `build/user/CP.E32` copies `SRC -> DST`; shell `TYPE`
  verifies; heap-CS discipline fixed (alloc/free under `ThreadEnterCS`).
- [ ] Full DLL loading, args/env, caps, compression.

# R1-STARTUP — genuine x86 process start-up, TLS, heap, leave, imports, relocation (research v0, NOT an ABI freeze)

Card: **R1-STARTUP** (Strong tier). Research only: static source reading, no code,
no build, no QEMU run, no hardware run. Nothing here is "QEMU-ready" or
"hardware-verified". It feeds R1-ABI/v1, R1-EUSER and R1-LOADER.

Revisions:

- Port baseline `fa62323eea7bc96cefd666e40f5189567731bd44`, detached worktree
  `/tmp/opencode/r1-startup` with `kernelhwsrv -> /home/yuzi/Documents/Sybian REAL/kernelhwsrv`
  (read-only symlink).
- Upstream `kernelhwsrv` `0c3208650587ac0230aed8a74e9bddb5288023eb`, 0 dirty files.
- **Port-side citations (`variant/e32load.cpp`, `user/start.S`, `tools/mke32.py`, `Makefile`)
  were read from the main workspace's uncommitted working tree**, because the `fa62323`
  worktree lacks the untracked baseline work. Their line numbers can drift.
- Upstream paths below are relative to `kernelhwsrv/`. `eka` = `kernel/eka`.

Complements `startup_paths_R1.md`, which maps the **port's** chain (shell → `E32Run`).
This note traces what a **genuine** x86 EXE expects once the kernel hands it control.

## 1. Genuine x86 start-up chain (static reading)

### 1.1 Kernel → user mode (`eka/kernel/x86/ckernel.cia:36-125`, `DThread::EpocThreadFunction`)

For `EThreadUser` (type 3) the kernel:

1. Marks `iUserThreadState = EUserThreadRunning` (line 74-75).
2. Fills the whole user stack with `0x29292929` (lines 76-86).
3. Copies the `SThreadCreateInfo` block (`iTotalSize` bytes) to the **top of the user stack**
   (lines 87-95).
4. Builds an `iretd` frame (lines 104-123):
   - SS = Ring3 data, ESP = just below the copied block.
   - EFLAGS = `0x41202` (AC=1, IOPL=1, IF=1).
   - CS = Ring3 code.
   - **EIP = `DCodeSeg::iEntryPtVeneer` for the first thread, `DProcess::iReentryPoint` for others** (lines 109-118).
   - DS/ES set to Ring3 data. GS is also loaded with Ring3 data before the stack fill.
5. Sets **EBX = 0 for the first (original) thread, 1 for any other** (lines 113-119, `KThreadFlagOriginal`).

Contract delivered to user code: **EBX = entry reason, ESP → creation-info block.**
No arguments are pushed. Command line is fetched by exec call, not passed.

Entry reasons seen (`eka/include/u32std.h:457-459`):

- 0 and 1: thread entry (inferred from the `_E32Startup` compare).
- `KModuleEntryReasonProcessAttach` = 2.
- `KModuleEntryReasonException` = 4 (set at `eka/kernel/x86/ckernel.cpp:166-168`, EIP = `iReentryPoint`).
- Detach value was not read in this trace.

`iReentryPoint = iCodeSeg->iEntryPtVeneer` (`eka/kernel/sprocess.cpp:505`).

**First-thread creation** (`eka/kernel/sprocess.cpp:236-255`): `SStdEpocThreadCreateInfo`
with `iType=EThreadUser`, `iFunction=iFileEntryPoint`, `iUserStackSize` and heap min/max from
the image header, **`iAllocator=NULL`**, `iTotalSize=sizeof(t)`, name `"Main"`,
flags `KThreadFlagProcessPermanent|KThreadFlagOriginal`.

Size rules (`eka/kernel/sthread.cpp:954-957`): `iTotalSize >= sizeof(SThreadCreateInfo)` and
`<= KMaxThreadCreateInfo (256)`; `u32std.h:298` says it must be a multiple of 8.

Layout (`u32std.h:286-324`): the base block has 9 words, a `TPtrC iName` and `iTotalSize`;
the std-EPOC extension adds `iAllocator`, `iHeapInitialSize`, `iHeapMaxSize`, `iFlags`.
My hand count on 32-bit is 48 + 16 = **64 bytes**. *Hand-computed, not compiled.*

### 1.2 Entry veneer on x86

`scodeseg.cpp:714-716` defaults `iEntryPtVeneer = iFileEntryPoint` ("modified by loader if
necessary"). A repo-wide grep for `iEntryPtVeneer\s*=` finds only: default/zero initialisers
(`sf_lepoc.cpp:1522,1873`), the default above, copies (`scodeseg.cpp:749,817`,
`pprocess.cpp:58`, `putils.cpp:321`) and an emulator assignment. The V7 entry stubs
(`up_utl.cpp:229-258`) are `#ifdef __MARM__`. **On x86 no veneer substitution was found, so
veneer == file entry point.** This resolves unknown #1 in `startup_paths_R1.md` §4 for
static reading: the port passing the file entry to both fields matches upstream. The
requirement that the entry be a *genuine* `_E32Startup` stays open.

### 1.3 `_E32Startup` (`eka/euser/epoc/x86/uc_exe.cia:28-85`) and `RunThread` (`uc_exe.cpp:37-66`)

- Entry bytes: `JMP .+16` (`EB 0E`), then pad so that **a zero word sits at entry+12**
  (comment "need zero word at offset 12 from entry point"), then:
  - `cmp ebx,2; jae` → else `ecx=ebx; edx=esp; jmp RunThread` (`__fastcall(aNotFirst, aInfo)`).
  - `ebx==4` → pushes ESP and calls `User::HandleException`, then pops the whole exception
    frame and `retf` (raw byte `0xCB`).
  - Anything else → `User::Invariant()`.
- `RunThread`:
  1. `UserHeap::SetupThreadHeap(aNotFirst, cinfo)` (§3).
  2. `UserSvr::DllSetTls(KGlobalDestructorTlsKey=-1, KDllUid_Special=-1, globalDestructorFunc)`.
  3. Not-first thread: `(*cinfo.iFunction)(cinfo.iPtr)`.
  4. First thread: `User::InitProcess()` (static entry points of implicitly linked DLLs), then
     `constructStatics()`, then `r = E32Main()`.
  5. `User::Exit(r)` (never returns).
- `constructStatics`/`destroyStatics` are macros from `eka/include/win32crt.h` and `nwdl.h`.
  **Not read in this trace** (see §7).

Build wiring: `eexe.lib` on X86 = `uc_exe.cia + uc_atx.cpp + uc_exe.cpp`, capability all
(`eexe.mmp`). `edll.lib` = `uc_dll.cia + uc_dll.cpp + uc_atx.cpp`. The bx86gcc link sets the
entry symbol `_E32Startup` for EXEs (`cl_x86gcc.pm:336,651`, from the SDK-route scratch).

### 1.4 Implicit DLL initialisation and exit (`eka/euser/us_func.cpp:795-895`)

- `User::InitProcess` → `CallStaticEntryPoints(ETrue)` → `E32Loader::StaticCallList`
  (`eka/kernel/scodeseg.cpp:2059-2089`): returns the dependency-ordered entry-point list,
  max `KMaxLibraryEntryPoints = 0x100` (`e32ldr_private.h:29`), **last element is always the
  process entry point**. It also clears `KThreadFlagOriginal` for the calling thread.
  Each is called with `KModuleEntryReasonProcessAttach`, protected by a
  `KNestedEntryPointCallKey` TLS slot. Then `StaticCallsDone` sets `EStaticCallsDone`.
- x86 DLL entry `_E32Dll` (`uc_dll.cia:29-49`) has the same `JMP .+16` / zero-word layout, then
  `jmp _E32Dll_Body`. `_E32Dll_Body` (`uc_dll.cpp:29-35`): attach → `constructStatics()`,
  detach → `destroyStatics()`, returns `KErrNone`.
- `User::Exit(r)`: `Exec::UserThreadExiting(r)` (`sexec.cpp:2768`; if process-permanent it
  kills all other threads first) → if last thread, call TLS `(-1,-1)` global-destructor, then
  `CallStaticEntryPoints(EFalse)` (detach in reverse) → close handle list
  (`Exec::LastThreadHandle`) → `ExitCurrentThread(EExitKill, r)`.

**Port today:** `process_exit` (exec 115) from `_start`. Statics, global destructors,
detach calls and handle cleanup via `User::Exit` are not part of the port's path.

## 2. TLS

- `Dll::SetTls/Tls/FreeTls` (`eka/euser/epoc/up_dll_tls.cpp:44-86`) call
  `UserSvr::DllSetTls/DllTls/DllFreeTls(MODULE_HANDLE, uid, ...)`.
  - `MODULE_HANDLE = (TInt)_E32Dll` (`up_std.h:26`).
  - Under `__EPOC32__` the uid argument is **`*(((TInt*)MODULE_HANDLE)+3)`**: the word at
    entry+12, i.e. the **code-segment ID** (the zero word of §1.3).
  - Non-`__EPOC32__` (emulator) uses `KDllUid_Special`.
- Kernel (`sexec.cpp:1206-1232`, `sthread.cpp:1239-1280`): a per-thread sorted array of
  `STls{iHandle, iDllUid, iPtr}` keyed by handle. `Tls()` returns the pointer only if
  `iDllUid` matches. Missing → NULL. `DllSetTls` inserts, so it can fail with `KErrNoMemory`.
- Process-level keys use `(-1,-1)`: `KGlobalDestructorTlsKey` (`u32std.h:545`) and the nested
  entry-point key.
- **Loader writes the ID** (`sf_lepoc.cpp:2519-2540`): at `iFileEntryPoint + KCodeSegIdOffset(12)`,
  if the word is 0, store `++NextCodeSegId` (fault on wrap); existing non-zero kept.
  `ValidateHeader` requires `iEntryPoint + 12 + 4 <= iCodeSize` (`f32image.h:741-745`).
- Memory-model dependency: the ARM-only `__CPU_HAS_CP15_THREAD_ID_REG` path
  (`TLocalThreadData`, `u32std.h:763-`) is not used on x86; `GetHeap()`,
  `GetActiveScheduler()`, `GetTrapHandler()` go through `Exec::Heap()`, `Exec::ActiveScheduler()`,
  `Exec::TrapHandler()` (`euser/us_data.h:44-66`). These exec calls are therefore on the
  start-up critical path.

**Port gaps:** `variant/` has no code-seg-ID handling (a grep for `KCodeSegIdOffset`/`csid`
hits only `tools/mke32.py:33` and `tools/e32read.py:160`, which merely bounds-check).
`mke32.py` demo images put x86 code at entry offset 0 (`mke32.py:361`), so **whether their
entry+12 bytes are zero is unverified**: a loader that stamps IDs must not corrupt them.

## 3. Heap

- `UserHeap::SetupThreadHeap` (`up_utl.cpp:137-157`):
  - `iAllocator == NULL && iHeapInitialSize > 0` → `CreateThreadHeap`.
  - `iAllocator != NULL` (child sharing a heap) → `Open()` + `User::SwitchAllocator`.
  - Neither (initial size 0 and no allocator) → **returns `KErrNone` with no heap**.
- `CreateThreadHeap` (`eka/common/heap_hybrid.cpp:3254-3317`): `min = align_up(initial, page)`,
  `max = Max(maxsize, min)`, creates a chunk via `TChunkCreateInfo::SetThreadHeap(0, max, "$HEAP")`
  (hybrid heap: disconnected chunk with doubled max, line 3273-3279), then `ChunkHeap(...,
  EChunkHeapSwitchTo | EChunkHeapDuplicate)`.
- Required kernel/exec surface: chunk create of thread-heap type, adjust/commit, handle
  duplicate, `SwitchAllocator` (all genuine `Exec::` calls compiled from this upstream tree).
- Header fields feeding it: `iHeapSizeMin/Max` (validated `0 <= min <= max`,
  `f32image.h:725-731`), `iStackSize >= 0`.

**Port:** `E32Run` passes header heap/stack through `TProcessCreateInfo`
(`e32load.cpp:667-669`), so the kernel side of the first-thread info is plausible. The port's
own `_start` never creates a user heap through this route. Whether `SetThreadHeap` chunks work
in the port's memory model is **unverified** (see §7).

## 4. Leave / trap / exception re-entry

- `__LEAVE_EQUALS_THROW__` is stripped unless `__SUPPORT_CPP_EXCEPTIONS__` (`e32def.h:29-31`),
  and a grep of `.mmh/.mmp/.inf/.mk/.flm/.xml/.hrh/.def` found no define (only `release.txt` and a
  conditional in `kerneltest/e32test/group/bld.inf:948`). **Working assumption: x86 builds use
  the trap-frame path**, not C++ exceptions. Not confirmed against a real build.
- Trap-frame path (`eka/euser/epoc/x86/uc_trp.cia`, `uc_trp.cpp`):
  - `TTrap::Trap` saves EIP, EBX, EBP, ESP, ESI, EDI, DS, ES, FS, GS into the frame
    (offsets 0..36), zeroes the result, stores `iResult`, then calls `DoTrap` →
    `Exec::PushTrapFrame` (`sexec.cpp:2375`).
  - `User::Leave` → `Exec::PopTrapFrame` (`sexec.cpp:2400`), panics
    `EUserLeaveWithoutTrap` if none, stores the reason, notifies the `TTrapHandler`, then
    `DoLeave` restores registers. (I read the `Trap` body; `DoLeave` was truncated in my
    read, so register-restore detail is not verified.)
- Exception re-entry: kernel pushes an exception frame on the user stack and re-enters at
  `iReentryPoint` with EBX=4 (`ckernel.cpp:160-168`); `_E32Startup` then calls
  `User::HandleException` and unwinds the frame with `retf`. The unwinding order in
  `uc_exe.cia:61-84` must match `PushExcInfoOnUserStack`; that function was **not read**.

## 5. EUSER import resolution

### 5.1 Genuine loader (`userlibandfileserver/fileserver/sfile/sf_lepoc.cpp`, `inc/f32image.h`)

- Import section: `E32ImportSection{iSize}` followed by `iDllRefTableCount` blocks
  `E32ImportBlock{iOffsetOfDllName, iNumberOfImports, [imports]}` (`f32image.h:381-439`).
  The list is **code offsets for ELF format, ordinals for PE format, and absent for PE2**
  (`f32image.h:397-426`).
- Image flag bits (`f32image.h:101-107`): `KImageImpFmt_ELF = 0x10000000`,
  `KImageImpFmt_PE2 = 0x20000000`, PE = 0. Anything above PE2 is rejected
  (`f32image.h:722-723`).
- ELF-derived fixup (`svElfDerivedFixupImportAddresses`, `sf_lepoc.cpp:583-736`): each listed
  code location holds **low 16 bits = ordinal, high 16 bits = addend**. The loader writes
  `exportDir[ordinal] + delta + addend`. Ordinal > export count, or an export slot that is 0
  or equals the exporter's **absent-ordinal marker (its `iFileEntryPoint`)**, returns
  `KErrNotSupported`. Ordinal 0 is allowed only if the exporter has named-export data.
- PE-derived fixup (`svFixupImportAddresses`, `sf_lepoc.cpp:481-576`): the IAT sits at
  `codeLoad + iTextSize` and holds ordinals, replaced in place by addresses. Same range and
  absent-marker checks.
- Dependency walk: `ProcessImports` (`sf_lepoc.cpp:2853-`) builds the DLL list by name from
  each block's `iOffsetOfDllName`; `FixupDlls` (`3170-`) fixes each importer, then
  `RelocateExports` (PE only, `2629-2643`) fixes the export dir. For an ELF image exports are
  ordinary relocations (`sf_lepoc.cpp:385-389`).

### 5.2 Export sets (cross-check with `r1_abi_contract_v0.md`)

`eka/bx86/euseru.def` (MSVC-mangled, e.g. `?SetupThreadHeap@UserHeap@@SAHHAAU...` at ordinal
1627) and `eka/bx86gcc/euseru.def` (Itanium-mangled, `_ZN8UserHeap15SetupThreadHeap...` at
ordinal 1360) both export the start-up symbols. **Ordinals differ per set** (for example
`User::Exit` is ordinal 825 in bx86 and 641 in bx86gcc, `User::InitProcess` 1027 vs 585,
`DllSetTls` 735 vs 1237). A genuine EXE's imports are only valid against the exact set it was
linked against. Which set R1 uses remains the R1-ABI decision.

Start-up-critical EUSER symbols present in both sets: `User::InitProcess`, `User::Exit`,
`User::HandleException`, `User::Invariant`, `User::Leave`, `User::SetTrapHandler`,
`User::SwitchAllocator`, `UserHeap::SetupThreadHeap`, `UserHeap::CreateThreadHeap`,
`UserSvr::DllSetTls`, `UserSvr::DllTls`, plus `TTrap::Trap`/`UnTrap` (non-throw build).

### 5.3 Toolchain output format (unknown)

`cl_x86gcc.pm:733-752` post-links with `petran` (`-nocompress`, deflate or bytepair variants),
i.e. **PE-derived** images. Whether `petran` emits import format PE or PE2, and whether it
decorates import names, is not answerable from this checkout (`petran` source is in a
different tree, `bintools/petools`, listed in the SDK-route scratch only). **Unknown.**

### 5.4 Port loader vs genuine (`variant/e32load.cpp`)

| Item | Genuine | Port | Gap |
|---|---|---|---|
| Import format | ELF, PE or PE2 | Accepts the flag values (`e32load.cpp:314-316`) but **parses only the ELF code-offset list** (`361-400`). `mke32.py:340` emits ELF only. | A PE/PE2 image would be rejected or mis-bound. Untested. |
| Ordinal source | Low 16 bits at the fixup site (+ addend in high 16) | **Positional**: list entry k = ordinal k+1 (`362`, `746-747`) | Incompatible with genuine ELF images. |
| Provider name | From `iOffsetOfDllName` | Must equal literal `EUSER\0` (`385-387`) | Genuine names are believed to carry version/UID decoration. **Not verified in this trace.** |
| DLL refs | Any number | `dllRefs <= 1` (`323`) | A real EXE with euser + efsrv + estlib fails. |
| Imports per block | Any | `2..4` (`377`) | Real apps import hundreds of EUSER ordinals. |
| Ordinal table | Genuine export directory | Pilot veneer `1=Exit, 2=PutC, 3=CmdLen` (`44-`, `362`) | Collides with real ordinals (ordinal 1 in bx86 is `memclr`). |
| Absent-ordinal marker | Rejected | Not checked | Add when real exports are loaded. |
| Dependency graph | `iDepCount`, `ListDeps` | `pinfo.iDepCount = 0` (`665`) | EUSER never becomes a dependency code seg, so its `_E32Dll` attach is never called by `StaticCallList`. |
| Code-seg ID | Stamped at entry+12 | Absent | See §2. |

## 6. x86 relocation and header requirements

Relocation section (`E32RelocSection{size,count}` + 4 KB-page blocks `{pageOffset, blockSize,
uint16 entries}`), applied by `svRelocateSection` (`sf_lepoc.cpp:335-382`):

- Entry: top 4 bits = type, low 12 = page offset; `0` is padding and skipped.
- `0x1000` text: add `codeDelta` (= run − `iCodeBase`). `0x2000` data: add `dataDelta`.
  `0x3000` inferred: if the word lies in `[iCodeBase, iCodeBase+iCodeSize)` add `codeDelta`,
  else `dataDelta` (constants `e32ldr_private.h:374-384`).
- `ValidateRelocations` rejects any other type (`f32image.h:1039`).

`ApplyRelocs` (`e32load.cpp:211-241`) mirrors this and `ValidateRelocs` (`166-208`) mirrors the
bounds checks. **This part matches.**

Header and format constraints that a genuine toolchain image must meet on x86:

- `iCpuIdentifier == ECpuX86 (0x1000)`; no `KImageEptMask` or `KImageABIMask` bits
  (`f32image.h:644-646,713-719`). Port matches (`e32load.cpp:281,310-313`).
- `KImageDll` set ⇔ `iUid1 == 0x10000079`, else `0x1000007A` (`692-698`). Port matches (`302`).
- Header CRC `0xc90fdaa2` initialiser, `'EPOC'` signature, UID checksum. Port matches (`283-299`).
- **Compression:** `petran` may emit deflate/bytepair (`cl_x86gcc.pm:740-752`); port rejects
  anything but `KFormatNotCompressed` (`e32load.cpp:306-307`). A genuine default-compressed
  image fails with `KErrNotSupported`. Using `-nocompress` is a build-side workaround, not a loader fix.
- **Header size:** port requires `codeOff == 156` and a zero export-description tail
  (`e32load.cpp:410-413`). A genuine DLL with a non-trivial export bitmap has a larger header
  and would be rejected. EXEs normally fit. **Unverified against a real image.**
- **Exports:** port checks export targets as code-relative offsets `< codeSize` (`352-357`)
  and rejects EXEs with exports (`343-344`). Handling of PE-style (file-offset) vs ELF-style
  (relocated address) export directories for a real `euser.dll` is not shown here. **Open.**
- Entry point `< iCodeSize` and `+16 <= iCodeSize` (`f32image.h:741-745`). Port matches
  (`e32load.cpp:317-320`).

## 7. What was not read or verified (do not treat as established)

- `win32crt.h` / `nwdl.h` (`constructStatics`/`destroyStatics`), including how `.init_array`/ctor
  lists are located and what the linker script must define.
- `PushExcInfoOnUserStack` and the exception frame layout; `DoLeave` register restore.
- `ValidateImports` name format, the `RImageFinder` lookup, and versioned DLL name rules.
- `petran` / `elf2e32` source (not in this checkout); the actual import format in x86gcc output.
- Which memory model the port really links: `Makefile:71` lists `mchunk … putils …` and `:325`
  lists `memmodel/epoc/direct/x86/xkernel.cpp`. Whether thread-heap chunks
  (`SetThreadHeap`) and handle duplication work there is unconfirmed.
- Whether the port boots with upstream `ckernel.cia` delivering EBX/ESP (it is built at
  `Makefile:135`, so probably yes, but no boot test was run).
- euser's own link-time requirements (`euser.mmp`, `nostdlib`, its own `_E32Dll` use).
- No build, QEMU or hardware run. **QEMU-ready: none. Hardware-verified: none.**

## 8. Port gaps (summary)

1. **Entry convention:** `start.S:1-7` calls `app_main` and ignores EBX/ESP. Genuine needs
   `_E32Startup` with the 16-byte prologue, then `RunThread`.
2. **Code-seg ID:** not stamped, so `Dll::Tls` keys collide or fail for genuine DLLs.
3. **Dependencies:** `iDepCount=0`, so no `StaticCallList` ordering and no DLL attach calls.
4. **Import resolution:** positional ordinals, literal `EUSER`, `dllRefs<=1`, `n<=4`, ELF only.
5. **Compression/header shape:** uncompressed and 156-byte header only.
6. **User heap:** no genuine `SetupThreadHeap` path exercised.
7. **Exit:** exec 115 instead of `User::Exit` (no destructors, detach, handle cleanup).
8. **Leave/exception:** genuine trap frames and `HandleException` re-entry not exercised.

## 9. Proposed test vectors (not run)

All need a genuine-toolchain or hand-built fixture; each names the first card able to run it.

| # | Vector | Pass signal | Owner |
|---|---|---|---|
| T1 | First thread: EBX=0, ESP→info with `iType==3`, `iTotalSize` multiple of 8 and `<=256`, `iAllocator==0`, `iHeapInitialSize/MaxSize == header`, `iFlags==0`. Second thread: EBX=1, EIP=`iReentryPoint`. | Values read by a tiny fixture | R1-LOADER |
| T2 | Two images loaded in one process: entry+12 receives distinct non-zero IDs; reload keeps existing non-zero. Fixture whose entry+12 is non-zero is left untouched. | IDs differ, stable | R1-LOADER |
| T3 | `Dll::SetTls/Tls/FreeTls` round trip; other thread sees NULL; two DLLs get different slots. | Values match | R1-EUSER |
| T4 | `User::Alloc` succeeds; heap max honoured; child with `iAllocator` shares the heap; zero heap size yields no heap (document behaviour). | Allocation results | R1-EUSER |
| T5 | EXE global with ctor runs before `E32Main`; its dtor runs on `User::Exit`; DLL static ctor runs once, in dependency order, detach in reverse. | Ordered log | R1-LOADER + R1-EUSER |
| T6 | `TRAPD(User::Leave(-5))` returns -5; nested traps; `Leave` without trap panics. | Result and panic | R1-EUSER |
| T7 | Import fixups: ordinal > count → `KErrNotSupported`; absent-marker slot → `KErrNotSupported`; ELF addend in high 16 applied; PE and PE2 images rejected cleanly or supported by decision. | Error codes | R1-LOADER |
| T8 | Relocation types `0x1000/0x2000/0x3000`, padding entries, count mismatch → `KErrCorrupt`. | Patched words | R1-LOADER |
| T9 | Compression and header shape: `-nocompress` image accepted; deflate image refused with the documented error; DLL with export bitmap refused or accepted by decision. | Error codes | R1-LOADER |
| T10 | `User::Exit(r)` with r≠0: process exit code is r, other threads killed, handles closed. | Exit code | R1-EUSER |
| T11 | Exception re-entry: EBX=4 reaches `User::HandleException`; unhandled → panic. | Panic vs recovery | R1-EUSER |

## 10. Decision points for R1-ABI/v1 (not decided here)

1. Run genuine `_E32Startup`/`RunThread`, or keep a port `_start` shim that emulates
   `RunThread` (heap, TLS destructor key, `InitProcess`, `User::Exit`)? A shim must still
   deliver the §1.3 behaviours, or the genuine `euser` code will not work.
2. Export set: bx86 (MSVC) vs bx86gcc (Itanium). Ordinals are not interchangeable (§5.2).
3. Import format the loader must accept (ELF, PE, PE2) once the real toolchain output is known.
4. Whether EUSER becomes a real dependency code seg (`iDepCount`, `StaticCallList`), which
   affects statics, TLS IDs and `_E32Dll` attach.
5. Compression and header-size policy for genuine images.

## 11. Handoff

- Card: R1-STARTUP.
- Base: port `fa62323eea7bc96cefd666e40f5189567731bd44` (detached worktree
  `/tmp/opencode/r1-startup`); upstream read-only `0c3208650587ac0230aed8a74e9bddb5288023eb`
  via `kernelhwsrv` symlink, clean. Port-side citations from the main workspace's uncommitted
  tree.
- Changed paths: **one new file**, `qemu-port/docs/r1_startup_research_v0.md` (in the worktree;
  not committed, not copied to the main tree). No upstream, shared-doc or implementation edits.
- Evidence: static reading of the sources cited above.
- Verification: source reading only. No build, QEMU or hardware run. QEMU-ready: none.
  Hardware-verified: none.
- Remaining gates: matching toolchain/SDK artifacts (EUSER, EFSRV, `petran` output format),
  R1-ABI/v1 decisions in §10, and a boot test of the genuine entry frame (T1).
  The unread items in §7 must be closed before any implementation card starts.
- Questions for the coordinator (not the owner): which of §10 items 1-5 the owner wants
  decided first, and whether `petran` source/output can be added to the research inputs.
- Next owner: integrator (R1-ABI/v1), then R1-LOADER and R1-EUSER.

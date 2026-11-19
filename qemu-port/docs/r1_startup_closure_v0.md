# R1-STARTUP-CLOSURE — closing the startup report's open items; one coherent R1-A loader/startup contract (research v0, NOT an ABI freeze)

Date: 2026-10-21. Card: R1-STARTUP-CLOSURE (`r1_next_wave.md`, "Agent 2"). Owner of this file: this card only.
Successor to `r1_startup_research_v0.md` (§7 "not read or verified"). Inputs: `r1_build_repro_v0.md`
(artifact hashes), upstream sources, and the actually built images.

**Verification wording: static + artifact inspection only. No E32 execution, no QEMU, no hardware.**
**QEMU-ready: none. Hardware-verified: none.** Nothing here freezes an ABI; section 11 is a *proposal* for the
integrator's R1-ABI/v1 decision.

Evidence labels used below:

- **[S]** source read (path:line given)
- **[A]** artifact inspected (host `nm`/`objdump`, private Python parsers, `petran -dump`)
- **[U]** unknown / needs a runtime vector (section 14)

---

## 0. Provenance

| Item | Value |
|---|---|
| Port base | `fa62323eea7bc96cefd666e40f5189567731bd44` (detached worktree `/tmp/opencode/r1-startup-closure`, `kernelhwsrv` symlink) |
| Upstream `kernelhwsrv` | `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean at card start |
| Main tree | dirty with other workers' uncommitted work; untracked docs were read from the main tree |
| Private scratch | `/tmp/opencode/r1-startup-closure-scratch/` (`e32dump.py`, `impcheck.py`, copied Wine prefix `wine/`) |
| Other workers' products | read only; the BUILD-REPRO prefix and outputs were not modified |

Artifact hashes (SHA-256, identical to `r1_build_repro_v0.md`):

```
0c833111d1f0656ef7d75848e76d9a90459ce449babc2f636d6d80e5f65dcf60  euser.dll
69051f33b9b7f07b16ca5352b7a8d938490ed151e2d19db05f810f09939040fe  efsrv.dll
c096c6a27d65df49d896d3f4b3fc2d5d04e53461cdab93f663c3d1405a94b133  t_cactw.exe
825d43353d3008ef8372aa2b6e878c6c530a9ff9456c83a8a7bf7f4bdf627faf  fhserver.exe
65f0015bb89d569748fd067d3adbdbd35950f1e2c5da1766af3c0e8d568c72e5  epoc32/tools/petran.exe
```

All artifacts come from `/tmp/opencode/r1-build-repro/epoc32/release/x86/urel/`. They were built with two scratch Raptor
source patches and fabricated variant macros (see BUILD-REPRO and the pending R1-ARTIFACT-AUDIT). They are
**runtime-unverified and not official-kit equivalent**. Every [A] statement below is about those bytes only.

`petran` (Tools V02.02 Build 002) ran under a private Wine prefix with `-dump` only. That is inspection of the tool's
output, not execution of an E32 image.

---

## 1. Closure table for `r1_startup_research_v0.md` §7

| §7 item | Status | Where |
|---|---|---|
| `win32crt.h`/`nwdl.h` ctor/dtor | closed [S][A] | §5 |
| `PushExcInfoOnUserStack`, exception frame, `DoLeave` | closed for layout of the re-entry path [S][A]; field-exact frame is [U] | §6, §7 |
| `ValidateImports`, import name format, `RImageFinder` lookup, versioned names | closed [S][A] | §4 |
| `petran`/`elf2e32` actual import format of x86gcc output | closed [A]: PE2, not ELF | §3 |
| Direct memory model thread-heap/chunk support | closed as a **gap list** [S]; behaviour [U] | §8 |
| Does ckernel.cia deliver EBX/ESP at entry | source-consistent [S]; runtime [U] | §6 |
| euser's own link requirements (`_E32Dll`, entry) | closed [A] | §3, §6 |
| No build/QEMU/hardware run | unchanged | §0 |

---

## 2. What changed relative to the earlier assumptions

1. The built images are **PE2 imports, variable header V2**, not the "ELF" import format the port generator emits (§3).
2. Import names are **decorated** (`euser{000a0000}[100039e5].dll`) and carry the provider's version and UID3 (§4).
3. Real DLLs have `codeoff` 0xc8 (euser) and 0xa8 (efsrv); the port loader hard-requires 156 (§12).
4. x86 uses the **trap-frame leave path**, not C++ exception throw (§7). Previously an unconfirmed assumption.
5. The thread heap on x86 asks for a **disconnected chunk**; no disconnected-chunk support was found in the port's runtime (§8).
6. The `t_cactw` console is reached via `econs.dll` → window server; neither is built (§9).

---

## 3. Image format actually produced [A]

All five images: E32 header format V (`KImageHdrFmt_V`), compression word at offset 28 = 0 (uncompressed), tool
version word at 32 = 0x20202, import format nibble `0x2` = `KImageImpFmt_PE2` (`f32image.h:107`). CPU x86.

| Image | codeoff | entry | flags | exports | euser imports | code relocs | notes |
|---|---|---|---|---|---|---|---|
| `t_cactw.exe` | 0x9c | 0x260 | 0x22004000 | 1 (dir word 0) | 50 (ord 85..2195) | 222, all 0x3000 | data 0x180, bss 0x6b0, heap 0x1000/0x100000, stack 0x2000, SID 0, caps 0, VID 0x70000001, code base 0x400000 |
| `fhserver.exe` | 0x9c | 0x260 | 0x22004000 | 1 | efsrv 22, euser 65 | 454 | SID 0x2323, caps 0 |
| `euser.dll` | 0xc8 | 0x264 | 0x22005101 | 2503 | — | 1610 | codebase 0x10000000, modver 0x000a0000, uid3 0x100039e5, caps 0x000fffff, export-desc type 2 (sparse bitmap, 0x2d bytes) |
| `efsrv.dll` | 0xa8 | 0x264 | 0x22004101 | 421 | 136 | 339 | SID/uid3 0x100039e4, caps 0x000fffff, export-desc type 2 (0xc bytes) |
| `rpipe.dll` | 0x9c | 0x264 | 0x22004001 | — | 24 | — | SID 0x102827d5; no export description |

Facts used by the contract:

- Import section: at `impoff` a header word, then blocks of `{nameOffset, count}` (8 bytes). The IAT lives at
  `codeoff + textsize` and holds euser ordinals contiguously across blocks. `ValidateImports` requires
  `iTextSize + 4*totalImports <= iCodeSize` (`f32image.h:1079-1188`) [S].
- Export directory entries are in the code segment; the absent marker equals the image's entry offset (10 occurrences in
  euser, 20 in efsrv) [A]. The loader compares against `exporter.iFileEntryPoint` (`sf_lepoc.cpp:528,657`). That field
  is assigned an offset at `:1872`, yet `:2524` uses it like an address. Which form the comparison uses at that moment is
  **[U]**; see vector V3.
- All code relocs are type 0x3000 (HIGHLOW). Offsets are relative to code start. Values are link VAs; PE section RVA
  adds 0x1000, so reloc values differ from symbol VAs by that amount [A].
- Ordinal identity: for every euser import in `t_cactw` (first 50), `efsrv` (136), `fhserver` (65) and `rpipe` (24) the
  `euseru.def` (`bx86gcc`) name at that ordinal equals the built symbol at `codebase + 0x1000 + exportOffset`:
  50/50, 136/136, 65/65, 24/24. fhserver→efsrv ordinals were **not** checked [A].
- Genuine entry word layout (all five images) [A]: `eb 0e` at entry, 14 zero bytes (the code-seg-ID word at
  `entry+12` is zero in the file), then the real entry dispatch at `entry+16`. See §6.

---

## 4. Import name, version and UID policy

Observed [A]: the import block name is `euser{000a0000}[100039e5].dll`, i.e. `{modver}` and `[uid3]` of the provider.

Genuine handling [S]:

1. `E32Image::ProcessImports` (`sf_lepoc.cpp:2853-3260`) takes the name from `iImportData + block->iOffsetOfDllName`,
   rejects it if longer than `KMaxKernelName` (`:2990` → `KErrNotSupported`).
2. `TFileNameInfo::Set(name, EAllowUid)` (`sf_ldr.cpp:463-537`) parses the bracketed UID only if it is the last ten
   characters before the extension (`[xxxxxxxx]`); a bracket anywhere else gives `KErrBadName`; the loader maps any
   parse failure to `KErrCorrupt` (`:2997`).
3. The base name plus extension becomes the file to look up; UID goes into `iRequestedUids[2]`; `{version}` into
   `iRequestedVersion` (`:2999-3002`).
4. The candidate is accepted if the requested UID is 0 or equal (`CheckUid`, `sf_image.h:342`), and version policy
   passes: `DetailedCompareVersions` (`sf_ldr.cpp:388-427`) with `KModuleVersionNull = 0xffffffff` and
   `KModuleVersionWild = 0xfffffffe` (`e32const_private.h:39-44`). Same major with a minor ≥ the request is acceptable;
   a lower minor or a different major is not.
5. Capability check: the loader requests the **importer's** capabilities from the exporter (`:3005`,
   `RLdrReq::CheckSecInfo`, `sf_ldr.cpp:859-871`: missing = requested & ~candidate).
6. Errors map to `KErrNotFound`, `KErrNotSupported`, `KErrPermissionDenied`, `KErrCorrupt` (`:908-919`).

Consequence for the port: **do not strip the name down to `EUSER`**. Parse it, and either resolve against the provider's
own `modver`/`uid3` or reject. The legacy `EUSER` plain name is the pilot format (§11 rule L).

---

## 5. Constructors and destructors [S][A]

- `constructStatics`/`destroyStatics` (`win32crt.h`, `__GCC32__`) walk `__CTOR_LIST__`/`__DTOR_LIST__` **from index 1**
  until a NULL word. Called from `uc_exe.cpp:34,61` and `uc_dll.cpp:32,34` (`_E32Dll_Body`).
- `nwdl.h` routes `operator new/delete` to `User::Alloc`/`User::Free`.
- In `t_cactw`: `__CTOR_LIST__` at 0x4031c0 is `0xffffffff, 0x401e50, 0x0`; the one entry is
  `__GLOBAL__I__ZN13CActionRunnerC2Ev` and its slot is covered by a code reloc. Other images' lists are empty
  (`0xffffffff, 0, ...`). DTOR lists empty.
- The list sits inside the code+rdata region; **no linker-script work is needed on the port side**, because the PE
  toolchain already placed it and the relocation table already covers it. The loader only has to apply relocs before
  the process entry runs.

---

## 6. Entry veneer, code-seg ID, exception re-entry [S][A]

`t_cactw` at `__E32Startup` 0x401260: `jmp +16`; 12 bytes zero; zero word at `entry+12` (ID slot, `KCodeSegIdOffset = 12`);
then `cmp ebx,2; jae not_thread_entry; mov ecx,ebx; mov edx,esp; jmp RunThread`. For `ebx == 4` (`KModuleEntryReasonException`)
the veneer pushes `esp`, calls `User::HandleException` (0x402eb0), pops the saved frame and ends in `retf`; any other
value calls `User::Invariant` (`0x4012a5`). The ID slot is not relocated.

- The loader stamps a unique ID into the slot only if it is zero (`sf_lepoc.cpp:2519-2540`, `++NextCodeSegId`, fault
  on wrap). Header validation guarantees `entry + 12 + 4 <= codeSize` (`f32image.h:741-746`).
- `RunThread` (`uc_exe.cpp:37-66`): `SetupThreadHeap`; `DllSetTls(KGlobalDestructorTlsKey=-1, KDllUid_Special=-1, globalDestructorFunc)`;
  first thread: `User::InitProcess()` → `CallStaticEntryPoints(ETrue)` via `E32Loader::StaticCallList`/`StaticCallsDone`
  (`us_func.cpp:820-852`), then `constructStatics()`, `E32Main()`; always `User::Exit(r)`.
- Kernel side: the exception path rewrites the user context to `iReentryPoint` with `ebx = KModuleEntryReasonException`
  after `PushExcInfoOnUserStack` (`ckernel.cpp:156-170`, `ckernel.cia:134-165`) [S]. The exact mapping of each pushed word
  to the veneer's pop sequence is consistent in shape but **not verified field-by-field** [U] (vector V6).
- Reasons: ProcessInit 0, ThreadInit 1, ProcessAttach 2, ProcessDetach 3, Exception 4 (`u32std.h:455-459`).

---

## 7. Leave / trap [S][A]

- `euser.dll.sym` contains `DoLeave`, `DoTrap`, `TTrap::Trap`, `User::Leave`, and no throw-based leave [A]. In `e32def.h:29-31`
  `__LEAVE_EQUALS_THROW__` is undefined without `__SUPPORT_CPP_EXCEPTIONS__`.
- `TTrap::Trap` saves ebx, ebp, esp, esi, edi, ds, es, fs, gs (`uc_trp.cia`); `DoTrap` calls the fast exec
  `PushTrapFrame`; `User::Leave` calls `PopTrapFrame` then `DoLeave`, which restores those registers and returns 1
  (`uc_trp.cia:53-69`).
- Kernel side is pure data: `ExecHandler::PushTrapFrame`/`PopTrapFrame` link `TTrap` through `iFrame`
  (`sexec.cpp:2375-2422`). Fast exec numbers in the port's generated table: `EFastExecPushTrapFrame = 3`,
  `EFastExecPopTrapFrame = 4`, `ActiveScheduler = 5`, `SetActiveScheduler = 6`
  (`build/export/exec_enum.h`).
- The port currently has no implementation of these fast execs (grep outside docs/export found none) — gap G5.

---

## 8. Thread heap and direct-model prerequisites

Chain [S]: `RunThread` → `UserHeap::SetupThreadHeap` (`up_utl.cpp:137`) → `CreateThreadHeap`
(`heap_hybrid.cpp:3254`); symbols present in the artifact [A]: `UserHeap::CreateThreadHeap`, `RHybridHeap::Map`,
`RHybridHeap::PagedAllocate`. The chunk is built with `TChunkCreateInfo::SetThreadHeap`, then wrapped in a hybrid heap
with `EChunkHeapSwitchTo | EChunkHeapDuplicate` (`:3300`), then the chunk handle is closed.

`USE_HYBRID_HEAP` is 1 for non-WINS (`heap_hybrid.h:24-28`), and the thread-heap chunk is requested through
`SetDisconnected(0,0,2*maxLength)` and later grown by `Map` → `chunk.Commit/Adjust/Allocate`/`Decommit`
(`heap_hybrid.cpp:1810-1860`). `iDLOnly` (Doug Lea only) can avoid `Allocate` calls, but only when
`iMinLength == iMaxLength` or `aOffset > 0` (`:351-355,3168`); `t_cactw` heap min 0x1000 and max 0x100000 do not satisfy that.

Direct model in this port [S] (`memmodel/epoc/direct/mchunk.cpp`, read in full for these functions):

| Call on a disconnected chunk | Result | Lines |
|---|---|---|
| `Commit(off, size, ECommitDiscontiguous)` | range check only, returns `KErrNone`; no pages are mapped or tracked (`DoCreate` has already reserved the region) | 202-230 |
| `Commit(..., ECommitContiguous)` | `KErrNoMemory` unless the existing pages are physically contiguous | 217-225 |
| `Allocate(size, guard, align)` | `KErrNotSupported` after argument checks | 232-245 |
| `Decommit` | range check only, returns `KErrNone`; nothing freed | 247-258 |
| `Adjust(newSize)` | `KErrGeneral` (it is for non-disconnected, non-double-ended chunks only) | 113-128 |

Consequences for the hybrid heap [S, by composition with `heap_hybrid.cpp:1810-1860`]:

- A call to `Map(p != NULL, sz)` (commit a known page range) would succeed as a no-op, because the RAM is already behind the chunk.
- A call to `Map(NULL, sz)` (find-and-allocate pages, used by `PagedAllocate`) goes through `chunk.Allocate`, which returns
  `KErrNotSupported`, so the allocation fails and the heap returns NULL for large (paged-size) requests.
- `iUseAdjust` is only true when the creator asks for it; if it were true the `Adjust` call would return `KErrGeneral` on a disconnected chunk.
- Whether `ChunkHeap` takes the `Commit` path (small heaps) or other paths for `t_cactw`'s sizes, and whether the freshly
  reserved direct-model region is addressable at once, is **[U]** (vector V5).

So the direct model is **not** a clean drop-in for the genuine thread heap: ordinary slab/DL allocations may work as no-op
commits, while large allocations (above the slab threshold, served by `PagedAllocate`) would fail. This is a source-derived
prediction, not a runtime result. The port's own repair `tools/fix_mchunk_adjust.py` already patches `Adjust` size
tracking. The port runtime has `chunk_create` with a `ChunkCreate` struct; no disconnected-chunk support was found.

Honest options (owner/integrator decision, **not decided here**):

| Option | Cost |
|---|---|
| H1: make `Allocate` on a disconnected chunk work in the direct model (today `KErrNotSupported`), and track committed pages | most faithful; real kernel work, and a deliberate change to upstream behaviour in a port-owned memory model |
| H2: force the hybrid heap to Doug-Lea-only (`iDLOnly`) so only `Adjust` is used | needs a deliberate, labelled divergence from upstream heap flags; heap behaviour differs from genuine |
| H3: port-private user heap object that bypasses `UserHeap::SetupThreadHeap` | changes `RunThread` semantics; not genuine startup |

This card cannot choose H1/H2/H3 from source alone. Evidence needed to decide is vector V5.

`DllSetTls` with `KDllUid_Special` is resolved in the generic `ExecHandler::DllSetTls` (`sexec.cpp:1219-1232`) and
`DThread::SetTls` (`sthread.cpp:1239`); the direct model has no override, so TLS is model-independent [S].

---

## 9. RTest → Console::NewL provider chain [S]

`t_cactw` declares `LOCAL_D RTest test(_L("T_CACTW"))` (`:84`). `RTest::CheckConsoleCreated` (`us_test.cpp:27-33`) calls
`Console::NewL` under `TRAPD` and **panics `ERTestCreateConsole` if it fails**.

`Console::NewL` (`ub_cons.cpp:319`) → `RLibrary::Load(aImplDll, TUidType(KNullUid, KSharedLibraryUid, KConsoleDllUid))`
(`KConsoleDllUid = 0x100039e7`) → `Lookup(1)` = `NewConsole` → `econs.dll` (`kernel/eka/ewsrv/econs.mmp`) → `RConsole::Init`
→ `CreateSession("!E32WindowServer")` (`co_cli.cpp:20-22`) → server `ewsrv.exe`.

`econs` and `ewsrv` are in `bld.inf` but are **not among the built artifacts** (only euser, efsrv, rpipe are DLLs) [A].
`RLibrary::Load` goes through the loader server `!Loader` (`us_ksvr.cpp:5094-5100`, `up_lib.cpp:71-134`).

Honest minimal requirements for the acceptance EXE to print anything (all labelled gaps, none implemented):

1. a loader service that can load a DLL by name and UID triplet (`RLibrary::Load`) — the same machinery as §4;
2. either (a) a genuine `econs.dll` plus a window-server stand-in that speaks the `E32WindowServer` protocol, or
   (b) a port-private console DLL with UID triplet `{0, 0x1000008d, 0x100039e7}` exporting `NewConsole` at ordinal 1
   (`KSharedLibraryUidValue = 0x1000008d`, `e32uid.h:75` [S]);
3. the choice between (a) and (b) is a labelled supplement, **not** a genuine-startup claim; the coordinator/owner decides.

Sizing note: `RTest::CloseHandleAndWaitForDestruction` (ordinal 2195, `us_test.cpp:524-561`) additionally needs
`NotifyDestruction`, `HalFunction(EHalGroupVariant, EVariantHalTimeoutExpansion)`, `NTickCount`, `CancelMiscNotifier`,
`AfterHighRes` and `EKernelHalSupervisorBarrier`.

---

## 10. `t_cactw` panic / logon / child-thread needs [S]

Test body (`t_cactw.cpp:168-284`): `RThread::Create(name, fn, KDefaultStackSize, KHeapSize=0x1000, KHeapSize, arg)`
four-plus times; `Logon(stat)`; `ExitCategory()`, `ExitReason()`, `ExitType()`; deliberate `User::Panic` paths in the
child (`EStartTwice`, `EStopUnstarted`, `EStopTwice`, ...) expecting `EExitPanic`; the normal path expects
`EExitKill` with category `"Kill"` and reason 222. `E32Main` also calls `User::JustInTime`/`SetJustInTime(EFalse)`
(so the "just in time debugger" path must be inert), `__UHEAP_MARK`, and uses `User::Exit`, `AllocZ/AllocZL`, `Panic`.

Required user-side machinery: thread create/resume/logon, `ThreadKill`(`EExitKill/Terminate/Panic`, `us_ksvr.cpp:3840-3926`),
request status completion and `User::WaitForRequest`, `CActiveScheduler` (`ub_act.cpp` with `TRAPD`, leaves), a
**child thread heap** (each child requests 0x1000/0x1000: `iMinLength == iMaxLength` → `EFixedSize` and `aDLOnly = ETrue`
in `heap_hybrid.cpp:351-355`, so the child heap does not use slab/paged growth or `Allocate`). It is still created on a
chunk and `ChunkHeap` commits the minimum length up front (`heap_hybrid.cpp:3215`), which is a no-op `KErrNone` in the
direct model (§8), so child threads are not blocked by the missing `Allocate`. The main thread's heap (0x1000/0x100000) additionally
grows via `Map`.

The panicking-child cases are *deliberate negative tests*; a panic in them is success, not an error.

---

## 11. Proposed single R1-A contract (research proposal, not frozen)

Rule L (legacy, preserved): the pilot format emitted by `tools/mke32.py` (`KImageImpFmt_ELF`, import name `EUSER`,
`codeoff` 156, one dll ref, code ID slot untouched) stays a **separate, explicitly labelled mode** and stays
accepted by the pilot path exactly as today. **The loader must not stamp code-seg IDs into pilot images** — their
`entry+12` is arbitrary demo instruction bytes. Stamping applies only to images whose import format is PE2
(§3) and whose slot is zero (§6).

Genuine mode (G):

1. **Header**: accept header format V with variable `codeoff` as read from the header (not a fixed 156); compression 0
   only; validate against `f32image.h` rules (entry within code, `entry+16 <= codeSize`, alignment, CRC).
2. **Import format**: PE2 only for genuine mode. No redundant ordinal list; IAT at `codeoff + textsize`.
3. **Import names**: parse with the `TFileNameInfo::EAllowUid` rule; reject malformed names (`KErrCorrupt`), names over
   `KMaxKernelName` (`KErrNotSupported`); never silently strip decoration (§4).
4. **Provider match**: provider `uid3` must equal the requested UID unless the request is 0; version via
   `DetailedCompareVersions` (same major, minor ≥ request); mismatch → `KErrNotSupported`/`KErrNotFound` per `sf_lepoc.cpp:908-919`.
5. **Ordinals**: identity is `bx86gcc/euseru.def`; verified consistent with the built provider for every import of the
   five images (§3). Fixup writes `exporter code run address + export entry` into the IAT.
6. **Relocation**: apply code relocs (type 0x3000 only; reject others) before any code runs; relocate export directory
   entries by the code delta (`RelocateExports`, `sf_lepoc.cpp:390-452`).
7. **Code-seg ID**: stamp a unique non-zero value at `entry+12` if and only if the slot is zero and the image is genuine mode.
8. **Startup**: enter at the entry point with `ebx = ProcessInit(0)` (or ThreadInit 1) and `esp` at the top of the user
   stack, `ecx`/`edx` set by the veneer as in §6; exception re-entry sets `ebx = 4` at `iReentryPoint`.
9. **Implicit dependency init**: `StaticCallList` returns dependency entry points in dependency order with the process
   entry last; `InitProcess` calls them with reason ProcessAttach(2), unwinding with ProcessDetach(3) in reverse.
10. **TLS**: model-independent `DllSetTls/DllTls/DllFreeTls`, with the two special keys `-1` (global destructor) and `-2`
    (nested entry-point list), UID `-1`.
11. **Heap**: unresolved between H1/H2/H3 (§8); contract states only the *observable* requirements: main heap
    min 0x1000 max 0x100000 grows by pages; child heap fixed 0x1000.
12. **Leave**: trap-frame path with the two fast execs of §7; no C++ exception unwinding.
13. **Request semantics**: `ThreadRequestSignal` + `WaitForAnyRequest` as in `r1_abi_contract_v0.md`; unchanged here.
14. **Capabilities**: header caps are *data* the loader reads. The zero-capability EXE (`t_cactw`, caps 0) must load; the DLLs
    carry 0x000fffff and the loader's rule is `requested & ~candidate == 0` where `requested` is the importer's caps
    (§4.5). Do not conflate with image privileges (what the kernel lets the process do); enforcement of the capability
    model is a separate, later decision.
15. **Errors**: reject (not repair) malformed, version-mismatched, UID-mismatched and unsupported-reloc images.

Decisions this card considers decided by source (do not ask the owner): items 1-10, 12-13, 15.
Decisions that need an integrator/owner choice: item 11 (H1/H2/H3), the console provider (§9), and which of the five
R1-STARTUP §10 decisions to settle first.

---

## 12. Port gaps relative to the contract

| # | Gap | Evidence |
|---|---|---|
| G1 | `variant/e32load.cpp` demands `codeOff == 156`, so genuine euser/efsrv (0xc8/0xa8) are rejected | `e32load.cpp:410` |
| G2 | loader supports 0 or 1 dll refs; fhserver has 2, efsrv/euser imports exceed the pilot's 8-export cap | `e32load.cpp:323`, `:455` |
| G3 | no import-name parser, no code-seg-ID stamp, no `ReadExportDirLoad`/PE2 fixup | `e32load.cpp` |
| G4 | `E32LoadDll` loads one fixed `\EUSER.DLL` with no data/bss/relocs/deps | `e32load.cpp:443-465` |
| G5 | no `PushTrapFrame`/`PopTrapFrame`, `StaticCallList`, `iReentryPoint` implementation | grep outside docs/export |
| G6 | direct model: disconnected `Allocate` is `KErrNotSupported`, `Commit`/`Decommit` are no-ops, `Adjust` is `KErrGeneral` | §8, `mchunk.cpp:113-258` |
| G7 | loader service for `RLibrary::Load`, console provider | §9 |
| G8 | `tools/mke32.py` emits ELF-style import flag and `EUSER` name; fine for the pilot, wrong for genuine | `mke32.py:31,340,418` |

---

## 13. Reject matrix (what a correct loader must refuse)

| Input | Result |
|---|---|
| import name with a `[` or `]` not in the final UID position | `KErrCorrupt` (via `KErrBadName`) |
| import name > `KMaxKernelName` | `KErrNotSupported` |
| `{version}` major differs or minor lower than the provider | `KErrNotSupported`/`KErrNotFound` |
| `[uid]` not equal to provider `uid3` (and non-zero) | not matched → `KErrNotFound` |
| importer caps not subset of provider caps | `KErrPermissionDenied` |
| IAT doesn't fit in `codeSize` | `KErrCorrupt` |
| non-HIGHLOW reloc type, reloc beyond code | `KErrCorrupt` |
| compressed image | `KErrNotSupported` (until compression is in scope) |
| entry word needs `entry+16 > codeSize` | `KErrCorrupt` |

---

## 14. Runtime-only unknowns and minimal future QEMU vectors (not run)

| Vector | Question | Minimal evidence |
|---|---|---|
| V1 | Does a PE2 fixup land the correct euser address for each of `t_cactw`'s 50 ordinals? | load euser + t_cactw; compare IAT with `.sym` |
| V2 | Does code-seg-ID stamping occur exactly once per image, and not in pilot images? | read slot before/after |
| V3 | Absent-marker comparison form in the loader (`sf_lepoc.cpp:528` vs `:2524`) | export a known-absent ordinal; expect rejection |
| V4 | Does `__CTOR_LIST__` run exactly once after relocs? | ctor with a serial marker |
| V5 | Does a hybrid thread heap survive the direct model's disconnected-chunk behaviour (no-op `Commit`, failing `Allocate`)? | create a main-thread heap, allocate a small cell, then a block above the slab threshold, then free; record each `Map` result |
| V6 | Exception frame field order vs veneer pops | force a fault, compare saved vs restored registers |
| V7 | `TRAPD` + `User::Leave` round trip | leave through two trap frames |
| V8 | `RThread::Create`/`Logon`/panic exit code | child that panics; observe `ExitType`/`ExitCategory` |
| V9 | Console route (§9) selected provider loads and prints | one `Printf` through `RTest` |

---

## 15. What this card did **not** do

- No E32 image was executed; no QEMU; no hardware. No claim of equivalence with the official toolchain.
- No code, Makefile, shared doc or `kernelhwsrv` change; no commit.
- `fhserver → efsrv` ordinals were not cross-checked; the shared-library UID for a private console DLL was not derived.
- Not decided: heap option H1/H2/H3, console provider, and the five R1-STARTUP §10 items.

---

## Handoff

- **Card**: R1-STARTUP-CLOSURE.
- **Base**: port `fa62323eea7bc96cefd666e40f5189567731bd44`, upstream `kernelhwsrv` `0c3208650587ac0230aed8a74e9bddb5288023eb`; main tree dirty with others' work.
- **Changed paths**: one new file, `qemu-port/docs/r1_startup_closure_v0.md` (written in the detached worktree `/tmp/opencode/r1-startup-closure/`; copy into the main tree is the integrator's step). Private scratch only: `/tmp/opencode/r1-startup-closure-scratch/`.
- **Evidence**: §3 byte-level image facts; §4-§10 with path:line references; hashes in §0. Private tools: `e32dump.py`, `impcheck.py`; `petran -dump` under a private Wine prefix.
- **Verification**: static + artifact inspection only; no E32 execution; no QEMU; no hardware. QEMU-ready: none. Hardware-verified: none.
- **Remaining gates**: R1-EUSER and R1-FS stay gated on a runtime-verified matching toolchain, proof-source selection, the console route (§9), the heap option (§8), and R1-ABI/v1.
- **Next owner**: integrator (R1-ABI/v1), after R1-ARTIFACT-AUDIT.
- **Questions/blockers for the coordinator**: (1) which of the five R1-STARTUP §10 decisions to settle first; (2) whether the two scratch Raptor patches in `artifacts/scratch_raptor_patches.diff` are acceptable as the build basis; (3) heap option H1/H2/H3 and console option (a)/(b).

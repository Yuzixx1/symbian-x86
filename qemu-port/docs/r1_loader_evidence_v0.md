# R1-LOADER-EVIDENCE — bounded evidence card (v0)

Card: `r1_next_wave.md` Agent 3 (R1-LOADER-EVIDENCE, "cheaper, larger
independent bundle"). This report was requested by the coordinator's
2026-10-22 note: "Trace remaining loader offset→run-address/pool/entry
concerns in a bounded evidence card before implementation"
(`r1_research_reconciliation_v0.md` §2026-10-22, decision 7 tail). It
completes four independent inventories without designing a loader or
deciding ABI. Ten matrix REVIEW flags are enumerated but not resolved.

Input reports read per brief: `r1_heap_exec_v0.md` (§2, §5, §6, §9),
`r1_startup_research_v0.md`, `r1_startup_closure_v0.md`,
`r1_build_repro_v0.md`, `r1_artifact_audit_v0.md`, `r1a_matrix_errata_v0.md`,
`r1_research_reconciliation_v0.md`. All upstream citations are the same
read-only checkout (`K=` root `kernelhwsrv/`; `EK=` abbreviates
`kernelhwsrv/kernel/eka/`). Port paths `P=` under `qemu-port/`.
Nothing was built or run in this pass (no QEMU, no provisioning).

---

## 1. `iFileEntryPoint` trace — the loader copy is converted by the kernel, statement located

`r1_heap_exec_v0.md` G-b listed as **[U]**: "Converted `iFileEntryPoint`
assignment point in the loader not located". That statement is now located:

**The definitive conversion statement is kernel-side, in
`EK=memmodel/epoc/pcodeseg.cpp:292-294`, inside `DEpocCodeSeg::DoCreate`:**

```cpp
r=DoCreateRam(aInfo, aProcess);
if (r==KErrNone)
	{
	aInfo.iFileEntryPoint+=ri.iCodeRunAddr;   // offset -> run address
	aInfo.iExportDir+=ri.iCodeRunAddr;
	...
```

Full file/line-backed succession (all [S] unless noted):

| # | Step | Where |
|---|---|---|
| 1 | Struct definition: `TCodeSegCreateInfo` carries both `iEntryPtVeneer` ("address of first instruction to be called") and `iFileEntryPoint` ("address of entry point within this code segment") | `EK=include/e32ldr_private.h:156-172` (server side also inherits into `TProcessCreateInfo` `:234`) |
| 2 | Loader fills the offset form: `E32Image` construction copies the header entry point — `iFileEntryPoint = iHeader->iEntryPoint; // just an offset at this stage` | `K=userlibandfileserver/fileserver/sfile/sf_lepoc.cpp:1872` (RAM images); `:1523` (`iFileEntryPoint = a.iEntryPoint`, ROM parse); ROM XIP path explicitly re-fills from the ROM header: `aInfo.iFileEntryPoint=rih.iEntryPoint` `K=…/sf_ldr.cpp:229` |
| 3 | Loader → kernel hop: `E32Image::DoLoadCodeSeg` passes itself (an `E32Image` derives from the create-info struct) to `E32Loader::CodeSegCreate(*this)`, then `CodeSegLoaded(*this)` after imports | `sf_lepoc.cpp:1450,1466` (RAM), `:1497` (XIP), `:2935` (chained-load finish); user-side wrapper layer `EK=euser/us_func.cpp:1616` + declarations `EK=include/e32ldr_private.h:321-327` |
| 4 | Kernel creates the code seg: `DCodeSeg::Create` → `DoCreate(aInfo,aProcess)` | `EK=kernel/scodeseg.cpp:710` (virtual entry, declaration `EK=include/kernel/kern_priv.h:1928`) |
| 5 | **Conversion:** `DEpocCodeSeg::DoCreate` mutates the very struct the loader passed, after RAM creation succeeds: `aInfo.iFileEntryPoint += ri.iCodeRunAddr;` — and also converts `iExportDir`, `iExceptionDescriptor` | `EK=memmodel/epoc/pcodeseg.cpp:293` (export dir `:294`; exception descriptor `:298-299`) |
| 6 | Kernel assigns its own state **from the converted struct**: `iFileEntryPoint=aInfo.iFileEntryPoint;` then `aInfo.iEntryPtVeneer=aInfo.iFileEntryPoint; // default - modified by loader if necessary` | `EK=kernel/scodeseg.cpp:714-715`; final veneer value arrives later via `DCodeSeg::Loaded` (`iEntryPtVeneer = aInfo.iEntryPtVeneer; // final value provided by loader`) `EK=kernel/scodeseg.cpp:748-750` |
| 7 | Copy-back to loader: `DCodeSeg::Info()` publishes `aInfo.iFileEntryPoint=(TUint32)iFileEntryPoint` (run-address form) + `iEntryPtVeneer` | `EK=kernel/scodeseg.cpp:811-824` (esp. `:818`) |
| 8 | Consumers — absent markers: `svFixupImportAddresses` computes `absentOrdinal = (TUint32)exporter.iFileEntryPoint` and compares against relocated export-directory entries | `sf_lepoc.cpp:528` and `:657`; exporter identity is an `E32Image` (loader-shared subclass of the create-info struct) `sf_lepoc.cpp:392` |
| 9 | Consumers — export relocation: `svRelocateExports` adds `iCodeRunAddress` to every export-directory entry in `[0, codeSize)` (so entries and the absent marker end up in one representation) | `sf_lepoc.cpp:390-452` (heap-exec C4 already cited this side) |
| 10 | Consumers — code-seg-ID stamp: `csid_addr = iFileEntryPoint+KCodeSegIdOffset-iCodeRunAddress+iCodeLoadAddress` (run address of `entry+12`) | `sf_lepoc.cpp:2500,2524` (heap-exec C5) |
| 11 | Consumers — anti-marker sanity: `CheckRamExports` rejects export entries equal to `xep = aExporter->iFileEntryPoint` | `sf_lepoc.cpp:2779-2790` |
| 12 | Consumers — initial thread: process create passes `t.iFunction=(TThreadFunction)aInfo.iFileEntryPoint; // kernel will call veneer, veneer will call this` | `EK=kernel/sprocess.cpp:239`; reentry from `iEntryPtVeneer` at `EK=kernel/sprocess.cpp:505` (heap-exec §6 already traced the frame) |

Consistency conclusion (arithmetic, not design): with rows 5→6→7 holding, the
kernel-stored `iFileEntryPoint` and the loader-visible copy are both in **run
address** form from `DCodeSeg::Create` onward, while the loader's `E32Image`
own field `iFileEntryPoint` (row 2) is separately relocated through the
export-dir machinery (row 9) — one representation at every consumer site,
never added twice. This closes heap-exec G-b as "**located**" from pure
source reading — no runtime was needed. It does **not** by itself authorise
the port loader changes: heap-exec C6's grep remains the operative port
statement — `P=variant/e32load.cpp` has **no** absent-marker comparison and
**no** code-seg-ID stamping today (verified by heap-exec; re-verified by grep
in this pass: zero hits for `absentOrdinal`/`KCodeSegIdOffset`-like logic in
`P=variant/e32load.cpp`).

Port-side hand-off shape recorded (matching the genuine pattern, per
heap-exec §6 [S]): `P=variant/e32load.cpp:663-664` sets
`pinfo.iEntryPtVeneer = pinfo.iFileEntryPoint = info.iEntry`
(offset, `iCodeLoadAddress=0`) and the **genuine linked kernel** performs the
row-5 conversion through the same `DEpocCodeSeg::DoCreate` path.

Search boundaries in case any reader wants to go further: the only
un-traced corner is which marshal layer (`ExecHandler` for
`E32Loader::CodeSegCreate`, `EK=euser/us_func.cpp:1616` region) does the
kernel-side struct copy in/out for that specific exec — irrelevant to both
C4 and C5 because the conversion happens *inside* `DCodeSeg::Create` on the
kernel-visible copy and is copied back from the same [S-step verified by
rows 4-7]; marked as a residual nicety, not a need.

---

## 2. User-data pool geometry, image heap/stack reservations, table limits

`r1_heap_exec_v0.md` G-d—"ROM-published user pool size vs reservations
[U]"—is answered arithmetically from the published init chain. **Arithmetic
only; no maximum-concurrency or memory-design claim is made.**

### 2.1 Construction of the pool (published exact values)

| Quantity | Value (exact) | Source chain |
|---|---|---|
| `TRomHeader.iUserDataAddress` (user pool base) | `0x02000000` | `P=variant/qemu_variant.h:29` (`kQemuUserDataAddress`); written `P=variant/qemu_variant_rom.cpp:68`; consumed `MM::UserDataSectionBase=TheRomHeader().iUserDataAddress` `EK=memmodel/epoc/direct/minit.cpp:27` |
| `TRomHeader.iTotalUserDataSize` | `0` | `P=variant/qemu_variant_rom.cpp:69`; consumed `MM::UserRomDataSectionEnd=iUserDataAddress+iTotalUserDataSize` `EK=memmodel/epoc/direct/minit.cpp:28` |
| `MM::UserDataSectionEnd` | `0x08000000` (= `iRamBase 0 + iTotalRamSize 0x08000000`) | `EK=memmodel/epoc/direct/minit.cpp:29`; superpage fields written `P=variant/qemu_variant_asic.cpp:251-252` from `P=variant/qemu_variant.h:30` (`kQemuTotalRamSize = 128 ul * 1024 ul * 1024 ul`) |
| Usable pool span (End - Base) | `0x06000000` = 96 MiB | arithmetic on the two lines above |
| Block granularity | 4 KiB (`RamBlockSize=0x1000`, `RamBlockShift=12`) | `EK=memmodel/epoc/direct/x86/xinit.cpp:26-27` |
| `RamAllocator` blocks | 24 576 (0x6000) blocks; **0 pre-allocated** (`used_blocks = (ROM-End - Base) >> RamBlockShift = 0`) | `EK=memmodel/epoc/direct/minit.cpp:34-43` (Allocator `New(blocks,ETrue)`, `Alloc(0,used_blocks)` when nonzero) |
| Kernel data region / limit | `iKernDataAddress=0x01000000`, region 0x200000, `iKernelLimit=0x01200000` | `P=variant/qemu_variant.h:27-28` (`kQemuKernDataAddress`, `kQemuKernRegionSize`); superpage `P=variant/qemu_variant_asic.cpp:253`; ROM header `P=variant/qemu_variant_rom.cpp:65-67` (`iTotalSvDataSize = 0x10000`) |
| Kernel stack / heap publish | `aStack = iKernDataAddress + RoundToPageSize(iTotalSvDataSize)` = 0x01010000; kernel heap above it; `iHeapSizeMin = iInitialHeapSize = 0x40000` | `EK=memmodel/epoc/putils.cpp:296-307` (`P::KernelInfo`); `P=variant/qemu_variant.h:31` (`kQemuInitialHeapSize`), written `P=variant/qemu_variant_asic.cpp:250` and `P=variant/qemu_variant_rom.cpp:114` |
| RAM bank list | one bank `base=0, size=0x08000000` | `P=variant/qemu_variant_asic.cpp:254-261` (`gRamBanks[2]`, `iRamBootData`) |
| Pool-related shell visibility | `meminfo` reports `ram: free=<blocks> blocks (<n> bytes) user pool`, `heap:`, `objs:` | `P=variant/kern_shell.cpp:643-658` (reports live, no paths to change) |

\* The `0x02000000` base lies above the kernel region (`0x01000000..0x01200000`)
and below pool end `0x08000000`; both checks (`aBase>=Base &&
aBase+aSize<=End` at `EK=memmodel/epoc/direct/mchunk.cpp:310,337`) hold.

### 2.2 Image-level heap/stack reservations (per process create)

| Reservation | Size source | Consumed as | Where |
|---|---|---|---|
| Data/BSS chunk `$DAT` | `iTotalDataSize` from header → `RoundToBlockSize` | one `EUserData` chunk, `iMaxSize = iInitialTop = rounded total`, `EAdjust|EAdd` — **no doubling, no extra headroom** | `EK=memmodel/epoc/direct/mprocess.cpp:88-115` (`DMemModelProcess::CreateDataBssStackArea`); invoked `EK=memmodel/epoc/pprocess.cpp:33` via `EK=memmodel/epoc/pcodeseg.cpp:206` |
| User stack (initial thread) | `iStackSize` from header/create-info | `DThread::AllocateUserStack(iUserStackSize)` → `MM::AllocRegion` (a *region*, not a chunk); caps: `PP::MaxUserThreadStack = 0x14000` (80 KiB), `PP::MaxStackSpacePerProcess = 0x200000` (2 MiB), guard `PP::UserThreadStackGuard = 0x2000` | `EK=kernel/sprocess.cpp:244` (`t.iUserStackSize=aInfo.iStackSize`); `EK=memmodel/epoc/direct/mkernel.cpp:60-79` (`AllocateUserStack`); caps `EK=memmodel/epoc/direct/x86/xinit.cpp:29-33` |
| Heap (created by `RunThread`, not by `ProcessCreate`) | `iHeapSizeMin/iHeapSizeMax` from create-info (`t.iHeapInitialSize/iHeapMaxSize` `EK=kernel/sprocess.cpp:246-247`) | genuine EUSER hybrid heap: chunk `minLength=0x1000, maxLength=0x100000` then **`SetDisconnected(0,0,0x200000)` — the chunk max is doubled**; heap object lives in the upper half | `r1_heap_exec_v0.md` §5.1 items 1-4 citing `EK=euser/heap_hybrid.cpp:3058-3254` and `:357` etc. (not re-derived here) |
| Port passes-through header values | `pinfo.iHeapSizeMin/iHeapSizeMax/iStackSize = info.iHeapMin/iHeapMax/iStackSize` | `P=variant/e32load.cpp:667-669`, populated by `E32Validate` at `P=variant/e32load.cpp:286-289` (raw header fields iHeapSizeMin/Max, iStackSize) | as recorded |

Doubled-heap capacity note (arithmetic only): a 1 MiB heap reserve becomes a
2 MiB chunk reservation; the heap object itself owns the upper half (1 MiB)
its DL zone can grow into; the *chunk-wide* totals live inside the 96 MiB
block pool along with stack regions and `$DAT` chunks. **No maximum process
concurrency is derived from these numbers** (the brief forbids it), only the
exact figures above.

### 2.3 Port table/limit constants as they exist today

| Table/limit | Bound | Source |
|---|---|---|
| `sProcs[16]` per-process CWD records | 16 entries; pre-create check `ProcExtAvailable()` | `P=variant/userfiles.cpp:62-66`; gate `P=variant/e32load.cpp:620` |
| `sExecs[16]` code-seg reaper records | 16 entries; pre-create check `ExecRecAvailable()` | `P=variant/e32load.cpp:849-856`; gate `:620` |
| Resident-DLL regime limits | `EUSER.DLL` only, from FAT root `\EUSER.DLL`, uid1=0x10000079, `2 ≤ iExpCount ≤ 8`, **no data/BSS/dllrefs/relocs, entry must be `0xC3`** ("resident pilot DLL" shape) | `P=variant/e32load.cpp:443-474` |
| Ordinal space served | only the loaded pilot DLL's ordinals positionally, else trampolines for ordinals 1-2 | `P=variant/e32load.cpp:739-751` |
| Pool/block failure | `MM::AllocFailed` signals when region allocation fails | `EK=memmodel/epoc/direct/mkernel.cpp:69-71` |
| Kernel object counters (live view only) | threads/procs/chunks/timers via `K::Containers` | `P=variant/kern_shell.cpp:655` |

---

## 3. Existing loader tests vs the required genuine-loader coverage

Cross-reference of `P=tools/test_*.py` assertions against each behavior the
genuine path implies. For each: covered (assertion), or **no coverage**
honestly marked. Coverage rows below are *port-loader* facts; the genuine
loader (rows 5-10 of §1) is not exercised by any of them today.

| Behavior | Test path & assertion | Verdict |
|---|---|---|
| E32 header validation (PE2-era v2 header: UID checksum, `'EPOC'` sig, CPU x86, header CRC, field parse) | `tools/test_e32_relocs.py:22-29,47-49` via `tools/e32read.py --quiet` `valid()`; malformed rows (outside-range data reloc `:36-38`, wrong count `:40-42`, invalid type `:44-46`, reloc-without-data `:52-56`) each `assert valid(...)` == False | **covered** for validation entry (upstream `E32Validate` port `P=variant/e32load.cpp:245-310` is what `e32read` mirrors in Python) |
| Code relocation engines (0x1000/0x2000/0x3000) applied at load | `tools/test_data_reloc.py:37-41` (generation-side validation) and runtime `:79-100` (`xp /1wx 0x2200010` monitor readback must equal `0x0a200a52` — BSS chain deref through relocated data pointer) | **covered** for data-table relocation end-to-end; engine identity asserted against `sf_lepoc.cpp:183-187` only by stationary comparison (heap-exec C3) — runtime equivalence to genuine relocation *kinds* is not separately asserted |
| Decorated dependency names (`{MMMMmmmm}[uid3]`), UID3/version matching on load | lib-name spot-checks only: `tools/test_dll.py:62` asserts the resolved import prints `"0x0e50"` marker; no test loads a *decorated* name, rejects a wrong UID3, or rejects an unmatching module version | **no coverage** — the port loader resolves one bare name `\EUSER.DLL` (`P=variant/e32load.cpp:446`) and never parses decorated names |
| Versioned/UID3 matched candidate across duplicates | none anywhere in `tools/` | **no coverage** (and no loader support: `P=variant/e32load.cpp` has no UID3-matching search) |
| Absent exports (absentOrdinal == exporter entry) | none — engine absent port-side (§1 last paragraph), so no assertion can exist yet | **no coverage** |
| Code-seg-ID stamp at run-address(entry)+12 | none port-side; upstream only `sf_lepoc.cpp:2500,2524` | **no coverage** |
| Dependency attach/detach ordering (`StaticCallList`/`DllSetTls`/first-thread init) | genuine path asserted statically by `r1_heap_exec_v0.md` §6 [S]; no runtime hook observed; `tools/test_interact.py:55` asserts lease/typed interaction but nothing about construct/destroy call order | **no coverage** (no port tests exercise `StaticCallList`; only the `interact`/`samples` fixtures call entry twice per process lifetime shape) |
| Failure cleanup (missing/invalid dependency leaves no half-loaded process; partial DLL load undone) | pre-checks exist port-side (`ExecRecAvailable/ProcExtAvailable`; DLL pre-validation `P=variant/e32load.cpp:620-629`; "Failing before ProcessCreate avoids untracked processes and leaked segs" comment `:617-620`) but no test drives a *failing* preflight (e.g. disk where dll is `KErrNotFound`) and asserts the `ps`/`reap` table stays clean, or asserts `ProcessCreate` was never attempted | **no coverage** for the failure path |
| Process lifecycle readbacks (data-run address, id survival, arguments) | `tools/test_args.py:41,80,87,98,101` (`BSS` readback via monitor, `args <id>`, `ps` record); `tools/test_crash.py:41-43` (reaper exit line names the crashed image, shell survives); `tools/test_wait.py` (live timeout vs dead record); `tools/test_conc.py` | **covered** at chrome level — genuine usage of these flows (codeseg refcounts, open counts on exit) not asserted beyond the port reaper line |
| Genuine-PE2 **kernel** path (codeseg create → converted entry → reentryPoint) | exercised implicitly by every boot test (all use `Kern::ProcessCreate` `P=variant/e32load.cpp:714`) and by `tools/test_sdk.py:66`, `test_samples.py:60` no-FAULT asserts, but nothing asserts the *kernel-visible* values (e.g. `iReentryPoint == run(iFileEntryPoint)` or a run-address stamp word) | **no coverage** |

Proposed observable assertions (drafts only; **not executable test code**,
per brief):

- OA-1 (kernel conversion, §1): after a process starts, the kernel-visible
  initial thread entry must equal `run(iFileEntryPoint)`; observable by
  comparing the `KPrintf`-logged `pinfo.iCodeRunAddress` against a monitor
  `xp` read of the first-thread instruction pointer register capture in a
  paused state — or, more cheaply, assert that the data-run address logged
  (`KPrintf("e32run: sections in (deltas %x/%x)")`) equals
  `MM::UserDataSectionBase + <block index> * 4096` (alive-checking the pool
  arithmetic of §2.1 against an actual run).
- OA-2 (absent marker, future genuine loader): on a developer-built exporter
  image with an absent ordinal, after `svFixupImportAddresses`-equivalent
  binding, the importer IAT slot must contain the exporter's **run-address**
  entry value (`run(iFileEntryPoint)`), not an unchanged file offset; this is
  monitor-observable at the importer's `iSlots[]` run address.
- OA-3 (code-seg-ID stamp, future genuine loader): the doubleword at
  `run(entry)+12` in the freshly-loaded EXE must equal its code-seg identity
  after run (upstream pre-condition: the word was 0 *before* stamping and
  stamping happens only after relocation — `sf_lepoc.cpp:2500-2530`).
- OA-4 (decorated-name failure cleanup, both loader regimes): loading an EXE
  whose import declaration names a module absent (or carrying a mismatching
  UID3/server-id) must leave both `sProcs` and `sExecs` untouched; checkable
  through the existing `ps`/`reap` shell verbs, no new toggles.
- OA-5 (attach/detach order): a pilot DLL with `iDllRefs>0` and construct
  calls must show one attach call before `E32Main` and its destroy-time
  on `User::Exit`; observable through an instrumented export or a
  kernel-trace line collected in the run log — no claim that this is cheap;
  listed for the genuine-loader scope.

Explicit non-goal: none of OA-1..OA-5 is claimed to be implementable now, and
none binds the loader design.

---

## 4. Requirements ledger — superseded claims and open review routes

This is the card's item-4 deliverable: a **ledger**, not fixes. Every row
cites both the original claim and the superseding evidence. Routing marks the
intended strong reviewer; nothing here is resolved by inference.

### 4.1 Claims superseded by linked heap/exec evidence (heap-exec-based adjudication)

| # | Earlier claim (source) | Superseded by | Route |
|---|---|---|---|
| L-1 | closure G5 said the port must implement trap-frame/`StaticCallList`/reentry helpers | §2.2 of `r1_heap_exec_v0.md` verified these are *already linked* in the genuine kernel; C1 "port must not implement them" withdrawn at the source | heap-card + integrator; affects loader attach/detach planning (§3 rows 7) |
| L-2 | closure §8 predicted large (paged-size) allocations would fail | heap-exec C2/§5.2: DL fallback exists; capacity is half the chunk; H0 provisionally chosen (reconciliation decision 1) | heap-card owners (HE-3…HE-6 vectors pending) |
| L-3 | closure §13 rule 6: reject all relocations except 0x3000 | overruled by coordinator; keep 0x1000/0x2000/0x3000 (heap-exec C3); port `ApplyRelocs` `P=variant/e32load.cpp:211-241` matches `sf_lepoc.cpp:183-187` [S] | loader owner; freeze wording in R1-ABI/v1 |
| L-4 | absent-marker representation "left open" (closure/C4) | heap-exec C4/C5 articulated run-address comparison; **this card locates the exact kernel conversion at `EK=memmodel/epoc/pcodeseg.cpp:292-294`** (§1); port still lacks the machinery entirely (heap-exec C6) | genuine-loader owner; contract wording only after R1-ABI/v1 review |
| L-5 | `entry+12` stamp placement ambiguity | heap-exec C5 fixed as `run(entry)+12`; stamp only if word is 0, only after relocation | genuine-loader owner (same gate as L-4) |
| L-6 | "loader must not resolve `{000a0000}[100039e5]`-style import strings yet" (port-owner interceptor note working from repro facts) | unchanged as a port fact; but genuine-route sense of "resolve" must include the loader decorated-name path (reconciliation decision 6: "Missing direct EXE imports cannot exclude transitive calls") | coordinator + R1-LOADER |

### 4.2 Claims superseded by owner/coordinator decisions (2026-10-21/22 reconciliation rows)

| # | Earlier claim (source) | Superseded by / decision | Route |
|---|---|---|---|
| L-7 | ABI/v0 "file subsession close opcode 27" | File close to use **28**; opcode 27 closes a directory (`r1_research_reconciliation_v0.md` "Resolved research contradictions", citing `F32plugin.h:71-72`, `sfile/sf_ops.h:55-70`) | R1-ABI frozen-text owner |
| L-8 | ABI/v0 "4-byte Size/Seek package" | packages are 8 bytes (`r1_wire_research_v0.md` §1/§3); do not claim RFile64 support | same |
| L-9 | expanded console report §1/§2 labels econs/DLL MMP field `0x1000008d` as "UID1" | coordinator: "Correct MMP UID2 versus E32 UID1 confusion (DLL UID1 is 0x10000079)" (reconciliation decision 6). The MMP `uid <a> <b>` line fills uid2/uid3; `0x1000008d` is `KSharedLibraryUid` (uid2) | console-card owner/coordinator note; no field-level numeric conflict, just label |
| L-10 | toolchain "MSVC bx86 defs establish GCC buildability" for econs etc. | decision 6: "MSVC bx86 defs do not establish GCC buildability"; trace actual `bx86gcc/eabi` resolution and the *linked* kernfull variant console, not only `qemu-port/console/` | console-feasibility (agent 2, current wave) |
| L-11 | "no-Panic-in-24-bytes heuristic proves RFile64 implemented" style grep-absence claims | decision 5: inspect function boundaries and expected IPC calls; grep-absence is not a positive proof | build/artifact reviewers |
| L-12 | build-config D7 recipe as "official-parity" tool | decision 3/5 bound it: "experimental local tool after fixes"; use `port-x86-rfile64-v1` research scope only (decision 2) | build-config owner |
| L-13 | console "write-only server suffices because requests are few" | decision 7: "A small client request set does not authorize a fake write-only server"; full-service breadth stays in review | console-feasibility |
| L-14 | t_cactw "build-green implies pass" and "clean-halt" | errata §A-F: build artifacts exist at config level (A) and P-1 fresh-boot persistence promoted to required regression (B) — stale automated assumptions already corrected in the errata, matrix v1 integration still pending | matrix owner |

### 4.3 Unresolved matrix semantic flags REVIEW-1…REVIEW-10 (enumerated; none resolved here)

The ten flags live in the matrix/errata texts; this card only routes them:

| Flag | Meaning (quote-anchor) | Matrix line | Route |
|---|---|---|---|
| REVIEW-1 | descriptor Width/capacity check is config-dependent (`sthread.cpp` note) | `r1a_acceptance_matrix_v0.md:88-91` | EUSER/wire review |
| REVIEW-2 | Flush-vs-Close error observability on a real built EFSRV | `:110-113` | EFSRV build owner (R1-FS) |
| REVIEW-3 | Start-beyond-EOF + limited-write: FAT-backend zero-fill vs clamp | `:128-140` | R1-FS strong review (from a real EFSRV trace, not intent) |
| REVIEW-4 | SetSize growth fill/position behavior documented unsettled upstream | `:142-144` | owner + upstream-source reading |
| REVIEW-5 | platform-security diagnostic config affects const/width rejection; pick one pinned configuration | `:160-164` | configuration owner |
| REVIEW-6 | error-code precedence (share/race) — not to be inferred by any single card | `:174-176` | R1-FS strong review |
| REVIEW-7 | whether transposed/W-domain share-matrix rows collapse for the fixture | `:192-196` | R1-FS fixture owner |
| REVIEW-8 | panic-vs-KErrBadHandle release semantics; genuine panic transport unimplemented → case *blocked*, not simplified | `:210-215` (errata §D/§F boundaries) | EUSER loader/protocol reviewer |
| REVIEW-9 | legacy write-cap probe size for `0x7fffffff` ceiling test | `:221-228` | acceptance-time fixture owner |
| REVIEW-10 | lifecycle/status-aliasing pitfalls; do not frame a fake status | `:253-257` | EUSER async owner |
| — | [REVIEW-8b] (config-note rows) is kept **folded into REVIEW-8** per the errata pinning (ten total) | errata §F | as REVIEW-8 |

Cross-flag caution (per brief): the matrix is written against the upstream
*API* but the currently enabled API scope is only `port-x86-rfile64-v1`
(reconciliation decision 2) — REVIEW rows must be re-read against *that*
pinned macro set before any of them moves from open → resolved.

### 4.4 Preserved legacy requirements (explicitly *not* superseded; must survive the genuine-loader work)

- **Prt-1 Legacy relocation kinds 0x1000/0x2000/0x3000 stay supported** (L-3);
  `P=variant/e32load.cpp:211-241` `ApplyRelocs` semantics per
  `EK=…/sf_lepoc.cpp:183-187` [S]; regression coverage lives in
  `tools/test_e32_relocs.py`/`test_data_reloc.py` and must keep passing.
- **Prt-2 Resident pilot-DLL regime** (`\EUSER.DLL` root-name, uid1
  0x10000079, 2 <= expCount <= 8, no data/BSS/deps, entry `0xC3`,
  `P=variant/e32load.cpp:443-474`) and its trampoline fallback for the
  older disks without the DLL (`P=variant/e32load.cpp:739-751`) both remain
  supported: the seeded legacy fixtures (`test_dll.py`, `test_args.py`)
  must keep parity.
- **Prt-3 Snapshot content model**: whole-file-snapshot semantics from
  `fat_paths_R1.md`/`qa_inventory_R0.md` pilot line remain the *legacy*
  baseline; the R1-FS backend may replace mechanics end-of-line but must
  keep the existing snapshot-regression cases observable until conclusively
  superseded (matrix E-2 pre-existing snapshot-regression compatibility check
  — with the "in-suite" label; and `fat_paths_R1.md`'s coordinator
  qualification that this is "not a mandate to replace legacy PortFile or
  inherit backend errors unchanged").
- **Prt-4 Both per-process tables** (`sProcs`/`sExecs`) and their
  pre-create overflow gates (`P=variant/e32load.cpp:620`) remain the
  resource guard for every new load path; genuine-loader work must add (not
  remove) failure-cleanup assertions of §3 OA-4 kind.
- **Prt-5 No runtime success before code**: reconciliation "Runtime
  validation follows the reviewed implementation contract; it is not a
  circular prerequisite for writing it" — i.e. approving loader code may
  hinge on *reviewed contract*, not on a passing stable suite (which
  itself cannot pass without the loader); this plane is binding on all
  review routing above, cf. §4.5.

### 4.4 Circular runtime-before-code dependencies flagged (record as review-order traps)

- **CIRC-1** Genuine-loader work (decorated names/UID3 match/absent-marker
  code, §3) needs R1-ABI/v1 freeze, which per the board needs proof source
  selection, which per the matrix needs the loader — a three-sided waiting
  cycle if any card demands runtime evidence of another before producing its
  own. Route: coordinator sequencing, not any single card.
- **CIRC-2** heap-exec G-d ("decode the user pool before concurrency claims")
  is answered here (§2.1), and its own text warns not to claim concurrency --
  the trap is ordering if someone routes "concurrency claim" work as a
  prerequisite of pool verification. This card publishes the arithmetic
  first; concurrency reviews depend on it, not the reverse.
- **CIRC-3** The stable-suite 16/16 pass (`qa_inventory_R0.md`) cannot
  validate a fresh worktree (documented), and some matrix cases assume build
  green; errata already corrected the assumption in *matrix* text, but any
  new card citing "build green" as a *gate for writing code* re-creates the
  circle the errata removed (see Prt-5).

---

## 5. Artifact provenance read in this pass (read-only)

| Artifact | Location | Currency note |
|---|---|---|
| Main-tree build objects (kernel .o, `kernfull.elf` etc.) | `P=build/` | built 2026-10-21 stable-suite run; usable read-only; provenance recorded by the suite |
| Repro E32 artifacts | `/tmp/opencode/r1-build-repro/epoc32/release/x86/urel/` (euser/efsrv/t_cactw/fhserver/rpipe) | from `r1_build_repro_v0.md` (D1–D8 deviations audited in `r1_artifact_audit_v0.md`); *not* superseded by a clean build yet — see below |
| Clean-build attempt | `/home/yuzi/.cache/sybian-r1/r1-build-v1` (canonical root per reconciliation scheduling amendment) | **no allocation/build occurred** (quota amendment 2026-10-22) — nothing new to read; old reproduce hashes remain the only E32-image guide |
| Test logs | `/tmp/opencode/*-test.log` (stabilization et al.) | shared hard-coded paths; flagged each run |

Stale-artifact warning: any §1/§2 claim about *linked behavior* reads the
upstream **source** plus the main-tree Makefile; the repro binaries are only
cited for import-list/format shape, never to prove runtime behavior.

---

## 6. Unknowns left open by this report (honest boundary)

- UNK-1 The marshal layer inside `ExecHandler`/`DCodeSeg::Create` that
  carries the loader's `TCodeSegCreateInfo` in/out is not enumerated
  function-by-function (harmless to §1's reading; see §1 note).
- UNK-2 The RAM-page alloc order after `M::Init2` for `code/pages`
  (chunk allocation from the same bitmap: `mchunk.cpp`) is cited but not
  tracked block-by-block.
- UNK-3 Numbers in §2.1/§2.2 that depend on runtime page rounding
  (`RoundToPageSize` in `P::KernelInfo`) are published as formulas; the
  actual resident sizes are [I] until a monitor readback (OA-1) exists.
- UNK-4 `EExecFsRegisterThread`/`SetWin32RuntimeHook` gateway items from
  heap-exec §9 (G-e/G-f) remain undecoded, unchanged, and outside this
  card.
- UNK-5 Nothing here decides H0-vs-H2 final wording (heap-card's freeze
  section) nor freezes R1-ABI/v1.

---

## 7. Handoff

```
- Card: R1-LOADER-EVIDENCE  Status: done (all four sections completed)
  Date: 2026-10-22
- Deliverable: qemu-port/docs/r1_loader_evidence_v0.md
- Summary: (1) located the missing iFileEntryPoint offset->run-address
  conversion: kernel-side DEpocCodeSeg::DoCreate, pcodeseg.cpp:292-294 -
  closes heap-exec G-b [U] by source reading; chain assigned/copy-back +
  absent-marker/, export relocation, code-seg-ID stamp, CheckRamExports and
  initial-thread consumers all cited (sf_lepoc.cpp:528,657,390-452,2500-2524,
  2779; scodeseg.cpp:710,714-715,748-750,818; sprocess.cpp:239/505;
  e32ldr_private.h:156-172,321-327).
  (2) Closed heap-exec G-d [U] with exact pool arithmetic: base 0x02000000,
  end 0x08000000 (96 MiB, 24,576 4 KiB blocks, 0 pre-allocated), udata/used 0
  from variant values (qemu_variant.h:27-31, rom.cpp:65-69,114,
  asic.cpp:250-261) through direct minit.cpp:27-43; kernel stack/heap
  publish at putils.cpp:296-307; per-process reservations tabled
  (mprocess.cpp:88-115; mkernel.cpp:60-79; xinit.cpp:26-33) including
  doubled heap chunk (cross: heap_exec §5.1); port table limits (sProcs/sExecs
  16, pilot-DLL regime) enumerated as-is. No concurrency claims made.
  (3) Coverage map across tools/test_*: E32 header validation/reloc engines
  and data-reloc runtime covered; decorated names/UID3/version, absent
  exports, code IDs, attach/detach and failure cleanup = no coverage;
  five OA-1..OA-5 observable assertion drafts (no code), plus decisions.
  (4) Ledger: 14 superseded-claim rows (L-1..L-14) with evidence+route;
  REVIEW-1..10 enumerated with citations and routes; preserved legacy
  requirements Prt-1..Prt-5; circular runtime-before-code traps CIRC-1..3.
- Non-goals honored: no loader design, no ABI decisions, no code changes,
  no builds, no QEMU, no scheduler/SCRUM edits. Existing artifacts read
  read-only, provenance listed; no upstream searches beyond the given
  checkout; no share/race precedence resolved.
- Caveats/flags for reviewers: UNK-1..5 listed; §3 OA drafts are proposed
  only; §2 numbers are published init-state values, not runtime spot checks;
  heap-exec G-b closure is a *report-level* completion - the [U] in that
  report's text was not edited and its owner should adopt or refine.
- Next steps for the owner: mark R1-LOADER-EVIDENCE returned; adopt G-b/G-d
  closures into heap-exec follow-up matrix; route L-7..L-11 wording fixes to
  their document owners; sequence loader scope via CIRC-1 (coordinator);
  REVIEW-1..10 remain owner-routed, no action by this card.
```

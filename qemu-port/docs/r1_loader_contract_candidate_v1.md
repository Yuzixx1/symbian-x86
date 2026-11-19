# R1-LOADER-CONTRACT — candidate v1

Date: 2026-10-23. **Proposed implementation contract, not frozen or implemented.**
Owned output: this report only. No E32 execution, QEMU, hardware, target rebuild,
upstream edits, commits or pushes were performed for this card.

## 1. Decision and provenance

Select two explicit regimes: **L (unchanged legacy pilot)** and **G (genuine
uncompressed x86 PE2/V)**. Never fall back from G to L after validation or
resolution failure. Select a port-owned compatible user `!Loader` provider,
not the full upstream F32 loader. Share the G image/graph algorithms between a
kernel bootstrap adapter and a later registered file-service-process adapter.
Use genuine kernel code-segment/process/library objects and existing loader
executives; no invented public kernel loader IPC or patched EUSER ordinals.

This is a bounded proposal [I], subject to coordinator review and ABI/v1 adoption.
Runtime compatibility is **unverified** [U]. It is sufficient to authorize bounded
code cards after review; runtime results are not a prerequisite for writing code.

Actual source baseline: port `fa62323eea7bc96cefd666e40f5189567731bd44`, branch
`main`, with dirty tracked code and untracked reports. Upstream reference:
`kernelhwsrv` at `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean.
Earlier investigation in this conversation read the shared dirty tree; it did
not use an isolated worktree. Final drafting uses detached worktree
`/tmp/opencode/r1-loader-contract-20261023/worktree`, with **193 existing files**
copied over HEAD at 2026-10-23T11:57:59Z. Snapshot hashes and actual provenance:
`/tmp/opencode/r1-loader-contract-20261023/baseline_sha256.json` and
`provenance.txt`. Its `kernelhwsrv` symlink is read-only by task policy, not an
OS-enforced read-only mount. No claim of a clean-HEAD experiment is made.

Inputs include latest reconciliation (including October 23 console/repeat
decisions), roadmap, parallel-work rules, CLEAN-BUILD, STARTUP-CLOSURE,
HEAP-EXEC and LOADER-EVIDENCE. H0 supersedes the old unresolved H1/H2/H3 choice;
the old G5 missing-exec claim is withdrawn. OA-2 absent-marker binding is rejected.
Console staging includes genuine ESHELL, not a stub or fabricated boot flag.

Evidence notation: [S] source; [A] inspected artifact/host measurement;
[I] proposed design/inference; [U] unverified. Paths below use `P=qemu-port`,
`K=kernelhwsrv`, `EK=K/kernel/eka`, and
`F=K/userlibandfileserver/fileserver`. Line numbers refer to the dirty snapshot
or the pinned upstream source, not a future implementation.

## 2. Findings requiring implementation changes

### 2.1 Validator divergence [S][A]

Do not treat the present port validator, its Python mirror, or generated pilot
images as an oracle for genuine E32. Three concrete differences matter:

1. `P/variant/e32load.cpp:122` and `P/tools/mke32.py:88` contain CRC-32 table
   entry 205 `e5d5c06d`; upstream `EK/euser/us_func.cpp:2023-2111` contains
   `e5d5be0d`, equal to the reflected polynomial `edb88320` table. The algorithms
   otherwise use initial CRC zero, with header CRC field replaced by
   `c90fdaa2` while hashing all `iCodeOffset` bytes.
2. `ValidateRelocs` at `e32load.cpp:173-189` interprets relocation `iSize` as
   including its eight-byte section header. Upstream
   `F/inc/f32image.h:986-1000` sets first-block pointer to `sectionHeader+1`
   and end to **that pointer plus iSize**. Import-section `iSize`, conversely,
   includes its four-byte header (`:1104-1114`). Do not conflate the two.
3. Pilot restrictions are not genuine validation: at most one dependency
   (`e32load.cpp:323-324`), no EXE exports (`:343-344`), only 2–4 positional
   imports (`:376-378`), fixed pilot naming/format later in `E32Validate`, and
   at most eight resident DLL exports (`E32LoadDll`, `:454`). Genuine EXEs here
   have a one-word zero export directory; this is not a malformed EXE.

Host extraction of the current C++ `E32Validate` body returned:

| CLEAN-BUILD image | Current result | First rejection |
|---|---:|---|
| t_cactw.exe | -20 | Header CRC, port line 299 |
| fhserver.exe | -20 | Header CRC, port line 299 |
| euser.dll | -20 | Relocation last block, port line 189 |
| efsrv.dll | -5 | Pilot import-count restriction, port line 378 |

Scratch-only replacement of the CRC table word moved t_cactw's first rejection
to EXE exports (344), fhserver's to dependency count (324). Nothing in the
working source was patched. This extraction is a host unit probe, not an exact
guest binary test or a whole upstream validation pass. Scratch:
`/tmp/opencode/r1-loader-contract-scratch/{vh.cpp,vh2.cpp,vh3.cpp,vh,vh2,vh3}`.
The host `tools/e32read.py` also rejected these images; that is not evidence
that the CLEAN-BUILD images are corrupt.

CRC cross-check using the upstream table:

| Image | Stored/upstream CRC | Present port-table CRC |
|---|---|---|
| t_cactw.exe | f7a062ae | 7ba17012 |
| fhserver.exe | ec84575d | 4a3d97a5 |
| euser.dll | 1db41438 | 1db41438 |
| efsrv.dll | ea871f97 | ea871f97 |

These four headers match upstream CRC calculation. This does not prove every
image field valid. Pilot `build_args_e32(0,0,0)` produces stored CRC `eb73f7d6`
where upstream calculation gives `4d909717`: a global table correction without
regime separation would change existing pilot acceptance.

### 2.2 New artifact facts [A]

Use **v1** products under
`/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel`, with full
hash identities in `r1_clean_build_v1.md` §7. Do not substitute the older D5
images or the independently rebuilt v2 products. Existing `logs/hdr_*`,
`dump_i_*`, `dump_e_*` plus scratch `e32facts.py`/`relstat.py` were inspected.

| Image | Code file offset / size | Data / BSS sizes | Dependencies | Exports |
|---|---|---|---:|---:|
| t_cactw.exe | 9c / 2e34 | 180 / 6b0 | 1 | 1 |
| fhserver.exe | 9c / 40f8 | 0 / 6c0 | 2 | 1 |
| euser.dll | c8 / 63a50 | 0 / 0 | 0 | 2503 |
| efsrv.dll | a8 / 111a0 | 0 / 0 | 1 | 421 |

All use V header, PE2 imports, no compression, module version `000a0000`.
EUSER UID3 is `100039e5`, EFSRV UID3 `100039e4`. Their PE export entries are
code-relative offsets, **not link addresses**. EUSER absent offsets equal
entry `264` at 10 ordinals; EFSRV at 20. Neither directory is covered by the
general relocation entries. All four code relocation streams contain type
`3000`; declared nonpadding counts are respectively 222, 454, 1610, 342 for
t_cactw, fhserver, euser, efsrv. Their last blocks end at
`relocationOffset + 8 + iSize`, not `relocationOffset + iSize`.

Dependency/IAT audit: t_cactw → EUSER 50; fhserver → EFSRV 22, EUSER 65;
EFSRV → EUSER 136. No ordinal zero, out-of-range or absent hit was found in
these lists. PE2 block order partitions a contiguous IAT beginning at
`code+iTextSize`; import blocks have only name offset and count. Names are
`euser{000a0000}[100039e5].dll` and
`efsrv{000a0000}[100039e4].dll`. Entry+12 is zero in all four files.

## 3. G validation and immutable representations [I]

Implement bounded readers without unaligned host-structure casts or unchecked
addition/multiplication. A failed parse cannot allocate executable objects.
Use upstream `F/inc/f32image.h:612-1210` as semantic reference; a bounded port
reader may be used in both environments, with differential tests. Do not
blindly include upstream in-place CRC mutation on a const file buffer.

Proposed portable interfaces (sketches, not code delivered):

```cpp
enum TE32Regime { EPilotLegacy, EGenuinePe2V };
struct TImageBytes { const TUint8* iBytes; TUint32 iSize; };
struct TCodeOffset { TUint32 iValue; };
struct TRunAddress { TUint32 iValue; };
struct TLoadAddress { TUint32 iValue; };
TInt ParseG(TImageBytes, TParsedImage&); // immutable file coordinates
TInt PlanG(MImageSource&, const TLoadRequest&, TLoadPlan&);
TInt MaterialiseG(const TLoadPlan&, MLoaderObjects&, TLoadResult&);
void AbortG(TLoadTransaction&, MLoaderObjects&);
```

`TParsedImage` owns validated counts/ranges and views, not kernel handles;
`TLoadPlan` owns immutable snapshots, provider identities, graph edges and
required ordinal sets. `TLoadTransaction` owns every temporary kernel object
reference, handle and storage allocation until publication. No pointer to
client memory survives descriptor decoding. Typed wrappers prevent confusing
file offsets, link addresses, code offsets and run addresses.

### Validation checklist

- Require V, x86 CPU `1000`, EPOC signature, checked UID checksum, standard
  upstream CRC over variable header length, module-version range, UID1/DLL
  flag agreement. No fixed 156-byte or 512-byte header assumption.
- For G require PE2 (`flags & f0000000 == 20000000`) and compression zero.
  PE/ELF, compression, ROM/XIP, demand paging and named-export lookup are
  explicit `KErrNotSupported` in this first contract. Default paging flags
  select unpaged loading; a required-paged image must not be silently accepted.
- Check nonnegative heap/stack/BSS, heap max ≥ min, code/text/data ranges,
  entry+16 within code, x86 ABI/entry-type restrictions, link-base alignment,
  data+BSS overflow, exports within code, import/relocation section extents,
  export-description size/type/coverage. Follow V NoHoles/full/sparse bitmap
  decoding (`f32image.h:876-948`); require agreement with absent entries.
- EXE exports are permitted. For a required DLL ordinal the address must be
  nonzero and non-absent. The EXEs' unused zero export is not itself a failure.
- Validate every relocation block using **8+iSize**, block size including
  its own eight-byte header, aligned page offset, even entries, valid type,
  bounded four-byte destination, and exact nonpadding count. Support types
  1000/2000/3000, as upstream does; do not perpetuate the earlier proposed
  3000-only restriction as if it were an upstream rule.
- Validate import `iSize` including header; exactly `iDllRefTableCount` blocks,
  every name NUL within the import section, checked sum of import counts,
  IAT end ≤ code size. No redundant ordinal-list reads in PE2. Bound graph,
  image bytes and allocation totals by configured budgets, not pilot arrays.
- Proposed initial budgets: 64 segments per transaction, 16 MiB per image,
  32 MiB aggregate snapshots; exceeding these returns `KErrTooBig`. These are
  port policy [I], not upstream limits or guaranteed available pool capacity.
  Malformed arithmetic returns `KErrCorrupt`; allocation failure `KErrNoMemory`.

### Stage table: never relocate a representation twice

| Stage | Code/data | Entry/export directory fields | Exports / IAT |
|---|---|---|---|
| Immutable snapshot | File offsets, link bases retained | Entry is code offset; export directory is file offset | PE entries are code/data concatenation offsets; IAT is ordinal words |
| Plan/create input | Sizes and link bases separate | `iFileEntryPoint=header.entry`; `iExportDir=exportFileOffset-codeFileOffset`; RAM `iCodeLoadAddress=0` | Still immutable file representation |
| Kernel create output | Load/run addresses returned | Both fields now run addresses | Copied code still needs relocation/export conversion |
| Materialised | Code copied; data template copied; BSS zero; general fixups applied | Converted fields unchanged | PE exports converted separately, exactly once |
| Bound | Same allocations | Stamp and entry validated | Each required IAT ordinal replaced by final exporter address |
| Published | No loader writes thereafter | Veneer equals converted entry for these fixtures | Real dependencies retained; genuine startup may run |

General relocation deltas are `codeRun-header.codeBase` and
`dataRun-header.dataBase`. Type 3000 infers code vs data from the **original**
word's membership in the link-time code interval (upstream
`sf_lepoc.cpp:335-381`). Apply to code and initialized data once, not BSS.
Reject overlapping fixup writes and writes to stamp storage in this bounded
profile; this stricter port policy must have dedicated negative tests.

PE export conversion is different: for raw x in `[0,codeSize)`, write
`codeRun+x`; for x in `[codeSize,codeSize+totalDataSize)`, write
`dataRun+(x-codeSize)`. Upstream reference `sf_lepoc.cpp:390-452`.
**Do not add codeDelta to PE raw export offsets.** A required unclassifiable
export is unsupported; it must not become an arbitrary callable address.

## 4. Create-info copyback and kernel object boundary [S][I]

The offset-to-run conversion is kernel-side:

1. `E32Image` sets entry offset (`sf_lepoc.cpp:1872`) and export directory
   offset (`:1848-1870`).
2. `ExecHandler::CodeSegCreate`, `EK/kernel/scodeseg.cpp:1429-1452`, copies
   input with `kumemget32`, creates the segment, then copies back beginning
   at `aInfo.iUids`, length `sizeof(info)-sizeof(info.iFileName)`.
3. `DEpocCodeSeg::DoCreate`, `EK/memmodel/epoc/pcodeseg.cpp:292-294`, adds
   `ri.iCodeRunAddr` to **both** entry and export-directory fields.
4. That copyback includes the converted fields. `CodeSegLoaded` also copies
   back (`scodeseg.cpp:1457-1469`). `ExecHandler::ProcessCreate` copies back
   the inherited fields (`:1676-1739`); the kernel bootstrap call
   `Kern::ProcessCreate` receives the same info by reference (`:1776`).

Consequently user-side and bootstrap adapters must consume returned run
addresses, never add a base again. A separate writable export view is
`codeLoad+(exportFileOffset-codeFileOffset)`, not a second adjustment of
returned `iExportDir`. Keep load and run distinct even though the direct
model currently assigns equal code addresses
(`EK/memmodel/epoc/direct/mcodeseg.cpp:67-90`).

Build ordinary user segments: do **not** inherit the pilot's blanket
`ECodeSegAttGlobal` (`e32load.cpp:656`). Upstream global RAM dependencies have
additional restrictions (`sf_lepoc.cpp:3117-3128`). Preserve UID/security,
module version, true `iTextSize`, dependency count, filename offsets and SMP
attributes. User library loads supply the real client process handle. Do not
put DLL code into a kernel-private heap and call it a user DCodeSeg.

## 5. Provider identity, absent exports and stamp [I]

### Matching

Parse decorated dependency names using `TFileNameInfo::Set(EAllowUid)`
semantics (`sf_ldr.cpp:463-553`, caller `sf_lepoc.cpp:2986-3020`). Bracketed UID
is exactly eight hex digits immediately before extension; hexadecimal version
is eight digits immediately before UID/extension. Strip decoration **only
after** extracting constraints. Case-fold base+extension; no truncation,
path escape, search-by-substring or pilot `EUSER` substitution. Names over
`KMaxKernelName=80` are unsupported; malformed import decoration is corrupt.

Initial deterministic search: explicit absolute filename, then importer
directory for dependencies, then configured `Z:\sys\bin` snapshot namespace.
No ambient shell CWD or unbounded drive scan. Canonical identity includes
resolved image snapshot, UIDs, module version and target-process/data policy.
File identity cannot be a pathname alone. Hold snapshots through publication.

Requested UID fields equal zero mean wildcard; nonzero fields must match.
DLL UID1 and shared-library UID2 must agree with the requested type; EUSER
and EFSRV are never interchangeable. Keep bx86gcc EUSER ordinals and the
documented transformed-eabi EFSRV set from these exact images.

For finite requested version use same major and candidate minor ≥ requested,
as a deliberately bounded strict selection policy. Prefer highest compatible
minor, stable canonical-path tie-break; verify all required exports before
selection. `KModuleVersionNull=ffffffff` means unspecified;
`KModuleVersionWild=fffffffe` selects highest supported version after import
checks. These sentinels are requests, not valid image versions. Upstream has
more elaborate non-strict major fallback (`sf_ldr.cpp:388-460`); this candidate
does not claim to implement it. A future broader policy needs an explicit
revision rather than silently changing selection.

Return `KErrNotFound` when no named/UID candidate exists;
`KErrNotSupported` for found but incompatible version/exports/profile;
`KErrPermissionDenied` for security mismatch. Keep this deterministic mapping
as port policy; do not claim the full upstream search-error precedence.

### Binding and absent rejection

Record every required ordinal before modifying the IAT. Ordinary ordinals
are 1..exportCount, mapped to directory index ordinal-1. Ordinal zero's
upstream named-symbol special case is outside this profile: reject it with
`KErrNotSupported`, never index before the array. Decode export-description
holes and compare converted export address against zero and converted
`iFileEntryPoint`. Any required hole/marker/out-of-range ordinal rejects the
**whole transaction**, with no resumed process or published library handle.
This follows the absent rejection in `sf_lepoc.cpp:526-550`; it does not bind
the importer to the module entry point. Never scan the def file to manufacture
an implementation for an ABSENT slot.

### Entry+12 stamp

For G require the validated fixture veneer shape (jump over reserved storage),
entry+16 within code, zero initial stamp, and no relocation or IAT write into
reserved stamp bytes. A future different genuine entry shape requires a
reviewed profile, not arbitrary stamping. After relocation/export conversion,
before publication, stamp via writable address
`returnedEntryRun + 12 - codeRun + codeLoad`. Compare against the saved
`entryOffset+12` calculation as an invariant. Allocate a serialized monotonically
increasing ID, excluding zero and ffffffff; fail closed before wrap. Failed
loads may consume IDs; never reuse an ID during the boot.

Upstream writes only when zero (`sf_lepoc.cpp:2519-2549`). This candidate
rejects nonzero stamps in a *fresh file snapshot*; an already loaded segment
is reused through its retained identity/object, not restamped from file.
The stamp is an identity token, not a signature, security credential or
substitute for kernel object validation. No stamp writes in L.

## 6. Minimal architecture and typed service boundaries [I]

### Initial kernel launch

`E32RunG` uses a kernel snapshot source and object adapter. It invokes the
shared plan/materialisation core, not the positional pilot DLL loader.
`Kern::ProcessCreate`, `M::NewCodeSeg`, `DCodeSeg::{Create,Loaded,AddDependency}`
and `DProcess::Loaded` are the real object boundaries. No public `!Loader`
claim is made until the user service is present. Kernel bootstrap authority
already passes `K::CheckFileServerAccess` (`EK/kernel/sutils.cpp:2810-2814`).

### Future compatible user provider

Create port-owned `portfilesrv.exe`: compatible `!FileServer` and `!Loader`
threads in the **same process**. FS-DESIGN owns file operations and transport;
this candidate owns only loader sessions/transactions. Coordinate executable
and process ownership before integration; it is not two competing file-server
registrants. `UserSvr::FsRegisterThread` registers that process once. Existing
loader execs only allow kernel or `K::TheFileServerProcess`; registration
also makes it critical/permanent and raises priority
(`sexec.cpp:2217-2239`). Bootstrap this service using the kernel G adapter,
with its dependencies loaded before ordinary clients are resumed.

Choose **port-owned compatible code**, not direct compilation of
`sf_ldr.cpp`/`sf_lepoc.cpp`: those include `sf_std.h` and its FS/plugin/cache
environment; `efile.mmh` couples them to full F32 sources. Reuse source
semantics, not claim an existing standalone build. Full upstream F32,
plugins, demand paging, driver loading, locale loading, lazy unload and hash
service are not implicitly enabled. This preserves a future replacement by
upstream F32 behind the same genuine client ABI.

| Boundary | Owner and types | Required handling |
|---|---|---|
| Client → `!Loader` | Upstream `RLoader`, version major/minor 1.0, build from genuine EUSER; `TLoaderMsg` and exact 32-bit `TLdrInfo` layout in `e32ldr_private.h:34-81` | Real CreateSession/SendReceive, not port freestanding IPC or string commands |
| ELoadProcess=1 | slot 0 writable `TPckg<TLdrInfo>`; slot 1 UTF-16 filename; slot 2 UTF-16 command line | Capture message client thread; requested UIDs, owner type, min stack; create suspended process; return client-owned final handle |
| ELoadLibrary=2 | slot 0 writable package; slot 1 UTF-16 filename; slot 2 UTF-16 search path | Use actual message client process, requested UID/version; return genuine library handle; caller's RLibrary::Init performs attach |
| Provider → file snapshot | FS-owned internal C++ interface, no new public opcode: `OpenImageSnapshot(path, handle)`, `Size(handle)`, `ReadAt(handle, offset, dst, length, actual)`, `Close(handle)` | Stable read-only snapshot or read/share lock for entire load; short reads fail, no mutable file view during relocation |
| Provider → kernel objects | Existing `E32Loader::{CodeSegCreate,CodeSegLoaded,CodeSegAddDependency,ProcessCreate,ProcessLoaded,LibraryCreate,CodeSegClose}` with upstream private structs | Real handles and client-thread identity; no client-supplied kernel pointers |
| Provider writes to code | Existing registered-process `UserSvr::ExecuteInSupervisorMode` as upstream uses (`sf_lepoc.cpp:245-294`) | Fixed provider-internal copy/fixup routines; never IPC-provided function pointers; checked transaction-owned ranges |

The last row is explicit use of an existing privileged executive, **not** a
new hidden kernel service or arbitrary bridge opcode. Kernel adapter performs
the same bounded writes directly. This choice is tied to the direct model;
a protected model requires a reviewed write/mapping adapter. No upstream
kernel modification is proposed. Verify the linked executive/registration
slots during the service card; do not add duplicate handlers on stale G5 claims.

`TLdrInfo` fields are requested UIDs, owner type, handle, secure ID, requested
version and minimum stack size; use genuine headers, not a hand-packed host
struct. Exact descriptor sizes/types/maximum lengths are validated before
allocation. Malformed descriptors follow upstream message panic behavior,
not a successful empty request; unsupported valid opcodes complete
`KErrNotSupported`. The provider must trace `RLdrReq` decoding/update/panic
paths before its code card is accepted. Invalid session version is rejected.
Never trust a process ID carried as a scalar in the message.

The loader worker serializes transactions, owns decoded buffers and completes
each message once. Retain the client thread/process reference through handle
transfer; client death aborts an unpublished transaction. No global code lock
across disk I/O or a client wait. Do not run DLL entry functions in the server.
Follow upstream final-handle creation in `ProcessLoaded` and `LibraryCreate`,
not returning the provider's process-local handle integer.

## 7. Dependency lifecycle, DLL data and H0 [I]

1. Snapshot/validate the complete closure. Deduplicate by identity, not import
   block spelling. First profile rejects dependency cycles with
   `KErrNotSupported`; it need not invent an attach order for them.
2. Create root EXE suspended, then required DLL segments with the final direct
   dependency count. Set user attributes and final entry veneer from create
   output. Allocate/copy/relocate/convert/stamp each new segment.
3. Resolve all required exports; bind IAT partitions in import-block order.
   Add each actual importer→exporter edge once per declared dependency block.
   Finalise dependencies before root using `Loaded`; root process completion
   calls `DProcess::Loaded`/`E32Loader::ProcessLoaded` exactly once.
4. Keep temporary references until `OpenDeps`/library publication has acquired
   process references. `AddDependency` itself only stores the pointer
   (`scodeseg.cpp:828-837`); it is **not** an additional owning Open.
   `DProcess::OpenDeps` maps and CheckedOpens the graph, rolls back partial
   mappings, and transfers the temporary EXE reference (`:1067-1111`).
5. Resume only after full success. Genuine main-thread veneer uses reason 0;
   child thread reason 1; exception reentry uses reason 4. Do not call E32Main
   directly. Set `iEntryPtVeneer` to returned entry before Loaded, preserving
   process `iReentryPoint` setup (`sprocess.cpp:463-515`).
6. Genuine `User::InitProcess` obtains `StaticCallList`, calls dependency entry
   points with ProcessAttach=2, then `StaticCallsDone`, EXE constructors and
   E32Main. `DCodeSeg::ListDeps` visits dependencies first; process entry is
   last and excluded by EUSER (`us_func.cpp:820-852`). Normal User::Exit runs
   reverse static detach=3 and destructors. Do not add a second loader-side
   ctor/dtor walker. `RLibrary::Load` calls Init after server completion
   (`epoc/up_lib.cpp:151-167`); use genuine attach/detach/close/TLS paths.

Do not promise that forced process death executes user destructors, or that
an attach error is automatically surfaced as load failure. The current
`CallStaticEntryPoints` ignores `CallEPs`' result; preserve that source behavior
and test it rather than fabricate transactional constructor rollback. Dynamic
attach failure/close must follow genuine `RLibrary::Init` and kernel closing
lists, not a provider-invented detach replay.

**DLL data policy:** first G profile accepts DLLs with data=BSS=0 only; this
covers the inspected EUSER/EFSRV, not all future console DLLs. Return
`KErrNotSupported` before objects are created for a writable DLL. EXE initialized
data/BSS are supported in its process data chunk. Copy initialized bytes to
returned `iDataLoadAddress` and apply data relocations there using the final
data-run delta. Genuine `DEpocCodeSeg::InitData` copies that template to
`iDataRunAddress` and zeros BSS (`pcodeseg.cpp:310-339`) when the startup call
list initializes data. Do not relocate the final copy again. Verify first-thread
values after `StaticCallList`; copying only to final data before Loaded does
not establish this and can be overwritten by the genuine initialization.
Future writable-DLL support must use `iClientProcessHandle`, kernel mapping/
data initialization and per-process isolation; no shared writable global DLL.
Its separate code card can extend this profile without delaying the initial
serial fixture. Audit genuine ESHELL/econs/ewsrv closure against this policy
before claiming console launch readiness; do not substitute private console.

**H0:** keep genuine EUSER's existing heap path and DL fallback. Pass image
heap min/max and stack correctly; do not install H1, patch chunk semantics,
or synthesize heap/TLS/trap executives. HEAP-EXEC shows the relevant trap,
scheduler and static-call handlers already linked. Validate HE-3…HE-6 fallback,
capacity and cleanup with fresh boot accounting later. The published 96 MiB
pool arithmetic is configuration evidence, not measured concurrency capacity.

## 8. Failure ownership and security configuration [I]

Use a transaction ledger of immutable bytes, created objects, temporary opens,
process/provider handles and publication state. Serialize ledger and cache
updates, but do storage reads outside kernel code/fast mutexes.

| Failure boundary | Required rollback |
|---|---|
| Parse/plan/name/version/export/security | Free snapshots/plan; no kernel object or process created |
| Segment create/copy/relocation/stamp | No resume; destroy unpublished root through genuine stillborn/Release path; close handles and temporary opens in reverse acquisition order |
| Edge/finalisation/process mapping | Root aborted before dependency temporaries close; let kernel undo mapped dependencies; never manually free code memory owned by a DCodeSeg |
| Client death / failed package write / failed handle transfer | Remove any newly installed client handle using genuine handle ownership path; abort unpublished transaction; never return success with an unreported handle |
| Published process/library | Process/library graph owns references; drop loader temporaries exactly once; release snapshots unless explicitly retained cache ownership is accounted |
| Process death | Kernel Release removes mappings/dependencies; port reaper drops only its recorded bootstrap creator reference, never each dependency again |

`Kern::ProcessCreate` transfers command-buffer ownership by nulling its HBuf
reference; if it remains nonnull caller deletes it (`scodeseg.cpp:1740-1799`).
Check this actual reference, not only `proc != NULL`. A failed create can
already have called Release; never blindly Release the returned pointer again.
Wrap kernel adapter states explicitly. No immortal `sDllLoaded` cache in G.
If a cache is later retained, each cache reference is separately opened,
budgeted and closed; use deferred-delete protocol when enumerating live code.

Security baseline: uniprocessor direct memory model, no isolation/NX promise.
Proposed service boot profile pins platform-security enforcement/logging and
sysbin search to **off**, and process isolation to **off**, explicitly as a
bring-up profile, not production security. Independently enforce loader UID,
version and capability-subset checks (`importerCaps & ~exporterCaps == 0`),
secure-ID constraint when requested, trusted bootstrap snapshots, and a fixed
service image allowlist. Preserve image SIDs/VIDs/caps; never grant all caps
to zero-cap fixtures. Sysbin-off does not mean arbitrary path traversal.

Integration must record **effective** kernel flags, paging state and pool
configuration from the linked image/guest; if the existing settings differ,
reconcile the profile before release, not silently toggle them in this card.
Registration is coarse authority and makes service death potentially fatal;
this profile is not hostile-code-safe. FS restart without whole-system reboot
is not promised while registered process permanence remains upstream behavior.

## 9. Exact proposed file split and bounded code cards [I]

All names below are proposals; no files other than this report are changed.
Coordinator assigns integration ownership before editing existing hotspots.

| Order/card | Files and functions | Boundary / acceptance |
|---|---|---|
| LC-1 format/core | New `P/loader/e32g_format.h`, `.cpp`: ParseG, ValidateRelocsG, DecodeExportsG, ParseImportNameG; `P/tools/test_e32g_format.py` plus host runner | Immutable parsing, upstream CRC and relocation-size differential vectors; no execution |
| LC-2 graph/core | New `P/loader/e32g_plan.h`, `.cpp`: PlanG, SelectProviderG, CheckRequiredExportsG; `e32g_load.cpp`: MaterialiseG, AbortG | Typed stage transitions, budgets, graph/order tests, fault-injected fake adapter |
| LC-3 kernel adapter | New `P/variant/e32g_kernel.cpp`, `.h`: kernel MImageSource/MLoaderObjects, E32RunG, bootstrap ledger/reaper; edit existing `variant/e32load.cpp` E32Run dispatch only, `qemu_variant.h` declarations, `kern_shell.cpp` launch selection | Actual DCodeSeg graph, no G pilot fallback, no double-base, object-count rollback; integration owner wires Makefile |
| LC-4 H0/serial bring-up | New `P/tools/test_e32g_startup.py`; use separately owned genuine fixture | Startup, heaps, TLS/traps/constructors/exit, partial-load cleanup; no new duplicate kernel exec handlers |
| LC-5 compatible service | New `P/services/loader/loader_server.cpp`, `loader_session.cpp`, `loader_objects.cpp`, `loader_snapshot.h`; `CPortLoaderServer::NewSessionL`, `CPortLoaderSession::ServiceL`, user MLoaderObjects | Genuine loader messages and final client handles; same-process file-service registration agreed with FS-DESIGN; no full F32 |
| LC-6 dynamic lifecycle | New `P/tools/test_e32g_library.py`, genuine test DLL sources under separately assigned tests directory | RLibrary attach/failure/close/reload and RProcess suspended create/resume; client/server death negatives |
| LC-7 extensions | Separately assigned writable-DLL/cycle/version-policy cards if closure audit requires them | Genuine data mapping/init/unload, explicit profile revision; upstream console/ESHELL integration after dependencies |

LC-1/2 can be reviewed host-only before a QEMU slot. LC-3 onward require
explicit implementation authorization and reserved test slot. Build wiring,
shared ABI/matrix/manual updates and stable-suite scheduling belong to the
integrator. L keeps existing `E32Validate`, `ApplyRelocs`, `E32LoadDll`,
trampolines and regression expectations. An explicit launch-regime argument
is required; do not infer L/G from the filename `EUSER.DLL`. Reserve separate
pilot and genuine image namespaces. Neither automatic downgrade nor a global
CRC/relocation change to L is allowed by this proposal.

## 10. Executable acceptance specification (not run)

Host runners should take explicit image/scratch paths, exit nonzero on any
assertion failure, and emit one named result per vector. Never write v1 images.

| ID | Vector and executable assertions |
|---|---|
| G-01 | Read all four v1 images; assert recorded hashes and parsed fields/counts; compare CRC with polynomial-generated table and upstream table, including index 205 |
| G-02 | Correct relocation end `off+8+iSize`; assert all block endpoints/counts; reject truncated final block, block overflow, bad type, count mismatch, destination outside code/data |
| G-03 | Recompute valid header CRC after each semantic mutation: bad text/IAT extent, malformed decoration, unterminated name, invalid export bitmap, entry+16 overflow; assert intended error branch, not incidental CRC failure |
| G-04 | Synthetic nonzero run/link/load bases; verify general relocs use link deltas, PE exports use section offsets, entry/export field copyback is already converted; assert no double base and no second relocation |
| G-05 | Bind exact 50/22/65/136 lists; all addresses equal selected provider exports; mutate one required ordinal to an actual absent slot (e.g. EUSER 699), zero, ordinal>count; reject whole load with unchanged published-object counts |
| G-06 | Provider UID/version mismatch, malformed version, caps mismatch, two compatible minors, null/wild requests, path-case aliases, cycles/budget overflow; assert deterministic selection/errors |
| G-07 | Stamp zero slot once; exact load/run formula; nonzero fresh slot, conflicting relocation, non-veneer entry, ID exhaustion reject. Reuse live segment retains ID; L bytes unchanged |
| G-08 | Inject failure at each allocation/object open/create/edge/finalise/handle-write boundary; fake ledger returns to baseline, and later guest object/pool counts return to measured baseline after reap |
| G-09 | Fresh guest: actual returned create-info fields vs code/run addresses before any user instruction; read IAT and stamp; EXE data/BSS valid after StaticCallList; no published root on failure |
| G-10 | Genuine fixture ctor marker once, dependency attach before EXE ctor, normal reverse detach, no forced-death destructor promise; repeat load/exit/reap without growing object count |
| G-11 | H0 HE-3…HE-6 allocation fallback/capacity/free; TLS special keys, nested TRAP/Leave, scheduler/request operations, child panic/logon, exception reentry; assert actual outcomes rather than patch heap on assumption |
| G-12 | Real RLoader sessions: version/descriptor negatives, unsupported opcode, UID/version/caps failures, client death during load, exactly-one completion, real caller-owned handle; RProcess remains suspended until client Resume |
| G-13 | RLibrary real attach/close/reload; attach-failing test DLL follows upstream Init behavior; reverse detach/TLS cleanup, no server-side DLL invocation, no leaked temporary/cached references |
| G-L | Run unchanged pilot import/data/args/DLL/kill/reap regressions under explicit L; no stamp mutation, no accidental genuine provider selection, unchanged pilot CRC acceptance |

After implementation only: targeted tests, reserved full stable-suite run on
combined tree, then owner manual PC check. Manual procedure uses verified
disposable media: launch G fixture twice, inspect visible load/exit/reap
diagnostics and counts, reject deliberately missing/absent provider without
resuming, then run L pilot. For console phase separately start registered
services, genuine ESHELL/econs/ewsrv closure with readiness beyond rendezvous;
check VGA/input exclusive ownership and ordinary keys. Serial-only success
does not establish owner-visible console or R1-B unchanged-app acceptance.

## 11. Limits and coordinator handoff

**Completed:** source/byte-backed candidate, explicit architecture/typed IPC,
staged representations and lifecycle, newly identified CRC/relocation-format
divergences, code-card sequence and acceptance specification.

**Tests actually run:** earlier host extraction of port validator (four rejected
images), scratch-only CRC-word variant, independent upstream/polynomial CRC
comparison, read-only byte inspection of relocation blocks/IATs/exports/stamps,
pilot generator in-memory comparisons. No claim these constitute a complete
new G validator, target tests, or runtime proof. Existing scripts and scratch
products were not deleted. Final report checks are recorded in the handoff.

**Blocked assumptions / bounded choices:** same-process loader/FS service host
must be reconciled with FS-DESIGN; initial no-writable-DLL and strict version
policy limit scope deliberately. Console closure may need LC-7 first. Genuine
header validator parity, effective security flags, startup data template,
user service registration and privileged-write dispatch require implementation
tests, not another general inventory. H0, direct model and ordinal identities
remain runtime-unverified. No licence/distribution approval is implied.

**Handoff:** card R1-LOADER-CONTRACT; base
`fa62323eea7bc96cefd666e40f5189567731bd44` + recorded dirty snapshot;
origin branch main, drafting detached HEAD; contract candidate v1, not ABI/v1
freeze. Changed path only `qemu-port/docs/r1_loader_contract_candidate_v1.md`.
Integration order: coordinator review → reconcile service host/security/profile
with FS and console candidates → integrator ABI/v1 → LC-1/2 → LC-3/4 → LC-5/6
→ required LC-7 extensions/console. No target runtime slot allocated here.
Manual step remains future, not performed. Return disagreements to coordinator;
do not ask owner to arbitrate these technical contracts.

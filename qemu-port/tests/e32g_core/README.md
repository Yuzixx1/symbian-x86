# R1-G-CORE — portable graph and transaction handoff

Card 11, 2026-10-23. Host-only implementation candidate; **not kernel, service,
ABI-freeze or runtime evidence**. Only `loader/e32g_plan.{h,cpp}`,
`e32g_load.{h,cpp}`, `r1_image_source.h`, `r1_object_adapter.h` and this directory
are owned. No parser, shared docs/build, adapter, IPC or upstream edits.

## Header checkpoint and integration

Four public headers were exported early to the shared tree, before implementation
tests finished. Strict host C++98 freestanding header compilation passed. Exact
hashes: `/tmp/opencode/r1-g-core-20261023/header_checkpoint.json`. Their signatures
and bytes remain unchanged at final handoff. This is an internal review checkpoint,
not an approved ABI freeze. Coordinator can assign a kernel adapter against them.

`ImageSource` supplies trusted manifest-authorized absolute candidates and
generation-token snapshots. `OpenSnapshot` failure acquires nothing; success
acquires exactly one close obligation. `Bytes` borrows the full immutable snapshot,
whose size must match `Size`. It remains stable until `Close`; namespace contents
must not change during a transaction. `ReadAt` has explicit actual-byte output;
this core uses `Bytes`, not a short-read staging implementation. Candidate names
and enumeration termination must be deterministic. Source owns drive/sysbin,
integrity and forced effective security policy, not CRC-based trust.

`InitPlan` takes disjoint caller-owned Node/Edge/order arrays. No allocation,
STL, exceptions, recursive graph walk or giant automatic arrays in shared code.
Do not copy a live plan or its owned nodes. `BuildPlan` accepts EmptyPlan only;
failure releases all acquired snapshots. A successful ReadyPlan retains them;
caller must `ReleasePlan` if not materializing. `Materialise` consumes a ReadyPlan
on success and failure, including insufficient ledger capacity. An invalid call
with a non-Ready plan does not consume someone else's in-progress plan. Requests,
name buffers, plan/ledger storage and adapters must remain live/nonconcurrent;
the caller must not mutate a ReadyPlan or source bytes. Init functions are for
fresh/released storage, not a way to discard live ownership.

Graph edges preserve import-block index. Immutable ordinal words are read from
file bytes, never from an already patched IAT. Nodes deduplicate case-insensitive
canonical paths; byte-inconsistent snapshots of one path fail closed. Completion
order is dependency-first; cycles fail. Source enumeration can include multiple
versions: core validates ASCII leaf identity, UID decorations against header,
base/extension against import, DLL UID1, requested UID2/3 and versions, then picks
highest compatible version (source order breaks ties). Finite versions require
same major and >= requested minor; ffffffff/fffffffe are unspecified/wildcard.
UID zero means unspecified. Dependency capability sets must contain importer
capabilities; a library root must contain requesting client's capabilities.
Process requested SID and requested UID constraints are checked. Actual
SID/VID/caps are passed unchanged into each CreateSpec. No security-off mode.

Limits bound image bytes, retained aggregate bytes, candidate enumeration, nodes,
edges and order. One extra Candidate call checks enumeration end at the limit;
it acquires no snapshot. Selection may hold a best and trial snapshot in addition
to the retained plan; source must budget backing storage for those temporary
views. The core's aggregate limit covers retained graph plus a trial (conservative
for duplicate nodes), not source caches or backing snapshot allocations. Worst
case preflight duplicate-relocation checking is quadratic; this is a bounded
bootstrap implementation, not a general high-throughput loader.

## Object and transaction contract

`LoaderObjects` is a complete injectable operation boundary, not kernel code:
CreateProcess/CreateCode, bounded copy/read/write, dependency refs, finalization,
boot-wide stamp allocation, atomic publication and non-failing abort/close.
Adapters must implement these contracts honestly; unsupported kernel operations
must return failure, never substitute fake success. Each successful create owns
one temporary code reference, and root process creation additionally owns its
unpublished suspended lifetime. A failed create cleans its own partial/stillborn
allocations; core never closes a token from a failed create. All later failed
operations must leave partial effects owned by the transaction and destructible.
AddDependency retains child refs until importer destruction; CloseCode must
release those recursively when appropriate. AbortProcess happens before closing
any temporary code refs. Close/abort are non-failing; abort is idempotent on the
same ledger/adapter and cannot kill a committed graph.

All Mapping fields are target uint32 addresses; file/region offsets are not
addresses. Adapter reports converted `entryRun`/`exportsRun`; core verifies them
against codeRun, never adds the base twice. codeLoad/dataLoad identify writable
allocation coordinates, codeRun/dataRun runtime coordinates. Write/ReadWord and
WriteWord operate on code or initialized-data-template offsets, never raw host
pointer IPC. Data copy/relocation targets **dataLoad template**, not just final
process data. FinaliseProcess must arrange genuine relocated-template copying,
BSS zeroing, dependency attachment and suspended root load before publication.
Create/Finalise adapters must reject unsupported protected-model memory mappings.

Core copies code and initialized data, applies relocation types 1000/2000/3000
(including unaligned words and intentional 32-bit delta wrap), converts PE2
code/data exports once and binds required nonzero, nonabsent, in-range ordinals.
It validates the EB 0E jump and fourteen zero reserved veneer bytes, rejects
overlapping relocations/IAT/exports/stamp writes, and stamps entry+12 using a
boot-wide nonzero/non-ffffffff monotonically allocated ID. Adapter owns ID
exhaustion and must never roll back/reuse consumed IDs. Fresh snapshots with
nonzero reserved bytes fail; there is no loaded-segment cache/reuse contract.
Unused present-zero exports stay zero; they cannot satisfy binding. Other invalid
export targets fail closed. No L parsing/fallback occurs anywhere in this module.

Finalization is dependency-first. Publish is last and atomic: failure publishes
no handle/reference, success retains root/dependency ownership independently of
temporaries and leaves process suspended. Then temporaries/snapshots close.
Publication returns a client handle, not a loader-thread handle. No Resume, IPC
copyout or handle installation API is hidden in the core. Service integration
must resolve genuine handle rollback/publication before enabling dynamic loads;
it may not call Publish, fail descriptor write, and leak a client handle.
`LoadResult` is zero on failure. Result codes are module-local Symbian-shaped
values; integration must map/diagnose them explicitly. Diagnostic node is a hint,
not a stable error ABI (format detail may reflect a rejected candidate).

## Validation

Run from repository root; scratch **must not already exist**:

```sh
python3 qemu-port/tests/e32g_core/run.py \
  --v1 /home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel \
  --fixture /home/yuzi/.cache/sybian-r1/r1-fixture-build-v1/epoc32/release/x86/urel/r1a_fixture.exe \
  --scratch /home/yuzi/.cache/sybian-r1/r1-g-core-20261023/final
```

Runner compiles actual shared sources with `-std=c++98 -pedantic -Wall -Wextra
-Werror -ffreestanding -fno-exceptions -fno-rtti`, links actual parser and C++ fake
tests, pins all four v1 hashes using format manifest and separately pins fixture
hash `87fde53b240e54ac9aecf4024546b77c91f06a57d83f60c0101c704e0ed6a6a0`.
No fixture bytes are altered. Synthetic vectors reuse the independently laid-out
format test constructor and its independent CRC/UID implementation, not pilot
generation. Parser source remains the dirty-baseline card-8 snapshot, compiled
in C++11 mode; card 12 independently owns its C++98 conversion. Only graph/load
and their public headers claim strict C++98 compile here.

Final run: PASS all five closures (t_cactw 2, fhserver 3, euser 1, efsrv 2,
R1-A fixture 3 segments); all 19 synthetic process, 24 data process, 9 library
object-operation failure positions; all 8 source-operation failure positions;
26 constraint/profile/ordinal/stamp/conflict negatives; higher-minor/wildcard/UID
selection; exact candidate limit; insufficient ledger; diamond shared dependency;
unaligned type1000, general data type3000, code-site type2000 and data-template
type1000 fixups; initialized template copying/BSS assertions; unique stamps
across two transactions. Fake tracks dependency references recursively and checks
zero live refs/snapshots, no publication and repeated-abort safety on failures.
Nonidentity codeLoad/codeRun/dataLoad/dataRun addresses are used throughout.
Published images' IAT and export words are asserted against original raw bytes.
This does not model genuine kernel locking, process-death or handle tables.

Exact argv/status/stdout/stderr: `final/commands.json`; preserved input hashes:
`final/inputs.json` in the scratch root above. Earlier run1 failed harness warning
checks; run2/3/4 passed progressively expanded tests. No sanitizer coverage claimed.
Initial `/tmp/opencode` worktree hit EDQUOT with an empty load.cpp; preserved,
not deleted. Work resumed in new home-backed detached worktree under
`/home/yuzi/.cache/sybian-r1/r1-g-core-20261023/worktree`; compiler TMPDIR is each
unique scratch. No old scratch/SDK roots cleaned, no installs/commits/pushes.

## Evidence and remaining integration gates

Port base fa62323eea7bc96cefd666e40f5189567731bd44; upstream read-only
0c3208650587ac0230aed8a74e9bddb5288023eb. Dirty baseline captured as 213
file hashes in `/tmp/opencode/r1-g-core-20261023/baseline.json`.
Policy follows latest reconciliation and service boundary, not superseded
security-off loader proposal. Semantics checked against upstream f32image.h,
sf_lepoc.cpp relocation/export binding (especially lines 335-449), and existing
service-boundary create/copyback/data-template lifecycle evidence. No upstream
source or immutable image redistribution is included.

Future separately authorized integration must test actual create-info copyback,
partial/stillborn failure, OpenDeps ordering/reference retention, data template
InitData/BSS, graph unload and library handles with genuine objects. Guest tests
must inspect run IAT/stamp before first instruction, verify true effective
security flags and SID/VID/caps, unchanged L behavior, no G fallback, source
namespace protection, handle cleanup at IPC descriptor failure and suspended
publication/explicit Resume. Pure fixture modes need EFSRV binding, not public
FS readiness; file modes wait B3, dynamic operations B4. Protected model,
concurrent loading, loaded-module reuse, paging/compression, writable DLLs,
other CPUs/import formats, search policy and general F32 remain unsupported or
adapter-owned. No kernel adapter/service IPC/shared build edits, target build,
E32 execution or QEMU run were performed. Host pass proves no runtime behavior.

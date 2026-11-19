# R1-KERNEL-ADAPTER — card 14 handoff

2026-10-23, implementation candidate for coordinator review. **Compile-only and
source evidence, not kernel lifecycle/runtime proof.** Own only new
`variant/r1_g_objects.{h,cpp}`, `variant/r1_g_bootstrap.{h,cpp}` and this directory.
No core/parser, legacy loader, shell, shared Makefile or upstream edits.

## Early checkpoint

Both headers were returned early to main, before implementation compilation.
Their bytes remain unchanged; hashes in
`/home/yuzi/.cache/sybian-r1/r1-kernel-adapter-20261023/header_checkpoint.json`.
No core signature correction was needed for this bounded bootstrap. Dynamic
arbitrary-client publication remains unsupported. This does not freeze an ABI.

Important source discoveries returned at checkpoint:

* `DCodeSeg::AddDependency` stores a pointer only, not a retained ref
  (`kernel/eka/kernel/scodeseg.cpp:828-838`). Adapter retains explicit edge holds
  until process OpenDeps succeeds or abort finishes, then drops them before
  temporary refs. Genuine process graph traversal owns steady-state refs.
* `Kern::ProcessCreate` may fail with a non-null stale process output; do not
  touch it. Its `HBuf*&` is nulled iff command ownership transferred
  (`scodeseg.cpp:1776-1800`). Delete only the returned command pointer.
* `MakeHandle` consumes a reference; bootstrap uses `MakeHandleAndOpen` instead
  (`sutils.cpp:1396-1448`), retaining the lifecycle ref required by process death.

## Integration calls (not wired by this card)

Include `variant/r1_g_bootstrap.h`. Construct kernel-owned immutable
`ManifestEntry[]` containing absolute `<drive>:\sys\bin\name` UTF-16 names and
complete genuine G bytes. Entries must be independently authenticated by trusted
staging/manifest policy before this call: CRC/UID are NOT trust. Do not expose
this kernel API as raw pointer IPC, and do not pass mutable disk buffers. Manifest
names, bytes and argument buffer stay live and immutable for the entire call.

```cpp
R1Kernel::BootRequest request = {};
request.manifest = trustedEntries;
request.count = trustedEntryCount;
request.root = fixtureIndex;
request.arguments = argumentOnlyUtf16; // e.g. heap or sched; NOT argv[0]
request.required.version = 0xffffffffu;
request.required.uid1 = 0x1000007au;
// Set required UID2/UID3/SID to staged fixture policy as appropriate.
R1Kernel::BootResult result;
TInt r = R1Kernel::LoadSuspended(request, result);
```

Thread context, interrupts enabled, unlocked kernel, no fast/code mutex held.
Serialized kernel-thread bootstrap calls only, not user threads or concurrent
loaders. Enters/leaves CS itself. One bounded Work allocation on kernel heap:
16 segments, 64 edges, 16 MiB/image, 32 MiB graph, 16 candidates; no giant stack
arrays. Snapshot slots bound retained/best/trial views and reject stale tokens.
Duplicate canonical names and paths outside exact sysbin namespace reject.
Core validates identities/constraints and chooses versions; never pilot fallback.
Bootstrap root must be zero capability; EUSER/EFSRV retain their real capabilities.

On success `result.handle` is **EOwnerThread in the calling kernel thread**;
`result.process` is borrowed while that handle remains live. It is NOT a handle
in an arbitrary user client, and is NOT a separately owned process reference.
No Resume, TSS change, descriptor copyout, effective security flag modification,
heap patch or replacement executive occurs here. Genuine H0 heap/startup remains.
Coordinator must arrange existing genuine ring-transition/TSS prerequisite before
eventual execution, independently of this adapter. Do not call legacy E32Run.

Before explicit integration Resume, diagnose returned process/CodeSeg, real
UIDs/version/SID/VID/caps, effective global security/sysbin flags, root/dep code
load/run addresses, relocated IAT/export/stamp and data template. Entry/export
create-info copyback is already absolute and must NOT be based twice. Process
data/BSS may not yet contain initialized values: genuine StaticCallList invokes
InitData during startup, not DProcess::Loaded. Do not manually replace that path.

Future authorized launch: in the same owning thread and a CS, take system lock,
call process Resume, unlock, then close the thread handle via
`Kern::CloseHandle(NULL, result.handle)` and leave CS when diagnostics no longer
need the borrowed pointer. Do not use result.process after closing its last
protecting handle. To abandon a published suspended process use genuine process
Die/handle close lifecycle under reviewed context, not adapter AbortProcess or a
second Release. Existing port reaper must NOT close G creator references again:
this adapter already releases every temporary creator ref at commit. Integration
must keep L bookkeeping separate. No manual launch/reap was performed here.

## Genuine ownership and memory mapping

`Objects::Begin/End` is single-use per adapter, CS throughout, caller kernel
thread fixed. Shared LoaderObjects methods map to real upstream operations:

| Core operation | Genuine path / ownership |
|---|---|
| CreateProcess | Kern::ProcessCreate, no early handle; main thread suspended; root creator code ref plus process iTempCodeSeg ref |
| CreateCode | M::NewCodeSeg + Create, creator ref; no writable-data DLL |
| Failed code create | Close original segment under code mutex |
| Failed process create | Kernel already Stillborn/Release; ignore returned process; close info.iHandle creator ref only if assigned |
| Write/fixup | Bounded codeLoad or dataLoad template bytes, including unaligned word accesses |
| AddDependency | Real AddDependency plus explicit CheckedOpen edge hold under code mutex |
| FinaliseCode | Loaded + EMarkLoaded under code mutex; real cache maintenance |
| FinaliseProcess | DProcess::Loaded -> code Loaded + OpenDeps, dependency mapping/ref transfer |
| AbortProcess | Dequeue unpublished main thread, clear link, Stillborn, process Release; before edge/creator closes |
| CloseCode | Drop edge holds once; Close creator ref under code mutex |
| PublishProcess | MakeHandleAndOpen in launching kernel thread, creator metadata, suspended |
| PublishLibrary | KErrNotSupported, no visible effect |

Source evidence: `sprocess.cpp:212-260,338-431,463-510`;
`scodeseg.cpp:542-574,669-838,1067-1114,1429-1471,1776-1800`;
`direct/mcodeseg.cpp:45-104,123-131`; `epoc/pcodeseg.cpp:191-307,310-339`;
`kern_priv.h:1277-1278`. Abort mirrors kernel creation-failure synchronous cleanup,
not a running-process kill. No system lock is held across allocation/Loaded/Release;
code mutex is acquired locally; Process Loaded/Release acquire their own mutex.
Partial dependency state remains transaction-owned and destructible.

Create-info is cleared/reconstructed preserving descriptor constructors; names
exclude version decorations; parsed UID/module/security/heap/stack/priority,
exception descriptor and SMP/debug flags propagate. Global user code, unpaged
data/code, unique security zone, no kernel-code privilege or fabricated caps.
Direct allocates code plus initialized template; EXE runtime data comes from
process chunk. Writes go to dataLoad; genuine InitData copies relocated template
and zeros BSS. CodeRun/entry/export mappings use real Create copyback, not guessed
addresses. This source targets the current Direct includes/flags only; protected
models, writable DLLs, loaded-image reuse and dynamic client publication are NOT
supported. Static dependency edge holds bridge genuine pointer-only AddDependency
to core contract; they are not permanent fake graph ownership.

## Checks and provenance

Base fa62323eea7bc96cefd666e40f5189567731bd44 + current dirty snapshot of **224
files**. Upstream read-only 0c3208650587ac0230aed8a74e9bddb5288023eb.
Isolated home-backed worktree:
`/home/yuzi/.cache/sybian-r1/r1-kernel-adapter-20261023/worktree`.
Baseline/hash/checkpoint evidence lives in its parent. No agents, installs,
commits/pushes, old scratch deletion or image redistribution.

```sh
python3 qemu-port/tests/r1_kernel_adapter/check.py
python3 qemu-port/tests/r1_kernel_adapter/compile.py \
  --exports '/home/yuzi/Documents/Sybian REAL/qemu-port/build/export' \
  --scratch /home/yuzi/.cache/sybian-r1/r1-kernel-adapter-20261023/final
```

**PASS** 19 source-contract regressions (text/source checks only).
**PASS** private compile of both actual C++ sources against current genuine
kernel flags/includes, `-m32 -march=i486 -std=gnu++98 -ffreestanding` etc.
`final/commands.json` records exact argv/status/source hashes/stdout/stderr.
Each scratch must be absent; compiler TMPDIR is private. Exports are read-only
existing generated kernel includes; no Makefile invocation, target link or SDK
rebuild. First `compile1` failed a harness path-with-spaces argv bug after
objects compiled; `compile2` and final passed. Final warnings: 11 objects / 10
bootstrap, existing upstream temporary-reference and nonstandard offsetof
warnings (objects has one extra genuine DThread link offsetof). No warning-free,
sanitizer, object-link-resolution or runtime claim.

## Required future separately authorized tests

* Build/link serialized integration, inspect unresolved symbols and effective
  configuration; don't infer real locking/ref validity from compile success.
* Inject failures in process allocation/Create/thread creation, code allocation,
  dependencies/OpenDeps, handle allocation: verify no stale-pointer access,
  code/process/chunk/handle leaks or prematurely executable process. Include
  pre-Loaded and post-Loaded abort and exact creator/edge/process reference counts.
* Run trusted pinned zero-cap R1-A heap/sched with genuine EUSER/EFSRV static
  binding. Public FS readiness is not required for those modes. Measure true
  SID/VID/caps and forced effective security flags; no security-off workaround.
* Inspect relocated template before Resume; after genuine startup verify copied
  initialized data/BSS, heap H0, TLS/traps/scheduler and process exit/unload.
* Check launch/abandon/reap, repeated loads/stamp uniqueness and unchanged L
  stable suite under a separately allocated QEMU slot. No G reaper double-close.
* File modes wait B3, dynamic operations B4; unsupported PublishLibrary remains
  failure until real client-handle rollback is implemented/reviewed. Manual
  disposable-PC launch/reap follows future authorized stable tests, not this card.

No shared integration edits, target link, E32 execution, QEMU or hardware tests
were performed. These checks cannot prove genuine runtime reference/CS safety.

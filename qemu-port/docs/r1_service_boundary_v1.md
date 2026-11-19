# R1-SERVICE-CLOSURE — shared host and bootstrap boundary v1

**Status:** candidate integration contract, 2026-10-23; no implementation,
execution or ABI freeze. This closes the intersection of
`r1_loader_contract_candidate_v1.md`, `r1_fs_design_candidate_v1.md` and
`r1_console_design_candidate_v1.md` under the **later** coordinator ruling in
`r1_research_reconciliation_v0.md:319-359,366-382`. Superseded loader §8
security-off and FS §6 production-server kill/restart proposals are **not**
requirements. This is a first-bootstrap contract, not full upstream F32.

## 1. Provenance and decisions

Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`; upstream read-only
`kernelhwsrv` HEAD `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean.
Detached worktree `/tmp/opencode/r1-service-closure` has an upstream symlink;
main workspace has **75** dirty/untracked paths at inspection. Current
reports, dirty `variant/e32load.cpp`, `variant/fat32.cpp`, `variant/kern_shell.cpp`
and associated sources were inspected explicitly in the main workspace, not
assumed present at worktree HEAD. Evidence key: **[S]** cited source,
**[A]** existing inspected artifact/report (not rerun), **[I]** proposed
integration decision, **[U]** genuinely unresolved integration evidence.
`K/` denotes upstream `kernelhwsrv/kernel/eka/`, `F/` denotes upstream
`kernelhwsrv/userlibandfileserver/fileserver/`, and `P/` the main-tree
`qemu-port/`; longer paths below are relative to `kernelhwsrv/` or workspace.

**Decision [I]:** one trusted G-format `portfilesrv.exe` user process hosts
two `CServer2` instances on **different owner threads**. FS thread owns
`!FileServer`, FS sessions/index/share state and FAT channel. Loader thread
owns `!Loader`, loader sessions, client handles and load transactions. Both
threads belong to the **same** process; only the FS thread calls
`UserSvr::FsRegisterThread()` **once**. Loader access after registration is
process-scoped; a second `FsRegisterThread` would not narrow it. Before this
call, *only the kernel* may use `E32Loader` object execs. [S]
`K/kernel/sutils.cpp:2810-2814`, `K/kernel/sexec.cpp:2217-2239` and
`F/sfile/sf_main.cpp:538-557` establish that boundary and system-permanent
process / registered-thread flags and priority change. Do not patch it away.

The B0 launcher must not assume the current shell's synchronous `RunImage`
can supervise an independent service: the bootstrap integrator owns an
explicit suspended-create/resume plus phase-acknowledgement path. A phase
acknowledgement is an internal launcher/host handshake, **not** a new public
RFs/loader opcode. Its minimal shared declaration is:

```cpp
// qemu-port/services/r1_service_state.h (host + kernel bootstrap integrator)
enum TR1ServicePhase { EHostUnstarted, EHostFsPrepared,
                       EHostRegistered, EHostFsReady, EHostLoaderReady,
                       EHostFatal };
struct TR1BootReply { TR1ServicePhase iPhase; TInt iError;
                      TUint32 iHostProcessId; };
// Bootstrap owns a single monotonic phase transition and wakeup per phase.
// The launched host reports prepare/ready; only bootstrap authorizes clients.
```

The integrator chooses a genuine typed thread/process notification mechanism
and owns its handle lifetime; never infer readiness merely from process
resume, server name presence or an early rendezvous. Once registered,
`EHostFatal` is a diagnostic/fault state, not a restart request. Repeated or
out-of-order phase reports fail closed.

## 2. Phase-locked creation and registration

| Phase / owner | Atomic public boundary and exit |
| --- | --- |
| B0, kernel bootstrap | Mount trusted FAT, choose a recorded system drive, initialize the dedicated private FAT channel device (not a user channel handle). G snapshot/parse/plan/materialize trusted `C:\sys\bin\portfilesrv.exe` and dependency closure through kernel adapter; validate image security and imports; create process suspended, then pin channel authorization to that **specific process object**, not name/SID alone. No pilot fallback. Failure: release pin, close all objects/snapshots, no service registration or client release. |
| B1, unregistered host | Resume trusted host with genuine EUSER startup/H0. FS owner thread starts private FAT channel and initializes path/share/index state, then creates `!FileServer` but **withholds its receive/readiness and any public connection** until B2. Loader worker may be created but does not register `!Loader`. No ordinary client or fixture is launched, no externally visible success rendezvous. Fail before registration: close service handles/channel and abort host under bootstrap supervision; never publish a partial phase. |
| B2, commit authority | FS owner thread (not an arbitrary client/loader thread) calls `UserSvr::FsRegisterThread()` exactly once **after** private FAT startup/verified readiness. This makes the entire process trusted and system-permanent. There is no recoverable kill/restart after this point. On registration or subsequent initialization failure, report fatal boot failure; do not return success or substitute an unregistered host. |
| B3, public FS-only | Activate FS receive loop and publish **FS-ready** only after B2. Kernel launcher may now release only the first approved G fixture or internal clients. RFs Connect itself does not start the server (`F/sfsrv/cl_cli.cpp:239-244`). Loader **remains unregistered**. Future loader thread may use internal FS snapshot methods without a public session, avoiding an RFs/loader startup cycle. |
| B4, loader-ready | On separate loader thread, initialize G graph/object adapters, snapshot/search policy and message transaction state; only then create `!Loader`, start its receive loop and publish **loader-ready**. Loader does **not** call `FsRegisterThread`. Console/ESHELL launch requires B4 plus later FS op/staging preflight; it is not implied by B3. |

Important qualification: `CServer2::Start` publishes a kernel server name and
clients can race a withheld receive; therefore B1 **must not call Start on
`!FileServer` at all** until B2. Above B1 “creates” means allocate
`CR1FsServer` and local resources only; B3 calls `Start`, starts receive and
reports readiness. Do not rely on shell scheduling as an access-control
barrier. [S] `K/kernel/sipc.cpp:825-837` requires ProtServ for `!` names.
Any ordinary FS request during B3 is only a file request, never a loader
control: public RFs opcodes cannot request code-seg creation, image snapshots
or registration. The private channel exposes **no** `E32Loader` executive.
Gated B4 prevents loader-session clients during partial loader setup; user
threads cannot cross into kernel loader execs merely by calling `!Loader`.
This gating is not an isolation guarantee in the direct memory model.

There is deliberately no interval where a *published* loader session accepts
requests without registration. B2 confers process-wide coarse authority even
on the FS worker; this is an unavoidable consequence of the selected upstream
exec and is contained by trusted host image, private channel, single owner
process and not running arbitrary client-supplied code in either worker. It is
**not** a hostile-code isolation proof; if that is required, the architecture
needs new kernel authority primitives and coordinator authorization.

**First fixture:** a single EXE links genuine EUSER **and EFSRV** even for
`heap`/`sched` modes (`tests/r1a_fixture/README.md:83-100`); B0 kernel G loader
must bind the static EFSRV import from staged, validated `sys\bin` image
*before* B3. It does not call `!Loader` for static imports, and the fixture
must not run until FS-ready. An FS-only B3 is therefore possible without
dynamic loader registration; a missing EFSRV DLL/import is a B0/fixture-load
failure, not something to paper over by postponing to B4. First G supports
EXE data/BSS but only DLLs with **zero writable data/BSS** (loader candidate
§7); validate EUSER/EFSRV and `portfilesrv.exe` closure against this before
B0. Writable DLL rejection is pre-object `KErrNotSupported`. No claim that
console's ECONS/EWSRV/ESHELL closure meets this limit; audit/extend in later
owned cards. Genuine ESHELL and false TestBootSequence are later console scope.

## 3. Common typed boundaries (proposed declarations)

These C++ declarations are **internal interfaces**, not EFSRV/loader IPC
opcodes or a second public ABI. Define them once in the named shared headers;
user and kernel adapters implement distinct ends without importing kernel
types into a user service. `TInt` is a Symbian error, never a byte count on
failure. All lengths/ranges are checked before widening/narrowing. An API
change here needs integrator review before graph/FAT/service wiring.

```cpp
// qemu-port/loader/r1_image_source.h (portable; included by graph + adapters)
struct R1ImageId { TUint32 iUid1, iUid2, iUid3, iVersion; };
struct R1Security { TUint32 iSecureId, iVendorId, iCaps[2]; };
struct R1ImageName { const TUint16* iText; TUint32 iUnits; }; // borrowed
struct R1ImageBytes { const TUint8* iData; TUint32 iLength; }; // borrowed view
struct R1Snapshot { TUint32 iSlot, iGeneration; }; // adapter-owned token
class MR1ImageSource {
public:
    virtual TInt OpenSnapshot(const R1ImageName& absoluteSysBinPath,
                              R1Snapshot& out) = 0;
    virtual TInt Size(R1Snapshot h, TUint32& out) = 0;
    virtual TInt ReadAt(R1Snapshot h, TUint32 offset, TUint8* dst,
                        TUint32 capacity, TUint32& actual) = 0;
    virtual TInt Bytes(R1Snapshot h, R1ImageBytes& out) = 0;
    virtual void Close(R1Snapshot h) = 0;
protected: virtual ~MR1ImageSource() {}
};
// Bytes view is immutable and valid until Close(h); no raw FAT or client pointer.
// ReadAt sets actual=0 on failure; caller rejects short read for image sections.
// PlanG owns retained handles, not the adapter; AbortG/plan destructor closes.
```

**Snapshot rule [I]:** immutable *copy* of the entire file under the shared FAT
volume lock, with checked reported size and complete short-read rejection;
release FAT lock before parsing, dependency traversal or any kernel object
exec. Each opened snapshot has its own generation/token, closes once; source
owns bytes until Close, plan owns handle until commit/abort. Limit initial G
to 16 MiB/image and 32 MiB aggregate (loader candidate §3 port policy, not
real FAT capacity). Return `KErrTooBig` before oversized allocation,
`KErrCorrupt` for inconsistent size/short read, original I/O errors for disk,
`KErrNoMemory` for allocation. Both kernel B0 source and registered-host
FS source implement the *same* interface. No snapshot includes a mutable
file view, sector pointer, client descriptor, file-server subsession or LDD
handle. Copy alone guards against changes *during* plan; executable namespace
still needs restricted staging after boot, below.

```cpp
// qemu-port/loader/r1_object_adapter.h (portable loader-facing boundary)
struct R1ClientContext { TInt iThreadHandle, iProcessHandle;
                         TUint32 iOwnerType; }; // adapter-owned live handles
struct R1LoadRequest { R1ImageName iName; R1ImageId iRequested;
                       R1ClientContext iClient; TUint32 iMinStack; };
struct R1ValidatedPlan; struct R1Ledger;
struct R1CodeToken { TUint32 iSlot, iGeneration; };
struct R1ProcessToken { TUint32 iSlot, iGeneration; };
class MR1LoaderObjects {
public:
    virtual TInt CreateCode(const R1ValidatedPlan&, R1Ledger&,
                            R1CodeToken& out) = 0;
    virtual TInt CreateProcess(const R1ValidatedPlan&, R1Ledger&,
                               R1ProcessToken& outSuspended) = 0;
    virtual TInt PublishProcess(R1ProcessToken, R1ClientContext&,
                                TInt& clientOwnedHandle) = 0;
    virtual TInt PublishLibrary(R1CodeToken, R1ClientContext&,
                                TInt& clientOwnedHandle) = 0;
    virtual void Abort(R1Ledger&) = 0;
protected: virtual ~MR1LoaderObjects() {}
};
// Exact typed create-info structures remain upstream e32ldr_private.h;
// these methods are adapter contracts, not replacements for those structs.
```

Graph owner defines `R1ValidatedPlan` and `R1Ledger` in the corresponding
loader header. Each token is adapter-local and owned by the ledger until
publication; `Abort` consumes remaining tokens exactly once.
**No `void*` IPC or user-supplied supervisor callback**. The
kernel adapter uses `Kern::ProcessCreate` and kernel `DCodeSeg` operations;
registered user adapter uses genuine `E32Loader` execs with
`TCodeSegCreateInfo`, `TProcessCreateInfo` and `TLibraryCreateInfo`. Both
consume returned run addresses once (loader candidate §4). `ParseG` copies
header SID/VID/caps unchanged into validated metadata; set both create-info
`iS` fields, never from client-provided `TLdrInfo` or executable filename.
[S] `K/kernel/scodeseg.cpp:703,824` stores/copies `iS`, and
`K/kernel/sprocess.cpp:142` stores `aInfo.iS`. Present pilot
`P/variant/e32load.cpp:640-689` zero-allocates create info and does not fill
`iS`: **G must not inherit that omission**. No forced `ECodeSegAttGlobal`
from pilot. Apply required capability/UID/version/security policies at graph
selection, and do not claim capability subset alone isolates memory.

```cpp
// qemu-port/fs/r1_fs_snapshot.h (user host, private FS -> loader adapter)
class CR1FsSnapshots : public MR1ImageSource {
public:
    TInt OpenSnapshot(const R1ImageName&, R1Snapshot&);
    TInt Bytes(R1Snapshot, R1ImageBytes&);
    void Close(R1Snapshot);
};
// qemu-port/variant/r1_fat_channel.h + fs/r1_fs_backend.h
struct R1FatKey { TUint32 iSlot, iGeneration; };
struct R1FatResult { TInt iError; TUint32 iCommitted; };
// Versioned channel requests: Lookup/Create, ReadAt, WriteAt, Resize,
// Flush, Close and SnapshotCopy; bounded descriptor data, no client pointers.
// SnapshotCopy is a private read path, not a public RFs opcode.
```

The FS owner implements snapshot opening over the same FAT mount and locking
as its file backend and legacy shell/PortFile (`P/variant/fat32.cpp:39-145`,
`P/variant/userfiles.cpp:181-188`). Snapshot handles and public RFs handles
are separate namespaces. FS snapshot caller is the *in-process loader
adapter only*; never provide raw snapshot bytes, privileged `sys\bin` file
handles or `R1FatKey` to an ordinary session. File service itself authorizes
public per-client path/mode and share rights before backend calls. A snapshot
never executes code; graph + object adapters own that decision.

**Channel authorization [I]:** kernel pins the exact `DProcess*` created for
the trusted B0 host, with lifetime reference, and requires that process on
`DoCreate`, every `Request` and `RequestUserHandle`; store and check immutable
process identity/generation, never only a recycled raw pointer, thread ID,
channel name, PID integer or SID. Validate image SID/caps first, require
ProtServ for its protected server names and verify channel's private host
credential as a second check. If the process dies, revoke/close outstanding
channels and release the pinned reference; no cloned/transferred handle to an
untrusted process. Before B0 only kernel snapshot adapter touches FAT; host
channel becomes available to pinned process at B1 **without registration**,
but it cannot call kernel loader execs until B2. Any bootstrap file selection
on this channel is a *fixed, validated sysbin path*; no caller-provided
arbitrary privileged file opens. Implement authorizer once under variant
integrator ownership. Server-only SID check is insufficient because SID can
be copied into an untrusted image and the direct model offers no isolation.

**Namespace/security [I]:** retain effective forced
`EKernelConfigPlatSecEnforcement|EKernelConfigPlatSecProcessIsolation|
EKernelConfigPlatSecEnforceSysBin` unless actual linked configuration says
otherwise (`K/include/u32std.h:526-530`); **no** `__PLATSEC_UNLOCKED__`,
sysbin-off policy or static all-cap grant. For G, normalize names under the
effective sysbin rule and search `?:\sys\bin` only on permitted mounted
drives, with reviewed drive order and explicit fixed first-boot drive; reject
`..`, path escapes, alternate directory, unsupported subst-drive and media
aliases before snapshot open. [S] upstream `F/sfile/sf_ldr.cpp:723-780` rewrites
paths and `F/sfile/sf_lepoc.cpp:865-905,1759-1774` chooses/guards sysbin.
Its removable-media path additionally checks a hash
(`sf_lepoc.cpp:1085-1131`): this port has no equivalent hash service yet.
Therefore first boot stages exact **trusted immutable image bytes/manifest**
at the fixed drive under kernel control; writable removable/unaudited media
must **fail closed** for executable loading. Header CRC and UID checksum
validate format, not publisher trust. Later dynamic paths or removable images
need an explicitly owned integrity/staging policy, not an arbitrary-directory
exception. Public fixture data remains under `<drive>:\R1TEST\...`, not
`sys\bin`; public FS must forbid writes to trusted executable namespace.

## 4. Loader IPC, handles and failure ownership

`!Loader` accepts genuine `RLoader` version 1.0 (build checked against linked
EUSER) and `TLoaderMsg` ELoadProcess=1/ELoadLibrary=2 with
`TPckg<TLdrInfo>` slot 0, UTF-16 path slot 1 and command/path slot 2
(`K/include/e32ldr_private.h:30-81`; `K/euser/us_ksvr.cpp:5097-5110`).
Other valid messages return `KErrNotSupported` in first profile; invalid
version/descriptor gets operation-specific error or upstream-required panic,
not an invented success. [S] Upstream `F/sfile/sf_ldr.cpp:1538-1584` obtains
the **actual** message client `RThread`, then its `RProcess`, sets
`iClientProcessHandle`, supplies the thread handle to LibraryCreate, and
copies back `TLdrInfo` (`:652-661`). `K/kernel/scodeseg.cpp:1475-1570`
creates a library handle *in that client thread*; `:1803-1841` makes final
process handle in the client and sets creator metadata. Returning a provider's
own handle or trusting a scalar client PID is invalid.

Loader thread owns the message and its one terminal completion, both opened
client handles, validated request bytes and transaction ledger. Open client
thread/process references via `RMessage2::Client` and `RThread::Process`,
keep alive through final transfer or abort; obtain client process security
from kernel for library-load compatibility. Build closure from immutable
snapshots; don't hold FAT lock across plan/object exec or send. For
`ELoadLibrary`, enforce genuine `E32Loader::CheckClientState(msg.Handle())`
before mutation as upstream `sf_ldr.cpp:1057-1066`. `ProcessLoaded`/
`LibraryCreate` transfer final handles to client; update slot 0 only after
success. On failed copy/client death, close the **client-owned** handle by a
reviewed genuine handle-owner path or abort transaction before publication;
do not `RHandleBase::Close` an integer in provider's table. **[U]** Exact
client-handle removal API/rollback interleaving is not established by these
sources: implementer must trace `DThread::MakeHandle`/`Close` and upstream
failed `UpdateLdrInfo` before integration; if no safe removal exists, prevent
publication and return blocker, never leak a hidden client handle. Process
creation stays suspended until its caller resumes; user service never runs
DLL entry points. Keep G object reference rollback exactly as loader §8
*other than its rejected security profile*.

The host executable must import genuine EUSER; loader worker must **not**
depend on public EFSRV RFs calls before B3. Shared internal FS snapshots use
the private adapter, not recursive `RLoader` sessions. `RFile` fixture API
continues genuine EFSRV wire; independent raw child/panic cases remain separate
from loader authorization. First loader-ready B4 is not necessary for B3
file-only fixture, but dynamic RLibrary/RProcess and genuine console require
B4. No automatic port-private console or stub ESHELL.

## 5. Death policy, file list and integration sequence

**Production [S/I]:** after B2 the process is process-critical and
system-permanent, registered FS thread system-permanent
(`sexec.cpp:2228-2236`). If production host dies, treat it as **whole-system
failure** and collect boot/fault diagnostics; do not kill/restart for ordinary
A-2 or remove permanence. Ordinary client death/invalid file index is still
isolated to its child (FS candidate §§2,4). **Transport test [I]:** run an
independent noncritical, *nonregistered* IPC service with genuine
`CServer2`/sessions to observe pending live client `KErrServerTerminated`
and controlled restart. This tests generic kernel transport, **not**
production FS/loader restart or its FAT state cleanup. Never register the
test service; do not use `!FileServer`/`!Loader` protected names. Production
queued read cancel/death cleanup uses ordinary client death/disconnect probes
only; exercise server-side failure via pre-B2 injected init error or fresh
boot, not a kill of the permanent host.

**One common file list for integrator assignment:**

| Owner | New files / exclusively edited shared paths at later integration |
| --- | --- |
| Shared host integrator | `qemu-port/services/portfilesrv_main.cpp`, `portfilesrv_bootstrap.{h,cpp}` and new `qemu-port/services/r1_service_state.h` phase/status boundary; solely wires Makefile/startup/`variant/kern_shell.cpp` under serialized ownership |
| Graph + kernel bootstrap owner | `qemu-port/loader/r1_image_source.h`, `r1_object_adapter.h`, `e32g_plan.{h,cpp}`, `e32g_load.cpp`; `qemu-port/variant/e32g_kernel.{h,cpp}`; explicit G dispatch edits in dirty `variant/e32load.cpp` and declarations in `qemu_variant.h` only under integrator review |
| FS/channel owner | `qemu-port/fs/r1_fs_main.cpp`, `r1_fs_session.{h,cpp}`, `r1_fs_ops.{h,cpp}`, `r1_fs_async.{h,cpp}`, `r1_fs_backend.{h,cpp}`, `r1_fs_snapshot.h`; `qemu-port/variant/r1_fat_channel.{h,cpp}`, `r1_fat_offset.cpp`; dirty `fat32.cpp` and legacy sharing edits only in reviewed integration |
| Loader user owner | `qemu-port/services/loader/loader_server.cpp`, `loader_session.cpp`, `loader_objects.cpp`, `loader_snapshot.h`; consumes shared headers and private FS source; does **not** register FS process again |
| Later console / transport-test owners | Console card files and genuine ESHELL/staging; separate nonregistered transport fixture; neither edits this service's registration/permanence |

Sequence: (1) coordinator integrates this boundary and graph/FS header types,
reviews effective flags and trusted image manifest; (2) isolated parser host
tests / separately authorized fixture import audit; (3) graph fake-adapter,
kernel G/bootstrap security propagation and FAT snapshot/channel authorization;
(4) FS host/operations, B0–B3 with static EFSRV fixture; (5) loader thread/
protocol B4 and dynamic lifecycle; (6) later console/ESHELL extension;
(7) serialized combined regression and required two-boot image persistence,
then owner disposable-PC confirmation. No test slot is assigned now.

**Residual [U] for coordinator, not an owner-blocking policy question:** pin
the trusted `sys\bin` staging manifest/drive and prohibit writable executable
mutation (or explicitly allocate a verified hash path); review exact
client-handle rollback API and live registration failure reporting; pin
`portfilesrv.exe` UID3/SID/caps, import closure and process-launch handoff
from emitted image, not from filenames; record actual effective flags,
paging/media attributes and absence of unsafe host image writable DLLs.
Neither a new security-off profile nor production kill/restart is an option.

```text
Handoff — R1-SERVICE-CLOSURE
Base: port fa62323eea7bc96cefd666e40f5189567731bd44 (detached
  /tmp/opencode/r1-service-closure); upstream read-only
  0c3208650587ac0230aed8a74e9bddb5288023eb; main dirty baseline read explicitly
Changed: qemu-port/docs/r1_service_boundary_v1.md only in private worktree
Evidence: [S] kernel registration/access, EUSER/EFSRV loader and FS sources;
  [A] existing candidate/build reports only; [I] phased host/interfaces;
  [U] concrete integration checks in §5
Verification: static + artifact inspection only; no E32 execution; no QEMU; no hardware
QEMU-ready: none. Hardware-verified: none. No implementation, freeze, commit or push.
Remaining gates: coordinator adoption/typed header review, trusted staging and
  rollback/security audit, owned source implementation, tests and persistence
Next owner: coordinator copies/reconciles this report and assigns serialized
  shared-path integration; graph/FS/loader owners consume its boundaries
```

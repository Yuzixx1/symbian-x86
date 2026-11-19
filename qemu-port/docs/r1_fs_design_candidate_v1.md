# R1-FS-DESIGN — compatible file-service candidate v1

Status: **implementation candidate for coordinator review, not an ABI freeze or a
runtime result**. 2026-10-23. This is a bounded `!FileServer` provider for the
selected R1 file fixture, **not** the upstream F32 server or a general F32
replacement. No code, build, E32, QEMU or hardware run belongs to this card.

## 0. Provenance and evidence key

Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`; upstream read-only
`kernelhwsrv` HEAD `0c3208650587ac0230aed8a74e9bddb5288023eb` (clean).
Drafted in detached `/tmp/opencode/r1-fs-design`, with a symlink to upstream.
The main tree has 75 dirty/untracked paths at this inspection, including `variant/fat32.cpp`, `variant/qemu_variant.h`
and `user/ipc.c`; its untracked/current reports and modified source were read
**from the main tree**, not silently replaced by worktree HEAD. The private
worktree does not contain those changes. In particular, the current FAT code's
multi-volume implementation and PortFile behavior are inputs, not committed
baseline. `P/` below means main-tree `qemu-port/`, `K/` means upstream
`kernel/eka/`, and `F/` means upstream `userlibandfileserver/fileserver/`.

`[S]` source fact, `[A]` inspected artifact fact, `[I]` design/inference,
`[U]` unresolved choice or runtime property. Adopted bounded source findings
are from `P/docs/r1_file_contract_review_v0.md` §§1–13 and
`P/docs/r1_research_reconciliation_v0.md:327-342`; **none** of REVIEW-1…10 is
a runtime pass. Approved research client variant is
`port-x86-rfile64-v1` (pinned platform paths and
`SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API`; other optional macros off). The clean
build report describes EUSER/EFSRV binaries, not a running file service.

## 1. Placement and ownership decision

**Choose one user-mode server process using genuine EUSER `CServer2` /
`CSession2` / `RMessage2`, backed by a separate kernel FAT logical channel.**
The server owns `!FileServer`, sessions, path/security rules, per-session file
indices, share graph, offsets and async-read queue. The kernel channel owns
serialized low-level FAT handles/offset operations and never receives raw
`RMessage2`, client descriptor addresses or `TRequestStatus*`. The channel is
private to the server (require owning process identity/privilege, not merely an
obscure LDD name). It must coexist with the original PortFile channel and shell.

[S] `RFs::Connect` only calls `CreateSession("!FileServer",Version(),slots)`;
`F/sfsrv/cl_cli.cpp:239-244,318-328`, `F/inc/f32ver.h:35,48,61` imply
version 2.0.4011. [S] `K/kernel/sipc.cpp:825-837` requires ProtServ to create
a name beginning `!`; `K/include/u32std.h:526-530` forces PlatSec enforcement
unless the kernel build defines `__PLATSEC_UNLOCKED__` (not found in the
inspected port Makefile). [S] Current `P/variant/e32load.cpp:640-690` does not
assign `TProcessCreateInfo::iS`; its allocation is zeroed. Thus a presently
launched port E32 cannot be presumed capable of registering `!FileServer`.
**Prerequisite interface to loader card:** create the service from a validated
image with approved SID/capabilities (including ProtServ), not a forced
security bypass or a kernel-created name masquerading as user service. Prove
image security propagation and a failed zero-capability impersonator case.
Capability subset checks alone do not establish isolation.

[S] Upstream `F/sfile/` spans a full F32 stack (drivers, plugins, cache,
mount, loader); no evidence its EXE can be linked/booted over this port's FAT.
Port-owned compatible subset minimizes the first proof, but must reject
unsupported opcodes (`KErrNotSupported` as in `F/sfile/sf_main.cpp:146-152`) and
must not claim ewsrv, ESHELL, full F32 or loader services work. Do **not** use
the existing freestanding `P/user/ipcsrv.c`: its C IPC smoke fixture has no
genuine EUSER C++/EFSRV server ABI, typed client panic, secure-ID policy or
F32 lifecycle. Do not implement the service as a kernel `DServer`: that would
mix message lifetime/client-thread operations with FAT and duplicate EUSER
session semantics. Exact file registration/startup remains the loader's remit.

### Interfaces to loader/startup (not loader internals)

1. Deliver and explicitly start one validated server image with matching
   genuine EUSER imports and enough heap/stack; expose a **typed** process
   launch, readiness/rendezvous and logon/exit result, with image security
   supplied to the kernel. Its server thread remains alive independently of
   the kernel shell's synchronous `RunImage`. No raw kernel-pointer service
   bridge or privilege exception.
2. Provide a way to open the private logical channel only from the approved
   service identity; return typed errors. A loader-facing file read interface
   for future `!Loader` consumers is separately owned by R1-LOADER-CONTRACT:
   `OpenImage(path, expected identity) -> image read handle`,
   `ReadAt(handle, offset, buffer) -> bytes/error`, `Close(handle)`; the FS
   backend may supply bytes but **does not** choose dependency resolution,
   PE2 validation, dynamic RLibrary behavior or loader protocol.
3. Stage `!FileServer` before fixture `RFs::Connect`, and report startup or
   service-death errors to the coordinator. No implicit RFs auto-launch.

## 2. Owned-file split and interface sketches (proposed, not existing ABI)

Implementation cards would own new files, not overwrite main-tree work:

| Proposed file | Responsibility |
| --- | --- |
| `qemu-port/fs/r1_fs_main.cpp` | EUSER server registration, scheduler, version check, rendezvous and exit; only this process registers `!FileServer` |
| `qemu-port/fs/r1_fs_session.{h,cpp}` | `CSession2`, connect/disconnect, session path, local typed index/generation, open-transaction rollback |
| `qemu-port/fs/r1_fs_ops.{h,cpp}` | exact EFSRV wire decoder, validation stages, copy/panic/close semantics, share/position operations |
| `qemu-port/fs/r1_fs_async.{h,cpp}` | retained `RMessage2`, per-share pending queue, cancellation, growth wake and death cleanup |
| `qemu-port/fs/r1_fs_backend.{h,cpp}` | user-side `RBusLogicalChannel` wrapper with versioned, **typed** fixed structs and bounded byte descriptor calls |
| `qemu-port/variant/r1_fat_channel.cpp`, `r1_fat_offset.cpp`, private header | server-restricted channel and FAT offset/metadata methods; lock with existing FAT users |

These names/splits require coordinator allocation before implementation;
linking/build rules in the shared Makefile are integration-owner work, **not**
authorized edits by this card. A `CServer2` can receive messages on its owner
thread (`K/euser/cbase/ub_svr.cpp:384-394,538-566`); all `RMessage2` copies,
panics and completions occur on that server thread. Never pass an `RMessage2`
or user pointer to the kernel FAT channel. A server-side sketch:

```cpp
// Illustrative, types and error exits to be reviewed against linked headers.
class CR1FsServer : public CServer2 {
    CSession2* NewSessionL(const TVersion& wanted,
                           const RMessage2& connect) const;
};
class CR1FsSession : public CSession2 {
    void ServiceL(const RMessage2& msg); // decode masked function + D0/D1/D2
    void ServiceError(const RMessage2& msg, TInt err); // explicit panic paths
    void Disconnect(const RMessage2& msg); // drain, release, then complete
    TFileIndex iFiles; TFilePath iPath; // no process-global CWD
};
struct TShare { TFileObject* file; TInt64 pos; TUint mode; TQueue reads; };
struct TFileObject { TCanonicalPath path; TUint64 size; TShareSet shares;
                     TBackendHandle backend; TUint refs; };
```

`RMessage2::Panic(category,reason)` uses `MessageKill` then completes the
message (`K/euser/us_ksvr.cpp:2094-2110`); default `CSession2::ServiceError`
simply completes an error (`K/euser/cbase/ub_svr.cpp:116-120`). Thus invalid
file index/type must be an **explicit** `Panic/-8`, not a caught `KErrBadHandle`
completion. Failed handle-reply copy must explicitly unwind and panic
`FSCLIENT/-22`; negative valid writable SetSize must panic `FSCLIENT/20`.
Other malformed copies need operation-specific review, not blanket panic.
The protected server uses `EGlobalSharableSessions` as upstream does
(`F/sfile/sf_main.cpp:57-62`); fixture clients still create **unsharable,
thread-owned** RFs sessions (`K/kernel/sipc.cpp:1296-1351`); sharing/transfer
APIs are not in this profile.

Proposed backend control contract (fixed-size controls plus descriptor data,
not executable addresses):

```cpp
struct TFileKey { TUint32 id, generation; };
struct TIoResult { TInt error; TUint32 bytes; }; // bytes valid on error only
                                                     // under stated partial rule
class RR1FatChannel : public RBusLogicalChannel {
    TInt Lookup(const TDesC16& canonical, TMeta& out);
    TInt Create(const TDesC16& canonical, TCreateDisposition how, TMeta& out);
    TIoResult ReadAt(TFileKey key, TUint64 offset, TDes8& dst);
    TIoResult WriteAt(TFileKey key, TUint64 offset, const TDesC8& src);
    TInt Resize(TFileKey key, TUint64 size, TMeta& out);
    TInt Flush(TFileKey key); // includes volume-level error propagation
    TInt Close(TFileKey key); // release backend reference, not EFSRV close
};
```

Channel controls validate sizes/overflow and copy buffers through genuine
kernel descriptor APIs; backend keys are separate from public subsession
numbers. No speculative 64-bit *transfer length*: descriptor count remains
`TInt`. File index gives session-local typed generation (as upstream
`F/sfile/sf_obj.cpp:423-469,519-568`); allocate a nonzero reply integer, then
write four bytes into slot 3, **only then** publish the index/share. Reserve
resources before backend create; if reply copy fails, remove reservation and
share and close backend. A Create side effect need not roll back; Open probe
uses an existing file. On exhaustion return a bounded error without wrapping a
live generation; stale tokens are rejected while invalid, not forever unique.
Foreign numeric equality can resolve only the receiving session's own object.

## 3. Wire/operation contract and baseline

The decoder masks low 16 bits for opcode and treats function D0/D1/D2 as
descriptor/package selectors; `TIpcArgs` slot types are a separate system
(`F/inc/f32file_private.h:61-92`, `F/sfile/sf_std.inl:226-241`). Do not infer
slot types from pointer-like integers. Base opcodes: SessionPath 14,
SetSessionPath 15, optional MkDir 16/Delete 19, file Close **28**, Open 30,
Create 31, Replace 32, Read 34, Write 35, Seek 38, Flush 39, Size 40,
SetSize 41 and FileReadCancel **120** (`F/inc/F32plugin.h:42-164`).

| Request | Required decode and result |
| --- | --- |
| Open/Create/Replace | in16 path slot 0; mode scalar slot 1; mutable out8 handle package slot 3, capacity >=4; completion is error, not handle; RFile64 adds mode `EFileBigFile` 0x40000 |
| Read/Write | out8/in8 data slot 0, scalar byte count slot 1, scalar position slot 2 (`-1` = current); D2 uses input 8-byte position package. Bound all copies by descriptor capacity and request count |
| Seek | ordinary 38|D2: scalar signed offset, mode, out8 8-byte package; RFile64 38|D0|D2: in8 offset package, mode, out8 8-byte result |
| Size | 40|D0 writes 8 bytes; ordinary EFSRV narrows to `TInt` and can return TooBig after writing low word; RFile64 returns 64-bit size |
| SetSize | ordinary scalar signed 41; RFile64 41|D0 uses input 8-byte package. Negative request on valid writable share reaches server-side `FSCLIENT/20` |
| Flush/Close/Cancel | 39 returns flush error; public void Close sends 28 and drops returned error; 120 targets status address in slot 0 or NULL for that share's queued reads |

Source references/limitations: `P/docs/r1_file_contract_review_v0.md` §§2–11.
Public zero-length explicit Read and over-capacity short circuit in EFSRV
without IPC; raw negative length is Argument on valid framing. Positive EOF
Read returns None, length zero. Wrong width/const flags at a **performed**
kernel copy reject under enforcement, independently of diagnostic logging;
copy layout/capacity checks always apply. No acceptance claim for arbitrary
malformed short input packages: implement bounded reads and error handling,
then review that behavior before negative harness is frozen.

Validation stages must follow the reviewed operation-specific order: parse
and policy before drive open; mount/full-name/locked/write-protect before
R|Write and AsyncAll|X/R mode errors, then locate, effective-R writer, Create
AlreadyExists and share validation. Missing leaf under existing parent
NotFound, missing parent PathNotFound, wildcard or no name-and-extension
BadName (an extensionless name is legal). SetSessionPath checks syntax/policy
but **does not stat** the directory. Do not invent a universal precedence for
compound failures (`F/sfile/sf_drv.cpp:1326-1391`, `F/sfile/sf_utl.cpp:407-550`).

Pin a healthy writable disposable FAT volume mounted before service launch,
public preseeded `C:\R1TEST\A\` and `B\` (or a coordinator-selected mounted
drive), valid descriptors, no competing plugins, no media hot-remove, no
cache interceptors. The proposed provider has **no** upstream F32 file cache
or plugin layer; reflect that as a scope limitation, not an assertion that
upstream's cache path ran. System drive defaults to C if its property is not
set (`F/sfsrv/cl_cli.cpp:161-190`); use explicit fixture drive and session
path. Its default path follows `C:\Private\<client SID>\` but does not create
that directory (`F/sfile/sf_main.cpp:108-126`). Get the client's actual SID
from `RMessage2` security APIs, not a hardcoded zero. Fixture image's SID and
capabilities and server image's ProtServ need artifact confirmation. Review
`F/sfile/sf_utl.cpp:136-213` rules and test public directory access separately;
do not substitute capability checks for storage isolation. [U] Full kernel
PlatSec/config diagnostics and property state must be recorded at integration;
forced flags in this build are source evidence, not an observed guest value.

## 4. State machines, concurrency and errors

**Open and share.** One canonical file object per identity on a mounted drive;
per-session share handles each have their own position and mode. Preserve all
four effective share states X/R/A/W and both first-open orders from
`P/docs/r1_file_contract_review_v0.md` §8: X blocks all; R admits R reader or
W reader, no writer; A admits A/W; W admits W/A, R only absent a writer.
R|Write is Argument; AsyncAll with X/R is Argument. Maintain reference counts
per mode; W→R and W→A promotion, then demotion on last non-W close. A lock
protects canonical-file lookup, share validation, publish and close as one
transaction. Handle a create/open race against file lookup under that lock;
backend I/O is serialized with FAT's existing lock, not performed under the
async queue lock. One server thread is the first-stage state owner; any later
worker/thread split requires marshalled results back to that owner, not shared
`RMessage2` access. No promise of generic F32 locks/transfer/dir handles.

**Position and size.** Resolve `-1` to the requesting share's position; Start
seek retains a requested nonnegative offset past EOF in the result, while
Current/End clamp. Positive write beyond EOF clamps to size *before* bound,
lock or backend allocation; no hole. Positioned successful I/O updates only
that share's position; positive EOF read clears output length and sets its
position to EOF. Source-following adopted SetSize qualification: resize changes
size, **not** share positions immediately; clamp when subsequent Current seek
or I/O takes that path. Shrink/regrow discriminator (200→100→200, preserved
position 150, distinct initialized bytes at 100 and 150) must not accidentally
interpose a position-normalizing call. Growth bytes are unspecified by the
public acceptance contract: the backend may zero freshly allocated clusters
as an explicit hygiene policy, but no fixture should assert generic zero-fill.

**Bounds/partial I/O.** Non-big shares reject writes ending beyond
0x7fffffff with TooBig after clamp; RFile64 uses EFileBigFile for larger size
and scalar positions below 0xffffffff except the reserved scalar sentinel;
D2 at/above that boundary. FAT32 theoretical max is 0xffffffff, **not** a
validated capacity of this port. Validate 64-bit additions before conversion
to 32-bit sector/cluster math, and refuse out-of-volume offsets without
allocation. Backend `TIoResult` distinguishes committed byte count from error:
successful Write completion may only advance the committed count; on an error,
publish any known completed prefix to metadata/position (or roll it back by a
proved transaction) and return the original error, never report uncommitted
bytes as success. For the first bounded adapter choose **sector-at-a-time
staging**, with an error after a written prefix returning the error and a
separately recorded committed count to server state; fixture must re-open and
verify the documented prefix/size after injected failure. Avoid copying output
to client before a backend read success unless the partial-copy policy is
explicit and tested. `KErrDiskFull` (-26) for allocation exhaustion, rather
than existing FAT `KErrNoMemory` (-4) alias; preserve actual NoMemory for heap
failure, Corrupt for broken chain, media I/O errors without collapsing to
NotFound. If an intermediate FAT metadata write leaves a chain inconsistent,
return a failure, mark volume in need of inspection/remount and never claim
atomic or power-loss-safe growth.

**Async read.** AsyncAll only queues an asynchronous Read with insufficient
data; synchronous Read is short/EOF. Queue entries own a copied `RMessage2`,
share reference, status-address identity, end offset and storage/copy plan.
Do not retain stack `TIpcArgs` or a raw client buffer pointer. EUSER/kernel
captures descriptor headers at send (`K/kernel/sipc.cpp:488-510`), not all
payloads; client keeps statuses, headers and payloads live until its own
completion. Only the server owner thread changes queue state; operations from
backend worker (if introduced) enqueue events. Under queue lock, transition
`Queued -> ClaimedForGrowth` or `Queued -> ClaimedForCancel` exactly once;
remove entry and detach it before unlock; terminal completion occurs once
outside lock. A cancel during `ClaimedForGrowth` can miss the queue; it must
not force Cancel after bytes won. Cancel 120 scans **that share**, matches
status address, targets first match; NULL drains that share only. No-match
cancel completes None; it is **not** a read completion barrier. Track retired
entries until their completion is sent; Close/Disconnect drains queued reads
and waits for claimed entries' release before freeing share/backend state.
Backend growth rechecks all affected entries, with output copy and completion
on server thread. Serialize growth, queue and cancel with a defined lock order:
state/share lock -> queue lock for short transitions, release both before
channel I/O, descriptor copy or `RMessage2::Complete`; revalidate references
after I/O. No waits while either lock held. Prove with instrumented queue
acceptance barrier and distinct output/status canaries, not sleeps.

**Death and resource cleanup.** Live session disconnect first prevents new
requests, drains queued messages, waits/releases claimed references, removes
all local indices/shares and backend keys, then completes disconnect. Client
thread death can close its unsharable session: do not assert a completion
arrives in a dead client's status. Kernel IPC suppresses delivery to dead or
closing clients; server detachment completes a still-pending *live* observer
with ServerTerminated (-15), subject to winning normal/cancel completion
(`K/kernel/sipc.cpp:624-630,729-736,1117-1161,1755-1777`). A controlled
server-kill test requires service restart, observer progress, exclusive
reacquisition and baseline resource census. `CServer2`'s default RunError
returns ordinary errors: add explicit policy so a handler's intended panic
cannot be accidentally transformed into a completion. Panic a child request,
never mark deliberate exits of ordinary positives as success.

## 5. FAT adapter and legacy separation

[S] Current `P/variant/fat32.cpp:39-145` switches active-volume globals,
uses a shared 512-byte sector buffer and volume table. `:146-262` supplies
`FatGet`, mirrored `FatSet`, `AllocClus`, best-effort FSInfo adjustments and
`FreeChain`; `:344-475` supplies directory lookup and entry writes.
`FatRead` (`:1043-1082`) requires a whole-file buffer, `FatWrite`
(`:1084-1171`) creates a replacement chain, then swaps the directory entry;
they cannot implement general offset I/O or per-handle size by wrapping them
with a 64-KiB buffer. `AllocClus` currently returns NoMemory for disk full,
and helpers can lose/mask I/O errors; adapt without breaking PortFile's
existing overwrite-before-commit preservation (`P/variant/userfiles.cpp`).

Implement `Lookup/Create/ReadAt/WriteAt/Resize/Flush/Close` **inside or beside**
`fat32.cpp` where its private geometry/dir/chain helpers are visible; do not
expose `Found` or mutable `sCur/sSec` to user mode. Under `UserFilesLock`, select
volume, resolve canonical directory plus leaf (retain 8.3/LFN lookup limits),
check bounds and traverse FAT from first cluster by `offset / clusterBytes`;
split transfers at sector/cluster boundaries. Read tail bytes by sector
scratch, return actual count at EOF. Write partial sector by read-modify-write,
allocate/link clusters only when required, record chain + entry metadata and
committed file size. Resize truncation locates terminal cluster, updates
directory size and frees tail with error-aware order; growth allocates with
checked rollback/orphan detection. Preserve mirror writes; treat FSInfo
maintenance failure as diagnostic or propagated error by explicit policy,
never silently use FSInfo count as guaranteed truth. Avoid a `FreeChain` path
that discards a FAT write failure. Short-name creation and extensionless lookup
must be tested; unsupported names return a specified error rather than being
silently truncated. Maintain a path-to-entry refresh/version rule on rename,
delete or remount; those operations are outside minimum fixture unless owned.

`P/variant/userfiles.cpp:181-188` defines `UserFilesLock`, used by PortFile and
port shell; all new FAT methods use it and restore prior active volume after
return. Do not hold it while waiting for async reads or client IPC. Existing
PortFile retains 65,536-byte snapshots, <=256-byte read chunks and staged
commit; its disk-full overwrite test checks that old destination survives
(`P/tools/test_cli_diskfull.py`). The new service **does not** translate
PortFile's snapshot into offset writes, share its channel census or change
its write-commit semantics. Old pilot image mode and legacy shell remain.
Shell direct FAT writes while service holds a file need an exclusion rule:
either stage fixtures on dedicated paths and forbid shell mutation of live
service files in R1, or integrate shell operations into the same share registry
in a later card. The lock prevents sector corruption, **not** share-policy
coherence. This is an explicit profile limitation, not claimed isolation.

Flush propagates both metadata write errors and a drive-cache flush result.
[S] `P/variant/qemu_ata.cpp:259-263` and `qemu_ahci.cpp:370-375` issue disk
cache flush after writes, but there is no separate visible `FatFlush` primitive
for a dirty service file. Design `FatFlush(handle)` to finish pending entry/FAT
writes, then issue and check an explicit device flush for the correct drive,
even when file is otherwise unmodified; Close attempts best-effort cleanup
but its public EFSRV result is discarded. Cache disabled in first profile;
write-through is not a guarantee against host cache/power loss. Design a
private **test-only** failure hook at this Flush boundary (never a production
silent-success stub), resettable without changing real FAT semantics; test
return error there rather than fail the preceding Write. Remount between
boots and record VM termination/host-cache boundary.

## 6. Bounded implementation cards and acceptance mapping

No implementation is authorized by this report. Proposed order and ownership:

1. **FS-T (transport)**: new `fs/r1_fs_main/session/backend` files; genuine
   EUSER server startup after loader prerequisite, version/security/path/index,
   typed IPC, valid descriptor frames, failed-open rollback, explicit panics,
   disconnect/kill census. Do not yet claim FS operations pass.
2. **FAT-O (offset adapter)**: variant channel and FAT offset methods, FAT
   allocation/truncation/flush/error map; isolated kernel-side or host-side
   structural/fault-injection tests in private scratch, then 64-KiB boundary,
   disk-full old-file preservation and PortFile regression. Changes to dirty
   FAT file require integrator-reviewed merge; do not overwrite current edits.
3. **FS-O (operations)**: scalar/package decoder, validation stages, share
   promotions, position/size, partial I/O, public/raw separation; exercise
   real EFSRV/RFile and RFile64 under selected build variant. Explicit
   coordinator adoption of synthetic-boundary backend scope.
4. **FS-A (async/lifecycle)**: queue/cancel/growth races, death cleanup and
   exactly-once instrumentation; raw negative child harness, server restart.
5. **Integration/harness owner**: reviewed loader/startup link, six serialized
   drivers below, existing `tools/test_stable.py` (15 tests plus build),
   persistence and later owner PC verification. No concurrent QEMU slots.

Six-driver grouping **proposal**, matching
`P/docs/r1a_matrix_errata_v0.md:215-246` (not an already-run suite):

| Driver / isolated image | Required assertions |
| --- | --- |
| A lifecycle, `drivera.img` | F-1 handle package/rollback; F-2 Flush vs Close, deterministic bytes; E-1 staged errors; F-3 EOF/no-hole/position; F-4 source-following shrink/regrow; F-5 public shortcuts; 0,1,65535,65536,65537 B and optional 1 MiB; RFile64 small-file frames |
| B sharing, `driverb.img` | M-1 both acquisition orders, W promotion/demotion, mode negatives; M-2 session-local handles and independent positions |
| C async/death, `driverc.img` | A-1 queue barrier, targeted/all/no-match and controlled growth race, exact-once status/canaries; A-2 live observer ServerTerminated, restart and cleanup |
| D deliberate negatives, `driverd.img` | child logon exact `Panic/-8`, `FSCLIENT panic/19`, `FSCLIENT/20`; malformed reply `FSCLIENT/-22`; shell/observer survival; no dependency on C's files |
| E persistence, preserved `drivere.img` | boot 1 write/check Flush/Close and record termination; **second serialized boot of the same image**, reopen and byte/size compare, no QEMU overlap |
| S-1 scheduler, own image | separate positive genuine CActive proof; no unexpected panic; `t_cactw` needs genuine console and is not an automatic pass from file fixture |

Use public zero-capability file fixture under `C:\R1TEST\...`, with separate
raw probe/negative child where needed; do not weaken EFSRV or kernel checks
for the fixture. Cheap synthetic backend size 0x7fffffff can verify TooBig on
non-big Write and raw 64-bit SetSize *without allocating gigabytes*, if
coordinator approves. A far seek on a tiny file proves framing/EOF only; real
>2-GiB FAT capacity is a separate optional media exercise. S-1 and negatives
have independent outcomes. Record which exact failure-injection point and
partial-write state each test observes. Combined stable-suite and future
regression runs must remain serialized, with unique disposable images/logs.

**Owner disposable-PC manual draft (later, not performed):** confirm the
target disk is disposable, record device/geometry and chosen drive; boot the
approved image, launch service then fixture, observe exact bytes, EOF,
sharing/cancel and separate child panic signatures plus shell recovery. Flush,
Close, and stop by a documented clean guest path if one exists; otherwise
record that VM-style termination is not a clean shutdown. Boot the **same**
media again and compare size/bytes. Do not run injected disk-full/crash cases
on the reference system disk or treat QEMU output as hardware confirmation.

## 7. Gates and coordinator decisions

- [U] Approve the port-owned bounded provider versus requiring full upstream
  F32 first; this proposal does **not** choose loader internals or claim the
  provider satisfies ewsrv's wider filesystem requirements.
- [U] Allocate the loader service-image/security/startup and backend-channel
  authorization interface: current E32Run omits `pinfo.iS`; `!` naming needs
  ProtServ. Do not bypass version/capability validation to run the server.
- [U] Pin port media selection, cache/flush command and error-injection policy,
  fixture SID/caps and any EFSRV calls beyond this minimum; confirm the
  mounted drive's format/geometry and the actual runtime security flags.
- [U] Decide whether optional MkDir/Delete, synthetic large-size backend and
  extra long-name policy belong to the first fixture. No silent support claims
  for directory handles, locks, plugins or generic F32. Approve staged card
  file ownership before source edits; the existing dirty FAT source needs
  deliberate integration.
- [U] Source review does not establish server startup, descriptor layout in
  the linked binaries, FAT partial-failure recoverability, async interleavings,
  physical flush durability or process isolation. These are verification
  targets **after** implementation, not runtime-before-code authorization.

```text
Handoff
Card/base: R1-FS-DESIGN / fa62323eea7bc96cefd666e40f5189567731bd44
  detached /tmp/opencode/r1-fs-design; upstream 0c3208650587ac0230aed8a74e9bddb5288023eb
Changed path: qemu-port/docs/r1_fs_design_candidate_v1.md only (private worktree)
Evidence: [S] genuine EFSRV/EUSER/kernel/FAT and current dirty port source;
  [A] prior clean-build report consulted, no independent binary result; [I] design
Verification: static + artifact inspection only; no E32 execution; no QEMU; no hardware
QEMU-ready: none. Hardware-verified: none. No ABI frozen; no commit/push.
Remaining gates: coordinator choices in §7; reviewed loader/startup/security
  interfaces; FS/FAT implementation, typed/raw tests, serialized stable suite,
  two-boot persistence and eventual owner PC observations
Next owner: coordinator reviews/adopts/splits cards and copies report to main tree;
  implementation owners act only after allocation
```

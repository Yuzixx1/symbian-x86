# R1-FILE-CONTRACT-REVIEW v0 — proposed file acceptance contract

Status: **source-backed review proposal, not frozen; no implementation or runtime evidence**.
Date: 2026-10-22. Owned deliverable: this report only.

## 0. Scope, provenance and reading key

Port HEAD: `fa62323eea7bc96cefd666e40f5189567731bd44`, with substantial existing
uncommitted implementation/report work in the main workspace. Drafting isolation:
detached worktree `/tmp/opencode/r1-file-contract` at that HEAD. Current untracked
reports and dirty port sources were read explicitly from
`/home/yuzi/Documents/Sybian REAL`, not assumed present in the worktree. This report
is handed back as the sole new main-workspace file; no baseline changes were
discarded. Upstream read-only reference HEAD independently checked as
`0c3208650587ac0230aed8a74e9bddb5288023eb`.

Reviewed: original matrix, matrix errata, wire report, FAT paths, BUILD-CONFIG,
roadmap, task board, current card and latest reconciliation. Also read
`r1_clean_build_v1.md:1-7,25-41,121-140`: it now **reports** one completed clean
build and real RFile64 bodies. This card neither rebuilt nor independently
audited its binaries. Old disabled-API panic-stub artifacts are not this contract.

Evidence shorthand, all workspace-relative:

- `F/` = `kernelhwsrv/userlibandfileserver/fileserver/`.
- `K/` = `kernelhwsrv/kernel/eka/`.
- `P/` = `qemu-port/`.
- **S** = source-settled for the stated path/preconditions; **D** = confirmation
  or scope choice deferred. An S finding is not a claim that the port implements it.
- Blockquotes in §§2–11 are **exact proposed matrix replacement wording**.
  Supporting discussion/citations outside the quotes are part of this review,
  not already-adopted matrix edits. All tests below are specifications, not runs.

Configuration: approved research variant `port-x86-rfile64-v1`,
`SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API` enabled; other optional macros remain off
per `P/docs/r1_research_reconciliation_v0.md:267-271`. This client-library variant
does **not** pin kernel PlatSec enforcement, runtime filesystem/cache selection,
server plugins, media properties or the fixture's SID/capabilities. Proposed
deterministic baseline: no intercepting plugins, healthy writable disposable FAT
volume, valid descriptors and valid session unless a case explicitly varies one.
Pin runtime security flags separately; do not enable/disable enforcement merely
to manufacture a desired negative-test result.

## 1. Ten-flag disposition

| Original flag | Source review result | Deferred, not a reason to invent behavior |
| --- | --- | --- |
| REVIEW-1 | **S:** handle reply is mutable out8 capacity >=4; capacity checks unconditional; failed reply unwinds handle and panics client on this path | D: compiled layout/frame capture and resource-baseline test |
| REVIEW-2 | **S:** Flush exposes its result; public void Close discards completion result | D: port flush failure injection and real media/cache boundary |
| REVIEW-3 | **S:** write beyond EOF clamps before end-size/lock/backend processing; no hole | D: integrated buffered/direct path verification |
| REVIEW-4 | **S:** no immediate per-share position adjustment in inspected SetSize chain; no portable zero-fill guarantee | D: coordinator adopts source behavior versus documented clamp; actual port growth-fill policy |
| REVIEW-5 | **S:** public explicit-length zero/overflow shortcuts differ from raw IPC; const/width conditional on enforcement, not logging | D: actual kernel flags and raw negative probes |
| REVIEW-6 | **S:** bounded stage precedence below; isolated missing leaf/path/name codes | D: unenumerated malformed combinations, mount/media races; no invented global total order |
| REVIEW-7 | **S:** keep both orders, access modes, async-all exclusions and W promotion/demotion | D: integrated multi-client assertions; no collapse to symmetric pairs |
| REVIEW-8 (incl. 8b) | **S:** invalid file index/type panics `Panic/-8` in release too; numeric equality across sessions is not proof of invalidity | D: actual panic transport/child containment; other EUSER bad-kernel-handle paths distinct |
| REVIEW-9 | **S:** ordinary versus big-file bounds/packages differ; far seek on tiny file cannot prove maximum | D: proposed small probe sizes and synthetic boundary fixture need adoption; real >2-GiB media tests separate |
| REVIEW-10 | **S:** identity is share + status address; targeted cancel is not a global completion barrier; aliasing prohibited in valid fixtures | D: port exactly-once implementation, resource cleanup and race runs |

There is no extra REVIEW-11: original REVIEW-8b is folded into 5/8. Semantic
review is complete within these boundaries, **not ten flags runtime-closed**.

## 2. REVIEW-1 — descriptors, reply transaction and capacity

Caller → IPC → server → kernel: public Open/Create/Replace calls EUSER
`K/euser/us_mes.cpp:90-127`, which substitutes slot 3 with `TPckgBuf<TInt>`.
`F/sfile/sf_file.cpp:236-263` calls drive open, writes `sizeof(TInt)` bytes in
slot 3, then increments resource count. A failed write invokes
`F/sfile/sf_drv.cpp:102-111` (`CheckSubClose`, then re-leave).
`F/sfile/sf_request.cpp:1128-1139` converts an escaping request-handler leave
to `FSCLIENT/<leaveValue>` panic and releases reserved open resources.

`K/kernel/sipc.cpp:1941-1961` rejects absent/non-descriptor slots with
KErrBadDescriptor regardless of enforcement; const/width flags are conditional
checks (§6). `K/kernel/sthread.cpp:1506-1525` independently rejects a nonwritable
descriptor layout and final length > maximum (KErrOverflow); this is **not**
diagnostics-dependent. Flags and actual descriptor header writability both matter.
Paths are in16; contents/packages are 8-bit; handle reply is 4 bytes, Size/Seek
outputs 8, not four. Descriptor capacity need not equal the write size exactly:
valid larger capacity is allowed; successful output length is the bytes written.

Proposed F-1 / REVIEW-1 replacement:

> Open/Create/Replace use an in16 path, integer mode and a mutable out8 handle
> package with capacity at least 4 bytes in slot 3. Public EUSER supplies the
> correct TInt package. Completion is an error code, not the handle. On success
> output length is 4 and the session-local integer handle is usable. In a raw
> child test, provide a valid mutable out8 reply of capacity 3 on an otherwise
> valid existing-file Open: expect client panic `FSCLIENT/KErrOverflow (-22)`
> and no retained open handle/share or reserved-close-resource leak. Verify a
> second client can open exclusively afterward and repeated failures do not
> consume the service's resource baseline. Do not demand that an underlying
> Create side effect be rolled back if replying with its handle fails.

Observable distinction: use **Open on an existing file** for the failure probe;
Create may already have created the directory entry before reply failure.
Insufficient capacity is not silent truncation, and this handler uses WriteL,
so it is not an ordinary returned Overflow to the raw caller. Actual compiled
sizeof(TInt)/descriptor frames and failure cleanup remain D. Arbitrarily short
**input** TInt64 packages are not validated by this conclusion: the handlers
ReadL into TPckg without explicit exact-length checks; do not claim complete
malformed-package semantics from an output-capacity test.

## 3. REVIEW-2 — Flush versus Close

`F/sfsrv/cl_file.cpp:1479-1520` sends 39 and returns its synchronous result or
uses the caller's async status. `F/sfile/sf_file.cpp:1481-1512` waits for that
share's in-progress requests, flushes dirty file cache if present, returns early
if unmodified, otherwise checks write mode/mount and TRAPs FlushDataL, returning
the trapped error. Reference FAT `F/sfat/sl_file.cpp:614-648` writes metadata
through FlushAllL; this is not a hardware power-loss guarantee.

Public Close calls 28 (`F/sfsrv/cl_file.cpp:531-549`). EUSER
`K/euser/us_mes.cpp:200-210` sends synchronously but **ignores the result** and
clears its local subsession handle. Server close has dirty-cache/busy handling
and removes the index, with completion cleanup on cancellation
(`F/sfile/sf_svr.cpp:23-112`). That is not equivalent to the explicit Flush
error-reporting path. Do not promise Close reproduces every Flush error code.

Proposed F-2 / REVIEW-2 replacement:

> Check every Write and explicit Flush completion before void Close. A forced
> failure in the explicit Flush backend/cache path must be visible as the
> corresponding negative completion (or async status), not KErrNone. Public
> Close has no error return and cannot certify durability or successful flush.
> After success, Close and reopen, compare deterministic bytes and size. Then
> perform a required serialized second boot of the same preserved disposable
> image and compare again. Record VM stop and host-cache boundary precisely;
> VM termination is not a claimed clean guest shutdown. Flush means requested
> I/O was issued, not atomicity or power-loss safety.

D: injection hook/error selection is a **port test proposal**; must exercise
Flush, not a prior Write failure. File-service close/error plumbing and the
host/guest media boundary remain runtime checks, not prerequisites to reviewing
the interface. Host hashing optional; no target SHA library required.

## 4. REVIEW-3 — seek past EOF and positioned I/O

Caller Seek (`F/sfsrv/cl_file.cpp:1465-1473,2782-2789`) → 38 with D2, or
D0|D2 for RFile64 → `F/sfile/sf_file.cpp:1397-1469`: Start stores nonnegative
position and returns without output-copy; Current/End clamp to [0,size]. The
preinitialized client package preserves Start's requested offset.

Write 35 → `F/sfile/sf_file.cpp:1187-1228` → CommonInit
`:936-975`: resolves -1/current, clamps to EOF, then checks legacy/backend
maximum and locks. CommonEnd `:978-1024` updates position and file/cache size
on success. Reference FAT offset-write backend is `F/sfat/sl_file.cpp:236-329`;
the no-hole decision is already made **above** it. Successful positioned Read
updates the same share's position (`F/sfile/sf_file.cpp:736-745`); at EOF it
sets position to EOF and writes an empty descriptor (`:814-848`).

Proposed F-3 / REVIEW-3 replacement:

> On a healthy writable file of size S, Seek(Start,S+10) succeeds and returns
> the unchanged requested offset. A following positive one-byte current-position
> Write starts at S, returns KErrNone, sets size and current position to S+1,
> preserves bytes [0,S), and writes the new byte at S: no hole or zero-filled
> gap is implied. Repeat with an explicit positioned Write beyond EOF. Test
> Current/End clamping separately. A positioned Read(10,3) on sufficient data
> sets that share's position to 13. Use small representable offsets; do not
> exercise signed TInt64 addition overflow as a normative seek test.

D: test direct/buffered port paths as supported; locks, media failure and
concurrent writers are excluded from this deterministic assertion. Separate
opens have separate CFileShare positions; do not generalize to all sessions.

## 5. REVIEW-4 — SetSize fill and position: source/comment conflict

RFile SetSize is scalar 41 (`F/sfsrv/cl_file.cpp:1578-1582`); enabled RFile64
uses D0 and an input TInt64 package (`:2847-2853`). Server checks write access,
mount, negative size, legacy bound, disk space on growth, locks and backend
SetSizeL, then size/cache metadata (`F/sfile/sf_file.cpp:1549-1626`). It does
**not assign share->iPos**. `CFileCB::SetSize64` only writes size words
(`:3438-3444`); SetCachedSize64 only delegates to that or cache size
(`:2723-2735`; `F/sfile/sf_file_cache.cpp:425-428`).

Reference FAT `F/sfat/sl_file.cpp:455-568` allocates/frees cluster chains and
may change its internal traversal position on truncate. That internal
`iCurrentPos` is **not the client's CFileShare::iPos**. Growth contains no
general content-zeroing loop; allocation `F/sfat/fat_table.cpp:351-366`
allocates/extends a chain. RAM FAT has separate zero-fill behavior
(`F/sfat/ram_fat_table.cpp:314-366`), so it is unsound to promote a RAM or
fresh-zeroed-image observation into a generic disk FAT guarantee.

Public comments `F/sfsrv/cl_file.cpp:1563-1566,2833-2836` promise position
clamping after shrink, but the implementation chain above does not do it
immediately. Seek(Current,0) itself clamps, as does a positive Read at EOF or
Write's CommonInit; observing through those calls can **mask** the difference.

Proposed F-4 / REVIEW-4 replacement (source-following choice, pending adoption):

> Check truncation/growth size and retained bytes below min(oldSize,newSize).
> Do not require newly exposed growth bytes to be zero or any fixed pattern;
> write deterministic content before relying on those bytes. For the reviewed
> source implementation, SetSize alone does not reset the per-share position.
> Distinguish that from the API comment's immediate-clamp promise. Set position
> 150 in a 200-byte file, shrink to 100, regrow to 200, then read one byte at
> current position without an intervening Seek(Current), Read or Write: the
> source-following position is 150 (then 151). Record the selected behavior as
> an explicit contract qualification, not an unexplained test tolerance.

For that discriminator, initialize distinct bytes at 100 and 150 **after**
regrowth using a separate share, since growth bytes themselves are unspecified.
This avoids a false position conclusion from recycled/undefined contents.
Keep all shares compatible (e.g. Any|Write). D: coordinator must adopt the
source-following rule or explicitly request documented-clamp divergence. A
deterministic zero-fill policy for the new port backend would also be a **new
design choice**, not proven upstream behavior. No kernel/backend patch is
authorized here. No general cross-filesystem fill/security claim is made.

## 6. REVIEW-5 — public shortcuts, raw wire and security policy

Explicit-length ordinary Read (`F/sfsrv/cl_file.cpp:791-804,982-997`) zeros
output and returns None at length 0, or returns Overflow if request > MaxLength,
without IPC. Negative explicit position is checked **first**. Enabled RFile64
does the same (`:2443-2468,2518-2544`), async branches completing via genuine
User::RequestComplete. Raw server read initialization
(`F/sfile/sf_file.cpp:788-840`) returns Argument for negative length; zero length
completes early without the public descriptor-zeroing guarantee; EOF with
positive length writes empty output/None. Async-all is different (§11).

Security correction: `K/kernel/sipc.cpp:1935-1961` uses __PSIF; its return
comes from `PlatSec::EmitDiagnostic`, `K/kernel/ssecure.cpp:420-438` (via
`K/include/u32std.h:690-693`). **Enforcement** determines nonzero rejection;
diagnostic logging only controls emitting a message. Removing diagnostic text
or code does not by itself switch off enforcement. Layout/capacity checks in
§2 remain independent. Wrong width is not inherently a capacity error.

Proposed F-5 / REVIEW-5 replacement:

> Use explicit-length public Read to test zero-length success with descriptor
> length zero, and over-capacity Overflow before IPC. Test ordinary positive
> Read at EOF as None/length zero, not KErrEof. Test raw negative length only
> with valid session/share/position/descriptor, expecting Argument. Raw zero
> length does not stand in for public descriptor clearing. Record kernel
> PlatSec enforcement and diagnostic settings independently. With enforcement
> enabled, wrong descriptor-width/const argument flags at an executed copy
> reject with KErrBadDescriptor at the kernel-copy layer; the enclosing server
> operation determines completion versus client panic. Never assert that
> logging alone governs rejection, or promise safe success when enforcement
> is off. Keep malformed descriptors confined to child/raw-transport tests.

D: exact flags in the integrated kernel; diagnostic presence is not an
acceptance criterion. A valid public argument test works in either logging
configuration. Raw handle reply (§2) gives a specifically traced panic path;
not every raw copy error in every handler has that transport.

## 7. REVIEW-6 — error precedence is staged, not a universal lookup table

Open/Create caller → 30/31 → `F/sfile/sf_file.cpp:384-454,275-301` parses
first, then policy; Create disk-space threshold and forced Write occur in
FsFileOpenL `:243-251`. Name parsing rejects wildcards or no NameOrExt
(`F/sfile/sf_utl.cpp:407-419,490-503`). **Extensionless names are legal**;
absence of an extension alone is not BadName.

Drive open order `F/sfile/sf_drv.cpp:1326-1391`: CheckMount → illegal full
name → locked mount → write-protected/ROM check if Write → R|Write Argument →
async-all X/R Argument → bad cache combinations/delete-on-close validation →
LocateFile → effective-R writer rejection → already-open Create AlreadyExists →
ValidateShare. Thus R|Write beats a normal share conflict **only after** earlier
mount/path/policy stages succeed. With an effective R holder, Create|Write can
return InUse before AlreadyExists; with other open holders Create may return
AlreadyExists before share incompatibility. Do not replace these with one rule.

Not-already-open reference FAT path `F/sfat/sl_mnt.cpp:1427-1471` first resolves
entry/path; existing directory/volume yields AccessDenied before Create's
AlreadyExists; existing regular file Create yields AlreadyExists before the
read-only-write check. `:2796-2805` calls FindLeafDirL, then leaf DoFindL with
NotFound; missing parent uses PathNotFound (`:1859-1922`). Drive FileOpen maps
Hidden/PathHidden, cleans partial resources (`F/sfile/sf_drv.cpp:1481-1507`).

Proposed E-1 / REVIEW-6 replacement:

> Establish valid mode, permitted path, mounted healthy media, no competing
> open handles and sufficient space before isolated error probes. Missing
> leaf under an existing directory yields NotFound; missing parent yields
> PathNotFound; wildcard or missing name-and-extension yields BadName.
> Extensionless regular filenames are not rejected merely for lacking a dot.
> SetSessionPath to a legal policy-permitted nonexistent directory succeeds;
> a later file operation there yields PathNotFound. Test compound failures
> only against the traced operation-specific stage order; do not infer a
> universal ordering of Argument, PermissionDenied, InUse, AlreadyExists,
> DiskFull and media errors. Do not collapse all backend failures to NotFound.

SetSessionPath parsing/policy/no-stat path is
`F/sfile/sf_svr.cpp:870-889` + `F/sfile/sf_utl.cpp:530-550`.
For concurrent open/delete/media changes, specify allowed outcomes from whichever
valid state the operation observes (e.g. None or InUse for incompatible opens
around holder close), not a fabricated winner. Require no leaked handle, no
incompatible simultaneous live shares, no server panic, no duplicate completion.
D: full malformed-name normalization and all compound error pairs were not
exhaustively reviewed; propose a bounded set of stage-order probes instead.

## 8. REVIEW-7 — share matrix and both acquisition orders

Open on a stable existing regular file → drive mode validation and LocateFile
(`F/sfile/sf_drv.cpp:1351-1391`) → ValidateShare `:827-889` → PromoteShare
(`F/sfile/sf_file.cpp:2443-2495`) → per-share handle. X=Exclusive,
R=ReadersOnly, A=Any, W=ReadersOrWriters; read access=0, Write=0x200.

| Existing effective mode | new X | new R (read) | new A | new W |
| --- | --- | --- | --- | --- |
| X | InUse | InUse | InUse | InUse |
| R | InUse | allow | InUse | allow read; writer InUse |
| A | InUse | InUse | allow | allow |
| W | InUse | allow only with no existing writer | allow | allow |

New R|Write is Argument before this table. AsyncAll (0x400) combined with X
or R is Argument even for the first open (`sf_drv.cpp:1355-1360`). It is an
orthogonal mode restriction, not an extra symmetric share row. W→R promotes
effective mode R; W→A promotes A; last non-W close demotes to W. An A→W
read holder still allows a subsequent W writer; R→W read holder does not.

Proposed M-1 / REVIEW-7 replacement:

> Retain all effective-share pairs with legal read/write variants and execute
> both first-open orders; do not collapse the matrix by symmetry. Assert
> InUse for table conflicts, Argument for R|Write and AsyncAll|X/R on healthy
> permitted paths. Keep Create/Replace outside the pure Open matrix. Hold a
> W reader, add R, verify a W writer is rejected, close R, then verify that
> writer succeeds. Repeat W→A promotion and demotion; test existing W writer
> preventing new R. Use handshakes, not timing assumptions, to prove holders
> remain live during each assertion. Size changes prove visibility, not share
> promotion by themselves; promotion must be observed through allowed/denied opens.

D: execution and port synchronization. Same/share numeric position and async
queue identities must not be conflated. No source justification for dropping
transposed cases or AsyncAll exclusions for a smaller fixture.

## 9. REVIEW-8 — panic transport, session ownership and child isolation

Normal read/size/seek initialization uses GetShareFromHandle and returns
BadHandle on failure (`F/sfile/sf_file.cpp:193-203`);
`F/sfile/sf_utl.cpp:595-613` looks up the **receiving session's** index and
FileShares type. `F/sfile/sf_request.cpp:1438-1447` panics `Panic/-8`, not
an ordinary completion, with no _DEBUG guard. Close validates handle/type in
RequestAllocator (`:150-184`), and ServiceL panics at
`F/sfile/sf_main.cpp:157-164`. EUSER cannot turn those message panics into an
ordinary returned error: kernel MessageKill kills the message's client thread
(`K/kernel/sipc.cpp:2042-2052`).

Handle equality caveat: `F/sfile/sf_obj.cpp:423-469,519-568` constructs and
checks index/instance/type **within each session**. Passing a number from A to
B does not resolve A, but if it happens to be a valid B handle it resolves B.
The matrix's unconditional cross-session-panic wording is incorrect. Likewise
stale rejection applies while that value is invalid, not a forever-unique token
claim over finite generation reuse. Repeated public Close of a cleared local
file sends nothing (`K/euser/us_mes.cpp:200-210`), so use raw stale-handle IPC.

Public negative positioned calls use __ASSERT_ALWAYS before IPC, but their
actual category is **`FSCLIENT panic`**, not `FSCLIENT`: see
`F/sfsrv/cl_main.cpp:24-32`, `cl_file.cpp:982-995,1309-1312`. Counting the
zero-based enum at `F/sfsrv/cl_std.h:31-54` gives EPosNegative=19,
ESizeNegative=20; the nearby `//20` comment is off by one. Negative SetSize
does **not** assert client-side: scalar/package reaches handler, whose
`User::Leave(ESizeNegative)` (`F/sfile/sf_file.cpp:1574-1575`) is converted to
**`FSCLIENT/20`** by `sf_request.cpp:1128-1139` and `sf_std.h:217`.
It requires a writable valid handle and healthy mount to reach that check.

Proposed M-2 / N-1 / N-3 / N-5 / REVIEW-8 replacement:

> Use separate child processes for deliberate panic tests and collect exact
> exit type/category/reason through logon. Invalid/wrong-type file subsession
> values on an otherwise valid RFs session panic `Panic/-8`. For the foreign
> numeric-handle probe use a second session with no corresponding local file
> handle: expect Panic/-8, not access to the first session. A numeric collision
> with a valid second-session handle must operate only on that local object.
> Negative public positioned Read/Write panic `FSCLIENT panic/19` before IPC;
> negative SetSize on a writable valid share panics `FSCLIENT/20` server-side.
> These deliberate child exits are successes only with the exact expected
> signature and surviving server, independent observer client and shell.
> Missing panic transport blocks these cases; never substitute BadHandle
> completion or accept arbitrary panic/timeout as success.

To prove no IPC for N-3, use a future port trace/counter or run the pure public
rejection call on an unopened RFile in the child; the position assertion precedes
SendReceive. Merely seeing no changed file contents is **not** proof of no IPC.
Bad kernel session-handle errors/panics are separate EUSER/kernel behavior, not
this invalid file-index assertion. Dedicated scheduler positives must complete
their own success criteria without unexpected panic; no file call is required
inside unchanged t_cactw. Thread/process critical flags can broaden panic impact
(`K/kernel/sthread.cpp:775-778`): use ordinary noncritical children.

## 10. REVIEW-9 — RFile/RFile64 wire and bounded large-file probes

Two flag systems remain separate: D0/D1/D2 in **function** choose packages
(`F/inc/f32file_private.h:61-92`, `F/sfile/sf_std.inl:226-241`); TIpcArgs
types describe 8/16-bit const/mutable arguments. Normal RSubSessionBase copies
slots 0–2/types and installs integer h in slot 3
(`K/euser/us_mes.cpp:234-267`), not an EHandle kernel object.

| Operation | Ordinary RFile | Enabled RFile64 |
| --- | --- | --- |
| Open/Create/Replace | 30/31/32, in16 name, scalar mode, out8 pkg32 h | same, adds EFileBigFile (0x40000) to mode |
| Read/Write positioned | 34/35, data out8/in8, scalar len, scalar pos; -1 current | same base; uses scalar for nonnegative pos <0xffffffff, else D2 + input pkg64 pos |
| Seek | 38|D2; scalar signed offset, scalar mode, out8 pkg64 | 38|D0|D2; input pkg64 offset, mode, out8 pkg64 |
| Size | 40|D0, out8 pkg64 internally; narrows to public TInt | 40|D0, out8 pkg64 directly into TInt64 |
| SetSize | 41, signed scalar TInt | 41|D0, input pkg64 TInt64 |
| Flush / Close / ReadCancel | 39 / 28 / 120 | inherited same framing, not a parallel service |

Sources: `F/sfsrv/cl_file.cpp:1465-1473,1535-1548,1578-1582,1950-1958,
2004,2058,2332-2343,2443-2466,2575-2590,2782-2789,2808-2853`.
Read/Write RFile64 scalar fast path tests `I64HIGH(aPos+1)==0`, excluding
0xffffffff because that scalar is reserved as current-position sentinel.
Do not assume all >2-GiB positions require D2; the scalar wire is zero-extended
server-side, unlike the ordinary signed public TInt API.

Overflow distinctions:

- Non-big-file share: write end >0x7fffffff yields TooBig **after EOF clamp**
  (`F/sfile/sf_file.cpp:936-958`); SetSize64 request on such a share has its
  own TooBig check (`:1577-1579`). Normal legacy Open rejects an existing
  larger file (`F/sfile/sf_drv.cpp:1492-1494`).
- In this enabled variant, ordinary Size first stores the low word in the
  caller's TInt, **then** returns TooBig for size>KMaxTInt
  (`F/sfsrv/cl_file.cpp:1537-1546`). On that error, don't treat output as a
  valid signed file size or promise it remains unchanged. Ordinary Seek
  narrows the successful reply without a client overflow check (`:1467-1473`);
  do not use an RFile view of a big-file share as a generic safe 64-bit API.
- Reference FAT declares max size 0xffffffff
  (`F/sfat/filesystem_fat.h:82`, `sl_file.cpp:201`), rejects high-word SetSize
  (`sl_file.cpp:455-466`) and read positions above 0xfffffffe (`:206-211`).
  This is source arithmetic, not the current port's validated capacity.
- Public RFile::SetSize cannot express a positive >KMaxTInt request. Casting
  0x80000000 to TInt exercises negative-size panic, **not** TooBig validation.
  RFile64 data lengths remain TInt/descriptors, not 64-bit transfer counts.

Proposed N-2 / REVIEW-9 replacement:

> Required ordinary-file sizes are 0, 1, 65535, 65536 and 65537 bytes, with a
> proposed additional bounded smoke size of 1 MiB if disposable-volume capacity
> permits. Report the largest actual size verified, not near-4-GiB capability.
> Add RFile64 small-file Create/Size/Seek/Read/Write/SetSize coverage under the
> named enabled variant. A far Start seek or positioned write on a tiny file
> tests package routing and EOF clamping only: it cannot prove a file ceiling.
> For cheap legacy-boundary validation propose a test backend with synthetic
> size 0x7fffffff and a non-big-file share: a one-byte current write must yield
> TooBig before backend write. A raw D0 SetSize request of 0x80000000 on that
> non-big share separately checks TooBig, without allocating a huge file.
> Classify these as protocol/validation tests, not real FAT capacity evidence.

Additional proposed nonallocating frame probes: RFile64 Read at 0xfffffffe,
0xffffffff and 0x100000000 on a tiny file checks scalar versus D2 and EOF
None/empty; Seek Start can check package output retention, then reset normally.
Avoid TInt64 max arithmetic in wrappers/seek tests. Actual synthetic backend,
raw harness, 1-MiB size and optional disposable >2-GiB image tests require
coordinator adoption; no such fixture was implemented. Reference FAT32 maximum,
off_t host sparse-file support and port disk geometry must be validated before
any large-image claim. None is a mandatory multi-gigabyte allocation this wave.

## 11. REVIEW-10 — async lifetime, aliasing and allowed cancel outcomes

Caller → async Read 34 + genuine transport status → kernel → server:
`K/euser/us_mes.cpp:72-85` marks Pending and completes send failure;
`F/sfile/sf_file.cpp:824-833` queues only AsyncAll **and** async ClientStatus
when insufficient data. Synchronous Read on an AsyncAll share still has ordinary
short/EOF behavior. AddAsyncReadRequest `:2519-2535` retains message/end/share;
TAsyncReadRequest `:2914-2939` stores status address, session and message.
Cancel 120 (raw pointer in slot 0; NULL means all for that share) →
`:2943-2960` → locked queue scan `:2539-2568`. Targeted cancel stops after the
first match; it is **not** proof duplicate status aliases are supported.

Growth notification `:2572-2616` removes a queued entry under the drive lock
then unlocks and reissues a new request. A cancel in that interval can find no
queued match even though read completion has not yet arrived. Public targeted
ReadCancel skips IPC if status is no longer Pending
(`F/sfsrv/cl_file.cpp:1068-1094`). Cancel completion and read completion are
different messages. Per-share serialization has an explicit plugin caveat
(`F/sfile/sf_file.cpp:778-781,1182-1185`); baseline excludes intercepting plugins.

Proposed A-1 / REVIEW-10 replacement:

> Give every outstanding async operation its own genuine TRequestStatus and
> nonoverlapping mutable output descriptor/storage. Keep statuses, descriptor
> headers and payloads alive and do not reuse/reset a Pending status. For
> AsyncAll use Any or ReadersOrWriters sharing and a separate compatible
> writer. Keep data insufficient and writer stopped until a queue-accepted
> barrier for deterministic targeted cancel: that queued read completes Cancel
> exactly once; the separate cancel request returns None; another share/status
> is unaffected. In the controlled growth-versus-cancel race on healthy media,
> allow Cancel or successful read with the expected bytes/length, exactly once,
> whichever path won ownership. Cancel returning None alone is not a barrier:
> wait for the read's own terminal completion before freeing its storage.
> Cancel-all applies only to queued reads of that share. Do not invent generic
> WriteCancel/FlushCancel or require Cancel after a read has left the wait queue.

Controlled race excludes server death, memory/media failure and external
shrinks; if those are injected, enumerate their errors as separate scenarios
rather than broaden the healthy-race oracle to “any error”. Async statuses may
complete before the API returns; never require an observable Pending interval.
Use explicit positions/distinct byte regions for the two queue tests so current
position changes do not create accidental ordering assumptions. Alias tests
that deliberately reuse a Pending status are outside the valid-client contract;
no particular benign completion/panic is established for that misuse.

Important descriptor lifetime nuance: `K/kernel/sipc.cpp:488-510` captures
descriptor **headers** during send; it does not snapshot all async write data.
`F/sfsrv/cl_file.cpp:2541-2544` itself uses a stack TPckgC offset, so do not
require callers to retain TIpcArgs/that internal package object beyond return.
The fixture should keep its own raw input packages persistent, while recognizing
that server initialization/capture timing for arbitrary late input copies needs
separate review. Async write **payload** must not be modified/freed until its
terminal completion; async output/status lifetime remains mandatory.

Observable tests: queue barrier (future instrumentation, not a sleep), distinct
targeted and cancel-all reads, wrong-status/no-match None with original still
pending, growth-first None with correct bytes, cancel-first Cancel, race repeated
with per-message completion accounting, fixed slots with a spare for cancel
(`F/sfsrv/cl_cli.cpp:225-244`). Guard bytes catch stray buffer writes; a status
value alone cannot prove no duplicate completion/request-signal leak. Drain/wait
all requests, verify resource baseline and observer progress. No new tests ran.

## 12. Ownership, client death and server death

RFs default CreateSession uses EIpcSession_Unsharable
(`F/sfsrv/cl_cli.cpp:239-244`, `K/euser/us_ksvr.cpp:2283-2293,2320-2323`);
kernel makes that a thread-owned handle (`K/kernel/sipc.cpp:1335-1351`). Use
one independently connected RFs per client thread/process for this fixture;
sharing/transfer/duplicate APIs are not silently covered. RFile's ordinary
no-close parent reference does not keep RFs alive independently
(`K/euser/us_mes.cpp:103-126`): retain parent until children/requests finish,
close files then session normally.

Client thread death is not a successful cancel-completion assertion to that
dead thread. Thread exit retains outstanding IPC references
(`K/kernel/sthread.cpp:783-789`), closes handles before freeing user stack
(`:193-199`); last session client-close sends disconnect
(`K/kernel/sipc.cpp:1197-1227`). Server Disconnect closes main-thread objects
and defers final cleanup behind access count (`F/sfile/sf_ses.cpp:90-112,
162-170`). Cleanup cancels async reads/reserved closes and deletes handle index
(`:44-79,476-496`); index destruction closes retained objects
(`F/sfile/sf_obj.cpp:395-420`); CFileShare destruction removes locks/share mode,
cancels queued reads and closes file (`F/sfile/sf_file.cpp:2759-2769`).
This is a reference lifecycle, not proof of all interleavings in the new port.

Kernel ordinary MessageComplete suppresses user completion when client dead or
session closing (`K/kernel/sipc.cpp:1755-1777`); message free releases IPC count
(`:624-630`). Server termination detaches with ServerTerminated (`:729-736`),
Detach drains pending messages and completes only live/nonclosing clients
(`:1117-1161`). Thus A-2 should guarantee -15 to a **live connected observer
with a genuinely still-pending request**, not to dead clients or requests that
already won normal/cancel completion. No double completion, UAF, retained share
or shell crash is allowed. For kill-client tests observe from another process:
reacquire exclusive open, repeat cycles, check service resource baseline and
progress. Do not depend on a dying child's status contents. Polling with a
bounded harness timeout is an observation deadline, not a source-specified
cleanup latency. Explicit service restart is sufficient; no R3 supervisor needed.

## 13. Minimum fixture services and implementation-facing split

This is the **proposed minimum file-fixture profile**, not a claim that the full
upstream file-server EXE boots with only this list. This card covers a compatible
R1 service behind genuine EFSRV, not a reduced fake replacement for the separately
owner-selected genuine console route.

1. Loader/startup binds matching genuine EUSER/EFSRV and their required imports,
   runs genuine heap/TLS/traps/CBase; kernel supplies typed IPC copies, sessions,
   request wait/completion, thread/process identity, panic/logon/death and cleanup.
   Enabled-library artifacts can be reviewed before these runtime gates exist.
2. Start a version-compatible `!FileServer` (client version 2.0.4011 per
   `F/sfsrv/cl_cli.cpp:318-328` and `F/inc/f32ver.h:35,48,61`)
   **before RFs::Connect**, which only
   CreateSessions and does not launch it (`F/sfsrv/cl_cli.cpp:239-244`). Implement
   connect/disconnect plus minimal tested operations: SessionPath 14,
   SetSessionPath 15, Open/Create/Replace 30/31/32 as selected by fixture,
   Read/Write 34/35, Seek 38, Flush 39, Size 40, SetSize 41, Close **28**, and
   ReadCancel **120**, including their function package bits and async forms.
   MkDir 16 only if fixture creates rather than pre-seeds its directories;
   Delete 19 only if it self-cleans rather than using disposable/preseeded images.
   Lock 36/37, directory handles/packed entries, plugins and transfer are outside
   this minimum; a wrong-type test needs a separate valid dir/raw object fixture.
3. Provide mounted writable disposable FAT, offset I/O, sizes, directory lookup,
   allocation and error propagation plus explicit flush. Preseed public
   `<drive>:\R1TEST\A\` and `B\` (proposed names), or own
   `Private\<actual-SID>\...`, and explicitly set each session path. Server
   default `<system-drive>:\Private\<SID>\` comes from
   `F/sfile/sf_main.cpp:108-126`; connect does not create that directory.
   SID checks and public-path success with zero capabilities follow
   `F/sfile/sf_utl.cpp:136-155,186-213` and Open/Create policy in §§6–7.
   Capability-none is not “SID zero” or permission to use another app's private
   directory. Normal file operations do not require the app to mount media,
   start privileged services or bypass path policy. Pin actual image caps/SID.
4. A noninteractive core fixture can use genuine RDebug output for harness
   collection, avoiding a **fixture** dependency on console input. If RTest or
   CConsoleBase output is selected, genuine econs/ewsrv and its dependencies
   from the independent console review are additional requirements; a small
   request list does not authorize a private/write-only console. Visible manual
   output still needs the selected real route. No window GUI, network, generic
   SDK provisioning or R3 supervisor is inferred for the file assertions.
5. Dedicated CActive scheduler positive is a separate executable/gate; async
   file completion can first be tested with User::WaitForRequest. No demand to
   modify unchanged t_cactw or make its success depend on deliberate child panics.

Small implementation/test ownership proposal (interfaces, **not new ABI**):

| Owner slice | Required boundary | Bounded proposed tests |
| --- | --- | --- |
| FS transport/lifecycle | typed message copy; version; session-local path/index/type; reserve/unwind open+close resources; client panic; async message ownership | raw 4/8-byte replies/canaries; failed-open cleanup; invalid index/type child; two sessions and collision-locality; client/server death |
| FS operation semantics | scalar/package decoding; share promotion/demotion; per-share position; validation stages; exactly-once queue/cancel | both-order share matrix; F-3; shrink/regrow discriminator; public/raw shortcut separation; targeted/all/no-match cancel and controlled races |
| FAT adapter | offset read/write; resize; metadata; allocate/free; backend max; explicit flush and noncollapsed errors | 64-KiB boundary + proposed 1 MiB; retained prefix; disk-full/media injection; flush-failure visibility; two-boot persistence |
| EUSER/loader integration | genuine request status, heap/TLS, waits, child logon/panic and import identity | exact child categories; live observer progress; separate scheduler positive; unsupported/missing import fails with cleanup |
| Harness/manual | isolated image/log names; deterministic bytes; explicit service launch; preserved second boot; stop/cache record | required automated persistence plus later owner PC boot/launch/output/persistence/shell recovery on confirmed disposable media |

No FAT private API is frozen here. Suggested adapter operations are read-at,
write-at, resize, stat-size/metadata, lookup/create and flush with explicit errors;
partial-write/error atomicity and maximum supported size must be documented by
the implementation owner, not inferred from a success-only wire table.
`P/docs/fat_paths_R1.md:105-177` inventories existing snapshot/commit hazards;
they are not a mandate for a new service to return old internal codes unchanged.
Legacy PortFile snapshot, disk-full preservation, pilot loader and all existing
stable regressions remain required on the eventual combined tree. No migration
or rewrite of those paths is part of this card.

## 14. Deferred checks, blockers and handoff

Coordinator adoption items, not owner policy questions:

- Adopt/correct the exact matrix replacements above; explicitly settle the
  source-versus-comment SetSize position qualification, unspecified growth
  contents, small probe budget and synthetic test-backend scope.
- Pin kernel security flags and actual FS/cache/plugin/media baseline. The named
  EFSRV variant alone cannot resolve these runtime parameters.
- Review new CLEAN-BUILD artifact evidence independently; capture real fixture
  frames, layouts, panic exits, exactly-once completion and lifetime behavior
  after implementation. This review requires no prior successful guest run to
  authorize the separately reviewed implementation contract.
- Genuine loader/startup, chosen console bring-up and R1 FS/FAT are still
  implementation/runtime gates, not facts established by source review. Full
  malformed-input ABI/security coverage and every media/error precedence remain
  explicitly outside this bounded result. sfat evidence is upstream reference
  behavior, not proof that current port variant/fat32.cpp is that implementation.

Manual draft: after integrated QEMU regressions pass, run the same bounded fixture
on owner-confirmed disposable PC media; record ordinary positives and exact
negative child signatures separately, visible byte/EOF/missing-file/cancel
results and independent shell recovery; Flush/Close, reboot and compare persisted
bytes. No crash/disk-full stress on the reference system disk; no assumption
that PC media is the host's QEMU image. No physical execution occurred.

```text
Card / base commit / owned branch:
  R1-FILE-CONTRACT-REVIEW / fa62323eea7bc96cefd666e40f5189567731bd44
  detached /tmp/opencode/r1-file-contract; current main dirty reports read explicitly
Contract revision and dependencies:
  proposed file-contract review v0, port-x86-rfile64-v1; not frozen
  upstream 0c3208650587ac0230aed8a74e9bddb5288023eb
What researched/changed:
  qemu-port/docs/r1_file_contract_review_v0.md only
  ten flags, exact matrix replacements, ordinary/RFile64 framing, panic categories,
  staged precedence, SetSize conflict, async/death ownership and FS test split
Tests run:
  none; source inspection and report-only structural QC, no builds/E32/QEMU
Manual step drafted/updated:
  this report section 14 only; no shared manual edit, no hardware claim
Compatibility and failure limits:
  source-settled versus binary/config/runtime deferred explicitly separated
  legacy snapshot/disk-full/pilot/stable-suite requirements preserved
  no SDK search/provisioning, upstream edit, shared-doc edit, commit or push
Blocked assumptions/requests:
  coordinator adoption of SetSize position qualification and proposed probe scope;
  kernel security flags + selected FS/cache/media; implementation then runtime checks
Recommended integration order and next owner:
  coordinator reviews/adopts replacement text and reconciles matrix v1 ->
  FS/FAT + transport/lifecycle implementation under reviewed contract ->
  matched genuine fixture / raw negative harness -> serialized QEMU tests +
  combined stable suite -> required two-boot persistence -> owner PC observations
```

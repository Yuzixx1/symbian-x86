# R1-WIRE/v0 — RFs/RFile request contract research

Status: **source-evidenced candidate, NOT FROZEN; no binary/runtime verification**.
Date: 2026-10-21. Port base: `fa62323eea7bc96cefd666e40f5189567731bd44`.
Upstream: `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean at inspection.
Owned artifact: this report only, in detached worktree `/tmp/opencode/r1-wire`.
The worktree has a read-only-reference symlink to the owner's `kernelhwsrv`;
the symlink is not a deliverable. Current uncommitted planning documents were
read from the owner's workspace, not inferred from the older tracked baseline.

This feeds R1-ABI/v1 and R1-FS. It does not authorize service implementation.
Evidence paths below use `K = kernelhwsrv/kernel/eka/` and
`F = kernelhwsrv/userlibandfileserver/fileserver/`, relative to the workspace.
Line references refer to the upstream revision above.

## 1. Findings requiring changes to the ABI candidate

1. **`EFsFileSubClose = 28`, not 27.** 27 is directory close. The enumeration
   in `F/inc/F32plugin.h:42-105` and dispatch in `F/sfile/sf_ops.h:55-70`
   agree. Correct `r1_abi_contract_v0.md` during integration; it was not edited.
2. `RFile::Size(TInt&)` uses an **8-byte TInt64 output package**, function
   `0x10028`; `RFile::Seek(TSeek,TInt&)` uses an **8-byte TInt64 slot-2
   package**, function `0x40026`. Public 32-bit parameters do not establish
   package width (`F/sfsrv/cl_file.cpp:1429-1548`).
3. F32 function flags and `TIpcArgs.iFlags` are separate namespaces. A filename
   descriptor does not require `KIpcArgSlot0Desc` in its function number.
4. Open returns its handle through writable descriptor **slot 3**, with
   completion `KErrNone`; subsequent requests carry the integer handle in
   slot 3. The handle belongs to a session, not a global file number.
5. EOF for ordinary file reads is success with zero bytes, not `KErrEof`.
   Invalid subsession handles generally **panic the client**, rather than
   returning an ordinary `KErrBadHandle` completion.
6. `SetSessionPath` parses/checks policy and stores a path without checking
   directory existence. PortFile's validated process CWD is not this contract.

## 2. Connection and EUSER transport

`RFs::Connect` calls `CreateSession("!FileServer", Version(), slots)` directly;
it does not start the service. Default slots are -1, dynamic pool allocation.
For fixed slots the client documentation explicitly reserves capacity for a
cancel request (`F/sfsrv/cl_cli.cpp:219-245`; `F/inc/f32file.h:100`).
Version is 2.0.4011 (`cl_cli.cpp:318-328`, `F/inc/f32ver.h:35,48,61`).

Connection is a kernel/session operation, not an F32 opcode in the table below.
`CServer2::DoConnectL` obtains the packed `TVersion` from `RMessage2::Int0`,
constructs the session, calls `CreateL` and installs the session pointer
(`K/euser/cbase/ub_svr.cpp:424-434`). `CServerFs::NewSessionL` uses
`User::QueryVersionSupported`, leaves `KErrNotSupported` on rejection, and
initializes the path to `<system-drive>:\Private\<client-SID-8-hex>\`
(`F/sfile/sf_main.cpp:108-126`). Do not hardcode C:\ as the upstream default.

`F/sfsrv/cl_shim.cpp:24-45` delegates RFs/RFile calls to EUSER; it does not
introduce another framing format. `K/euser/us_mes.cpp:90-127` copies slots
0–2, replaces slot 3 with `TPckgBuf<TInt> reply`, and saves the returned handle
only after success. Failed creation clears the local handles. Normal
subsessions retain a no-close reference to the parent session. Closing a file
does not close RFs; auto-close subsessions are a separate EUSER facility.

`us_mes.cpp:200-267`: close sends the handle in slot 3 then clears it locally;
repeated public Close on an already-cleared handle sends nothing. Normal
subsession calls copy slots 0–2, retain only their nine type-flag bits, and put
the handle in slot 3 as an untyped integer. Never mark this server-owned
subsession index as an `EHandle` kernel handle.

Sync calls return the completion code; async calls set `KRequestPending` and
use the request-status address as transport metadata. Send failure completes
that status too (`us_mes.cpp:37-85`). The status is not generally a fifth
argument or a file-request slot. `MessageClientStatus()` returns NULL for a
synchronous message (`K/kernel/sipc.cpp:1927-1932`).

## 3. Descriptor ABI and two independent flag words

`K/include/e32cmn.h:6318-6383,6511-6520` defines four TInt argument words,
followed by TInt flags: expected 20 bytes on this 32-bit x86 target, still to
be checked in the selected toolchain's compiled artifact. Type per slot is
`(flags >> (3*slot)) & 7`:

| Value | Type |
|---|---|
| 0 | integer/untyped pointer, including request-status identity |
| 1 | kernel handle (not file subsession handle) |
| 4 / 5 | mutable 8-bit / 16-bit descriptor |
| 6 / 7 | const 8-bit / 16-bit descriptor |

Pin flags occupy bits 12–15. Default construction initializes flags only;
`ENothing` does not guarantee zero argument words. `Set` overloads **OR**
type bits rather than clearing prior types (`K/include/e32cmn.inl:6689-6800`).
Construct fresh typed arguments; do not reuse a descriptor slot as integer
without clearing its old flags.

A descriptor argument points to a descriptor header, not to raw bytes or a
NUL-terminated string. Header upper four bits are descriptor type, lower 28
length (`K/include/e32des8_private.h:27`, `e32des8.h:24`,
`u32std.h:119`; `K/kernel/sutils.cpp:3732-3774`):

| Header type | 32-bit words / data location |
|---|---|
| EBufC=0 | type/length; inline data follows |
| EPtrC=1 | type/length, data pointer |
| EPtr=2 | type/length, maximum length, data pointer |
| EBuf=3 | type/length, maximum length; inline data follows |
| EBufCPtr=4 | type/length, maximum length, backing descriptor pointer |

Widths come from IPC type flags; lengths of 16-bit descriptors are character
counts. RFs paths are 16-bit, file contents and scalar packages are 8-bit.
`TPckg<T>` wraps `sizeof(T)` bytes (`K/include/e32cmn.inl:2540-2588`). Thus
handle reply is four bytes, while Size/Seek replies here are eight bytes.
Use genuine message read/write facilities rather than dereferencing client
pointers in a service. Kernel parsing rejects bad layouts; message copy checks
descriptor typing, direction and width (`K/kernel/sipc.cpp:1941-1961`). Some
const/width rejection is conditional on platform-security diagnostics policy;
record the built configuration for negative tests. Descriptor capacity overflow
is checked in `K/kernel/sthread.cpp:1500-1525`.

Separately, `F/inc/f32file_private.h:61-92` defines function flags
`D0=0x10000`, `D1=0x20000`, `D2=0x40000`. These select scalar-versus-package
interpretations for specific operations. `CFsRequest::IsDescData` tests
function bit `16+slot`, not IPC argument flags (`F/sfile/sf_std.inl:226-241`).
Ordinary Read's data descriptor still has function 34 without D0.

## 4. Request/response table (R1-WIRE/v0)

Notation: `in16` const path descriptor; `out16` mutable path descriptor;
`in8`/`out8` const/mutable binary descriptor; `pkg32`/`pkg64` mutable 8-bit
packages of exactly 4/8 bytes; `h` session-local subsession integer; `—`
unused/unspecified. All numbers are decimal unless prefixed `0x`.
Responses are completion codes, not byte counts or handles, unless explicitly
noted. Read length comes from the output descriptor length.

| API / function | Slot 0 | Slot 1 | Slot 2 | Slot 3 | Success output / dispatch |
|---|---|---|---|---|---|
| RFs SessionPath / 14 | out16 | — | — | — | path; TFsSessionPath |
| RFs SetSessionPath / 15 | in16 | — | — | — | none; TFsSetSessionPath |
| RFs MkDir / 16 | in16 | 0 | — | — | none; TFsMkDir |
| RFs MkDirAll / 16 | in16 | 1 | — | — | same handler, recursive mode |
| RFs RmDir / 17 | in16 | — | — | — | none; TFsRmDir |
| RFs Delete / 19 | in16 | — | — | — | none; TFsDelete |
| RFs Rename / 20 | old in16 | new in16 | — | — | none; TFsRename |
| RFs Replace / 21 | old in16 | new in16 | — | — | rename/replace operation, not RFile Replace |
| RFile Close / 28 | — | — | — | h | none; TFsSubClose; public API returns void |
| RFile Open / 30 | in16 | mode | — | pkg32 | new h written in slot 3 |
| RFile Create / 31 | in16 | mode | — | pkg32 | new h; TFsFileCreate |
| RFile Replace / 32 | in16 | mode | — | pkg32 | new h; TFsFileReplace |
| RFile Read / 34 | out8 | requested length | -1=current or position | h | bytes, descriptor length; TFsFileRead |
| RFile Write / 35 | in8 | requested length | -1=current or position | h | none; TFsFileWrite |
| RFile Lock / 36 | position | length | — | h | none; TFsFileLock |
| RFile UnLock / 37 | position | length | — | h | none; TFsFileUnlock |
| RFile Seek / `0x40026` | signed offset | seek mode | pkg64 | h | new position except ESeekStart |
| RFile Flush / 39 | — | — | — | h | flush result; TFsFileFlush |
| RFile Size / `0x10028` | pkg64 | — | — | h | size; TFsFileSize |
| RFile SetSize / 41 | signed size | — | — | h | none; TFsFileSetSize |
| RFile ReadCancel / 120 | status pointer or NULL | — | — | h | cancel request succeeds separately |

Client evidence: `F/sfsrv/cl_cli.cpp:1480,1522,1599,1649,1703,2098,2165,2220`;
`F/sfsrv/cl_file.cpp:473-649,697-1353,1390-1582`.
Server mapping: `F/sfile/sf_ops.h:43-70` and the EFsFileReadCancel entry;
handler implementations: `sf_svr.cpp:23-112,852-897`,
`sf_nbs.cpp:18-200`, `sf_file.cpp:193-332,640-1626,2943-2960`.
Read/Write/Flush have both sync and async public overloads with the same
function/slots; distinguish them by transport request status.

RFile64 uses the same base opcodes. For offsets needing a package, Read/Write
set D2 and use an input TInt64 slot-2 package; Seek sets D0|D2 with input
offset and output position packages; SetSize sets D0; Size remains D0.
Open/Create/Replace additionally set `EFileBigFile` in mode. See
`cl_file.cpp:1955,2004,2058,2337-2342,2787,2812,2851`.
Do not advertise RFile64 merely because the ordinary Size reply is 64-bit.

## 5. Server dispatch and object lifecycle

`CSessionFs::ServiceL` computes `function & 0xffff`, bounds-checks against
`EMaxClientOperations`, obtains `OperationArray[index]`, allocates a request
and dispatches (`F/sfile/sf_main.cpp:140-171`). Out-of-range operations complete
`KErrNotSupported`. Operation flags govern internal dispatch; `ESync` is not
an alternate client wire opcode or proof that a public call is synchronous.

`FsFileOpenL` obtains the mode, adds EFileWrite for Create/Replace, calls the
drive open and writes the four-byte handle to slot 3. Failed reply handling
uses `CheckForLeaveAfterOpenL`, so successful backend open alone is not a
completed open transaction (`sf_file.cpp:236-263`). Each session creates an
object index (`sf_ses.cpp:104-112`); lookup checks both the session and object
type (`sf_utl.cpp:595-612`). A numerically equal handle in another session
does not identify the original file.

Close reserves request resources at open time and validates object type
(`sf_request.cpp:124-229`). File close waits for outstanding fair-scheduled
writes/cache flush, removes the handle and decrements resource count
(`sf_svr.cpp:23-112`). Session disconnect closes main-thread objects and
defers final destruction until request references are released
(`sf_ses.cpp:90-111,162-170`). Implementation must cover client death and
pending requests, not just public Close.

## 6. Access, share, path and position semantics

Modes: exclusive=0, readers-only=1, any=2, readers-or-writers=3; share mask
0xff; read=0, write=0x200 (read **and** write), async-read-all=0x400.
Text=0x100 is independent (`F/inc/f32file.h:950-1132,1215`).
Create/Replace force write server-side; do not test them as read-only opens.

For an already-open file, the table is for its **effective current share
mode**, not just the first handle. X=exclusive, R=readers-only, A=any,
W=readers-or-writers:

| Existing effective mode | New X | New R | New A | New W |
|---|---|---|---|---|
| X | InUse | InUse | InUse | InUse |
| R | InUse | allow read | InUse | allow read only |
| A | InUse | InUse | allow | allow |
| W | InUse | allow only if no writer exists | allow | allow |

New R combined with write is `KErrArgument` before this matrix. Existing R
rejects a new writer with `KErrInUse`. Promotion from W to R/A and demotion
when promoted handles close are necessary (`sf_drv.cpp:827-889,1326-1391`;
`sf_file.cpp:2450-2493`). This is not a POSIX access-bit-only matrix.

Paths use session-relative parsing and native backslash semantics; wildcard
file-open names are rejected, absent name/extension is BadName
(`sf_utl.cpp:407-419,490-503`). `SetSessionPath` accepts a parsed legal,
policy-permitted directory path even if it does not exist; it does not mount
or stat that directory (`sf_utl.cpp:530-550`, `sf_svr.cpp:870-889`). Capability
checks on private/system/resource paths are real upstream behavior
(`sf_file.cpp:384-454`), not established by today's unprotected port.

Read initializes from the -1 sentinel or explicit position, clamps past-EOF
to EOF, and at EOF writes an empty descriptor and succeeds. Short reads are
successful. Successful explicit-position reads also update the share position
(`sf_file.cpp:736-745,788-848`). Negative requested length returns Argument.
Length-zero public Read overloads zero the descriptor without IPC; an oversized
requested read completes/returns Overflow client-side
(`cl_file.cpp:785-802,980-995,1050-1061`). Do not use raw zero-length IPC as an
oracle for those client-side effects.

Seek modes: Address=0, Start=1, Current=2, End=3 (`f32file.h:1300-1332`).
Current/End clamp to [0,size]. Start accepts a nonnegative position, including
beyond EOF, and does not write the reply package. The client preinitializes
that package, preserving the caller's aPos. Write then clamps a beyond-EOF
position to file size: **do not assume POSIX hole creation**
(`sf_file.cpp:936-975,1397-1469`; `cl_file.cpp:1465-1473`).
Ordinary public positioned Read/Write reject negative positions via FSCLIENT
19 panic. Wire scalar -1 remains the legitimate current-position sentinel;
other raw scalar positions are zero-extended on the server
(`sf_file.cpp:790-801,1189-1200`).

Legacy file size ceiling is 0x7fffffff (`sf_std.h:144`); writing across it
returns TooBig for non-big-file shares (`sf_file.cpp:947-958`). The
`SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API` configuration affects the client
Size overflow check (`cl_file.cpp:1543-1546`). Freeze that configuration with
R1-TOOLCHAIN; near-4-GiB FAT capability is a separate RFile64/backend question.

## 7. Error and asynchronous completion contract

| Scenario | Source-backed outcome / evidence |
|---|---|
| Open absent leaf, parent exists | NotFound (-1), `F/sfat/sl_mnt.cpp:1427-1471` |
| Missing directory path | PathNotFound (-12), FAT path traversal `sl_mnt.cpp:1538-1571`; propagated by FileOpen |
| Create existing regular file | AlreadyExists (-11), `sl_mnt.cpp:1455-1463`; already-open path also `sf_drv.cpp:1383-1388` |
| Open directory as file, write read-only file | AccessDenied (-21), `sl_mnt.cpp:1458-1463` |
| Positive-length write through read-only handle | AccessDenied, `sf_file.cpp:1213-1221`; zero length completes earlier |
| Sharing conflict | InUse (-14), matrix above |
| ReadersOnly + Write, incompatible async-all share | Argument (-6), `sf_drv.cpp:1351-1360` |
| Ordinary Read at EOF | None (0), descriptor length 0, `sf_file.cpp:814-848` |
| Unknown opcode outside operation table | NotSupported (-5), `sf_main.cpp:146-152` |
| Bad/stale/cross-session subsession handle | client panic category `Panic`, reason BadHandle (-8), `sf_request.cpp:1438-1447`; close allocator path `sf_main.cpp:158-164` |
| Negative public positioned Read/Write | FSCLIENT 19 before sending, `cl_file.cpp:980-995,1300-1312` |
| Negative SetSize | positive ESizeNegative leave converted to client panic; `sf_file.cpp:1574-1575`, `sf_request.cpp:1135`; documented FSCLIENT 20 in `cl_file.cpp:1570-1575` |
| Canceled waiting async read | Cancel (-3), `sf_file.cpp:2539-2552,2928-2960` |
| Server death | ServerTerminated (-15), kernel detach `K/kernel/sipc.cpp:736` |

Numeric error definitions are in `K/include/e32err.h`. Error precedence
matters: an invalid mode, path-policy failure or mount error can occur before
the filesystem checks above. DiskFull, media failures, lock conflicts and
failed descriptor copies must propagate; no all-errors-to-NotFound mapping.
This table does not assert exhaustive precedence for malformed raw messages.

Async Read/Write/Flush must preserve status lifetime and exactly-once
completion. Normal async reads can complete short at EOF. With
`EFileReadAsyncAll` and an async client status, insufficient data queues the
read until growth (`sf_file.cpp:824-833`); writes/SetSize notify readers.
ReadCancel is function 120 with a raw status address (or NULL for all reads
of that share), not a descriptor and not receive cancellation. It completes
matching stored read messages with Cancel; the cancel message independently
returns None. Public targeted cancel skips IPC if status is no longer pending
(`cl_file.cpp:1068-1093`; `sf_file.cpp:2539-2552,2914-2960`). No generic
WriteCancel/FlushCancel wire operation was established; do not invent one.

Per-share read/write request serialization is attempted, with a plugin caveat
explicit in source (`sf_file.cpp:778-781,1182-1185`). Flush waits for in-flight
writes, flushes dirty cache and calls filesystem FlushDataL
(`sf_file.cpp:1481-1512`). Close's void public return hides flush errors;
use explicit Flush to observe errors (`cl_file.cpp:1479-1520`). Neither this
trace nor R1 promises atomicity under power loss.

## 8. Acceptance vectors and outstanding freeze gates

These are **test specifications, not executed regressions**. Build a client
with genuine EUSER/EFSRV and the selected matching x86 toolchain, and a raw
transport fixture for malformed messages. Use fresh disposable image/log
names under `/tmp/opencode/r1-wire-*` in a reserved QEMU slot.

1. Capture Open: function 30, path type 7, mode integer, slot-3 type 4,
   flags 0x807, four-byte reply h and completion 0. Capture Read flags 4,
   Write flags 6, integer slot-3 h; Size function 0x10028/flags 4 and
   eight-byte reply; Seek function 0x40026/flags 0x100 and eight-byte reply.
   Compare actual built-library frames; reject a guessed four-byte Size.
2. Create/write 65537 deterministic bytes, Flush, Close, reopen, Size=65537,
   read at 65535 for two bytes, then current Read gives length zero/result 0.
   Reboot and compare bytes. Record completion codes, lengths and SHA-256.
3. Seek Start to size+10 succeeds; a subsequent one-byte Write appends at
   EOF (size grows by one). Seek Current/End beyond bounds clamps. Positioned
   Read at 10 of length 3 sets current position to 13. Test zero-length and
   over-capacity public Read separately from raw wire requests.
4. Run all share-table pairs in both open orders on an existing regular file,
   including W promotion/demotion after close. Assert Argument for R|Write,
   InUse for incompatible opens and successful open after holders close.
5. Two RFs sessions set different paths and open relative names. Set a legal
   nonexistent session directory successfully, then observe PathNotFound
   on a file operation there. Use a path permitted to the client's SID.
6. Child-process negative tests: forged handle and wrong-type close panic
   `Panic/-8`; negative public position panics FSCLIENT/19; server and another
   client survive. Exhaust handle-reply capacity and verify cleanup after
   failed open; record descriptor error/panic behavior before freezing it.
7. Open async-all reader with shared mode and a separate writer. Queue two
   reads with distinct statuses, cancel one specifically (Cancel exactly
   once), grow file to satisfy the other, then cancel-all. Repeat cancellation
   racing completion; retain buffers/statuses until completion. Fixed-slot
   test must leave a spare cancel slot. Kill server with a pending read and
   expect ServerTerminated; reconnect and repeat after restarting service.
8. Check explicit Flush error reporting and session/client-death cleanup,
   resource baseline after repeated failed opens, stale handles, and pending
   reads. A Close-only persistence test cannot certify flush error handling.

Manual step for later R1 integration: on the owner's disposable FAT test
volume run the matched-library fixture, photograph reported byte comparison,
EOF, missing-file and async-cancel results; reboot and repeat the byte check.
Physical execution is blocked on the implementation/toolchain gates.

**Remaining decisions:** R1-TOOLCHAIN must pin the exact EFSRV/EUSER build,
compiler ABI, feature macros and import metadata; R1-PROOF-SOURCE must supply
the actual app operation/dependency graph. This report covers core RFs/RFile,
not packed TEntry/RDir layout, RFs GetDir sorting, notifications, plugins,
handle transfer or every filesystem/security error path. If the selected app
uses these, expand the contract before freeze. SetSize position behavior
(public comment versus share/backend handling), malformed package lengths,
full path normalization and lock/error precedence need targeted differential
tests before broad compatibility claims. Today's PortFile CWD, 8.3 creation
and 64-KiB snapshot semantics are not substitutes for these interfaces.

R1-FS needs session-local handles/path, typed IPC copy, request-status identity,
panic support and async lifetime management. R1-FAT needs offset I/O, sizes,
metadata/errors and explicit flush. R1-EUSER/STARTUP must supply genuine
status/wait/scheduler behavior. Integrator reviews these against the other
research reports and issues R1-ABI/v1; no card is frozen by this document.

## 9. Verification and handoff

Source-only research; no build, QEMU regression, hardware test or media write.
No service code or upstream edits. Existing suites were not run; they cannot
validate an unimplemented RFs service. Verification commands from the owner's
workspace (no persistent log; output retained in the agent session):

```sh
git -C kernelhwsrv rev-parse HEAD
git -C kernelhwsrv status --short
git -C /tmp/opencode/r1-wire diff --check
git -C /tmp/opencode/r1-wire status --short
```

Results: upstream revision matches section 1, upstream status empty; diff check
clean (the new report is untracked, so this alone does not lint it). A Python
source assertion additionally parsed TFsMessage, checked Close=28, Open=30,
Read=34, Write=35, Seek=38, Size=40, ReadCancel=120 against sf_ops.h entries,
checked masked function/flag constants and checked report trailing whitespace
and balanced fences: PASS. These checks do not establish runtime framing.
Worktree status contains only the untracked reference symlink and new report.
The report can be integrated by copying this single new file after review;
no commits were created.

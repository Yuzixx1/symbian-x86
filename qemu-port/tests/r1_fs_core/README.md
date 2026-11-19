# R1-FS-CORE: private first-stage candidate

**Host-tested portable logic; EUSER source compile-only. NOT linked, launched,
guest-tested, QEMU-ready or hardware-verified.** Detached worktree:
`/home/yuzi/.cache/sybian-r1/r1-fs-core-worktree`, port HEAD
`fa62323eea7bc96cefd666e40f5189567731bd44`; genuine upstream read-only
reference `kernelhwsrv` `0c3208650587ac0230aed8a74e9bddb5288023eb`.
Only `qemu-port/fs/` and this test directory are owned. No SDK-root changes,
target link, guest execution, shared-host integration, commit or push.

## Coordinator / FAT-channel handoff

The concrete **C++98** injection contract is `qemu-port/fs/r1_fs_backend.h`.
`Backend` is an interface, not an implemented channel. It takes absolute
canonical UTF-16 `Path`, returns generation-bearing `Key`, authoritative
`Meta.size`, and `IoResult { error, committed, size, stable }`; `Lookup`,
`Open`, `ReadAt`, `WriteAt`, `Resize`, `Flush`, `Close` are required. Byte calls
are <=4096; length, offset, generation, volume and path validation remain
backend duties, including **per-request** pinned-host authorization. Reject
unknown names/layouts, other volumes, unauthorized channels and sysbin write;
do not truncate names or return synthetic durable Flush. An unsuccessful
Create/Replace may have modified the directory; failed handle-reply abort
releases the open key, **not** the created entry. `stable=false` poisons all
shares on that canonical path until close; no remount/recovery is claimed.

`r1_fs_core.cpp` is a fixed-budget single-FS-thread state owner: 16 sessions,
64 global shares, monotonic non-reused handles, one mounted drive selected
before Connect, public `<drive>:\\R1TEST\\...` data path only. It tracks SID
private default path without creating it; `SetSessionPath` validates/policies
without stat. File read/write/resize/flush, per-share positions, share
promotion/demotion, reply-copy abort, close/disconnect and decoder errors
are portable. A first-stage safe subset deliberately rejects other APIs,
AsyncAll, read cancel queue semantics and single transfers >1 MiB; no fake
success. ASCII-only path normalization is an explicit restriction; non-ASCII
F32 filenames, named metadata and generic namespace are **not supported**.

`r1_fs_server.cpp` uses genuine `CServer2`/`CSession2`, validates client F32
version, derives default path SID from `RMessage2::SecureId`, receives typed
descriptors with `RMessage2::Read/Write` and uses four-byte handle replies and
eight-byte size/seek/position packages. Bad file handle explicitly panics
`Panic/-8`; failed four-byte reply overflow aborts and panics
`FSCLIENT/-22`; negative valid writable size panics `FSCLIENT/20`.
Operations are synchronous on the registered owner thread. The server does
**not** register the thread, launch another server or own E32Main: shared host
integrator first authenticates/opens the backend, creates `FileServer::NewL`,
then calls `UserSvr::FsRegisterThread` only at B2 and `Publish()` at B3,
signals FS-ready before `!Loader`, and only then allows public clients.
`FileServer::State().SetDrive(...)` must be called before connection. Never
publish during B1. Post-registration service death is whole-system failure,
not a restart test. `r1_fs_core.mmp` is a proposed host MMP, **not** an
independently linkable EXE; the coordinator supplies entry/backend and
reconciles its placeholder UID3/process identity. The six capabilities
mirror genuine `sfile/group/f32caps.mmh` (TCB, ProtServ, DiskAdmin, AllFiles,
PowerMgmt, CommDD); shrink against actual shared-host imports/security
policy before integration. The fixture's capability remains NONE.

## Reproduce evidence

From the detached worktree:

```sh
g++ -std=c++98 -ffreestanding -fno-exceptions -fno-rtti \
  -Wall -Wextra -Werror -pedantic -Iqemu-port/fs \
  qemu-port/fs/r1_fs_core.cpp qemu-port/tests/r1_fs_core/core_test.cpp \
  -o /home/yuzi/.cache/sybian-r1/r1-fs-core-test
/home/yuzi/.cache/sybian-r1/r1-fs-core-test
```

Result: `R1-FS-CORE portable decoder/state PASS`. Tests execute actual core
and fake backend: malformed function flags/package/capacity, denied path,
SID default, wrong-session and stale share, failed open, handle-reply abort,
share promotions and both acquisition orders, EOF and no-hole writes,
65,535/65,536/65,537 crossing, shrink/regrow retained position, committed
prefix on disk-full, unstable backend poisoning and explicit Flush error.
No fake IPC execution is claimed. ASan/UBSan linkage is unavailable in the
host environment (missing libasan.so.8/libubsan.so.1); no sanitizer pass.

Pinned `r1-build-v1` MinGW GCC 3.4.5 via Wine, `-std=c++98 -fno-exceptions
-fno-rtti -D__X86__ -D_UNICODE -D__SYMBIAN32__ -D__EPOC32__ -D__X86GCC__
-D__GCC32__`, preinclude `epoc32/include/gcc.h`, include
`epoc32/include` and `epoc32/include/platform`: **both**
`r1_fs_core.cpp` and `r1_fs_server.cpp` compiled to COFF objects at
`/home/yuzi/.cache/sybian-r1/r1-fs-core-final.o` and
`r1-fs-server-final.o`. This is compile-only; no EUSER link or target execution.

## Explicit exclusions / integration gates

* No FAT channel implementation, sector I/O, mounted-volume authorization,
  power-loss safety, cache flush proof or backend concurrency test. Direct
  shell/legacy FAT mutation of live service files must be excluded by staging
  policy; a FAT lock alone does not give coherent share state.
* Synchronous EUSER calls can complete via ordinary server scheduling; no
  retained `RMessage2`, targeted/all cancel queue, asynchronous read-all or
  death-race contract. AsyncAll mode is `NotSupported`, invalid AsyncAll|X/R
  is `Argument`; ReadCancel is `NotSupported` on a valid share. A future async
  owner must add exactly-once queue/cancel/close cleanup before advertising it.
* Console/ESHELL calls are **not** handled here: no console session, input,
  window-server/keyboard IPC, econs/ewsrv, or replacement shell success.
  Genuine ESHELL requires the staged console and loader dependency closure;
  first file-fixture RDebug output does not establish visible-console success.
* Six-driver grouping (heap, scheduler, file data, file semantics, negative
  child/panic, persistence/observer) remains a coordinator proposal, not a
  completed suite. Exact IPC canaries, client child panic signatures,
  independent observer, stable regression suite, two-boot preserved-image
  Flush/Close persistence, QEMU run and later manual hardware launch/reap,
  visible output and shell recovery all remain **unverified**.
* Backend/Open stage ordering for compound errors, true F32 Unicode/path
  behavior, FAT share identity aliases, `RFile64` >2GiB and error-injected
  partial-transfer equivalence require integration review. Bounded fixture
  source is not a drop-in general file server.

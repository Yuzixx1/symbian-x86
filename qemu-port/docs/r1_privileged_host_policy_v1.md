# R1-PRIVILEGED-HOST-POLICY v1 — trusted portfilesrv.exe host policy

2026-10-28. Card 29 — **strong; source/review only.** No implementation, build,
QEMU or runtime. Returns concrete declarations and the exact remaining decisions
for coordinator integration; no broad architecture inventory. No commits/pushes.
No QEMU slot allocated.

Owns `qemu-port/docs/r1_privileged_host_policy_v1.md` and the reviewed shared
host/bootstrap declarations in `qemu-port/services/r1_service_state.h`. Builds
on `r1_service_boundary_v1.md` (B0–B4 phase gating), the loader contract
candidate and the console closure evidence (cards 24/26/28).

## 1. Purpose

Resolve how `portfilesrv.exe` becomes the trusted registered host without
removing the bootstrap zero-cap-root guard. One trusted G-format `portfilesrv.exe`
user process hosts two `CServer2` instances on different owner threads: the FS
thread owns `!FileServer` and the loader thread owns `!Loader`. Only the FS
thread calls `UserSvr::FsRegisterThread()`, exactly once, making the whole
process trusted and system-permanent. Loader access after registration is
process-scoped; the loader thread does **not** call `FsRegisterThread`.

## 2. Trusted host image — pinned provisioning and hash verification

`portfilesrv.exe` is provisioned at B0 from staged, byte-verified `sys\bin`
image bytes under kernel control, not downloaded or built at boot:

* Image placement: `C:\sys\bin\portfilesrv.exe` on the recorded first-boot
  system drive (fixed drive, normalized under the effective sysbin rule).
* Staged inputs: `HAL.DLL`, `EKDATA.DLL` and `EKDATA.00` are byte-identical
  copies of the accepted card-24/26 images, copied from the verified staging
  directory by card 28's `stage_r1_g.py`/manifest. Default builds remain
  asset-free; no redistribution permission.
* The B0 kernel adapter validates image security metadata (UID/SID/caps,
  header CRC, UID checksum, zero writable DLL data where required) and the
  import closure against pinned provider hashes before materialize. A hash or
  security mismatch aborts B0 with `KErrCorrupt`; no service registration and
  no client release.
* The manifest pins `portfilesrv.exe` UID3/SID/caps, import closure and
  process-launch handoff **from the emitted image**, not from filenames.

The pin covers only the trusted host and its first static dependencies. Public
fixture data stays under `<drive>:\R1TEST\...`, not `sys\bin`; public FS must
forbid writes to the trusted executable namespace. Writable removable/un-audited
media fails closed for executable loading.

## 3. Process and thread ownership

| Owner | Responsibility |
| --- | --- |
| FS owner thread (one thread in `portfilesrv.exe`) | Starts private FAT channel at B1; allocates `!FileServer` resources at B1 (not started); calls `FsRegisterThread()` exactly once at B2; starts `!FileServer` receive and publishes FS-ready at B3. |
| Loader owner thread (separate thread in same process) | May be created at B1 but does not register `!Loader`; initializes G graph/object adapters, snapshot/search policy and message transaction state; creates `!Loader`, starts its receive loop and publishes loader-ready only at B4. |
| Bootstrap integrator | Creates the trusted process suspended at B0, resumes it at B1, owns the single monotonic phase transition and wakeup per phase, and pins channel authorization to the exact `DProcess*` object (lifetime reference, immutable process identity/generation). |

## 4. B0–B4 phase gating

The phase acknowledgement is an internal launcher/host handshake, **not** a
new public RFs/loader opcode. Bootstrap owns a single monotonic phase transition
and wakeup per phase; repeated or out-of-order phase reports fail closed.

| Phase | Boundary and exit |
| --- | --- |
| **B0 — kernel bootstrap** | Mount trusted FAT, choose a recorded system drive, initialize the dedicated private FAT channel device (not a user channel handle). G snapshot/parse/plan/materialize trusted `C:\sys\bin\portfilesrv.exe` and dependency closure through the kernel adapter; validate image security and imports; create process suspended; pin channel authorization to that **specific process object**, not name/SID alone. No pilot fallback. Failure: release pin, close all objects/snapshots, no service registration or client release. |
| **B1 — unregistered host** | Resume trusted host with genuine EUSER/H0 startup. FS owner thread starts private FAT channel and initializes path/share/index state, then creates `!FileServer` but **withholds** its receive/readiness and any public connection until B2. Loader worker may be created but does not register `!Loader`. No ordinary client or fixture is launched; no externally visible success rendezvous. Fail before registration: close service handles/channel and abort host under bootstrap supervision; never publish a partial phase. |
| **B2 — commit authority** | FS owner thread (not an arbitrary client/loader thread) calls `UserSvr::FsRegisterThread()` exactly once **after** private FAT startup/verified readiness. This makes the entire process trusted and system-permanent. There is **no recoverable kill/restart after this point**. On registration or subsequent initialization failure, report fatal boot failure; do not return success or substitute an unregistered host. |
| **B3 — public FS-only** | Activate FS receive loop and publish **FS-ready** only after B2. Kernel launcher may now release only the first approved G fixture or internal clients. `RFs::Connect` itself does not start the server. Loader **remains unregistered**. Future loader thread may use internal FS snapshot methods without a public session, avoiding an RFs/loader startup cycle. `!FileServer` `Start` is **not** called at B1 — `CServer2::Start` publishes the kernel server name and clients can race a withheld receive. |
| **B4 — loader-ready** | On separate loader thread, initialize G graph/object adapters, snapshot/search policy and message transaction state; only then create `!Loader`, start its receive loop and publish **loader-ready**. Loader does **not** call `FsRegisterThread`. Console/ESHELL launch requires B4 plus later FS op/staging preflight; it is not implied by B3. |

`CServer2::Start` on `!FileServer` is deferred to B3; before then "creates" means
allocate `CR1FsServer` and local resources only. There is deliberately no
interval where a *published* loader session accepts requests without
registration.

## 5. `FsRegisterThread` exactly once — genuine kernel semantics

The registration maps to the genuine kernel exec
`ExecHandler::FsRegisterThread` (`K/kernel/sexec.cpp:2217-2233`):

```cpp
void ExecHandler::FsRegisterThread()
    {
    DThread* pT = TheCurrentThread;
    DProcess* pP = pT->iOwningProcess;
    if (K::TheFileServerProcess && K::TheFileServerProcess!=pP)
        K::PanicCurrentThread(EAccessDenied);
    pP->iFlags |= (KThreadFlagProcessCritical | KProcessFlagSystemPermanent);
    pT->iFlags |= KThreadFlagSystemPermanent;
    K::TheFileServerProcess=pP;
    K::ThreadEnterCS();
    pP->SetPriority(EPriorityFileServer);
    M::FsRegisterThread();
    K::ThreadLeaveCS();
    }
```

Flags (genuine definitions, `K/include/u32std.h`):

* `KThreadFlagProcessCritical = 0x00000001` — thread panic panics the process.
* `KThreadFlagSystemPermanent = 0x00000008` — thread exit of any kind reboots the entire system.
* `KProcessFlagSystemPermanent = KThreadFlagSystemPermanent` — process exit of any kind reboots the entire system.

After `FsRegisterThread`:
* The process carries `KThreadFlagProcessCritical` (process-local) and
  `KProcessFlagSystemPermanent`.
* The registering thread carries `KThreadFlagSystemPermanent`.
* `K::TheFileServerProcess` points at this process; any other process attempting
  registration panics `EAccessDenied`.
* Priority is set to `EPriorityFileServer` (750).

`K::CheckFileServerAccess` (`K/kernel/sutils.cpp:2814`) then panics unless the
caller is the kernel or `K::TheFileServerProcess`. Protected `!` server names
require `ECapabilityProtServ` (`K/kernel/sipc.cpp:837`).

Declaration (in `qemu-port/services/r1_service_state.h`):

```cpp
// Call exactly once at B2 by the FS owner thread, after private FAT
// startup/verified readiness and before !FileServer Start. Returns KErrNone only
// on successful registration; any failure reports fatal boot failure.
TInt RegisterFsThreadOnce();
```

## 6. Death is failure — not restart, not substitute

After B2 the process is process-critical and system-permanent. There is no
recoverable kill/restart:

* If the production host dies after registration, treat it as **whole-system
  failure** and collect boot/fault diagnostics; do not kill/restart for ordinary
  A-2 and do not remove permanence.
* Ordinary client death / invalid file index remains isolated to its child
  (FS candidate §§2,4); it is not production host death.
* A transport test may run an independent **noncritical, nonregistered** IPC
  service with genuine `CServer2`/sessions to observe pending live client
  `KErrServerTerminated` and controlled restart. This tests generic kernel
  transport only — never register the test service, never use `!FileServer`/
  `!Loader` protected names.
* Production queued read cancel/death cleanup uses ordinary client
  death/disconnect probes only; exercise server-side failure via pre-B2 injected
  init error or fresh boot, never a kill of the permanent host.

Declaration:

```cpp
// After EHostRegistered, observing host death is whole-system failure: no
// restart, no substitute unregistered host. Before registration, host death
// aborts the bootstrap under supervision.
TInt DeathIsFailure(TR1ServicePhase aPhase);
```

## 7. Minimum trusted-host capabilities

Justified from source, **not all caps**:

* `ECapabilityProtServ` — required solely to create the protected `!FileServer`
  and later `!Loader` server names (`sipc.cpp:837`). This is the only
  host-facing capability justified by the selected design.
* No `__PLATSEC_UNLOCKED__`, no sysbin-off policy, no static all-cap grant.
  Effective forced platform-security flags are retained unless the actual linked
  configuration says otherwise (`K/include/u32std.h`).

## 8. Static EUSER/EFSRV import binding before B3 (without `!Loader`)

The G kernel adapter binds static EUSER/EFSRV fixture imports at B0 materialize
time through the kernel object adapter (`CodeSegCreate` with dependency
closure), **not** via `!Loader`:

* The required UID/SID and import closure are validated against staged
  `sys\bin` image bytes before B3.
* A missing EFSRV DLL/import is a **B0/fixture-load failure**, not something
  postponed to B4 or papered over.
* The first G fixture EXE links genuine EUSER **and EFSRV** even for `heap`/
  `sched` modes; B0 kernel G loader binds the static EFSRV import from staged,
  validated `sys\bin` before B3. It does not call `!Loader` for static imports,
  and the fixture must not run until FS-ready.
* Header CRC and UID checksum validate format, not publisher trust.
* Only DLLs with **zero writable data/BSS** are supported in the first G
  profile; writable-DLL rejection is pre-object `KErrNotSupported`.

## 9. Dynamic library publication — `KErrNotSupported` until rollback reviewed

Until genuine client-handle rollback is implemented and reviewed, dynamic
`E32Loader` library publication (`ELoadLibrary`) returns `KErrNotSupported`:

* Loader-thread `ELoadLibrary` handling is not enabled at B4 in the first
  profile; `!Loader` accepts only process-load (`ELoadProcess=1`) or returns
  `KErrNotSupported` for library loads pending the rollback review.
* The exact client-handle removal/rollback API and interleaving (genuine
  `DThread::MakeHandle`/`Close`, failed `UpdateLdrInfo`) is **not established**
  by current sources ([U] in `r1_service_boundary_v1.md §4`). The implementer
  must trace that path before integration; if no safe removal exists, prevent
  publication and return the blocker, never leak a hidden client handle.
* On failed copy/client death, close the **client-owned** handle by a reviewed
  genuine handle-owner path or abort the transaction before publication; do not
  `RHandleBase::Close` an integer in the provider's handle table.
* Build closure from immutable snapshots; don't hold the FAT lock across plan/
  object exec or send. Process creation stays suspended until its caller
  resumes; the user service never runs DLL entry points.

## 10. Channel authorization (shared with card 27)

The kernel pins the exact `DProcess*` created for the trusted B0 host, with a
lifetime reference, and requires that process on `DoCreate`, every `Request` and
`RequestUserHandle`. Store and check immutable process identity/generation —
never only a recycled raw pointer, thread ID, PID integer or SID. Validate
image SID/caps first, require `ProtServ` for protected server names, and verify
the channel's private host credential as a second check. If the process dies,
revoke/close outstanding channels and release the pinned reference; no
cloned/transferred handle to an untrusted process. Server-only SID check is
insufficient because SID can be copied into an untrusted image and the direct
model offers no isolation.

## 11. Concrete remaining decisions (coordinator)

1. **Trusted staging manifest/drive:** pin the trusted `sys\bin` staging
   manifest/drive and prohibit writable executable mutation (or explicitly
   allocate a verified hash path).
2. **Client-handle rollback:** review the exact removal API and live
   registration-failure reporting before enabling dynamic `ELoadLibrary`.
3. **UID3/SID/caps and launch handoff:** pin `portfilesrv.exe` identity and
   import closure from the emitted image, not filenames; record actual effective
   flags, paging/media attributes and absence of unsafe host-image writable
   DLLs.
4. **Phase acknowledgement mechanism:** the integrator chooses a genuine typed
   thread/process notification mechanism and owns its handle lifetime; never
   infer readiness from process resume, server-name presence or an early
   rendezvous.
5. **Integration sequence:** (1) coordinator adopts this boundary and reviews
   effective flags and the trusted image manifest; (2) isolated parser host
   tests / separately authorized fixture import audit; (3) graph fake-adapter,
   kernel G/bootstrap security propagation and FAT snapshot/channel
   authorization; (4) FS host/operations B0–B3 with static EFSRV fixture; (5)
   loader-thread/protocol B4 and dynamic lifecycle; (6) later console/ESHELL
   extension; (7) serialized combined regression and two-boot image persistence,
   then owner disposable-PC confirmation.

Neither a new security-off profile nor production kill/restart is an option.
Console closure, visible-console acceptance, translation, input, display and
hardware claims remain separately authorized and are not implied by this card.

## 12. Source changes

New files (no existing file edited):

* `qemu-port/services/r1_service_state.h` — shared phase/death/registration
  declarations (TR1ServicePhase, TR1BootReply, RegisterFsThreadOnce,
  DeathIsFailure, PhaseAdvance, capability annotation).
* `qemu-port/docs/r1_privileged_host_policy_v1.md` — this policy document.

No HAL/keymap logic, shared build file, QEMU image, target execution, commit or
push. Host parsing/declaration review cannot establish loader behavior or
console acceptance.

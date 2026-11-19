# R1-FILE-SERVICE-HOST v1 (card 31)

## Scope and ownership
Card 31 owns only `qemu-port/services/r1_service_host.{h,cpp}` and this
document. Source/declaration level only: no build, QEMU, target link or runtime
claim. Explicitly does NOT touch `qemu-port/Makefile`, `variant/r1_fs_channel.*`,
`fs/r1_fs_*` or card 30's paths. QEMU: none.

Consumes `qemu-port/services/r1_service_state.h` (card 29):
`TR1ServicePhase`, `TR1BootReply`, `RegisterFsThreadOnce`, `DeathIsFailure`,
`PhaseAdvance`, `KHostRequiredCaps`.

## B0 — pinned image materialize + static import binding
`portfilesrv.exe` is provisioned at B0 from staged, byte-verified `sys\bin`
image bytes under kernel control:

- Image placement: `C:\sys\bin\portfilesrv.exe` on the recorded first-boot
  system drive (fixed drive, normalized under the effective sysbin rule).
- Staged inputs: `HAL.DLL`, `EKDATA.DLL` and `EKDATA.00` are byte-identical
  copies of the accepted card-24/26 images, copied by card 28's
  `stage_r1_g.py`/manifest. Default builds remain asset-free.
- The B0 kernel adapter validates image security metadata (UID/SID/caps,
  header CRC, UID checksum, zero writable DLL data where required) and the
  import closure against pinned provider hashes before materialize. Hash or
  security mismatch aborts B0 with `KErrCorrupt`; no service registration and
  no client release.
- The manifest pins `portfilesrv.exe` UID3/SID/caps, import closure and
  process-launch handoff from the emitted image, not from filenames.
- Static EUSER/EFSRV imports are bound at B0 materialize time through the
  kernel object adapter (`CodeSegCreate` with dependency closure), not via
  `!Loader`. A missing EFSRV DLL/import is a B0/fixture-load failure.

## B2 — FsRegisterThread exactly once
The FS owner thread (one thread in `portfilesrv.exe`) calls
`UserSvr::FsRegisterThread()` exactly once after private FAT startup/verified
readiness. Genuine kernel exec (`sexec.cpp:2217-2239`):

- panics `EAccessDenied` if another process is already registered
- sets process flags `KThreadFlagProcessCritical | KProcessFlagSystemPermanent`
  and thread flag `KThreadFlagSystemPermanent`
- records `K::TheFileServerProcess`; sets `EPriorityFileServer` (750)

After registration the process is critical and system-permanent; there is no
recoverable kill/restart. The host declares `ECapabilityProtServ` only —
required solely to create the protected `!FileServer` and later `!Loader`
server names. No `__PLATSEC_UNLOCKED__`, no sysbin-off, no static all-cap
grant.

## B3 — deferred CServer2::Start
`!FileServer` resources are allocated at B1 but `CServer2::Start()` is deferred
to B3, after registration, so clients cannot race a withheld receive. FS-ready
is published only after B2. Loader remains unregistered; no public client
until FS-ready.

## B4 — !Loader thread ownership
On a separate loader thread: initialize G graph/object adapters,
snapshot/search policy and message transaction state; only then create
`!Loader`, start its receive loop and publish loader-ready. Loader does not
call `FsRegisterThread`. Dynamic `ELoadLibrary` returns `KErrNotSupported`
until genuine client-handle rollback is reviewed; `!Loader` accepts only
process-load (`ELoadProcess=1`) in the first profile.

## Phase acknowledgement
`PhaseAck(TR1ServicePhase, TR1BootReply&)` is the internal launcher/host
handshake — not a public RFs/loader opcode. Single monotonic transition per
phase; repeated or out-of-order reports fail closed. Never infer readiness from
process resume, server-name presence or an early rendezvous.

## Death policy
After `EHostRegistered` host death is whole-system failure: collect boot/fault
diagnostics, do not kill/restart for ordinary A-2 and never substitute an
unregistered host. Before registration, host death aborts the bootstrap under
supervision. Transport tests are limited to an independent noncritical,
nonregistered IPC service with genuine `CServer2`/sessions — never register the
test service, never use `!FileServer`/`!Loader` protected names.

## UID/SID/caps (from emitted fixture image)
`KPortFileSrvIdentity` in `r1_service_host.h` is a placeholder until the
coordinator records the emitted `portfilesrv.exe` metadata:

| Field | Placeholder | Source |
| --- | --- | --- |
| UID3 | 0 | emitted image UID3 |
| SecureId | 0 | emitted image SID |
| VendorId | 0x70000001 | matches card-28 staged console assets |
| Caps0 | `1 << 0` (ProtServ) | protected server names only |
| Caps1 | 0 | — |
| EUSER sha256 | zeros | coordinator pins from EUSER image |
| EFSRV sha256 | zeros | coordinator pins from EFSRV image |

Immutable image / zero-data requirements for `portfilesrv.exe`: no writable
DLL data where required; writable-DLL rejection is pre-object `KErrNotSupported`
(first G profile supports DLLs with zero writable data/BSS only).

## Concrete remaining decisions (coordinator)
1. Trusted staging manifest/drive: pin the trusted `sys\bin` staging manifest
   and drive and prohibit writable executable mutation (or explicitly allocate
   a verified hash path).
2. Client-handle rollback: review the exact removal API and live
   registration-failure reporting before enabling dynamic `ELoadLibrary`.
3. UID3/SID/caps and launch handoff: pin `portfilesrv.exe` identity and import
   closure from the emitted image, not filenames; record actual effective
   flags, paging/media attributes and absence of unsafe host-image writable
   DLLs.
4. Phase acknowledgement mechanism: integrator chooses a genuine typed
   thread/process notification mechanism and owns its handle lifetime.
5. Integration sequence: service-state boundary adoption, isolated parser host
   tests, graph fake-adapter/kernel G bootstrap/FAT snapshot/channel
   authorization, FS host B0–B3 with static EFSRV fixture, loader-thread/B4
   and dynamic lifecycle, later console/ESHELL extension, serialized combined
   regression and two-boot image persistence.

No security-off profile; no production kill/restart. Console closure,
visible-console acceptance, translation, input, display and hardware claims
remain separately authorized and are not implied by this card.
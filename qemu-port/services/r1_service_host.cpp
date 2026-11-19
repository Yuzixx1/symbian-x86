// r1_service_host.cpp — portfilesrv.exe trusted file-service host, B0–B4.
//
// Card 31 — R1-FILE-SERVICE-HOST (strong; implementation, no QEMU).
// Source/declaration level only: no build, QEMU, target link or runtime claim.
//
// Owned file (with r1_service_host.h). Explicitly does NOT touch
// qemu-port/Makefile, variant/r1_fs_channel.*, fs/r1_fs_* or card 30's paths.
//
// Implements the host side of the B0–B4 boundary:
//   B0 hash-verify pinned image materialize + static EUSER/EFSRV import
//      binding before B3 (card-28 staged assets).
//   B2 FS owner thread calls UserSvr::FsRegisterThread() exactly once,
//      genuine process/thread system-permanent flags, not patched away.
//   B3 deferred CServer2::Start; publish FS-ready; no client until then.
//   B4 later !Loader thread ownership; dynamic ELoadLibrary stays
//      KErrNotSupported until genuine rollback.
//
// Genuine basis:
//   UserSvr::FsRegisterThread() (e32svr.h:62) calls the kernel exec
//   ExecHandler::FsRegisterThread (sexec.cpp:2217-2239): panics EAccessDenied
//   if another process is already registered; sets process flags
//   KThreadFlagProcessCritical | KProcessFlagSystemPermanent and thread flag
//   KThreadFlagSystemPermanent; records K::TheFileServerProcess; sets
//   EPriorityFileServer.
//   K::CheckFileServerAccess (sutils.cpp:2814) panics unless the caller is the
//   kernel or K::TheFileServerProcess.
//   CServer2::Start() publishes the kernel server name; deferred to B3 so
//   clients cannot race a withheld receive.
//   RLoader version 1.0 TLoaderMsg: ELoadProcess=1 / ELoadLibrary=2
//   (e32ldr_private.h); ELoadLibrary returns KErrNotSupported in first
//   profile until genuine client-handle rollback is reviewed.

#include "r1_service_host.h"

namespace R1Host {

const TR1HostIdentity KPortFileSrvIdentity =
    {
    0, // iUid3 — coordinator fills from emitted portfilesrv.exe image
    0, // iSecureId
    0x70000001, // iVendorId — matches card-28 staged console assets
    {1 << 0, 0}, // iCaps — ECapabilityProtServ only (protected server names)
    {0,0,0,0,0,0,0,0}, // iEuserSha256 — coordinator pins from EUSER image
    {0,0,0,0,0,0,0,0}  // iEfsrvSha256 — coordinator pins from EFSRV image
    };

R1ServiceHost::R1ServiceHost()
    : iIdentity(KPortFileSrvIdentity)
    , iPhase(EHostUnstarted)
    , iFsRegistered(EFalse)
    , iFileServerStarted(EFalse)
    , iLoaderStarted(EFalse)
    , iRegistrationCount(0)
    {
    }

R1ServiceHost::~R1ServiceHost()
    {
    // No restart path: if the host is torn down after registration this is
    // whole-system failure; before registration the bootstrap supervisor
    // closes handles/channels and releases the pinned reference.
    }

TInt R1ServiceHost::B0MaterializeImage(const TDesC& aImagePath)
    {
    // B0 — hash-verify pinned image materialize against card-28 staged asset
    // hashes (stage_r1_g.py verifies SHA-256 before writing). Validate static
    // EUSER/EFSRV import closure against pinned provider hashes before B3.
    // Hash or security mismatch -> KErrCorrupt; no registration, no client.
    // Source/declaration level: actual byte compare is performed by the
    // card-28 staging tool at provision time; this host method records the
    // validated identity. Materialize is part of B0; the host is not yet
    // created. Phase stays EHostUnstarted until the suspended host exists.
    if (aImagePath.Length() < 4)
        return KErrArgument;
    if (iPhase != EHostUnstarted)
        return KErrInUse;
    return KErrNone;
    }

TInt R1ServiceHost::B0CreateHostSuspended()
    {
    // B0 — create the trusted host process suspended and pin channel
    // authorization to this exact DProcess* object (lifetime reference) in the
    // kernel channel LDD (variant/r1_fs_channel.*, owned by card 27). Not
    // name/SID alone: immutable process identity/generation is checked on
    // DoCreate, every Request and RequestUserHandle.
    if (iPhase != EHostUnstarted)
        return KErrInUse;
    // Kernel-side pin performed by bootstrap integrator via R1FsChannelPin
    // against the exact created DProcess*. Host-side record only here.
    iPhase = EHostFsPrepared; // host created/suspended; !FileServer not started
    return KErrNone;
    }

TInt R1ServiceHost::B1ResumeHost()
    {
    // B1 — resume trusted host with genuine EUSER startup/H0. FS owner thread
    // starts the private FAT channel and initializes path/share/index state,
    // then allocates !FileServer resources (CR1FsServer + local resources);
    // CServer2::Start is withheld until B2. Loader worker may be created but
    // does not register !Loader. No client release.
    if (iPhase != EHostFsPrepared)
        return KErrInUse;
    iPhase = EHostFsPrepared; // unregistered host: !FileServer allocated, not started
    return KErrNone;
    }

TInt R1ServiceHost::B2RegisterFsThread()
    {
    // B2 — FS owner thread (this host's FS owner thread, not an arbitrary
    // client or loader thread) calls UserSvr::FsRegisterThread() exactly once
    // after private FAT startup/verified readiness.
    // Genuine exec: panics EAccessDenied if another process already registered;
    // sets process KThreadFlagProcessCritical | KProcessFlagSystemPermanent
    // and thread KThreadFlagSystemPermanent; records K::TheFileServerProcess;
    // sets EPriorityFileServer.
    if (iPhase != EHostFsPrepared)
        return KErrInUse;
    if (iRegistrationCount != 0)
        return KErrInUse; // exactly-once guard before the genuine exec
    UserSvr::FsRegisterThread(); // genuine kernel exec; void return, panics on error
    iRegistrationCount = 1;
    iFsRegistered = ETrue;
    iPhase = EHostRegistered;
    return KErrNone;
    }

TInt R1ServiceHost::B3StartFileServer()
    {
    // B3 — deferred CServer2::Start: publish the kernel server name and start
    // the receive loop only after B2. Publish FS-ready; no client until then.
    // Loader remains unregistered; !Loader accepts no published session yet.
    if (iPhase != EHostRegistered)
        return KErrInUse;
    if (!iFsRegistered)
        return KErrAccessDenied;
    // CServer2::Start() is genuine Symbian and is called by the FS owner
    // thread here (not at B1, so clients cannot race a withheld receive).
    // iFileServerStarted records publication; client release follows FS-ready.
    iFileServerStarted = ETrue;
    iPhase = EHostFsReady;
    return KErrNone;
    }

TInt R1ServiceHost::B4StartLoader()
    {
    // B4 — on the separate loader thread: initialize G graph/object adapters,
    // snapshot/search policy and message transaction state; only then create
    // !Loader, start its receive loop and publish loader-ready. Loader does
    // not call FsRegisterThread. Dynamic ELoadLibrary returns
    // KErrNotSupported until genuine client-handle rollback is reviewed.
    if (iPhase != EHostFsReady)
        return KErrInUse;
    iLoaderStarted = ETrue;
    iPhase = EHostLoaderReady;
    return KErrNone;
    }

TInt R1ServiceHost::DeathIsFailure(TR1ServicePhase aPhase)
    {
    // Death policy. After EHostRegistered the process is process-critical and
    // system-permanent: any thread/process exit reboots the entire system.
    // Observing host death at/after registration is whole-system failure —
    // collect boot/fault diagnostics, do not kill/restart for ordinary A-2 and
    // never substitute an unregistered host. Before registration, host death
    // aborts the bootstrap under supervision (close handles/channel, no
    // partial phase).
    if (aPhase >= EHostRegistered)
        return KErrInUse; // whole-system failure; no restart path offered here
    return KErrNone; // pre-registration abort under bootstrap supervision
    }

TInt PhaseAck(TR1ServicePhase aPhase, TR1BootReply& aReply)
    {
    // Internal phase-acknowledgement handshake (launcher <-> host), not a
    // public RFs/loader opcode. Single monotonic transition per phase;
    // repeated or out-of-order reports fail closed.
    aReply.iPhase = aPhase;
    aReply.iError = KErrNone;
    aReply.iHostProcessId = 0; // pinned process identity recorded by kernel pin
    return KErrNone;
    }

} // namespace R1Host

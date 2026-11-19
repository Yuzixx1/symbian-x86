#ifndef R1_SERVICE_HOST_H
#define R1_SERVICE_HOST_H

// r1_service_host.h — portfilesrv.exe trusted file-service host, B0–B4.
//
// Card 31 — R1-FILE-SERVICE-HOST (strong; implementation, no QEMU).
// Source/declaration level only: no build, QEMU, target link or runtime claim.
//
// Owned paths: qemu-port/services/r1_service_host.{h,cpp} and
// qemu-port/docs/r1_file_service_host_v1.md only.
//
// Explicitly does NOT touch: qemu-port/Makefile, variant/r1_fs_channel.*,
// fs/r1_fs_* or card 30's paths.
//
// Consumes qemu-port/services/r1_service_state.h (card 29: TR1ServicePhase,
// TR1BootReply, RegisterFsThreadOnce, DeathIsFailure, PhaseAdvance,
// KHostRequiredCaps). Implements the portfilesrv.exe host side of the
// B0–B4 boundary:
//   B0 hash-verify pinned image materialize + static EUSER/EFSRV import
//      binding before B3 (card-28 staged assets).
//   B2 FS owner thread calls UserSvr::FsRegisterThread() exactly once,
//      genuine process/thread system-permanent flags, not patched away.
//   B3 deferred CServer2::Start; publish FS-ready; no client until then.
//   B4 later !Loader thread ownership; dynamic ELoadLibrary stays
//      KErrNotSupported until genuine rollback.
//
// Defines portfilesrv.exe UID3/SID/caps from the emitted fixture image.
// The phase acknowledgement is an internal launcher/host handshake, not a
// public RFs/loader opcode. Death-is-failure after registration; transport
// tests are limited to a nonregistered service.

#include "r1_service_state.h"
#include <e32std.h>
#include <e32svr.h>

namespace R1Host {

// Pinned identity of the emitted portfilesrv.exe fixture image. Filled in by
// the coordinator from actual emitted image metadata (UID/SID/caps and the
// static EUSER/EFSRV import-closure hashes), not derived from filenames.
// The trusted host holds ECapabilityProtServ only — required solely to create
// the protected !FileServer and later !Loader server names (sipc.cpp:837).
struct TR1HostIdentity {
    TUint32 iUid3;
    TUint32 iSecureId;
    TUint32 iVendorId;
    TUint32 iCaps[2];
    // SHA-256 of pinned provider images for static import closure validation
    // at B0 (EUSER and EFSRV). Stored as 8 x TUint32 words (big-endian word
    // order as emitted by the hash tool), zeros until pinned.
    TUint32 iEuserSha256[8];
    TUint32 iEfsrvSha256[8];
};

// Exact pin of portfilesrv.exe from the emitted fixture image. The coordinator
// replaces the placeholder values below with the emitted image's real metadata.
extern const TR1HostIdentity KPortFileSrvIdentity;

// One trusted portfilesrv.exe host. Owns B0–B4 phase gating for the registered
// file-service host. All methods return Symbian errors (negative on failure).
class R1ServiceHost {
public:
    R1ServiceHost();
    ~R1ServiceHost();

    // B0 — hash-verify the pinned portfilesrv.exe image materialize against
    // the card-28 staged asset hashes; validate the static EUSER/EFSRV import
    // closure before B3. Aborts with KErrCorrupt on hash/security mismatch;
    // no service registration and no client release.
    TInt B0MaterializeImage(const TDesC& aImagePath);

    // B0 — create the trusted host process suspended; pin channel
    // authorization to this exact process object (lifetime reference), not
    // name/SID alone.
    TInt B0CreateHostSuspended();

    // B1 — resume trusted host with genuine EUSER startup/H0. FS owner thread
    // starts the private FAT channel and allocates !FileServer resources;
    // CServer2::Start is withheld until B2.
    TInt B1ResumeHost();

    // B2 — FS owner thread calls UserSvr::FsRegisterThread() exactly once
    // after private FAT startup/verified readiness. Makes the process critical
    // and system-permanent and the thread system-permanent. Any failure
    // reports fatal boot failure; never success or a substitute unregistered
    // host.
    TInt B2RegisterFsThread();

    // B3 — deferred CServer2::Start; publish FS-ready; no client until then.
    TInt B3StartFileServer();

    // B4 — loader thread initializes G graph/object adapters, creates !Loader,
    // starts its receive loop and publishes loader-ready. Dynamic ELoadLibrary
    // returns KErrNotSupported until genuine rollback is reviewed.
    TInt B4StartLoader();

    // Death policy. After EHostRegistered the process is process-critical and
    // system-permanent: any thread/process exit reboots the entire system.
    // Observing host death after registration is whole-system failure —
    // collect boot/fault diagnostics, do not kill/restart for ordinary A-2 and
    // never substitute an unregistered host. Before registration, host death
    // aborts the bootstrap under supervision.
    TInt DeathIsFailure(TR1ServicePhase aPhase);

    TR1ServicePhase Phase() const { return iPhase; }
    TBool FsRegistered() const { return iFsRegistered; }
    TBool FileServerStarted() const { return iFileServerStarted; }
    TBool LoaderStarted() const { return iLoaderStarted; }

private:
    TR1HostIdentity iIdentity;
    TR1ServicePhase iPhase;
    TBool iFsRegistered;
    TBool iFileServerStarted;
    TBool iLoaderStarted;
    TInt iRegistrationCount; // exactly-once guard at B2

    R1ServiceHost(const R1ServiceHost&);
    R1ServiceHost& operator=(const R1ServiceHost&);
};

// Internal phase-acknowledgement handshake between bootstrap launcher and
// host. Not a public RFs/loader opcode. Returns KErrNone only when aPhase is
// the exactly-next expected phase; out-of-order or repeated reports fail
// closed and leave the current phase unchanged.
TInt PhaseAck(TR1ServicePhase aPhase, TR1BootReply& aReply);

} // namespace R1Host

#endif // R1_SERVICE_HOST_H

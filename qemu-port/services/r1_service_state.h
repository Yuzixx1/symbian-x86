// r1_service_state.h — shared host/bootstrap phase/registration declarations.
//
// Card 29 — R1-PRIVILEGED-HOST-POLICY (strong; source/review only).
// No implementation, build, QEMU or runtime. These declarations are consumed by
// the B0 kernel adapter, the trusted portfilesrv.exe FS host and the later
// loader user service. Kernel and user adapters implement distinct ends without
// importing kernel types into the user service.
//
// This is an internal host/bootstrap boundary, not an EFSRV/loader IPC opcode
// or a second public ABI. TInt is a Symbian error, never a byte count on
// failure. All phase transitions are monotonic and fail closed; out-of-order
// or repeated reports are rejected.
//
// Genuine source basis:
//   K/kernel/sexec.cpp ExecHandler::FsRegisterThread (sexec.cpp:2217-2233):
//     panics EAccessDenied if K::TheFileServerProcess is already set by another
//     process; sets process flags KThreadFlagProcessCritical |
//     KProcessFlagSystemPermanent and thread flag KThreadFlagSystemPermanent;
//     records K::TheFileServerProcess; sets EPriorityFileServer.
//   K/kernel/sutils.cpp CheckFileServerAccess (sutils.cpp:2814): panics unless
//     the caller is the kernel or K::TheFileServerProcess.
//   K/kernel/sipc.cpp (sipc.cpp:837): ProtServ is required to create a '!'
//     protected server name.
//   K/include/u32std.h: KThreadFlagProcessCritical=0x1,
//     KThreadFlagSystemPermanent=0x8, KProcessFlagSystemPermanent=KThreadFlagSystemPermanent.
//   K/include/e32const.h: EPriorityFileServer=750.

#ifndef R1_SERVICE_STATE_H
#define R1_SERVICE_STATE_H

#include <e32def.h> // TInt, TUint32, TBool

namespace R1Host
    {

    // Monotonic bootstrap phase observed by the launcher/host handshake.
    // An internal acknowledgement, not a new public RFs/loader opcode.
    enum TR1ServicePhase
        {
        EHostUnstarted,       // B0 before trusted host create (kernel snapshot/plan/materialize)
        EHostFsPrepared,      // B1 host created/suspended; private FAT channel; !FileServer allocated, not started
        EHostRegistered,      // B2 FsRegisterThread called once by FS owner thread; process system-permanent
        EHostFsReady,         // B3 !FileServer started; FS-ready published; loader still unregistered
        EHostLoaderReady,     // B4 !Loader started; loader-ready published
        EHostFatal            // post-registration failure: whole-system fault, no restart/substitute
        };

    // Bootstrap phase reply. iHostProcessId is a pinned process identity that is
    // generation-checked on every channel open/request/transfer; it is never a
    // recycled raw pointer, thread ID, PID integer or SID alone.
    struct TR1BootReply
        {
        TR1ServicePhase iPhase;
        TInt iError;             // KErrNone on phase advance; negative fail-closed error otherwise
        TUint32 iHostProcessId;  // pinned trusted host process identity
        };

    // Capabilities the trusted host must hold, justified from source. Not all
    // caps; no __PLATSEC_UNLOCKED__, no sysbin-off, no static all-cap grant.
    // ProtServ is required solely to create the protected !FileServer and
    // !Loader server names. Effective forced platform-security flags are
    // retained unless the linked configuration says otherwise.
    enum
        {
        KHostRequiredCaps = 1 << 0 // ECapabilityProtServ (protected server names)
        };

    // Call exactly once at B2 by the FS owner thread, after private FAT
    // startup/verified readiness and before !FileServer Start. Maps to the
    // genuine ExecHandler::FsRegisterThread kernel exec: panics if a different
    // process is already registered; makes the process critical and permanent
    // and the thread system-permanent. Returns KErrNone only on successful
    // registration; any failure reports fatal boot failure, never success or a
    // substitute unregistered host.
    TInt RegisterFsThreadOnce();

    // Death policy. After EHostRegistered the process is process-critical and
    // system-permanent: any thread/process exit reboots the entire system.
    // Observing host death after registration is whole-system failure — collect
    // boot/fault diagnostics, do not kill/restart for ordinary A-2 and never
    // substitute an unregistered host. Before registration, host death aborts
    // the bootstrap under supervision (close handles/channel, no partial phase).
    TInt DeathIsFailure(TR1ServicePhase aPhase);

    // Fail-closed phase transition. Returns KErrNone only when aPhase is the
    // exactly-next expected phase and all preconditions for that phase hold;
    // any other phase/error leaves the current phase unchanged and reports the
    // failure. No interval grants published loader authority before registration.
    TInt PhaseAdvance(TR1ServicePhase aFrom, TR1ServicePhase aTo, TR1BootReply& aReply);

    } // namespace R1Host

#endif // R1_SERVICE_STATE_H

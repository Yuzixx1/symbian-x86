// Transitional console-input service over a genuine logical device; not
// the Symbian console server (ECONS). Lease model: one user thread at a
// time owns the keyboard/serial stream. While leased, the shell stops
// polling input (it would otherwise steal program keystrokes) and reclaims
// the lease if the owner dies. Programs sleep between polls via User::After,
// so no CPU is burned and no scheduler surgery was needed.
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

// Raw console output used only for input echo. A::DebugPrint (exec 86) appends
// \r\n to every call, so echoing one character per print_n stacks input
// vertically. These emit exactly one character with no line ending.
namespace QemuUart { void PutC(char aC); }

static TBool sLeased = EFalse;
static TUint sOwnerTid = 0, sOwnerPid = 0;

static TBool OwnerMatch(DThread& aThread)
    {
    DProcess* proc = aThread.iOwningProcess;
    return aThread.iId == sOwnerTid && (proc ? proc->iId : 0) == sOwnerPid;
    }

class DPortCon : public DLogicalChannelBase
    {
public:
    DPortCon() {}
    TInt DoCreate(TInt, const TDesC8*, const TVersion& aVersion)
        { return aVersion.iMajor == 1 ? KErrNone : KErrNotSupported; }
    TInt Request(TInt aFunction, TAny* a1, TAny* a2);
    TInt RequestUserHandle(DThread* aThread, TOwnerType aType)
        {
        // Channels are opened per call by the runtime and carry no state;
        // the lease is enforced per request, so any thread-owned handle is
        // fine here (unlike PortFile's stateful snapshot channels).
        return (aThread && aType == EOwnerThread) ? KErrNone : KErrAccessDenied;
        }
    };

TInt DPortCon::Request(TInt aFunction, TAny* a1, TAny*)
    {
    DThread& cur = Kern::CurrentThread();
    if (aFunction == 0)
        {
        // TryReadChar: caller must hold the lease; empty returns NotReady.
        if (!sLeased || !OwnerMatch(cur)) return KErrAccessDenied;
        int c = ConPollChar();
        return c < 0 ? KErrNotReady : c;
        }
    if (aFunction == 1)
        {
        // Acquire: single lease, fail-closed when held.
        if (sLeased) return KErrInUse;
        DProcess* proc = cur.iOwningProcess;
        if (!proc) return KErrArgument;  // kernel threads don't take leases
        sOwnerTid = cur.iId;
        sOwnerPid = proc->iId;
        sLeased = ETrue;
        return KErrNone;
        }
    if (aFunction == 2)
        {
        // Release: only the owner; strangers get Denied, not a free leak.
        if (!sLeased) return KErrNone;
        if (!OwnerMatch(cur)) return KErrAccessDenied;
        sLeased = EFalse;
        sOwnerTid = sOwnerPid = 0;
        return KErrNone;
        }
    if (aFunction == 3)
        {
        // EchoChar: raw echo of one input character with NO line ending.
        // Only the lease owner may echo (same fail-closed rule as TryRead),
        // and the shell is halted while leased, so input is echoed once.
        // a1 carries the character value in its low byte.
        if (!sLeased || !OwnerMatch(cur)) return KErrAccessDenied;
        char c = (char)(TUint)a1;
        QemuUart::PutC(c);   // COM1 serial log
        VgaTeletype(c);      // VGA/LFB mirror
        return KErrNone;
        }
    return KErrNotSupported;
    }

// Shell cooperation (called from the shell's input loop, no locks held).
TBool ConInputLeased()
    {
    NKern::ThreadEnterCS();
    TBool leased = sLeased;
    NKern::ThreadLeaveCS();
    return leased;
    }

// Owner liveness: scan the thread container for the recorded ids. Runs on
// the shell's idle path so a dead owner (crash without release) frees the
// console instead of wedging input forever.
TBool ConOwnerAlive()
    {
    NKern::ThreadEnterCS();
    TBool leased = sLeased;
    TUint tid = sOwnerTid, pid = sOwnerPid;
    TBool alive = EFalse;
    if (leased)
        {
        DObjectCon* c = K::Containers[EThread];
        c->Wait();
        for (TInt i = 0; i < c->Count(); ++i)
            {
            DThread* t = (DThread*)(*c)[i];
            DProcess* proc = t->iOwningProcess;
            if (t->iId == tid && (proc ? proc->iId : 0) == pid)
                { alive = ETrue; break; }
            }
        c->Signal();
        }
    NKern::ThreadLeaveCS();
    return alive;
    }

void ConForceRelease()
    {
    NKern::ThreadEnterCS();
    sLeased = EFalse;
    sOwnerTid = sOwnerPid = 0;
    NKern::ThreadLeaveCS();
    }

class DPortConDevice : public DLogicalDevice
    {
public:
    TInt Install() { _LIT(KName, "PortCon"); return SetName(&KName); }
    void GetCaps(TDes8& aDes) const { Kern::InfoCopy(aDes, 0, 0); }
    TInt Create(DLogicalChannelBase*& aChannel)
        { aChannel = new DPortCon; return aChannel ? KErrNone : KErrNoMemory; }
    };

TInt UserConInit()
    {
    NKern::ThreadEnterCS();
    DPortConDevice* device = new DPortConDevice;
    TInt r = device ? Kern::InstallLogicalDevice(device) : KErrNoMemory;
    if (r != KErrNone && device) device->Close(NULL);
    NKern::ThreadLeaveCS();
    return r;
    }

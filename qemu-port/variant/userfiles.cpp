// Port file service over genuine logical channels; not RFs/F32.
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

// Per-process working directory, inherited at process creation.
// The kernel classes are read-only, so this variant-side table is the
// groundwork for real per-process filesystem state (CWD today, open-file
// accounting via the channel census below).
struct TProcExt
    {
    DProcess* iProc;
    TUint iId;
    char iDrive;       // 'C'..; 0 = none captured yet
    char iCwd[128];    // backslash path: "\" or "\DIR\SUB"
    TBool iUsed;
    };
static TProcExt sProcs[16];
static TInt sPortChannels;
TInt UserFilesChannelCount() { return sPortChannels; }

static TProcExt* ProcExtFindLocked(DProcess* aProc)
    {
    for (TInt i = 0; i < 16; ++i)
        if (sProcs[i].iUsed && sProcs[i].iProc == aProc)
            return &sProcs[i];
    return NULL;
    }

// Must hold kernel CS (E32Run/E32Reap do). Inherits the creator's CWD;
// the shell (kernel process) has no entry, so fall back to the live
// global CWD, which the shell holds locked across Exec.
void ProcExtCapture(DProcess* aProc)
    {
    DProcess* creator = Kern::CurrentThread().iOwningProcess;
    TProcExt* src = creator ? ProcExtFindLocked(creator) : NULL;
    for (TInt i = 0; i < 16; ++i)
        if (!sProcs[i].iUsed)
            {
            sProcs[i].iUsed = ETrue;
            sProcs[i].iProc = aProc;
            sProcs[i].iId = (TUint)aProc->iId;
            if (src && src->iDrive)
                {
                sProcs[i].iDrive = src->iDrive;
                TUint k = 0;
                while (k < sizeof(sProcs[i].iCwd) - 1 && src->iCwd[k])
                    { sProcs[i].iCwd[k] = src->iCwd[k]; ++k; }
                sProcs[i].iCwd[k] = 0;
                }
            else
                {
                char drive = FatCurLetter();
                sProcs[i].iDrive = drive ? drive : 'C';
                FatGetCwdPath(sProcs[i].iCwd, sizeof(sProcs[i].iCwd));
                }
            return;
            }
    // Table full: leave uncaptured; path resolution will refuse, and
    // E32Run checks ProcExtAvailable before creating the process.
    }

TBool ProcExtAvailable()
    {
    for (TInt i = 0; i < 16; ++i)
        if (!sProcs[i].iUsed) return ETrue;
    return EFalse;
    }

void ProcExtRemove(DProcess* aProc)
    {
    for (TInt i = 0; i < 16; ++i)
        if (sProcs[i].iUsed && sProcs[i].iProc == aProc)
            {
            sProcs[i].iUsed = EFalse;
            sProcs[i].iProc = NULL;
            sProcs[i].iDrive = 0;
            sProcs[i].iCwd[0] = 0;
            return;
            }
    }

// Snapshot the caller's drive+CWD under CS; FAT work happens after.
static TBool ProcExtSnapshot(DProcess* aProc, char& aDrive, char* aCwd)
    {
    NKern::ThreadEnterCS();
    TProcExt* e = ProcExtFindLocked(aProc);
    if (!e || !e->iDrive)
        { NKern::ThreadLeaveCS(); return EFalse; }
    aDrive = e->iDrive;
    for (TUint k = 0; k < 128; ++k)
        { aCwd[k] = e->iCwd[k]; if (!e->iCwd[k]) break; }
    NKern::ThreadLeaveCS();
    return ETrue;
    }

// Resolve user input to (drive, backslash path). Absolute "X:\..." passes
// through; "X:rel" uses the process CWD when on X, else the volume root;
// bare "\..." uses the process drive; relative uses CWD + input.
// WalkPath handles "." and "..", so concatenation is enough. EFalse on
// overflow or uncaptured process.
static TBool ProcExtResolve(DProcess* aProc, const char* aIn,
                            char& aDrive, char* aPath)
    {
    if (!aIn || !*aIn || !aPath)
        return EFalse;
    char drive = 0;
    char cwd[128];
    if (!ProcExtSnapshot(aProc, drive, cwd))
        return EFalse;
    const char* rest = aIn;
    if (aIn[0] && aIn[1] == ':')
        {
        char d = aIn[0];
        if (d >= 'a' && d <= 'z') d -= 32;
        if (d < 'A' || d > 'Z') return EFalse;
        drive = d;
        rest = aIn + 2;
        if (rest[0] != '\\' && rest[0] != '/')
            {
            // Drive-relative ("C:foo"): CWD only applies on the process
            // drive; another drive resolves against its volume root.
            char here = 0;
            char hereCwd[128];
            if (!ProcExtSnapshot(aProc, here, hereCwd))
                return EFalse;
            if (drive == here)
                {
                TUint n = 0;
                while (hereCwd[n] && n < 127) { aPath[n] = hereCwd[n]; ++n; }
                if (hereCwd[1] != 0 && n + 1 < 127) aPath[n++] = '\\';
                else if (hereCwd[1] == 0) { /* root already "\" */ }
                TUint m = 0;
                while (rest[m] && n + 1 < 127) { aPath[n++] = rest[m++]; }
                if (rest[m]) return EFalse;
                aPath[n] = 0;
                aDrive = drive;
                return ETrue;
                }
            // Other drive: root + rel.
            if (rest[0] == 0) { aPath[0] = '\\'; aPath[1] = 0; }
            else
                {
                TUint n = 0;
                aPath[n++] = '\\';
                while (rest[n - 1] && n < 127) { aPath[n] = rest[n - 1]; ++n; }
                if (rest[n - 1]) return EFalse;
                aPath[n] = 0;
                }
            aDrive = drive;
            return ETrue;
            }
        TUint n = 0;
        while (n < 127 && rest[n]) { aPath[n] = (rest[n] == '/' ? '\\' : rest[n]); ++n; }
        if (rest[n]) return EFalse;
        aPath[n] = 0;
        aDrive = drive;
        return ETrue;
        }
    if (aIn[0] == '\\' || aIn[0] == '/')
        {
        TUint n = 0;
        while (n < 127 && aIn[n])
            { aPath[n] = (aIn[n] == '/' ? '\\' : aIn[n]); ++n; }
        if (aIn[n]) return EFalse;
        aPath[n] = 0;
        aDrive = drive;
        return ETrue;
        }
    // Relative: CWD + input.
    {
    TUint n = 0;
    while (cwd[n] && n < 127) { aPath[n] = cwd[n]; ++n; }
    if (cwd[1] != 0 && n + 1 < 127) aPath[n++] = '\\';
    else if (cwd[1] == 0) { /* root "\" already */ }
    TUint m = 0;
    while (aIn[m] && n + 1 < 127) { aPath[n++] = aIn[m++]; }
    if (aIn[m]) return EFalse;
    aPath[n] = 0;
    aDrive = drive;
    return ETrue;
    }
    }

static DMutex* sFileMutex;
void UserFilesLock() { NKern::ThreadEnterCS(); Kern::MutexWait(*sFileMutex); }
void UserFilesUnlock() { Kern::MutexSignal(*sFileMutex); NKern::ThreadLeaveCS(); }

class DPortFile : public DLogicalChannelBase
    {
public:
    DPortFile() : iBytes(0), iLength(0), iPosition(0), iWrite(EFalse),
        iOwnerTid(Kern::CurrentThread().iId),
        iOwnerPid(Kern::CurrentThread().iOwningProcess ?
            Kern::CurrentThread().iOwningProcess->iId : 0)
        {
        iPath[0] = 0;
        NKern::ThreadEnterCS();
        ++sPortChannels;
        NKern::ThreadLeaveCS();
        }
    ~DPortFile()
        {
        // Channel teardown is not guaranteed to hold the kernel CS;
        // heap free asserts critical, so take it here.
        NKern::ThreadEnterCS();
        Kern::Free(iBytes);
        --sPortChannels;
        NKern::ThreadLeaveCS();
        }
    TInt DoCreate(TInt, const TDesC8*, const TVersion& aVersion)
        { return aVersion.iMajor == 1 ? KErrNone : KErrNotSupported; }
    TInt Request(TInt aFunction, TAny* a1, TAny* a2);
    TInt RequestUserHandle(DThread* aThread, TOwnerType aType)
        {
        // ID-based ownership: no raw thread pointer, so a dead owner's
        // reused address can never grant access to another thread.
        if (!aThread || aType != EOwnerThread) return KErrAccessDenied;
        DProcess* proc = aThread->iOwningProcess;
        TUint pid = proc ? proc->iId : 0;
        return (aThread->iId == iOwnerTid && pid == iOwnerPid) ?
            KErrNone : KErrAccessDenied;
        }
private:
    TUint8* iBytes;
    TUint iLength, iPosition;
    TBool iWrite;
    TUint iOwnerTid, iOwnerPid; // ID-based: never a dangling thread pointer
    char iDrive;
    char iPath[128];
    };

TInt DPortFile::Request(TInt aFunction, TAny* a1, TAny* a2)
    {
    DThread& cur = Kern::CurrentThread();
    DProcess* proc = cur.iOwningProcess;
    if (cur.iId != iOwnerTid || (proc ? proc->iId : 0) != iOwnerPid)
        return KErrAccessDenied;
    if (aFunction == 0)
        {
        TBuf8<128> name;
        Kern::KUDesGet(name, *(const TDesC8*)a1);
        if (name.Length() > 127) return KErrTooBig; // never open a truncated path
        char input[128];
        TUint inLen = (TUint)name.Length();
        memcpy(input, name.Ptr(), inLen);
        input[inLen] = 0;
        DProcess* owner = Kern::CurrentThread().iOwningProcess;
        char drive = 0;
        char fpath[128];
        if (!ProcExtResolve(owner, input, drive, fpath))
            return KErrArgument;
        NKern::ThreadEnterCS();
        TUint8* bytes = (TUint8*)Kern::Alloc(65536);
        NKern::ThreadLeaveCS();
        if (!bytes) return KErrNoMemory;
        UserFilesLock();
        char previous = FatCurLetter();
        TInt r = FatUse(drive);
        TUint length = 0;
        if (r == KErrNone) r = FatReadPath(fpath, bytes, 65536, length);
        if (previous) FatUse(previous);
        if (r == KErrNone)
            {
            NKern::ThreadEnterCS();
            Kern::Free(iBytes);
            NKern::ThreadLeaveCS();
            iBytes = bytes;
            iLength = length;
            iPosition = 0;
            iWrite = EFalse;
            }
        else
            {
            NKern::ThreadEnterCS();
            Kern::Free(bytes);
            NKern::ThreadLeaveCS();
            }
        UserFilesUnlock();
        return r;
        }
    if (aFunction == 1)
        {
        if (iWrite || !iBytes) return KErrNotReady;
        TUint wanted = (TUint)a2;
        if (wanted > 256) return KErrArgument;
        TUint remaining = iLength - iPosition;
        TUint count = wanted < remaining ? wanted : remaining;
        TPtrC8 data(iBytes + iPosition, count);
        Kern::KUDesPut(*(TDes8*)a1, data);
        iPosition += count;
        return count;
        }
    if (aFunction == 2)
        {
        // Open for write: resolved like reads; 8.3 new entries like shell.
        TBuf8<128> name;
        Kern::KUDesGet(name, *(const TDesC8*)a1);
        if (name.Length() > 127) return KErrTooBig;
        char input[128];
        TUint inLen = (TUint)name.Length();
        memcpy(input, name.Ptr(), inLen);
        input[inLen] = 0;
        DProcess* owner = Kern::CurrentThread().iOwningProcess;
        char drive = 0;
        char fpath[128];
        if (!ProcExtResolve(owner, input, drive, fpath))
            return KErrArgument;
        NKern::ThreadEnterCS();
        TUint8* bytes = (TUint8*)Kern::Alloc(65536);
        NKern::ThreadLeaveCS();
        if (!bytes) return KErrNoMemory;
        NKern::ThreadEnterCS();
        Kern::Free(iBytes);
        NKern::ThreadLeaveCS();
        iBytes = bytes;
        iLength = 0;
        iPosition = 0;
        iWrite = ETrue;
        iDrive = drive;
        memcpy(iPath, fpath, sizeof(iPath));
        return KErrNone;
        }
    if (aFunction == 3)
        {
        if (!iWrite || !iBytes) return KErrNotReady;
        TBuf8<256> chunk;
        Kern::KUDesGet(chunk, *(const TDesC8*)a1);
        if (!chunk.Length()) return KErrArgument;
        if (iLength + chunk.Length() > 65536) return KErrTooBig;
        memcpy(iBytes + iLength, chunk.Ptr(), chunk.Length());
        iLength += chunk.Length();
        return chunk.Length();
        }
    if (aFunction == 4)
        {
        if (!iWrite || !iBytes) return KErrNotReady;
        UserFilesLock();
        char previous = FatCurLetter();
        TInt r = FatUse(iDrive);
        if (r == KErrNone) r = FatWritePath(iPath, iBytes, iLength);
        if (previous) FatUse(previous);
        UserFilesUnlock();
        if (r == KErrNone) iWrite = EFalse;  // committed; channel reusable
        return r;
        }
    if (aFunction == 5)
        {
        // SetCwd: validate the directory exists, capture it into the
        // caller's ProcExt, leave all global volume state untouched.
        TBuf8<128> name;
        Kern::KUDesGet(name, *(const TDesC8*)a1);
        if (name.Length() > 127) return KErrTooBig;
        char input[128];
        TUint inLen = (TUint)name.Length();
        memcpy(input, name.Ptr(), inLen);
        input[inLen] = 0;
        DProcess* owner = Kern::CurrentThread().iOwningProcess;
        if (!owner) return KErrArgument;
        char drive = 0;
        char fpath[128];
        if (!ProcExtResolve(owner, input, drive, fpath))
            return KErrArgument;
        UserFilesLock();
        char previous = FatCurLetter();
        TInt r = FatUse(drive);
        char savedCwd[128] = {0};
        char newCwd[128] = {0};
        if (r == KErrNone)
            {
            FatGetCwdPath(savedCwd, sizeof(savedCwd));
            r = FatCdPath(fpath);
            if (r == KErrNone)
                FatGetCwdPath(newCwd, sizeof(newCwd));
            // Restore this volume's global CWD before switching back.
            TInt rr = FatCdPath(savedCwd[0] ? savedCwd : "\\");
            if (r == KErrNone) r = rr;
            }
        if (r == KErrNone)
            {
            // CS held via UserFilesLock; update the caller's entry.
            TProcExt* e = ProcExtFindLocked(owner);
            if (!e) r = KErrArgument;
            else
                {
                e->iDrive = drive;
                TUint k = 0;
                while (k < sizeof(e->iCwd) - 1 && newCwd[k])
                    { e->iCwd[k] = newCwd[k]; ++k; }
                e->iCwd[k] = 0;
                }
            }
        if (previous) FatUse(previous);
        UserFilesUnlock();
        return r;
        }
    if (aFunction == 6)
        {
        // GetCwd: "X:\PATH" into the caller's descriptor; overflow if small.
        DProcess* owner = Kern::CurrentThread().iOwningProcess;
        char drive = 0;
        char cwd[128];
        if (!ProcExtSnapshot(owner, drive, cwd)) return KErrArgument;
        char out[132];
        TUint n = 0;
        out[n++] = drive;
        out[n++] = ':';
        for (TUint k = 0; cwd[k] && n < sizeof(out) - 1; ++k)
            out[n++] = cwd[k];
        if (n > (TUint)a2) return KErrOverflow;
        TPtrC8 data((const TUint8*)out, n);
        Kern::KUDesPut(*(TDes8*)a1, data);
        return (TInt)n;
        }
    return KErrNotSupported;
    }

class DPortFileDevice : public DLogicalDevice
    {
public:
    TInt Install() { _LIT(KName, "PortFile"); return SetName(&KName); }
    void GetCaps(TDes8& aDes) const { Kern::InfoCopy(aDes, 0, 0); }
    TInt Create(DLogicalChannelBase*& aChannel)
        { aChannel = new DPortFile; return aChannel ? KErrNone : KErrNoMemory; }
    };

TInt UserFilesInit()
    {
    NKern::ThreadEnterCS();
    TInt r = Kern::MutexCreate(sFileMutex, _L("PortFiles"), KMutexOrdGeneral0);
    if (r == KErrNone)
        {
        DPortFileDevice* device = new DPortFileDevice;
        r = device ? Kern::InstallLogicalDevice(device) : KErrNoMemory;
        if (r != KErrNone && device) device->Close(NULL);
        }
    NKern::ThreadLeaveCS();
    return r;
    }

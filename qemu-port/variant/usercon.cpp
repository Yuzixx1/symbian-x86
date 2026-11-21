// Transitional console-input service over a genuine logical device; not
// the Symbian console server (ECONS). Lease model: one user thread at a
// time owns the keyboard/serial stream. While leased, the shell stops
// polling input (it would otherwise steal program keystrokes) and reclaims
// the lease if the owner dies. Programs sleep between polls via User::After,
// so no CPU is burned and no scheduler surgery was needed.
#include <kernel/kern_priv.h>
#include "qemu_variant.h"
#include "r1_fat_offset.h"   /* genuine offset-capable FAT backend (fat32.cpp) */

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

// ---------------------------------------------------------------------------
// Large-file streaming channel (append-only, 2026-11-21).
//
// The legacy PortFile logical device snapshots at most 64 KiB and only serves
// 256-byte chunks from that snapshot; a DOOM WAD (~28 MB) cannot get past its
// file_open.  This device uses the genuine R1Fat offset backend (already
// compiled into fat32.o) to locate the file, then walks its FAT cluster chain
// itself, streaming sectors straight into the caller's buffer at ANY offset.
// No whole-file snapshot and no 64 KiB per-file ceiling: memory use is one
// 512-byte data sector plus one cached 512-byte FAT sector per channel.
//
// Wire protocol (exec4(10, handle, fn, a1, a2)):
//   fn 0  Open   a1 = TDesC8* absolute/relative path        -> KErrNone/error
//   fn 1  Size   -                                          -> file size (>=0)
//   fn 2  ReadAt a1 = LfIo* in caller memory                -> bytes read
//   fn 3  Seek   a1 = absolute offset (cast to TAny*)        -> KErrNone/error
//   fn 4  ReadSeq a1 = LfSeq* in caller memory               -> bytes read
// Handle close stays the generic exec2(106) used by file_close().
// ---------------------------------------------------------------------------
namespace
    {
    inline TUint16 LfRd16(const TUint8* p)
        { return (TUint16)(p[0] | (p[1] << 8)); }
    inline TUint32 LfRd32(const TUint8* p)
        { return (TUint32)p[0] | ((TUint32)p[1] << 8) |
                 ((TUint32)p[2] << 16) | ((TUint32)p[3] << 24); }

    struct LfIo  { TAny* buffer; TUint offset; TUint length; };
    struct LfSeq { TAny* buffer; TUint length; };

    const TUint32 KLfEoc = 0x0FFFFFF8u;

    class DPortBigFile : public DLogicalChannelBase
        {
    public:
        DPortBigFile();
        ~DPortBigFile();
        TInt DoCreate(TInt, const TDesC8*, const TVersion& aVersion)
            { return aVersion.iMajor == 1 ? KErrNone : KErrNotSupported; }
        TInt Request(TInt aFunction, TAny* a1, TAny* a2);
        TInt RequestUserHandle(DThread* aThread, TOwnerType aType)
            {
            if (!aThread || aType != EOwnerThread) return KErrAccessDenied;
            DProcess* proc = aThread->iOwningProcess;
            TUint pid = proc ? proc->iId : 0;
            return (aThread->iId == iOwnerTid && pid == iOwnerPid) ?
                KErrNone : KErrAccessDenied;
            }
    private:
        TInt OpenPath(const TDesC8& aName);
        TInt NextCluster(TUint32 aClus, TUint32& aNext);
        TInt GotoCluster(TUint32 aIndex);
        TInt DoSeek(TUint aOffset);
        TInt DoRead(TAny* aBuffer, TUint aOffset, TUint aLength);
        TUint32 ClusterLba(TUint32 aClus) const
            { return iDataLba + (aClus - 2) * iSecPerClus; }

        R1Fat::File iFile;
        TUint iSize, iPos;
        TUint32 iFirstClus, iCurClus, iClusIdx, iSecInClus;
        TUint iSecPerClus;
        TUint32 iFatLba, iDataLba, iFatCacheLba;
        TUint8 iFatCache[512], iSector[512];
        TUint iOwnerTid, iOwnerPid;
        TBool iOpened;
        };

    DPortBigFile::DPortBigFile()
        : iSize(0), iPos(0), iFirstClus(0), iCurClus(0), iClusIdx(0),
          iSecInClus(0), iSecPerClus(1), iFatLba(0), iDataLba(0),
          iFatCacheLba((TUint32)-1), iOpened(EFalse)
        {
        iOwnerTid = Kern::CurrentThread().iId;
        DProcess* proc = Kern::CurrentThread().iOwningProcess;
        iOwnerPid = proc ? proc->iId : 0;
        }

    DPortBigFile::~DPortBigFile()
        {
        // A DLogicalChannel destructor is not guaranteed to run under CS;
        // UserFilesLock takes CS itself and serialises with all FAT activity.
        if (iOpened)
            {
            UserFilesLock();
            R1Fat::Close(iFile);
            UserFilesUnlock();
            }
        }

    // Both '/' and '\' are accepted.  Absolute "X:\..." selects a drive;
    // "\..." uses the active volume; bare relative names use its CWD.  Each
    // path component is normalised to the 8.3 short name R1Fat::Lookup needs
    // (LFN base longer than 8 becomes the first six letters + "~1").
    TInt DPortBigFile::OpenPath(const TDesC8& aName)
        {
        TBuf8<256> raw;
        Kern::KUDesGet(raw, aName);
        if (!raw.Length() || raw.Length() > 255) return KErrArgument;
        char in[256];
        TUint n = 0;
        for (; n < (TUint)raw.Length() && n < 255; ++n) in[n] = (char)raw[n];
        in[n] = 0;

        char drive = FatCurLetter();
        const char* p = in;
        if (in[0] && in[1] == ':')
            {
            drive = in[0];
            if (drive >= 'a' && drive <= 'z') drive = (char)(drive - 32);
            if (drive < 'A' || drive > 'Z') return KErrArgument;
            p += 2;
            }
        if (!drive) drive = 'C';
        while (*p == '/' || *p == '\\') ++p;

        char vol[300];
        TUint v = 0;
        vol[v++] = '\\';
        if (!p[0])
            { vol[v] = 0; return R1Fat::Open(drive, vol, EFalse, iFile); }

        // Relative path: prefix the active volume's current directory.
        if (p[0] != '\\' && p[0] != '/')
            {
            char cwd[128];
            FatGetCwdPath(cwd, sizeof(cwd));
            for (TUint k = 1; cwd[k] && v < sizeof(vol) - 1; ++k)
                {
                if (cwd[k] == '/') vol[v++] = '\\';
                else vol[v++] = cwd[k];
                }
            if (v > 1 && vol[v - 1] != '\\') vol[v++] = '\\';
            }

        while (*p && p[0] != '/' && p[0] != '\\')
            {
            char part[13]; TUint m = 0;
            while (*p && *p != '/' && *p != '\\')
                {
                if (m >= 12) return KErrArgument;
                part[m++] = *p++;
                }
            part[m] = 0;
            while (*p == '/' || *p == '\\') ++p;

            // Normalise to 8.3 (R1Fat::Open/Prep83 rejects longer bases).
            char base[9], ext[4]; TUint b = 0, e = 0;
            TUint dotAt = m;
            for (TUint k = 0; k < m; ++k)
                if (part[k] == '.') { dotAt = k; break; }
            for (TUint k = 0; k < dotAt && b < 8; ++k) base[b++] = part[k];
            for (TUint k = dotAt + 1; k < m && e < 3; ++k) ext[e++] = part[k];
            part[0] = 0;
            for (TUint k = 0; k < b; ++k) part[k] = base[k];
            if (dotAt > 8)
                {
                // Long base: fold to first six characters + "~1".
                b = 6;
                part[b++] = '~'; part[b++] = '1';
                }
            part[b] = 0;
            if (e)
                {
                part[b++] = '.';
                for (TUint k = 0; k < e; ++k) part[b++] = ext[k];
                part[b] = 0;
                }
            for (TUint k = 0; part[k] && v < sizeof(vol) - 2; ++k)
                vol[v++] = (part[k] == '/') ? '\\' : part[k];
            vol[v++] = '\\';
            }
        vol[v ? v - 1 : 0] = 0;  // drop trailing separator
        return R1Fat::Open(drive, vol, EFalse, iFile);
        }

    TInt DPortBigFile::NextCluster(TUint32 aClus, TUint32& aNext)
        {
        TUint32 sec = iFatLba + (aClus * 4) / 512;
        if (sec != iFatCacheLba)
            {
            TInt r = AtaRead(iFile.device, sec, 1, iFatCache);
            if (r) return r;
            iFatCacheLba = sec;
            }
        aNext = LfRd32(iFatCache + (aClus * 4) % 512) & 0x0FFFFFFFu;
        return KErrNone;
        }

    TInt DPortBigFile::GotoCluster(TUint32 aIndex)
        {
        if (aIndex == iClusIdx) return KErrNone;
        if (aIndex < iClusIdx)
            { iCurClus = iFirstClus; iClusIdx = 0; iSecInClus = 0; }
        while (iClusIdx < aIndex)
            {
            TUint32 next;
            TInt r = NextCluster(iCurClus, next);
            if (r) return r;
            if (next < 2 || next >= KLfEoc) return KErrCorrupt;
            iCurClus = next;
            ++iClusIdx;
            }
        return KErrNone;
        }

    TInt DPortBigFile::DoSeek(TUint aOffset)
        {
        if (aOffset > iSize) aOffset = iSize;
        if (!iSize || !iFirstClus)
            { iPos = aOffset; return KErrNone; }
        TUint32 index = (TUint32)(aOffset / (iSecPerClus * 512u));
        TInt r = GotoCluster(index);
        if (r) return r;
        iSecInClus = (TUint)((aOffset / 512u) % iSecPerClus);
        iPos = aOffset;
        return KErrNone;
        }

    TInt DPortBigFile::DoRead(TAny* aBuffer, TUint aOffset, TUint aLength)
        {
        if (!aBuffer && aLength) return KErrArgument;
        TInt r = DoSeek(aOffset);
        if (r) return r;
        TUint avail = iSize > iPos ? iSize - iPos : 0;
        TUint count = aLength < avail ? aLength : avail;
        TUint done = 0;
        TUint8* dst = (TUint8*)aBuffer;
        while (done < count)
            {
            TUint inSec = iPos % 512u;
            TUint n = 512u - inSec;
            if (n > count - done) n = count - done;
            r = AtaRead(iFile.device, ClusterLba(iCurClus) + iSecInClus, 1, iSector);
            if (r) return r;
            r = Kern::ThreadRawWrite(&Kern::CurrentThread(), dst + done,
                                     iSector + inSec, (TInt)n);
            if (r) return r;
            done += n; iPos += n;
            if (++iSecInClus >= iSecPerClus)
                {
                TUint32 next;
                r = NextCluster(iCurClus, next);
                if (r) return r;
                if (next < KLfEoc)
                    { iCurClus = next; ++iClusIdx; iSecInClus = 0; }
                else if (done < count)
                    return KErrCorrupt;   // directory size outlives the FAT chain
                }
            }
        return (TInt)done;
        }

    TInt DPortBigFile::Request(TInt aFunction, TAny* a1, TAny* a2)
        {
        DThread& cur = Kern::CurrentThread();
        DProcess* proc = cur.iOwningProcess;
        if (cur.iId != iOwnerTid || (proc ? proc->iId : 0) != iOwnerPid)
            return KErrAccessDenied;

        UserFilesLock();   // serialise with all FAT/ATA activity (R1Fat contract)
        TInt r = KErrNotSupported;
        if (aFunction == 0)
            {
            r = OpenPath(*(const TDesC8*)a1);
            if (r == KErrNone && !iOpened)
                {
                // Snapshot the volume geometry and directory entry once.  R1Fat
                // has already validated the lookup; we only need the raw values
                // to walk the chain incrementally (no per-sector FAT re-scan).
                TUint8 boot[512];
                r = AtaRead(iFile.device, iFile.partition, 1, boot);
                if (r == KErrNone)
                    {
                    iSecPerClus = boot[13] ? boot[13] : 1;
                    TUint32 rsvd = LfRd16(boot + 14);
                    TUint nfats = boot[16] ? boot[16] : 1;
                    TUint32 fatSz = LfRd32(boot + 36);
                    if (!iSecPerClus || iSecPerClus > 128 || !fatSz)
                        r = KErrCorrupt;
                    else
                        {
                        iFatLba = iFile.partition + rsvd;
                        iDataLba = iFatLba + nfats * fatSz;
                        TUint8 ent[512];
                        r = AtaRead(iFile.device,
                                    ClusterLba(iFile.dirCluster) + iFile.dirSector,
                                    1, ent);
                        if (r == KErrNone)
                            {
                            const TUint8* e = ent + iFile.dirIndex * 32;
                            iSize = LfRd32(e + 28);
                            iFirstClus = ((TUint32)LfRd16(e + 20) << 16) |
                                          LfRd16(e + 26);
                            iCurClus = iFirstClus;
                            iClusIdx = 0; iSecInClus = 0; iPos = 0;
                            iFatCacheLba = (TUint32)-1;
                            iOpened = ETrue;
                            }
                        }
                    }
                if (r != KErrNone)
                    {
                    R1Fat::Close(iFile);   // still under UserFilesLock
                    iSize = 0; iFirstClus = 0;
                    }
                }
            }
        else if (aFunction == 1)
            { r = (TInt)iSize; }
        else if (aFunction == 2)
            {
            LfIo io;
            r = Kern::ThreadRawRead(&cur, a1, &io, sizeof(io));
            if (!r) r = DoRead(io.buffer, io.offset, io.length);
            }
        else if (aFunction == 3)
            { r = DoSeek((TUint)a1); }
        else if (aFunction == 4)
            {
            LfSeq seq;
            r = Kern::ThreadRawRead(&cur, a1, &seq, sizeof(seq));
            if (!r) r = DoRead(seq.buffer, iPos, seq.length);
            }
        (void)a2;
        UserFilesUnlock();
        return r;
        }

    class DPortBigFileDevice : public DLogicalDevice
        {
    public:
        TInt Install() { _LIT(KName, "PortBigFile"); return SetName(&KName); }
        void GetCaps(TDes8& aDes) const { Kern::InfoCopy(aDes, 0, 0); }
        TInt Create(DLogicalChannelBase*& aChannel)
            { aChannel = new DPortBigFile; return aChannel ? KErrNone : KErrNoMemory; }
        };
    }

static TInt UserBigFileInit()
    {
    NKern::ThreadEnterCS();
    DPortBigFileDevice* device = new DPortBigFileDevice;
    TInt r = device ? Kern::InstallLogicalDevice(device) : KErrNoMemory;
    if (r != KErrNone && device) device->Close(NULL);
    NKern::ThreadLeaveCS();
    return r;
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
    TInt big = UserBigFileInit();
    if (big != KErrNone)
        return big;   // large-file streaming device is mandatory
    NKern::ThreadEnterCS();
    DPortConDevice* device = new DPortConDevice;
    TInt r = device ? Kern::InstallLogicalDevice(device) : KErrNoMemory;
    if (r != KErrNone && device) device->Close(NULL);
    NKern::ThreadLeaveCS();
    return r;
    }

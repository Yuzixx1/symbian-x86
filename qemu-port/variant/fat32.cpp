// fat32.cpp — MBR partitions + FAT32 read/write/format on ATA (PIO).
//
// Scope: up to 8 mounted volumes (C:, D:, ...) across drives/partitions,
// each with its own CWD; 8.3 writes, LFN reads, files + dirs (mkdir/del),
// whole-disk `format`. No FAT cache: every FAT update writes all copies
// through immediately (slow, obviously correct).
// Single-threaded shell use only (one shared sector buffer).
//
// References: Microsoft FAT32 spec (BPB, FAT, directory entries). Variant
// code; no genuine sources involved.

#include <x86.h>
#include <e32err.h>
#include "qemu_variant.h"

namespace QemuUart
    {
    void PutC(char aC);
    }

static void EmitRaw(char aC)
    {
    QemuUart::PutC(aC);
    VgaTeletype(aC);
    }

static void EmitRawS(const char* aS)
    {
    while (*aS) EmitRaw(*aS++);
    }

static TUint16 Rd16(const TUint8* p) { return (TUint16)(p[0] | (p[1] << 8)); }
static TUint32 Rd32(const TUint8* p)
    { return (TUint32)p[0] | ((TUint32)p[1] << 8) | ((TUint32)p[2] << 16) | ((TUint32)p[3] << 24); }
static void Wr16(TUint8* p, TUint16 v) { p[0] = (TUint8)v; p[1] = (TUint8)(v >> 8); }
static void Wr32(TUint8* p, TUint32 v)
    { p[0] = (TUint8)v; p[1] = (TUint8)(v >> 8); p[2] = (TUint8)(v >> 16); p[3] = (TUint8)(v >> 24); }

namespace
    {
    const TUint32 kEoc = 0x0FFFFFF8u;
    const TUint32 kFree = 0u;

    TBool sMounted = EFalse;
    int sDev = 0;
    TUint32 sPartLba = 0;
    TUint32 sFatLba = 0;
    TUint32 sDataLba = 0;
    TUint sSecPerClus = 0, sRsvd = 0, sNumFats = 0;
    TUint32 sFatSz = 0, sRootClus = 2, sTotalClus = 0;
    TUint32 sCwdClus = 2;
    TUint32 sCwdStack[8];
    char sCwdNames[8][13];
    TInt sCwdDepth = 0;
    TUint8 sSec[512];

    // Multi-volume state: the s* globals above are the ACTIVE volume's
    // working copy; each mounted volume keeps its own in the table.
    struct VolState
        {
        TBool mounted;
        char letter;
        int dev;
        TUint32 partLba, fatLba, dataLba;
        TUint secPerClus, rsvd, numFats;
        TUint32 fatSz, rootClus, totalClus;
        TUint32 cwdClus;
        TUint32 cwdStack[8];
        char cwdNames[8][13];
        TInt cwdDepth;
        };
    VolState sVols[8];
    TInt sCur = -1;

    char FreeLetter()
        {
        for (char l = 'C'; l <= 'Z'; ++l)
            {
            TBool used = EFalse;
            for (int i = 0; i < 8; ++i)
                if (sVols[i].mounted && sVols[i].letter == l)
                    { used = ETrue; break; }
            if (!used)
                return l;
            }
        return 0;
        }

    void VolToGlobals(const VolState& aV)
        {
        sMounted = aV.mounted;
        sDev = aV.dev;
        sPartLba = aV.partLba;
        sFatLba = aV.fatLba;
        sDataLba = aV.dataLba;
        sSecPerClus = aV.secPerClus;
        sRsvd = aV.rsvd;
        sNumFats = aV.numFats;
        sFatSz = aV.fatSz;
        sRootClus = aV.rootClus;
        sTotalClus = aV.totalClus;
        sCwdClus = aV.cwdClus;
        for (int i = 0; i < 8; ++i)
            {
            sCwdStack[i] = aV.cwdStack[i];
            for (int k = 0; k < 13; ++k)
                sCwdNames[i][k] = aV.cwdNames[i][k];
            }
        sCwdDepth = aV.cwdDepth;
        }

    void GlobalsToVol(VolState& aV)
        {
        aV.mounted = sMounted;
        aV.dev = sDev;
        aV.partLba = sPartLba;
        aV.fatLba = sFatLba;
        aV.dataLba = sDataLba;
        aV.secPerClus = sSecPerClus;
        aV.rsvd = sRsvd;
        aV.numFats = sNumFats;
        aV.fatSz = sFatSz;
        aV.rootClus = sRootClus;
        aV.totalClus = sTotalClus;
        aV.cwdClus = sCwdClus;
        for (int i = 0; i < 8; ++i)
            {
            aV.cwdStack[i] = sCwdStack[i];
            for (int k = 0; k < 13; ++k)
                aV.cwdNames[i][k] = sCwdNames[i][k];
            }
        aV.cwdDepth = sCwdDepth;
        }

    void SaveCur()
        {
        if (sCur >= 0)
            GlobalsToVol(sVols[sCur]);
        }

    void LoadCur()
        {
        VolToGlobals(sVols[sCur]);
        }

    TUint32 ClusLba(TUint32 aClus)  // absolute LBA of first sector
        {
        return sPartLba + sDataLba + (aClus - 2) * sSecPerClus;
        }

    TInt FatGet(TUint32 aClus, TUint32& aVal)
        {
        if (aClus < 2)
            return KErrArgument;
        TUint32 off = aClus * 4;
        TUint32 sec = sFatLba + off / 512;
        TInt r = AtaRead(sDev, sec, 1, sSec);
        if (r != KErrNone)
            return r;
        aVal = Rd32(sSec + off % 512) & 0x0FFFFFFFu;
        return KErrNone;
        }

    TInt FatSet(TUint32 aClus, TUint32 aVal)
        {
        if (aClus < 2)
            return KErrArgument;
        TUint32 off = aClus * 4;
        TUint32 sec = sFatLba + off / 512;
        for (TUint f = 0; f < sNumFats; ++f)
            {
            TInt r = AtaRead(sDev, sec + f * sFatSz, 1, sSec);
            if (r != KErrNone)
                return r;
            TUint32 cur = Rd32(sSec + off % 512);
            cur = (cur & 0xF0000000u) | (aVal & 0x0FFFFFFFu);
            Wr32(sSec + off % 512, cur);
            r = AtaWrite(sDev, sec + f * sFatSz, 1, sSec);
            if (r != KErrNone)
                return r;
            }
        return KErrNone;
        }

    // Best-effort FSInfo free-count maintenance (cosmetic if it fails).
    void FsInfoAdjust(TInt aDelta)
        {
        TUint8 fs[512];
        if (AtaRead(sDev, sPartLba + 1, 1, fs) != KErrNone)
            return;
        if (Rd32(fs) != 0x41615252u || Rd32(fs + 484) != 0x61417272u)
            return;
        TInt free = (TInt)Rd32(fs + 488) + aDelta;
        if (free < 0)
            free = 0;
        Wr32(fs + 488, (TUint32)free);
        AtaWrite(sDev, sPartLba + 1, 1, fs);
        }

    // Allocate one zeroed cluster; 0 = disk full.
    TInt AllocClus(TUint32& aClus)
        {
        for (TUint32 c = 2; c < sTotalClus + 2; ++c)
            {
            TUint32 v = 1;
            if (FatGet(c, v) != KErrNone)
                return KErrGeneral;
            if (v != kFree)
                continue;
            if (FatSet(c, kEoc) != KErrNone)
                return KErrGeneral;
            for (TUint s = 0; s < sSecPerClus; ++s)
                {
                TUint32 lba = ClusLba(c) + s;
                // Zero via the scratch buffer (careful: sSec is shared).
                for (int i = 0; i < 512; ++i)
                    sSec[i] = 0;
                TInt r = AtaWrite(sDev, lba, 1, sSec);
                if (r != KErrNone)
                    return r;
                }
            aClus = c;
            FsInfoAdjust(-1);
            return KErrNone;
            }
        return KErrNoMemory;  // disk full
        }

    void FreeChain(TUint32 aStart)
        {
        TUint32 c = aStart;
        TInt freed = 0;
        while (c >= 2 && c < kEoc)
            {
            TUint32 next = kEoc;
            if (FatGet(c, next) != KErrNone)
                return;
            FatSet(c, kFree);
            ++freed;
            c = next;
            }
        if (freed)
            FsInfoAdjust(freed);
        }

    // ---- 8.3 name handling ----
    TBool NameOk(char aC)
        {
        if ((aC >= 'A' && aC <= 'Z') || (aC >= '0' && aC <= '9'))
            return ETrue;
        const char* ok = "$%'-_@~`!(){}^#&";
        for (const char* p = ok; *p; ++p)
            if (*p == aC)
                return ETrue;
        return EFalse;
        }

    // Returns KErrNone + 11-char padded name, or KErrArgument.
    TInt Prep83(const char* aName, char* aOut11)
        {
        for (int i = 0; i < 11; ++i)
            aOut11[i] = ' ';
        int ni = 0, dot = -1;
        for (; aName[ni] && ni < 13; ++ni)
            if (aName[ni] == '.')
                { dot = ni; break; }
        int baseLen = (dot < 0) ? ni : dot;
        if (aName[ni] != 0 && dot < 0)
            return KErrArgument;  // no dot, too long
        int extLen = 0;
        if (dot >= 0)
            {
            for (extLen = 0; aName[dot + 1 + extLen]; ++extLen)
                if (extLen > 3)
                    return KErrArgument;
            }
        if (baseLen < 1 || baseLen > 8 || extLen > 3)
            return KErrArgument;
        for (int i = 0; i < baseLen; ++i)
            {
            char c = aName[i];
            if (c >= 'a' && c <= 'z')
                c = (char)(c - 32);
            if (!NameOk(c))
                return KErrArgument;
            aOut11[i] = c;
            }
        for (int i = 0; i < extLen; ++i)
            {
            char c = aName[dot + 1 + i];
            if (c >= 'a' && c <= 'z')
                c = (char)(c - 32);
            if (!NameOk(c))
                return KErrArgument;
            aOut11[8 + i] = c;
            }
        return KErrNone;
        }

    int CaseEq(const char* a, const char* b)
        {
        for (;; ++a, ++b)
            {
            char ca = *a, cb = *b;
            if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 32);
            if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 32);
            if (ca != cb) return 0;
            if (!ca) return 1;
            }
        }

    // ---- directory scan ----
    // Locate `aName` in CWD. On success fills entry location + metadata.
    // Handles LFN (display/compare) + 8.3 fallback. Deleted/end markers ok.
    struct Found
        {
        TUint32 dirClus;   // cluster holding the short entry
        TUint dirSec;      // sector index within cluster
        TUint dirIdx;      // entry index within sector
        TUint8 attr;
        TUint32 firstClus;
        TUint32 size;
        };
    }

static void LfnToAscii(const TUint8* aEnt, char* aDst, int aMax)
    {
    // One LFN entry (32B at aEnt) appends up to 13 UCS-2 chars (caller
    // prepends in sequence order; sequence runs backwards on disk).
    static const TUint8 offs[13] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
    int k = 0;
    while (aDst[k] && k < aMax - 1)
        ++k;
    for (int i = 0; i < 13 && k < aMax - 1; ++i)
        {
        TUint16 ch = (TUint16)(aEnt[offs[i]] | (aEnt[offs[i] + 1] << 8));
        if (ch == 0x0000 || ch == 0xFFFF)
            break;
        aDst[k++] = (ch < 128) ? (char)ch : '?';
        }
    aDst[k] = 0;
    }

static TInt DirFind(const char* aName, Found& aFound)
    {
    // Returns KErrNone (found), KErrNotFound, or other error.
    char want83[11];
    TBool has83 = (Prep83(aName, want83) == KErrNone);
    TUint32 clus = sCwdClus;
    char lfn[256];
    lfn[0] = 0;
    TBool haveLfn = EFalse;
    while (clus >= 2 && clus < kEoc)
        {
        for (TUint s = 0; s < sSecPerClus; ++s)
            {
            TInt r = AtaRead(sDev, ClusLba(clus) + s, 1, sSec);
            if (r != KErrNone)
                return r;
            for (TUint e = 0; e < 16; ++e)
                {
                const TUint8* en = sSec + e * 32;
                if (en[0] == 0x00)
                    return KErrNotFound;  // end of directory
                if (en[0] == 0xE5)
                    { lfn[0] = 0; haveLfn = EFalse; continue; }
                if (en[11] == 0x0F)
                    {
                    // LFN part: sequence runs N..1 on disk; prepend each.
                    char part[14];
                    part[0] = 0;
                    LfnToAscii(en, part, 14);
                    char tmp[256];
                    int i = 0;
                    while (part[i] && i < 255) { tmp[i] = part[i]; ++i; }
                    int j = 0;
                    while (lfn[j] && i < 255) { tmp[i++] = lfn[j++]; }
                    tmp[i] = 0;
                    for (int k = 0; tmp[k]; ++k) lfn[k] = tmp[k];
                    lfn[i] = 0;
                    haveLfn = ETrue;
                    continue;
                    }
                TBool match = EFalse;
                if (haveLfn && CaseEq(lfn, aName))
                    match = ETrue;
                else if (has83)
                    {
                    int k = 0;
                    for (; k < 11; ++k)
                        if (en[k] != (TUint8)want83[k])
                            break;
                    match = (k == 11);
                    }
                lfn[0] = 0;
                haveLfn = EFalse;
                if ((en[11] & 0x08) && !match)
                    continue;  // volume label, never matches
                if (!match)
                    continue;
                aFound.dirClus = clus;
                aFound.dirSec = s;
                aFound.dirIdx = e;
                aFound.attr = en[11];
                aFound.firstClus = (TUint32)Rd16(en + 20) << 16 | Rd16(en + 26);
                aFound.size = Rd32(en + 28);
                return KErrNone;
                }
            }
        TUint32 next = kEoc;
        TInt r = FatGet(clus, next);
        if (r != KErrNone)
            return r;
        clus = next;
        }
    return KErrNotFound;
    }

// Find a free slot in CWD (extends the directory by one cluster if full).
static TInt DirAllocSlot(Found& aSlot)
    {
    TUint32 clus = sCwdClus;
    for (;;)
        {
        for (TUint s = 0; s < sSecPerClus; ++s)
            {
            TInt r = AtaRead(sDev, ClusLba(clus) + s, 1, sSec);
            if (r != KErrNone)
                return r;
            for (TUint e = 0; e < 16; ++e)
                {
                TUint8 c0 = sSec[e * 32];
                if (c0 == 0x00 || c0 == 0xE5)
                    {
                    aSlot.dirClus = clus;
                    aSlot.dirSec = s;
                    aSlot.dirIdx = e;
                    return KErrNone;
                    }
                }
            }
        // Full: append a cluster (zeroed by AllocClus).
        TUint32 next = kEoc;
        TInt r = FatGet(clus, next);
        if (r != KErrNone)
            return r;
        if (next < kEoc)
            { clus = next; continue; }
        TUint32 nc = 0;
        r = AllocClus(nc);
        if (r != KErrNone)
            return r;
        r = FatSet(clus, nc);
        if (r != KErrNone)
            return r;
        aSlot.dirClus = nc;
        aSlot.dirSec = 0;
        aSlot.dirIdx = 0;
        return KErrNone;
        }
    }

// Write a 32B entry at a slot (reads-modifies-writes the sector).
static TInt DirWriteAt(const Found& aSlot, const TUint8* aEnt)
    {
    TInt r = AtaRead(sDev, ClusLba(aSlot.dirClus) + aSlot.dirSec, 1, sSec);
    if (r != KErrNone)
        return r;
    for (int i = 0; i < 32; ++i)
        sSec[aSlot.dirIdx * 32 + i] = aEnt[i];
    return AtaWrite(sDev, ClusLba(aSlot.dirClus) + aSlot.dirSec, 1, sSec);
    }

// Stamp create/write/access dates from the RTC (zeros if unreadable).
static void StampNow(TUint8* aEnt)
    {
    TInt s = QemuRtcSeconds();
    if (s < 0)
        return;
    static const TInt mdays[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};
    TInt days = s / 86400;
    TInt rem = s % 86400;
    TInt year = 2000;
    for (;;)
        {
        TInt leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
        TInt ydays = leap ? 366 : 365;
        if (days < ydays) break;
        days -= ydays;
        ++year;
        }
    TInt mon = 1;
    for (;;)
        {
        TInt leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
        TInt md = mdays[mon - 1] + ((mon == 2 && leap) ? 1 : 0);
        if (days < md) break;
        days -= md;
        ++mon;
        }
    TUint16 date = (TUint16)(((year - 1980) << 9) | (mon << 5) | (days + 1));
    TUint16 time = (TUint16)(((rem / 3600) << 11) | (((rem % 3600) / 60) << 5) | ((rem % 60) / 2));
    Wr16(aEnt + 14, time);   // create time
    Wr16(aEnt + 16, date);    // create date
    Wr16(aEnt + 18, date);    // last access date
    Wr16(aEnt + 22, time);    // write time
    Wr16(aEnt + 24, date);    // write date (this is what dir listings show)
    }

static void Make83Ent(TUint8* aEnt, const char* aName11, TUint8 aAttr,
                      TUint32 aFirstClus, TUint32 aSize)
    {
    for (int i = 0; i < 32; ++i)
        aEnt[i] = 0;
    for (int i = 0; i < 11; ++i)
        aEnt[i] = (TUint8)aName11[i];
    aEnt[11] = aAttr;
    Wr16(aEnt + 20, (TUint16)(aFirstClus >> 16));
    Wr16(aEnt + 26, (TUint16)aFirstClus);
    Wr32(aEnt + 28, aSize);
    }

static void Show83(const TUint8* aEnt, char* aDst)
    {
    int k = 0, e = 8;
    while (e > 0 && aEnt[e - 1] == ' ')
        --e;
    for (int i = 0; i < 8 && aEnt[i] != ' '; ++i)
        aDst[k++] = (char)aEnt[i];
    (void)e;
    int x = 8;
    while (x < 11 && aEnt[x] == ' ')
        ++x;
    if (x < 11)
        {
        aDst[k++] = '.';
        for (; x < 11 && aEnt[x] != ' '; ++x)
            aDst[k++] = (char)aEnt[x];
        }
    aDst[k] = 0;
    }

TInt FatParts(int aDev)
    {
    AtaInit();
    if (!AtaPresent(aDev))
        return KErrNotFound;
    TUint8 mbr[512];
    TInt r = AtaRead(aDev, 0, 1, mbr);
    if (r != KErrNone)
        return r;
    if (mbr[510] != 0x55 || mbr[511] != 0xAA)
        {
        KPrintf("dev %d: no MBR signature (unformatted?)\r\n", aDev);
        return KErrNotFound;
        }
    int fats = 0;
    for (int i = 0; i < 4; ++i)
        {
        const TUint8* p = mbr + 446 + i * 16;
        TUint8 type = p[4];
        if (!type)
            continue;
        TUint32 lba = Rd32(p + 8), cnt = Rd32(p + 12);
        const char* tag = (type == 0x0B || type == 0x0C) ? "FAT32" : "";
        KPrintf("dev %d part %d: type=0x%02x lba=%u size=%uMB %s\r\n",
            aDev, i, type, lba, cnt / 2048, tag);
        if (type == 0x0B || type == 0x0C)
            ++fats;
        }
    if (!fats)
        KPrintf("dev %d: no FAT32 partitions\r\n", aDev);
    return fats ? KErrNone : KErrNotFound;
    }

static TInt FatMountInner(int aDev, int aPart)
    {
    AtaInit();
    if (!AtaPresent(aDev))
        return KErrNotFound;
    TUint8 mbr[512];
    TInt r = AtaRead(aDev, 0, 1, mbr);
    if (r != KErrNone)
        return r;
    if (mbr[510] != 0x55 || mbr[511] != 0xAA)
        return KErrNotFound;
    TUint32 partLba = 0;
    int seen = -1;
    for (int i = 0; i < 4; ++i)
        {
        const TUint8* p = mbr + 446 + i * 16;
        TUint8 type = p[4];
        if (type != 0x0B && type != 0x0C)
            continue;
        ++seen;
        if (aPart < 0 || aPart == i)
            {
            partLba = Rd32(p + 8);
            break;
            }
        }
    if (!partLba)
        return KErrNotFound;
    TUint8 bpb[512];
    r = AtaRead(aDev, partLba, 1, bpb);
    if (r != KErrNone)
        return r;
    if (bpb[510] != 0x55 || bpb[511] != 0xAA)
        return KErrCorrupt;
    if (Rd16(bpb + 11) != 512)
        return KErrNotSupported;
    TUint spc = bpb[13];
    if (spc == 0 || (spc & (spc - 1)))
        return KErrCorrupt;
    TUint32 fatSz = Rd32(bpb + 36);
    if (!fatSz)
        return KErrCorrupt;
    sDev = aDev;
    sPartLba = partLba;
    sRsvd = Rd16(bpb + 14);
    sNumFats = bpb[16];
    sSecPerClus = spc;
    sFatSz = fatSz;
    sFatLba = partLba + sRsvd;
    sDataLba = 0;  // relative; ClusLba adds sPartLba
    {
    TUint32 totSec = Rd32(bpb + 32);
    if (!totSec)
        totSec = Rd16(bpb + 19);
    TUint32 dataSec = totSec - sRsvd - sNumFats * sFatSz;
    sTotalClus = dataSec / spc;
    sDataLba = sRsvd + sNumFats * sFatSz;
    }
    sRootClus = Rd32(bpb + 44);
    if (sRootClus < 2)
        return KErrCorrupt;
    sCwdClus = sRootClus;
    sCwdDepth = 0;
    sMounted = ETrue;
    return KErrNone;
    }

// Multi-volume front end: same dev+partition reuses its letter (and resets
// its cwd to root); a new volume takes the next free slot/letter and becomes
// active. A failed mount leaves the previously active volume untouched.
TInt FatMount(int aDev, int aPart)
    {
    VolState keep;
    TBool had = (sCur >= 0);
    if (had)
        keep = sVols[sCur];
    TInt r = FatMountInner(aDev, aPart);
    if (r != KErrNone)
        {
        if (had)
            VolToGlobals(keep);
        else
            sMounted = EFalse;
        return r;
        }
    int slot = -1;
    for (int i = 0; i < 8; ++i)
        if (sVols[i].mounted && sVols[i].dev == aDev && sVols[i].partLba == sPartLba)
            { slot = i; break; }
    if (slot < 0)
        {
        for (int i = 0; i < 8; ++i)
            if (!sVols[i].mounted)
                { slot = i; break; }
        if (slot < 0)
            {
            if (had)
                VolToGlobals(keep);
            else
                sMounted = EFalse;
            return KErrOverflow;
            }
        sVols[slot].letter = FreeLetter();
        if (!sVols[slot].letter)
            {
            sVols[slot].mounted = EFalse;
            if (had)
                VolToGlobals(keep);
            else
                sMounted = EFalse;
            return KErrOverflow;
            }
        }
    sCur = slot;
    SaveCur();
    return KErrNone;
    }

char FatCurLetter()
    {
    if (sCur < 0 || !sVols[sCur].mounted)
        return 0;
    return sVols[sCur].letter;
    }

TInt FatUse(char aLetter)
    {
    if (aLetter >= 'a' && aLetter <= 'z')
        aLetter -= 32;
    for (int i = 0; i < 8; ++i)
        if (sVols[i].mounted && sVols[i].letter == aLetter)
            {
            SaveCur();
            sCur = i;
            LoadCur();
            return KErrNone;
            }
    return KErrNotFound;
    }

void FatList()
    {
    int n = 0;
    for (int i = 0; i < 8; ++i)
        {
        if (!sVols[i].mounted)
            continue;
        ++n;
        KPrintf("%c: dev %d partlba %u %s\r\n", sVols[i].letter,
            sVols[i].dev, sVols[i].partLba, i == sCur ? "*" : "");
        }
    if (!n)
        KPrintf("no volumes mounted (use `mount`)\r\n");
    }

void FatPrompt()
    {
    char l = FatCurLetter();
    if (!l)
        {
        EmitRawS("EKA2> ");
        return;
        }
    EmitRaw(l);
    EmitRaw(':');
    EmitRaw('/');
    for (TInt d = 0; d < sCwdDepth; ++d)
        {
        const char* p = sCwdNames[d];
        while (*p)
            EmitRaw(*p++);
        EmitRaw('/');
        }
    EmitRawS("> ");
    }

// Active volume's CWD as a backslash path for per-process inheritance.
// Caller must hold UserFilesLock (reads the switched-in volume globals).
void FatGetCwdPath(char* aOut, TUint aMax)
    {
    if (!aOut || aMax < 2)
        return;
    aOut[0] = '\\';
    aOut[1] = 0;
    if (!sMounted)
        return;
    TUint pos = 1;
    for (TInt d = 0; d < sCwdDepth && pos + 1 < aMax; ++d)
        {
        const char* p = sCwdNames[d];
        while (*p && pos + 1 < aMax)
            aOut[pos++] = *p++;
        if (pos + 1 < aMax)
            aOut[pos++] = '\\';
        }
    if (pos > 1 && pos < aMax && aOut[pos - 1] == '\\')
        aOut[pos - 1] = 0;  // "\A\B", root stays "\"
    else if (pos < aMax)
        aOut[pos] = 0;
    else
        aOut[aMax - 1] = 0;
    }

void FatDropDev(int aDev)
    {
    for (int i = 0; i < 8; ++i)
        if (sVols[i].mounted && sVols[i].dev == aDev)
            sVols[i].mounted = EFalse;
    if (sCur >= 0 && !sVols[sCur].mounted)
        {
        sCur = -1;
        sMounted = EFalse;
        }
    }

static TBool WildMatch(const char* aPat, const char* aName)
    {
    // Case-insensitive * and ? match (DOS-style).
    const char* p = aPat;
    const char* n = aName;
    const char* star = 0;
    const char* starN = 0;
    while (*n)
        {
        char pc = *p;
        char nc = *n;
        if (pc >= 'a' && pc <= 'z') pc -= 32;
        if (nc >= 'a' && nc <= 'z') nc -= 32;
        if (pc == '*')
            { star = ++p; starN = n; continue; }
        if (pc == '?' || pc == nc)
            { ++p; ++n; continue; }
        if (star)
            { p = star; n = ++starN; continue; }
        return EFalse;
        }
    while (*p == '*') ++p;
    return *p == 0 ? ETrue : EFalse;
    }

static TInt DirList(TBool aWide, const char* aFilter = 0)
    {
    if (!sMounted)
        return KErrNotReady;
    char l = FatCurLetter();
    if (l)
        {
        EmitRaw(l);
        EmitRaw(':');
        }
    EmitRaw('/');
    for (TInt d = 0; d < sCwdDepth; ++d)
        {
        const char* p = sCwdNames[d];
        while (*p)
            EmitRaw(*p++);
        EmitRaw('/');
        }
    EmitRawS("\r\n");
    TUint32 clus = sCwdClus;
    TUint columns = 0;
    TUint nFiles = 0, nDirs = 0, totalBytes = 0;
    char lfn[256];
    lfn[0] = 0;
    TBool haveLfn = EFalse;
    while (clus >= 2 && clus < kEoc)
        {
        for (TUint s = 0; s < sSecPerClus; ++s)
            {
            TInt r = AtaRead(sDev, ClusLba(clus) + s, 1, sSec);
            if (r != KErrNone)
                return r;
            for (TUint e = 0; e < 16; ++e)
                {
                const TUint8* en = sSec + e * 32;
                if (en[0] == 0x00)
                    {
                    if (aWide && columns) EmitRawS("\r\n");
                    if (aFilter && aFilter[0] && !nFiles && !nDirs)
                        return KErrNotFound;
                    KPrintf("%u file(s) %u bytes, %u dir(s)\r\n",
                        nFiles, totalBytes, nDirs);
                    return KErrNone;
                    }
                if (en[0] == 0xE5)
                    { lfn[0] = 0; haveLfn = EFalse; continue; }
                if (en[11] == 0x0F)
                    {
                    char part[14];
                    part[0] = 0;
                    LfnToAscii(en, part, 14);
                    char tmp[256];
                    int i = 0;
                    while (part[i] && i < 255) { tmp[i] = part[i]; ++i; }
                    int j = 0;
                    while (lfn[j] && i < 255) { tmp[i++] = lfn[j++]; }
                    tmp[i] = 0;
                    for (int k = 0; tmp[k]; ++k) lfn[k] = tmp[k];
                    lfn[i] = 0;
                    haveLfn = ETrue;
                    continue;
                    }
                if (en[11] & 0x08)
                    { lfn[0] = 0; haveLfn = EFalse; continue; }
                const char* show = 0;
                char shortn[13];
                if (haveLfn && lfn[0])
                    show = lfn;
                else
                    {
                    Show83(en, shortn);
                    show = shortn;
                    }
                TBool isDir = (en[11] & 0x10) ? ETrue : EFalse;
                TUint32 fsize = Rd32(en + 28);
                if (aFilter && aFilter[0] && !WildMatch(aFilter, show))
                    { lfn[0] = 0; haveLfn = EFalse; continue; }
                if (isDir) ++nDirs; else { ++nFiles; totalBytes += fsize; }
                TUint nameLen = 0;
                while (show[nameLen]) ++nameLen;
                if (aWide)
                    {
                    if (en[11] & 0x10) EmitRaw('[');
                    EmitRawS(show);
                    if (en[11] & 0x10) EmitRaw(']');
                    if (++columns == 3)
                        { EmitRawS("\r\n"); columns = 0; }
                    else
                        EmitRawS("  ");
                    }
                else if (isDir)
                    {
                    EmitRawS(show);
                    for (TUint i = nameLen; i < 16; ++i) EmitRaw(' ');
                    EmitRawS("<DIR>\r\n");
                    }
                else
                    {
                    EmitRawS(show);
                    for (TUint i = nameLen; i < 16; ++i) EmitRaw(' ');
                    KPrintf("%u bytes\r\n", fsize);
                    }
                lfn[0] = 0;  // reset AFTER printing: show may alias lfn
                haveLfn = EFalse;
                }
            }
        TUint32 next = kEoc;
        TInt r = FatGet(clus, next);
        if (r != KErrNone)
            return r;
        clus = next;
        }
    if (aWide && columns) EmitRawS("\r\n");
    if (aFilter && aFilter[0] && !nFiles && !nDirs)
        return KErrNotFound;
    KPrintf("%u file(s) %u bytes, %u dir(s)\r\n", nFiles, totalBytes, nDirs);
    return KErrNone;
    }

TInt FatDir() { return DirList(EFalse); }
TInt FatDirWide() { return DirList(ETrue); }

TInt FatDirPath(const char* aPath, TBool aWide)
    {
    if (!sMounted) return KErrNotReady;
    if (!aPath || !*aPath) return DirList(aWide);
    TUint32 oldClus = sCwdClus, oldStack[8];
    char oldNames[8][13];
    TInt oldDepth = sCwdDepth;
    for (int i = 0; i < 8; ++i)
        {
        oldStack[i] = sCwdStack[i];
        for (int j = 0; j < 13; ++j) oldNames[i][j] = sCwdNames[i][j];
        }
    TInt r = FatCdPath(aPath);
    if (r == KErrNone) r = DirList(aWide);
    if (r != KErrNone && r != KErrNotReady)
        {
        // Maybe the tail is a file pattern (DIR *.TXT, DIR C:\D\*.E32).
        // Split off the last component and retry as parent + filter.
        const char* slash = 0;
        for (const char* q = aPath; *q; ++q)
            if (*q == '/' || *q == '\\') slash = q;
        const char* leaf = slash ? slash + 1 : aPath;
        if (leaf[0])
            {
            char dirPart[128];
            if (!slash)
                dirPart[0] = 0;
            else if (slash == aPath)
                { dirPart[0] = '\\'; dirPart[1] = 0; }
            else
                {
                TUint n = (TUint)(slash - aPath);
                if (n > sizeof(dirPart) - 1) n = sizeof(dirPart) - 1;
                for (TUint i = 0; i < n; ++i) dirPart[i] = aPath[i];
                dirPart[n] = 0;
                }
            sCwdClus = oldClus;
            sCwdDepth = oldDepth;
            for (int i = 0; i < 8; ++i)
                {
                sCwdStack[i] = oldStack[i];
                for (int j = 0; j < 13; ++j) sCwdNames[i][j] = oldNames[i][j];
                }
            r = FatCdPath(dirPart[0] ? dirPart : 0);
            if (r == KErrNone) r = DirList(aWide, leaf);
            }
        }
    sCwdClus = oldClus;
    sCwdDepth = oldDepth;
    for (int i = 0; i < 8; ++i)
        {
        sCwdStack[i] = oldStack[i];
        for (int j = 0; j < 13; ++j) sCwdNames[i][j] = oldNames[i][j];
        }
    return r;
    }

TInt FatExists(const char* aName)
    {
    if (!sMounted) return KErrNotReady;
    Found f;
    TInt r = DirFind(aName, f);
    return r == KErrNone && (f.attr & 0x10) ? KErrAlreadyExists : r;
    }

TInt FatType(const char* aName)
    {
    if (!sMounted)
        return KErrNotReady;
    Found f;
    TInt r = DirFind(aName, f);
    if (r != KErrNone)
        return r;
    if (f.attr & 0x10)
        return KErrNotSupported;  // directory: use dir/cd
    TUint32 clus = f.firstClus, left = f.size;
    TInt prev = 0;
    while (left && clus >= 2 && clus < kEoc)
        {
        for (TUint s = 0; s < sSecPerClus && left; ++s)
            {
            r = AtaRead(sDev, ClusLba(clus) + s, 1, sSec);
            if (r != KErrNone)
                return r;
            TUint n = left > 512 ? 512 : left;
            for (TUint i = 0; i < n; ++i)
                {
                char c = (char)sSec[i];
                if (c == '\n' && prev != '\r')
                    EmitRaw('\r');
                EmitRaw(c);
                prev = c;
                }
            left -= n;
            }
        r = FatGet(clus, clus);
        if (r != KErrNone)
            return r;
        }
    if (prev != '\n' && prev != '\r')
        EmitRawS("\r\n");
    return KErrNone;
    }

TInt FatRead(const char* aName, TUint8* aBuf, TUint aMax, TUint& aOutLen)
    {
    aOutLen = 0;
    if (!sMounted)
        return KErrNotReady;
    if (!aBuf && aMax)
        return KErrArgument;
    Found f;
    TInt r = DirFind(aName, f);
    if (r != KErrNone)
        return r;
    if (f.attr & 0x10)
        return KErrNotSupported;  // directory: use dir/cd
    TUint32 clus = f.firstClus, left = f.size;
    if (left > aMax)
        return KErrTooBig;  // caller's buffer too small
    TUint8* dst = aBuf;
    if (left && clus < 2)  // nonzero size but no chain
        return KErrCorrupt;
    while (left && clus >= 2 && clus < kEoc)
        {
        for (TUint s = 0; s < sSecPerClus && left; ++s)
            {
            r = AtaRead(sDev, ClusLba(clus) + s, 1, sSec);
            if (r != KErrNone)
                return r;
            TUint n = left > 512 ? 512 : left;
            for (TUint i = 0; i < n; ++i)
                *dst++ = sSec[i];
            left -= n;
            }
        r = FatGet(clus, clus);
        if (r != KErrNone)
            return r;
        }
    if (left)
        return KErrCorrupt;  // chain ended early
    aOutLen = f.size;
    return KErrNone;
    }

TInt FatWrite(const char* aName, const TUint8* aData, TUint aLen)
    {
    if (!sMounted)
        return KErrNotReady;
    char name83[11];
    if (Prep83(aName, name83) != KErrNone)
        return KErrArgument;
    Found f;
    TInt r = DirFind(aName, f);
    TBool exists = (r == KErrNone);
    if (r != KErrNone && r != KErrNotFound)
        return r;
    if (exists)
        {
        if (f.attr & 0x10)
            return KErrNotSupported;
        // Keep the old chain attached until the new data and directory entry
        // have both been written. A full disk must not erase the old file.
        }
    else
        {
        r = DirAllocSlot(f);
        if (r != KErrNone)
            return r;
        }
    // Write data clusters (exact bytes; caller terminates lines).
    TUint32 first = 0, prev = 0;
    TUint done = 0;
    TUint8 buf[512];
    while (done < aLen)
        {
        TUint32 nc = 0;
        r = AllocClus(nc);
        if (r != KErrNone)
            {
            if (first) FreeChain(first);
            return r;
            }
        if (!first)
            first = nc;
        else
            {
            r = FatSet(prev, nc);
            if (r != KErrNone)
                {
                FreeChain(first);
                FreeChain(nc);
                return r;
                }
            }
        prev = nc;
        for (TUint s = 0; s < sSecPerClus && done < aLen; ++s)
            {
            // NOTE: sSec is the shared scratch buffer; stage in buf first.
            for (int i = 0; i < 512; ++i)
                buf[i] = 0;
            for (int i = 0; i < 512 && done < aLen; ++i, ++done)
                buf[i] = aData[done];
            r = AtaWrite(sDev, ClusLba(nc) + s, 1, buf);
            if (r != KErrNone)
                {
                FreeChain(first);
                return r;
                }
            }
        }
    TUint8 ent[32];
    Make83Ent(ent, name83, 0x20, first, aLen);
    StampNow(ent);
    if (exists)
        {
        // Preserve the old attribute byte, keeping archive set.
        TInt rr = AtaRead(sDev, ClusLba(f.dirClus) + f.dirSec, 1, sSec);
        if (rr != KErrNone)
            return rr;
        ent[11] = sSec[f.dirIdx * 32 + 11] | 0x20;
        }
    r = DirWriteAt(f, ent);
    if (r != KErrNone)
        {
        if (first) FreeChain(first);
        return r;
        }
    if (exists && f.firstClus) FreeChain(f.firstClus);
    return KErrNone;
    }

// Same-directory 8.3 rename: change just the short directory entry. No data
// clusters are copied and the old name remains if the sector write fails.
TInt FatRename(const char* aOld, const char* aNew)
    {
    if (!sMounted) return KErrNotReady;
    char old83[11], new83[11];
    if (Prep83(aOld, old83) != KErrNone || Prep83(aNew, new83) != KErrNone)
        return KErrArgument;
    Found oldEntry;
    TInt r = DirFind(aOld, oldEntry);
    if (r != KErrNone) return r;
    if (oldEntry.attr & (0x10 | 0x08)) return KErrNotSupported;
    // Refuse long-name entries: changing their short alias alone would leave
    // the associated LFN records pointing to an inconsistent checksum.
    r = AtaRead(sDev, ClusLba(oldEntry.dirClus) + oldEntry.dirSec, 1, sSec);
    if (r != KErrNone) return r;
    TUint8* entry = sSec + oldEntry.dirIdx * 32;
    for (int i = 0; i < 11; ++i)
        if (entry[i] != (TUint8)old83[i]) return KErrNotSupported;
    if (oldEntry.dirIdx == 0)
        {
        if (oldEntry.dirSec == 0)
            {
            // In the first root sector no preceding entry exists. At the
            // start of any other cluster, the predecessor is not local.
            if (oldEntry.dirClus != sRootClus) return KErrNotSupported;
            }
        else
            {
            r = AtaRead(sDev, ClusLba(oldEntry.dirClus) + oldEntry.dirSec - 1,
                        1, sSec);
            if (r != KErrNone) return r;
            if (sSec[15 * 32 + 11] == 0x0F) return KErrNotSupported;
            }
        }
    else if (sSec[(oldEntry.dirIdx - 1) * 32 + 11] == 0x0F)
        return KErrNotSupported;
    Found collision;
    r = DirFind(aNew, collision);
    if (r == KErrNone) return KErrAlreadyExists;
    if (r != KErrNotFound) return r;
    r = AtaRead(sDev, ClusLba(oldEntry.dirClus) + oldEntry.dirSec, 1, sSec);
    if (r != KErrNone) return r;
    entry = sSec + oldEntry.dirIdx * 32;
    for (int i = 0; i < 11; ++i) entry[i] = (TUint8)new83[i];
    return AtaWrite(sDev, ClusLba(oldEntry.dirClus) + oldEntry.dirSec, 1, sSec);
    }

TInt FatDel(const char* aName)
    {
    if (!sMounted)
        return KErrNotReady;
    Found f;
    TInt r = DirFind(aName, f);
    if (r != KErrNone)
        return r;
    if (f.attr & 0x10)
        {
        // Directories must be empty (only . and ..).
        TUint32 clus = f.firstClus;
        while (clus >= 2 && clus < kEoc)
            {
            for (TUint s = 0; s < sSecPerClus; ++s)
                {
                r = AtaRead(sDev, ClusLba(clus) + s, 1, sSec);
                if (r != KErrNone)
                    return r;
                for (TUint e = 0; e < 16; ++e)
                    {
                    const TUint8* en = sSec + e * 32;
                    if (en[0] == 0x00)
                        break;
                    if (en[0] == 0xE5 || en[11] == 0x0F)
                        continue;
                    // . and .. have attr dir; anything else blocks.
                    if (en[0] == '.')
                        continue;
                    return KErrInUse;
                    }
                }
            r = FatGet(clus, clus);
            if (r != KErrNone)
                return r;
            }
        }
    FreeChain(f.firstClus);
    r = AtaRead(sDev, ClusLba(f.dirClus) + f.dirSec, 1, sSec);
    if (r != KErrNone)
        return r;
    sSec[f.dirIdx * 32] = 0xE5;
    return AtaWrite(sDev, ClusLba(f.dirClus) + f.dirSec, 1, sSec);
    }

TInt FatMkdir(const char* aName)
    {
    if (!sMounted)
        return KErrNotReady;
    char name83[11];
    if (Prep83(aName, name83) != KErrNone)
        return KErrArgument;
    Found f;
    TInt r = DirFind(aName, f);
    if (r == KErrNone)
        return KErrAlreadyExists;
    if (r != KErrNotFound)
        return r;
    r = DirAllocSlot(f);
    if (r != KErrNone)
        return r;
    TUint32 nc = 0;
    r = AllocClus(nc);  // zeroed
    if (r != KErrNone)
        return r;
    // '.' and '..' entries in the first sector.
    TUint8 dot[32], dotdot[32];
    char dot83[11], dotdot83[11];
    for (int i = 0; i < 11; ++i)
        { dot83[i] = ' '; dotdot83[i] = ' '; }
    dot83[0] = '.';
    dotdot83[0] = '.'; dotdot83[1] = '.';
    Make83Ent(dot, dot83, 0x10, nc, 0);
    Make83Ent(dotdot, dotdot83, 0x10,
              (sCwdDepth ? sCwdClus : 0), 0);
    StampNow(dot);
    StampNow(dotdot);
    r = AtaRead(sDev, ClusLba(nc), 1, sSec);
    if (r != KErrNone)
        return r;
    for (int i = 0; i < 32; ++i)
        {
        sSec[i] = dot[i];
        sSec[32 + i] = dotdot[i];
        }
    r = AtaWrite(sDev, ClusLba(nc), 1, sSec);
    if (r != KErrNone)
        {
        FreeChain(nc);
        return r;
        }
    TUint8 ent[32];
    Make83Ent(ent, name83, 0x10, nc, 0);
    StampNow(ent);
    return DirWriteAt(f, ent);
    }

TInt FatCd(const char* aName)
    {
    if (!sMounted)
        return KErrNotReady;
    if (!aName || !aName[0] || (aName[0] == '\\' && !aName[1]) ||
        (aName[0] == '/' && !aName[1]))
        {
        sCwdClus = sRootClus;
        sCwdDepth = 0;
        return KErrNone;
        }
    if ((aName[0] == '.' && aName[1] == '.' && !aName[2]))
        {
        if (sCwdDepth > 0)
            {
            sCwdClus = sCwdStack[sCwdDepth - 1];
            --sCwdDepth;
            }
        return KErrNone;
        }
    Found f;
    TInt r = DirFind(aName, f);
    if (r != KErrNone)
        return r;
    if (!(f.attr & 0x10))
        return KErrNotSupported;
    if (sCwdDepth >= 8)
        return KErrOverflow;
    sCwdStack[sCwdDepth] = sCwdClus;
    int k = 0;
    while (aName[k] && k < 12)
        {
        char c = aName[k];
        sCwdNames[sCwdDepth][k] = c;
        ++k;
        }
    sCwdNames[sCwdDepth][k] = 0;
    sCwdClus = f.firstClus ? f.firstClus : sRootClus;
    ++sCwdDepth;
    return KErrNone;
    }

// Temporarily walk a path while preserving the active volume's CWD. Shell
// commands select the drive separately; these helpers only change directories.
namespace
    {
    struct CwdSnapshot
        {
        TUint32 clus, stack[8];
        char names[8][13];
        TInt depth;
        void Save()
            {
            clus = sCwdClus; depth = sCwdDepth;
            for (int i = 0; i < 8; ++i)
                {
                stack[i] = sCwdStack[i];
                for (int j = 0; j < 13; ++j) names[i][j] = sCwdNames[i][j];
                }
            }
        void Restore()
            {
            sCwdClus = clus; sCwdDepth = depth;
            for (int i = 0; i < 8; ++i)
                {
                sCwdStack[i] = stack[i];
                for (int j = 0; j < 13; ++j) sCwdNames[i][j] = names[i][j];
                }
            }
        };

    TInt WalkPath(const char* aPath, TBool aParent, const char*& aLeaf)
        {
        if (!sMounted) return KErrNotReady;
        if (!aPath) return KErrArgument;
        const char* p = aPath;
        if (*p == '/' || *p == '\\')
            {
            TInt r = FatCd(0);
            if (r != KErrNone) return r;
            ++p;
            }
        for (;;)
            {
            while (*p == '/' || *p == '\\') ++p;
            if (!*p)
                {
                if (aParent) return KErrArgument;
                aLeaf = 0; return KErrNone;
                }
            const char* begin = p;
            int n = 0;
            while (*p && *p != '/' && *p != '\\')
                {
                if (++n > 12) return KErrArgument;
                ++p;
                }
            const char* next = p;
            while (*next == '/' || *next == '\\') ++next;
            if (!*next && aParent)
                {
                if (*p) return KErrArgument; // file names cannot end in slash
                aLeaf = begin;
                return KErrNone;
                }
            char part[13];
            for (int i = 0; i < n; ++i) part[i] = begin[i];
            part[n] = 0;
            TInt r = (n == 1 && part[0] == '.') ? KErrNone : FatCd(part);
            if (r != KErrNone) return r;
            if (!*next) { aLeaf = 0; return KErrNone; }
            p = next;
            }
        }

    // WalkPath's leaf points into the input and is NUL-terminated when
    // called with aParent because a trailing slash is rejected above.
    TInt FilePath(const char* aPath, CwdSnapshot& aSave, const char*& aLeaf)
        {
        aSave.Save();
        return WalkPath(aPath, ETrue, aLeaf);
        }
    }

TInt FatCdPath(const char* aPath)
    {
    CwdSnapshot save; save.Save();
    const char* leaf = 0;
    TInt r = WalkPath(aPath ? aPath : "\\", EFalse, leaf);
    if (r != KErrNone) save.Restore();
    return r;
    }

TInt FatTypePath(const char* aPath)
    {
    CwdSnapshot save; const char* leaf = 0;
    TInt r = FilePath(aPath, save, leaf);
    if (r == KErrNone) r = FatType(leaf);
    save.Restore(); return r;
    }
TInt FatReadPath(const char* aPath, TUint8* aBuf, TUint aMax, TUint& aOutLen)
    {
    CwdSnapshot save; const char* leaf = 0;
    TInt r = FilePath(aPath, save, leaf);
    if (r == KErrNone) r = FatRead(leaf, aBuf, aMax, aOutLen);
    save.Restore(); return r;
    }
TInt FatWritePath(const char* aPath, const TUint8* aData, TUint aLen)
    {
    CwdSnapshot save; const char* leaf = 0;
    TInt r = FilePath(aPath, save, leaf);
    if (r == KErrNone) r = FatWrite(leaf, aData, aLen);
    save.Restore(); return r;
    }
TInt FatExistsPath(const char* aPath)
    {
    CwdSnapshot save; const char* leaf = 0;
    TInt r = FilePath(aPath, save, leaf);
    if (r == KErrNone) r = FatExists(leaf);
    save.Restore(); return r;
    }
TInt FatDelPath(const char* aPath)
    {
    CwdSnapshot save; const char* leaf = 0;
    TInt r = FilePath(aPath, save, leaf);
    if (r == KErrNone) r = FatDel(leaf);
    save.Restore(); return r;
    }
TInt FatMkdirPath(const char* aPath)
    {
    CwdSnapshot save; const char* leaf = 0;
    TInt r = FilePath(aPath, save, leaf);
    if (r == KErrNone) r = FatMkdir(leaf);
    save.Restore(); return r;
    }
TInt FatRenamePath(const char* aOld, const char* aNew)
    {
    // Resolve both parents and compare actual directory clusters; no moves.
    CwdSnapshot save; save.Save();
    const char* oldLeaf = 0;
    TInt r = WalkPath(aOld, ETrue, oldLeaf);
    TUint32 oldParent = sCwdClus;
    save.Restore();
    if (r != KErrNone) return r;
    const char* newLeaf = 0;
    r = WalkPath(aNew, ETrue, newLeaf);
    if (r == KErrNone && oldParent != sCwdClus) r = KErrNotSupported;
    if (r == KErrNone) r = FatRename(oldLeaf, newLeaf);
    save.Restore(); return r;
    }

TInt FatFormat(int aDev)
    {
    AtaInit();
    if (!AtaPresent(aDev))
        return KErrNotFound;
    TUint32 total = AtaSectors(aDev);
    if (total < 131072)  // <64MB: FAT32 needs >=65525 clusters
        return KErrArgument;
    const TUint32 kPartLba = 2048;
    if (total <= kPartLba + 4096)
        return KErrArgument;
    TUint32 partSects = total - kPartLba;
    TUint spc = 1;
    if (partSects >= 2097152)
        spc = 8;
    else if (partSects >= 524288)
        spc = 4;
    else if (partSects >= 262144)
        spc = 2;
    const TUint rsvd = 32, fats = 2;
    // Iterative FAT sizing (converges immediately at these sizes).
    TUint32 clusters = (partSects - rsvd) / spc;
    TUint32 fat = 0;
    for (int i = 0; i < 3; ++i)
        {
        fat = (clusters * 4 + 511) / 512;
        clusters = (partSects - rsvd - fats * fat) / spc;
        }
    fat = (clusters * 4 + 511) / 512;
    if (clusters < 65525)
        return KErrArgument;
    TUint8 mbr[512];
    for (int i = 0; i < 512; ++i)
        mbr[i] = 0;
    mbr[446] = 0x00;
    mbr[447] = 0x01; mbr[448] = 0x01; mbr[449] = 0x00;
    mbr[450] = 0x0C;
    mbr[451] = 0xFE; mbr[452] = 0xFF; mbr[453] = 0xFF;
    Wr32(mbr + 454, kPartLba);
    Wr32(mbr + 458, partSects);
    mbr[510] = 0x55; mbr[511] = 0xAA;
    FatDropDev(aDev);  // stale mounts of this disk die here, not later
    TInt r = AtaWrite(aDev, 0, 1, mbr);
    if (r != KErrNone)
        return r;
    for (int i = 0; i < 512; ++i)
        sSec[i] = 0;
    sSec[0] = 0xEB; sSec[1] = 0x58; sSec[2] = 0x90;
    const char* oem = "SYBIAN  ";
    for (int i = 0; i < 8; ++i)
        sSec[3 + i] = (TUint8)oem[i];
    Wr16(sSec + 11, 512);
    sSec[13] = (TUint8)spc;
    Wr16(sSec + 14, (TUint16)rsvd);
    sSec[16] = (TUint8)fats;
    Wr16(sSec + 17, 0);
    Wr16(sSec + 19, 0);
    sSec[21] = 0xF8;
    Wr16(sSec + 22, 0);
    sSec[24] = 63; sSec[25] = 0;
    sSec[26] = (TUint8)255; sSec[27] = 0;
    Wr32(sSec + 28, kPartLba);
    Wr32(sSec + 32, partSects);
    Wr32(sSec + 36, fat);
    Wr16(sSec + 40, 0);
    Wr16(sSec + 42, 0);
    Wr32(sSec + 44, 2);
    Wr16(sSec + 48, 1);
    Wr16(sSec + 50, 6);
    sSec[64] = 0x80;
    sSec[66] = 0x29;
    Wr32(sSec + 67, 0x53444259u);  // "Y B D S" volume id
    const char* vol = "SYBIAN DISK";
    for (int i = 0; i < 11; ++i)
        sSec[71 + i] = (TUint8)vol[i];
    const char* fst = "FAT32   ";
    for (int i = 0; i < 8; ++i)
        sSec[82 + i] = (TUint8)fst[i];
    sSec[510] = 0x55; sSec[511] = 0xAA;
    r = AtaWrite(aDev, kPartLba, 1, sSec);
    if (r != KErrNone)
        return r;
    r = AtaWrite(aDev, kPartLba + 6, 1, sSec);  // backup boot
    if (r != KErrNone)
        return r;
    // FSInfo at +1.
    for (int i = 0; i < 512; ++i)
        sSec[i] = 0;
    Wr32(sSec + 0, 0x41615252u);
    Wr32(sSec + 484, 0x61417272u);
    Wr32(sSec + 488, clusters - 1);
    Wr32(sSec + 492, 3);
    sSec[510] = 0x55; sSec[511] = 0xAA;
    r = AtaWrite(aDev, kPartLba + 1, 1, sSec);
    if (r != KErrNone)
        return r;
    // FATs: media + EOC, root cluster EOC.
    for (int i = 0; i < 512; ++i)
        sSec[i] = 0;
    Wr32(sSec + 0, 0x0FFFFFF8u);
    Wr32(sSec + 4, 0x0FFFFFFFu);
    Wr32(sSec + 8, kEoc);
    for (TUint f = 0; f < fats; ++f)
        {
        r = AtaWrite(aDev, kPartLba + rsvd + f * fat, 1, sSec);
        if (r != KErrNone)
            return r;
        }
    // Zero the root cluster, then stamp the volume label entry.
    for (int i = 0; i < 512; ++i)
        sSec[i] = 0;
    TUint32 rootLba = kPartLba + rsvd + fats * fat;
    for (TUint s = 0; s < spc; ++s)
        {
        r = AtaWrite(aDev, rootLba + s, 1, sSec);
        if (r != KErrNone)
            return r;
        }
    TUint8 vent[32];
    for (int i = 0; i < 32; ++i)
        vent[i] = 0;
    const char* vl = "SYBIAN DISK";
    for (int i = 0; i < 11; ++i)
        vent[i] = (TUint8)vl[i];
    vent[11] = 0x08;
    r = AtaRead(aDev, rootLba, 1, sSec);
    if (r != KErrNone)
        return r;
    for (int i = 0; i < 32; ++i)
        sSec[i] = vent[i];
    r = AtaWrite(aDev, rootLba, 1, sSec);
    if (r != KErrNone)
        return r;
    // Mount what we just made.
    return FatMount(aDev, 0);
    }

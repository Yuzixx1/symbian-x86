// e32load.cpp — in-kernel E32 image validator (stage 3a: validate + report).
//
// Mirrors the header-subset of E32ImageHeaderV::ValidateHeader /
// ValidateWholeImage from
//   kernelhwsrv/userlibandfileserver/fileserver/inc/f32image.h
// CRC tables + UID-checksum algorithm are the genuine ones from
//   kernelhwsrv/kernel/eka/euser/us_func.cpp
// (crcTab, CrcTab32, TCheckedUid::Check with KMaxCheckedUid=3).
// Host-side twin: tools/e32read.py (must agree on every verdict).
//
// Scope: uncompressed EXEs and DLLs, with code/data relocations, export
// directories, and EUSER-ordinal imports resolved from a real DLL file.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include <kernel/kern_priv.h>
#include <kernel/kernel.h>
#include <kernel/x86/x86.h>
#include <e32ldr_private.h>
#include "qemu_variant.h"
#include "qemu_fbblit.h"   // fb blit/info ops appended to port ctl exec 270
#include "../mmu/pd.h"  // M-03: per-process PD side-table (port-side only)

extern "C" TUint8 qemu_tss[];  // 104B bootstrap TSS (bootcpu_gas.S BSS)
static TUint8 sTssStack[4096];
static TUint sSvcLastPid = 0;   // S-02: pid of the most recent E32Run success

// --- single-owner guard for the genuine file server --------------------------
// EFILE publishes the protected kernel object "!FileServer".  Only one process
// may own that name: a second EFILE either fails CServerFs::Start() or steals
// the name when the first owner's thread exits (DServer::Close detaches every
// session with KErrServerTerminated, seen by F32CLIENT as -15).  The guard
// below refuses a second EFILE-family image from any boot path (genuine boot
// table or prove script shell command).
static TBool IsEwsrvProcess(const char* n);  // defined below; EWSRV shares UID3

// Genuine EFILE UID3 (also shared by genuine EWSRV, hence the name check).
// Aliases EFILEX/EFILERFS are the *same image*, so UID3 is authoritative.
const TUint32 KPortEfileUid3 = 0x100000bfu;

static TBool PortIsEfileName(const char* aName)
    {
    if (!aName) return EFalse;
    const char* base = aName;
    for (const char* p = aName; *p; ++p)
        if (*p == '\\' || *p == '/' || *p == ':') base = p + 1;
    // Boot name, prove aliases, and the name-patched RFS prove image.
    const char* names[4] = { "EFILE", "EFILEX", "EFILERFS", "EFSRV" };
    for (TInt k = 0; k < 4; ++k)
        {
        const char* n = names[k];
        TInt i = 0;
        while (n[i] && base[i] && (base[i] == n[i] || base[i] == n[i] + 32)) ++i;
        if (!n[i] && (!base[i] || base[i] == '.')) return ETrue;
        }
    return EFalse;
    }

// UID-keyed image test.  Name is only a fallback for a name-patched image;
// EWSRV is excluded explicitly because it carries the same UID3.
static TBool PortIsEfileImage(const TE32Info& aInfo, const char* aName)
    {
    if (IsEwsrvProcess(aName)) return EFalse;
    return (aInfo.iUid3 == KPortEfileUid3) || PortIsEfileName(aName);
    }

// Atomic single-owner claim: read AND set under NKern CS (no preemption
// between check and register).  Replaces the old TOCTOU name-only check.
static TBool sPortEfileOwner = EFalse;

// True when a live DServer already published the genuine file-server name.
// Must be called with the kernel unlocked (object-container mutex is legal).
static TBool PortFileServerPublished()
    {
    DObjectCon& servers = *K::Containers[EServer];
    servers.Wait();
    TFindHandle fh;
    TKName found;
    TBool yes = (servers.FindByName(fh, _L("!FileServer"), found) == KErrNone);
    servers.Signal();
    return yes;
    }

// True when a live EFILE-family process already exists.  Complements the
// published-name check: a just-launched EFILE has not published the name yet.
static TBool PortEfileProcessRunning()
    {
    DObjectCon& procs = *K::Containers[EProcess];
    procs.Wait();
    TBool yes = EFalse;
    for (TInt i = 0; i < procs.Count() && !yes; ++i)
        {
        DProcess* p = (DProcess*)procs[i];
        if (!p) continue;
        TKName n;
        p->Name(n);  // e.g. "EFILE.Uid-..."
        TPtrC8 base((const TUint8*)n.Ptr(), n.Length());
        TInt dot = base.Locate('.');
        if (dot > 0) base.Set(base.Left(dot));
        if (PortIsEfileName((const char*)base.Ptr()))
            yes = ETrue;
        }
    procs.Signal();
    return yes;
    }

// First user thread needs a real TSS: our port never runs the genuine
// X86::Init1Interrupts (which would LTR + publish X86_TSS_Ptr), so the
// first ring-3 preemption triple-faults (#TS) and every reschedule
// scribbles ESP0 to address 4 (X86_TSS_Ptr==NULL). Fix both here, lazily.
static void TssInitOnce()
    {
    static TBool done = EFalse;
    if (done)
        return;
    done = ETrue;
    TX86Tss* tss = (TX86Tss*)qemu_tss;
    tss->iSs0 = 0x10;  // R0DS: supervisor stack for ring transitions
    tss->iEsp0 = (TLinAddr)(sTssStack + sizeof(sTssStack));
    // No LTR here: genuine __lidt already LTRs 0x28 as a side effect at
    // boot (vectors.cia), and reloading a live TR is what faulted.
    X86_TSS_Ptr = tss;  // reschedule ESP0 updates land in the live TSS
    }

// M-03: tag every thread of an E32 process with its page directory. The
// genuine ncsched.cia process handler (PdProcessSwitch) reads
// NThread.iAddressSpace and reloads CR3 on each address-space switch.
// Called with the system locked, immediately before proc->Resume().
static void TagProcessAddressSpace(DProcess* aProc)
    {
    const TUint pd = PdPhys(aProc);
    if (!pd)
        return;
    SDblQueLink* link = aProc->iThreadQ.First();
    while (link != &aProc->iThreadQ.iA)
        {
        DThread* t = _LOFF(link, DThread, iProcessLink);
        link = link->iNext;
        t->iNThread.SetAddressSpace((TAny*)pd);
        t->iNThread.ModifyAttributes(0, KThreadAttAddressSpace);
        }
    KPrintf("e32run: tagged PD=%x for process %x\r\n", pd, (TUint)aProc);
    }

// --- pilot user-DLL veneer (EUSER ordinals 1=Exit, 2=PutC) ------------------
// Genuine euser would live in a user DLL; our pilot serves two imports
// from kernel-owned trampolines instead (Direct model: all RAM is
// user-executable, no paging/NX). Both stubs are position-independent:
// Exit takes no stack args (handle constant), PutC takes cdecl [esp+4].
static const TUint8 sTrampExit[] =
    {
    0xB9, 0x01, 0x80, 0xFF, 0xFF,  // mov ecx,0xFFFF8001 (KCurrentThreadHandle)
    0x31, 0xD2,                    // xor edx,edx (EExitKill)
    0x31, 0xDB,                    // xor ebx,ebx (reason 0)
    0x31, 0xF6,                    // xor esi,esi (NULL category)
    0xB8, 0x73, 0x00, 0x00, 0x00,  // mov eax,115 (EExecThreadKill)
    0xCD, 0x21,                    // int 0x21 (kills this thread)
    0xEB, 0xFE,                    // jmp $ backstop if exec ever returns
    };
static const TUint8 sTrampPutc[] =
    {
    0x0F, 0xB6, 0x4C, 0x24, 0x04,  // movzx ecx,byte ptr [esp+4]
    0xB5, 0x0E,                    // mov ch,0x0E (yellow on black)
    0x66, 0x89, 0x0D, 0x20, 0x8D, 0x0B, 0x00,  // mov [0xB8D20],cx (row 21 col 0)
    0xC3,                          // ret
    };

// --- genuine CRC tables (us_func.cpp) ---------------------------------------
static const TUint gCrc16Tab[256] =
    {
	0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,0x8108,0x9129,0xa14a,
	0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,
	0x72f7,0x62d6,0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,0x2462,
	0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,0xa56a,0xb54b,0x8528,0x9509,
	0xe5ee,0xf5cf,0xc5ac,0xd58d,0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,
	0x46b4,0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,0x48c4,0x58e5,
	0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,
	0x9969,0xa90a,0xb92b,0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
	0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,0x6ca6,0x7c87,0x4ce4,
	0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,
	0x8d68,0x9d49,0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,0xff9f,
	0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,0x9188,0x81a9,0xb1ca,0xa1eb,
	0xd10c,0xc12d,0xf14e,0xe16f,0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,
	0x6067,0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,0x02b1,0x1290,
	0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,
	0xe54f,0xd52c,0xc50d,0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
	0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,0x26d3,0x36f2,0x0691,
	0x16b0,0x6657,0x7676,0x4615,0x5634,0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,
	0xb98a,0xa9ab,0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,0xcb7d,
	0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,0x4a75,0x5a54,0x6a37,0x7a16,
	0x0af1,0x1ad0,0x2ab3,0x3a92,0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,
	0x8dc9,0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,0xef1f,0xff3e,
	0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,
	0x3eb2,0x0ed1,0x1ef0
    };

static const TUint32 gCrc32Tab[256] =
    {
	0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
	0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
	0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
	0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
	0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
	0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
	0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
	0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
	0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
	0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
	0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
	0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
	0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
	0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
	0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
	0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
	0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
	0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
	0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
	0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
	0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
	0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
	0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
	0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
	0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
	0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
	0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
	0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
	0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
	0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
	0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
	0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
    };

static TUint Crc16(const TUint8* aP, TUint aLen)
    {
    TUint crc = 0;
    for (TUint i = 0; i < aLen; ++i)
        crc = (crc << 8) ^ gCrc16Tab[((crc >> 8) ^ aP[i]) & 0xFF];
    return crc & 0xFFFF;
    }

static TUint32 Crc32(const TUint8* aP, TUint aLen)
    {
    TUint32 crc = 0;
    for (TUint i = 0; i < aLen; ++i)
        crc = (crc >> 8) ^ gCrc32Tab[(crc ^ aP[i]) & 0xFF];
    return crc;
    }

// TCheckedUid::Check over the 3 UIDs at aP (12 bytes, little-endian).
static TUint32 UidChecksum(const TUint8* aP)
    {
    TUint8 even[6], odd[6];
    for (int i = 0; i < 6; ++i)
        {
        even[i] = aP[i * 2];
        odd[i] = aP[i * 2 + 1];
        }
    return (TUint32)((Crc16(odd, 6) << 16) | Crc16(even, 6));
    }

static TUint32 Rd32(const TUint8* p)
    { return (TUint32)(p[0] | (p[1] << 8) | (p[2] << 16) | ((TUint32)p[3] << 24)); }
static TInt Rd32s(const TUint8* p) { return (TInt)Rd32(p); }
static TUint16 Rd16(const TUint8* p) { return (TUint16)(p[0] | (p[1] << 8)); }

// Mirror of E32ImageHeaderV::ValidateRelocations (f32image.h): bounds only,
// no writes. aSectionSize bounds the fixup sites (code or data section).
static TInt ValidateRelocs(const TUint8* aFile, TUint aLen, TUint aRelOff,
                           TUint aSectionSize)
    {
    if (!aRelOff)
        return KErrNone;
    if (aRelOff >= aLen)
        return KErrNone;  // reloc section beyond file - nothing to validate
    if (aRelOff + 8 < aRelOff || aRelOff + 8 > aLen)
        return KErrCorrupt;
    TUint32 size = Rd32(aFile + aRelOff);
    TInt n = Rd32s(aFile + aRelOff + 4);
    if (size < 8 || (size & 3) || n < 0)
        return KErrCorrupt;
    // Clamp section size to available data: some E32 images (e.g. EUSER.DLL)
    // have reloc sections that extend slightly beyond EOF due to toolchain
    // padding. Process only the relocations that are present.
    if (aRelOff + size > aLen)
        size = aLen - aRelOff;
    TUint p = aRelOff + 8, end = aRelOff + size;
    TInt count = 0;
    while (p < end)
        {
        if (p + 8 < p || p + 8 > end)
            return KErrCorrupt;
        TUint32 page = Rd32(aFile + p), bs = Rd32(aFile + p + 4);
        if ((page & 0xFFF) || bs < 8 || (bs & 3))
            return KErrCorrupt;
        // Clamp block size to available data
        if (p + bs > end)
            break;
        if (bs < 8 || bs & 3)
            return KErrCorrupt;
        for (TUint q = p + 8; q < p + bs; q += 2)
            {
            TUint e = Rd16(aFile + q);
            if (!e)
                continue;
            TUint t = e & 0xF000;
            if (t != 0x1000 && t != 0x2000 && t != 0x3000)
                return KErrCorrupt;
            TUint off = page + (e & 0xFFF);
            if (off + 4 < off || off + 4 > aSectionSize)
                return KErrCorrupt;
            ++count;
            }
        p += bs;
        }
    // Only enforce count match when we processed all reloc blocks
    // without clamping. If we broke out early due to clamped bounds,
    // accept whatever count we found.
    // Only enforce count match when we processed all blocks without
    // clamping. If we broke out early due to clamped bounds (p + bs > end),
    // accept whatever count we found.
    if (p >= end && count != n)
        return KErrCorrupt;
    return KErrNone;
    }

// Mirror of svRelocateSection (sf_lepoc.cpp): add code/data delta per type.
// aLen bounds the file data available for reading relocations.
static void ApplyRelocs(const TUint8* aFile, TUint aLen, TUint aRelOff,
                        TUint8* aRun,
                        TUint32 aCodeDelta, TUint32 aDataDelta,
                        TUint32 aCodeBase, TUint32 aCodeSize)
    {
    TUint32 size = Rd32(aFile + aRelOff);
    if (aRelOff + size > aLen)
        size = aLen - aRelOff;
    TUint p = aRelOff + 8, end = aRelOff + size;
    while (p < end)
        {
        TUint32 page = Rd32(aFile + p), bs = Rd32(aFile + p + 4);
        if ((page & 0xFFF) || bs < 8 || (bs & 3))
            return;
        if (p + bs > end)
            bs = end - p;
        for (TUint q = p + 8; q < p + bs; q += 2)
            {
            TUint e = Rd16(aFile + q);
            if (!e)
                continue;
            TUint t = e & 0xF000;
            TUint off = page + (e & 0xFFF);
            TUint32* dst = (TUint32*)(aRun + off);
            TUint32 v = *dst;
            if (t == 0x1000)
                v += aCodeDelta;
            else if (t == 0x2000)
                v += aDataDelta;
            else if (v >= aCodeBase && v < aCodeBase + aCodeSize)
                v += aCodeDelta;
            else
                v += aDataDelta;
            *dst = v;
            }
        p += bs;
        }
    }

// Validate an E32 image in memory. aLen is the bytes available.
// Returns KErrNone (fills aInfo), KErrCorrupt, KErrNotSupported, KErrArgument.
TInt E32Validate(const TUint8* aBuf, TUint aLen, TE32Info& aInfo)
    {
    if (!aBuf || aLen < 156)
        return KErrArgument;
    TUint32 uid1 = Rd32(aBuf + 0), uid2 = Rd32(aBuf + 4), uid3 = Rd32(aBuf + 8);
    TUint32 uidChk = Rd32(aBuf + 12);
    KPrintf("E32Validate: uid1=%08x uid2=%08x uid3=%08x uidChk=%08x from aBuf[0..3]=%02x%02x%02x%02x\r\n", uid1, uid2, uid3, uidChk, aBuf[0], aBuf[1], aBuf[2], aBuf[3]);
    TUint32 sig = Rd32(aBuf + 16);
    TUint32 hdrCrc = Rd32(aBuf + 20);
    TUint32 modver = Rd32(aBuf + 24);
    TUint32 comp = Rd32(aBuf + 28);
    TUint32 flags = Rd32(aBuf + 44);
    TInt codeSize = Rd32s(aBuf + 48), dataSize = Rd32s(aBuf + 52);
    TInt heapMin = Rd32s(aBuf + 56), heapMax = Rd32s(aBuf + 60);
    TInt stackSize = Rd32s(aBuf + 64), bssSize = Rd32s(aBuf + 68);
    TUint32 entry = Rd32(aBuf + 72);
    TUint32 codeBase = Rd32(aBuf + 76), dataBase = Rd32(aBuf + 80);
    TInt dllRefs = Rd32s(aBuf + 84);
    TUint32 expOff = Rd32(aBuf + 88);
    TInt expCount = Rd32s(aBuf + 92);
    TInt textSize = Rd32s(aBuf + 96);
    TUint32 codeOff = Rd32(aBuf + 100), dataOff = Rd32(aBuf + 104);
    TUint32 impOff = Rd32(aBuf + 108);
    TUint32 codeRelocOff = Rd32(aBuf + 112), dataRelocOff = Rd32(aBuf + 116);
    TUint16 cpu = Rd16(aBuf + 122);
    TUint16 pri = Rd16(aBuf + 120);
    const TUint32 kHdrFmtV = 0x02000000u;
    const TUint32 kCpuX86 = 0x1000u;
    const TUint32 kEpoc = 0x434F5045u;
    const TUint32 kCrcInit = 0xC90FDAA2u;
    const TUint32 kExeUid = 0x1000007Au;
    if ((flags & 0x0F000000u) != kHdrFmtV)
        { KPrintf("E32Validate: flags fail flags=0x%08x kHdrFmtV=0x%08x\r\n", flags, kHdrFmtV); return KErrNotSupported; }
    if (codeOff > aLen || codeOff < 156)
        { KPrintf("E32Validate: codeOff fail codeOff=%d aLen=%d\r\n", codeOff, aLen); return KErrCorrupt; }
    if (cpu != kCpuX86)
        { KPrintf("E32Validate: cpu fail cpu=%d kCpuX86=%d\r\n", cpu, kCpuX86); return KErrNotSupported; }
    if (UidChecksum(aBuf) != uidChk)
        { KPrintf("E32Validate: uidChk fail computed=0x%08x hdr=0x%08x\r\n", UidChecksum(aBuf), uidChk); return KErrCorrupt; }
    if (sig != kEpoc)
        { KPrintf("E32Validate: sig fail sig=0x%08x kEpoc=0x%08x\r\n", sig, kEpoc); return KErrCorrupt; }
    KPrintf("E32Validate: sig OK\r\n");
    // Header CRC: recompute over a copy with the init constant in the field.
    // (Caller guarantees aLen >= codeOff; use the stack for the 156B copy.)
    if (codeOff > 512)
        { KPrintf("E32Validate: codeOff512 fail codeOff=%d\r\n", codeOff); return KErrCorrupt; }
    KPrintf("E32Validate: codeOff512 OK\r\n");
    TUint8 tmp[512];
    for (TUint i = 0; i < codeOff; ++i)
        tmp[i] = aBuf[i];
    tmp[20] = (TUint8)(kCrcInit & 0xFF);
    tmp[21] = (TUint8)((kCrcInit >> 8) & 0xFF);
    tmp[22] = (TUint8)((kCrcInit >> 16) & 0xFF);
    tmp[23] = (TUint8)((kCrcInit >> 24) & 0xFF);
    if (Crc32(tmp, codeOff) != hdrCrc)
        { KPrintf("E32Validate: crc fail computed=0x%08x hdr=0x%08x\r\n", Crc32(tmp, codeOff), hdrCrc); return KErrCorrupt; }
    KPrintf("E32Validate: crc OK\r\n");
    const TUint32 kDllUid = 0x10000079u;
    TBool isDll = (flags & 0x00000001u) != 0;
            if (uid1 != kExeUid && uid1 != kDllUid)
        { KPrintf("E32Validate: uid1 fail isDll=%d uid1=0x%08x expected=0x%08x\r\n", isDll, uid1, isDll ? kDllUid : kExeUid); return KErrCorrupt; }
    KPrintf("E32Validate: uid1 OK\r\n");
    if (modver >= 0x80000000u || (modver & 0xFFFFu) > 0x8000u)
        { KPrintf("E32Validate: modver fail modver=0x%08x\r\n", modver); return KErrNotSupported; }
    KPrintf("E32Validate: modver OK\r\n");
    if (comp != 0)
        { KPrintf("E32Validate: comp fail comp=%d\r\n", comp); return KErrNotSupported; }
    KPrintf("E32Validate: comp OK\r\n");
    if (heapMin < 0 || heapMax < heapMin || stackSize < 0 || bssSize < 0)
        { KPrintf("E32Validate: heap fail heapMin=%d heapMax=%d stackSize=%d bssSize=%d\r\n", heapMin, heapMax, stackSize, bssSize); return KErrCorrupt; }
    KPrintf("E32Validate: heap OK\r\n");
    if (flags & 0x000000E0u)  // x86: no entry-point-type bits
        { KPrintf("E32Validate: entry-type bits fail flags=0x%08x\r\n", flags); return KErrNotSupported; }
    KPrintf("E32Validate: entry-type bits OK\r\n");
    if (flags & 0x00000018u)  // x86: no ABI bits
        { KPrintf("E32Validate: ABI bits fail flags=0x%08x\r\n", flags); return KErrNotSupported; }
    KPrintf("E32Validate: ABI bits OK\r\n");
    TUint32 impFmt = flags & 0xF0000000u;
    if (impFmt != 0 && impFmt != 0x10000000u && impFmt != 0x20000000u)
        { KPrintf("E32Validate: impFmt fail impFmt=0x%08x\r\n", impFmt); return KErrNotSupported; }
    KPrintf("E32Validate: impFmt OK\r\n");
    if (entry >= (TUint32)codeSize)
        { KPrintf("E32Validate: entry fail entry=0x%08x codeSize=%d\r\n", entry, codeSize); return KErrCorrupt; }
    KPrintf("E32Validate: entry OK\r\n");
    if (entry + 12 + 4 > (TUint32)codeSize)
        { KPrintf("E32Validate: entry+16 fail entry=0x%08x codeSize=%d\r\n", entry, codeSize); return KErrCorrupt; }
    KPrintf("E32Validate: entry+16 OK\r\n");
    if ((codeBase & 3) || (dataBase & 3))
        { KPrintf("E32Validate: base align fail codeBase=0x%08x dataBase=0x%08x\r\n", codeBase, dataBase); return KErrCorrupt; }
    KPrintf("E32Validate: base align OK\r\n");
    if (dllRefs < 0 || dllRefs > 1)
        { KPrintf("E32Validate: dllRefs fail dllRefs=%d\r\n", dllRefs); return KErrCorrupt; }
    KPrintf("E32Validate: dllRefs OK\r\n");
    if (dllRefs == 0 && impOff)
        { KPrintf("E32Validate: dllRefs/impOff mismatch dllRefs=%d impOff=0x%08x\r\n", dllRefs, impOff); return KErrCorrupt; }
    KPrintf("E32Validate: dllRefs/impOff OK\r\n");
    TUint32 codeEnd = codeOff + (TUint32)codeSize;
    if (codeEnd < codeOff || codeEnd > aLen)
        { KPrintf("E32Validate: codeEnd fail codeEnd=0x%08x aLen=%d\r\n", codeEnd, aLen); return KErrCorrupt; }
    KPrintf("E32Validate: codeEnd OK\r\n");
    if (dataOff == 0)
        {
        if (dataSize != 0)
            { KPrintf("E32Validate: dataOff0/dataSize fail dataSize=%d\r\n", dataSize); return KErrCorrupt; }
        KPrintf("E32Validate: dataOff0/dataSize OK\r\n");
        }
    else
        {
        TUint32 dataEnd = dataOff + (TUint32)dataSize;
        if (dataEnd < dataOff || dataOff < codeEnd || dataEnd > aLen)
            { KPrintf("E32Validate: dataEnd fail dataEnd=0x%08x dataOff=0x%08x codeEnd=0x%08x aLen=%d\r\n", dataEnd, dataOff, codeEnd, aLen); return KErrCorrupt; }
        KPrintf("E32Validate: dataEnd OK\r\n");
        }
    if (expCount < 0 || expCount > 65535)
        { KPrintf("E32Validate: expCount fail expCount=%d\r\n", expCount); return KErrCorrupt; }
    KPrintf("E32Validate: expCount OK\r\n");
    if (!isDll && expCount != 0)  // EXEs carry no exports here
        { KPrintf("E32Validate: EXE exports fail expCount=%d\r\n", expCount); return KErrNotSupported; }
    KPrintf("E32Validate: EXE exports OK\r\n");
    if (isDll && expCount <= 0)
        { KPrintf("E32Validate: DLL exports fail expCount=%d\r\n", expCount); return KErrCorrupt; }
    KPrintf("E32Validate: DLL exports OK\r\n");
    if (expCount > 0)
        {
        TUint32 expEnd = expOff + (TUint32)expCount * 4;
        if (expEnd < expOff || expOff < codeOff || expEnd > codeEnd)
            { KPrintf("E32Validate: export bounds fail expEnd=0x%08x codeEnd=0x%08x\r\n", expEnd, codeEnd); return KErrCorrupt; }
        KPrintf("E32Validate: export bounds OK\r\n");
        for (TInt i = 0; i < expCount; ++i)
            {
            TUint32 target = Rd32(aBuf + expOff + (TUint)i * 4);
            if (target >= (TUint32)codeSize)
                { KPrintf("E32Validate: export target fail target=0x%08x codeSize=%d\r\n", target, codeSize); return KErrCorrupt; }
            }
        KPrintf("E32Validate: export targets OK\r\n");
        }
    if (textSize < 0 || textSize > codeSize)
        { KPrintf("E32Validate: textSize fail textSize=%d codeSize=%d\r\n", textSize, codeSize); return KErrCorrupt; }
    KPrintf("E32Validate: textSize OK\r\n");
    KPrintf("E32Validate: about to call ValidateRelocs codeRelocOff=0x%08x codeSize=%d\r\n", codeRelocOff, codeSize);
    // EUSER-ordinal imports (ELF-style code-offset list, positional:
    // list entry k serves ordinal k+1: 1=Exit, 2=PutC, 3=CmdLen, ...).
    // Mirrors E32ImageHeaderV::ValidateImports bounds discipline.
    aInfo.iImpOff = 0; aInfo.iImpN = 0; aInfo.iPe2 = EFalse;
    if (impOff)
        {
        if (dllRefs != 1)
            { return KErrCorrupt; }
        if (impOff + 4 < impOff || impOff + 4 > aLen)
            { return KErrCorrupt; }
        TUint32 isize = Rd32(aBuf + impOff);
        if (isize < 22 || impOff + 4 + isize < impOff + 4 ||
            impOff + 4 + isize > aLen)
            { return KErrCorrupt; }
        const TUint8* sec = aBuf + impOff + 4;
        TUint32 nameOff = Rd32(sec), n = (TUint32)Rd32s(sec + 4);
        TBool isPe2 = (flags & 0x20000000u) != 0;
        if (!isPe2 && (n < 2 || n > 4))
            { return KErrNotSupported; }
        if (isPe2)
            {
            // PE2-style: section holds DLL name only.  The IAT lives at the
            // end of the text section, pre-linked with 1-based ordinals.
            if (n < 1 || n > 512)   // genuine EWSRV binds 219 EUSER ordinals
                { return KErrNotSupported; }
            if (isize < 8 + 6)
                { return KErrCorrupt; }
            if (textSize < 0 ||
                (TUint32)textSize + 4u * (TUint)n > (TUint32)codeSize)
                { return KErrCorrupt; }
            for (TUint k = 0; k < n; ++k)
                {
                TUint32 slot = (TUint32)textSize + 4u * k;
                TUint32 ord = Rd32(aBuf + codeOff + slot);
                if (ord == 0 || ord > 65535u)
                    { return KErrCorrupt; }
                aInfo.iSlots[k] = slot;
                }
            }
        else
            {
            if (isize < 14 + 4 * n)
                { return KErrCorrupt; }
            }
        // name+NUL must lie within header+iSize (offsets count the header).
        if (nameOff + 6 < nameOff || nameOff + 6 > isize + 4)
            { return KErrCorrupt; }
        const TUint8* nm = aBuf + impOff + nameOff;
        if (!isPe2)
            {
            if (nm[0] != 'E' || nm[1] != 'U' || nm[2] != 'S' ||
                nm[3] != 'E' || nm[4] != 'R' || nm[5] != 0)
                { return KErrNotSupported; }
            }
        else
            {
            // PE2: DLL name is mangled (e.g. "euser{000a0000}[100039e5].dll").
            // Accept any non-empty name ending in ".dll\\0".
            TUint nmLen = 0;
            while (nmLen < 64 && nm[nmLen]) ++nmLen;
            if (nmLen < 4 || nm[nmLen] != 0 ||
                nm[nmLen-4] != '.' || nm[nmLen-3] != 'd' ||
                nm[nmLen-2] != 'l' || nm[nmLen-1] != 'l')
                { return KErrNotSupported; }
            }
        if (!isPe2)
            {
            for (TUint k = 0; k < n; ++k)
                {
                TUint32 s = Rd32(sec + 8 + 4 * k);
                if ((s & 3) || s < (TUint32)textSize ||
                    s + 4 < s || s + 4 > (TUint32)codeSize)
                    { return KErrCorrupt; }
                for (TUint j = 0; j < k; ++j)
                    if (Rd32(sec + 8 + 4 * j) == s)
                        { return KErrCorrupt; }
                aInfo.iSlots[k] = s;
                }
            }
        aInfo.iImpOff = impOff; aInfo.iImpN = (TInt)n; aInfo.iPe2 = isPe2;
        }
    TInt rr = ValidateRelocs(aBuf, aLen, codeRelocOff, (TUint)codeSize);
    if (rr != KErrNone)
        { return rr; }
    if (dataRelocOff && (!dataOff || !dataSize))
        { return KErrCorrupt; }
    rr = ValidateRelocs(aBuf, aLen, dataRelocOff, (TUint)dataSize);
    if (rr != KErrNone)
        { return rr; }
    // Export-desc tail: offsetof(iExportDescSize)=152.
    // Mirrors tools/e32read.py lines 244-277.
    if (codeOff < 156)
        { KPrintf("E32Validate: codeOff too small codeOff=%d\r\n", codeOff); return KErrNotSupported; }
    KPrintf("E32Validate: codeOff OK\r\n");
    TUint16 expDescSize = Rd16(aBuf + 152);
    TUint8  expDescType = aBuf[154];
    TUint32 edSize = (expDescSize + 2u + 1u + 3u) & ~3u;
    if (152u + edSize != codeOff)
        { return KErrNotSupported; }
    KPrintf("E32Validate: expDesc tail OK expDescSize=%u expDescType=%u\r\n", expDescSize, expDescType);
    if (expDescType == 0x00)
        { /* NoHoles: ok */ }
    else if (expDescType == 0x01)
        {
        if ((((TUint32)expCount + 7u) >> 3) != expDescSize)
            { return KErrNotSupported; }
        }
    else if (expDescType == 0x02)
        {
        TUint32 bitmapSize = ((TUint32)expCount + 7u) >> 3;
        TUint32 metaSize = (bitmapSize + 7u) >> 3;
        if (metaSize > expDescSize || 155u + metaSize > codeOff)
            { return KErrNotSupported; }
        TUint32 total = metaSize;
        for (TUint32 m = 0; m < metaSize; ++m)
            {
            TUint8 b = aBuf[155 + m];
            while (b) { if (b & 1) ++total; b >>= 1; }
            }
        if (total != expDescSize)
            { return KErrNotSupported; }
        }
    else
        { return KErrNotSupported; }

    aInfo.iUid1 = uid1; aInfo.iUid2 = uid2; aInfo.iUid3 = uid3;
    aInfo.iEntry = entry;
    aInfo.iCodeBase = codeBase; aInfo.iDataBase = dataBase;
    aInfo.iCodeSize = codeSize; aInfo.iDataSize = dataSize;
    aInfo.iBssSize = bssSize;
    aInfo.iHeapMin = heapMin; aInfo.iHeapMax = heapMax;
    aInfo.iStackSize = stackSize;
    aInfo.iDllRefs = dllRefs;
    aInfo.iCodeOff = codeOff; aInfo.iDataOff = dataOff;
    aInfo.iPriority = pri;
    aInfo.iCodeRelocOff = codeRelocOff;
    aInfo.iDataRelocOff = dataRelocOff;
    aInfo.iExpOff = (TUint32)(expCount > 0 ? expOff : 0);
    aInfo.iExpCount = expCount > 0 ? expCount : 0;
    return KErrNone;
    }

// Resident EUSER.DLL: loaded once from FAT32, exports serve EXE imports.
// Direct model: kernel-heap RAM is user-executable, like the trampolines.
namespace
    {
    TBool sDllLoaded = EFalse;
    TUint8* sDllCode = 0;
    TUint32 sDllExp[4096];
    TInt sDllExpN = 0;
    TUint8 sDllFile[512 * 1024];  // EUSER.DLL is ~412KB
    }

static TInt E32LoadDll()
    {
    if (sDllLoaded)
        return KErrNone;
    TUint got = 0;
    TInt r = FatReadPath("\\EUSER.DLL", sDllFile, sizeof(sDllFile), got);
    KPrintf("E32LoadDll: FatReadPath r=%d got=%u first4=%02x%02x%02x%02x\r\n", r, got, sDllFile[0], sDllFile[1], sDllFile[2], sDllFile[3]);
    if (r != KErrNone)
        return r;
    TE32Info di;
    r = E32Validate(sDllFile, got, di);
    KPrintf("E32LoadDll: E32Validate r=%d uid1=%08x expCount=%d codeRelocOff=%08x\r\n", r, di.iUid1, di.iExpCount, di.iCodeRelocOff);
    if (r != KErrNone)
        return r;
    if (!(di.iUid1 == 0x10000079u) || di.iExpCount < 2 || di.iExpCount > 4096)
        return KErrCorrupt;
    // Resident pilot DLL: relocate in place inside the file buffer.  The
    // kernel image is loaded RWX, so this static buffer is executable; a
    // ~400 KB Kern::Alloc destabilises the hybrid heap, so avoid it.
    TUint8* run = sDllFile + di.iCodeOff;
    TUint32 codeDelta = (TUint32)run - di.iCodeBase;
    TUint32 dataDelta = 0;
    TUint8* runData = 0;
    if (di.iDataSize || di.iBssSize) {
        TUint32 dataRunSize = di.iDataSize + di.iBssSize;
        runData = (TUint8*)Kern::AllocZ((TInt)dataRunSize);
        if (!runData) {
            Kern::Free(run);
            return KErrNoMemory;
        }
        if (di.iDataSize)
            memcpy(runData, sDllFile + di.iDataOff, (TUint)di.iDataSize);
        dataDelta = (TUint32)runData - di.iDataBase;
    }
    if (di.iCodeRelocOff)
        ApplyRelocs(sDllFile, got, di.iCodeRelocOff, run, codeDelta, dataDelta,
                     di.iCodeBase, (TUint32)di.iCodeSize);
    if (di.iDataRelocOff && runData)
        ApplyRelocs(sDllFile, got, di.iDataRelocOff, runData, codeDelta, dataDelta,
                     di.iCodeBase, (TUint32)di.iDataSize);
    for (TInt i = 0; i < di.iExpCount && i < 4096; ++i)
        sDllExp[i] = (TUint32)(run + Rd32(sDllFile + di.iExpOff + (TUint)i * 4));
    sDllExpN = di.iExpCount > 4096 ? 4096 : di.iExpCount;
    sDllCode = run;
    sDllLoaded = ETrue;
    KPrintf("e32run: EUSER.DLL resident code=%x data=%x exports=%d\r\n", run, runData, sDllExpN);
    return KErrNone;
    }

// Run an import-free, uncompressed EXE as a genuine user process.
// Loader copy: kernel allocates the code region (AllocRegion ignores the
// linked base); we copy the file's code section to the returned run
// address (position-independent code needs no fixups). Thread starts
// suspended; Loaded() marks ready, Resume() starts it.
static void ExecRecRemember(DProcess* aProc, DCodeSeg* aSeg, const char* aName);
static TBool ExecRecAvailable();

// PTRINJ proof wrapper (defined below with the IAT rebinding helpers); the
// genuine ord-1868 address is cached here so the wrapper can tail-call it.
extern "C" TUint32 sPortOrigPos;
extern "C" void PortEwsrvPosHook();
static TBool IsEwsrvProcess(const char* n);
static TBool PointerInjectionActive();

// Reliable exit status: ring of recent reaps, surviving the teardown that
// destroys the DProcess itself. Written under the reaper's locks (CS +
// code-seg + container); read under CS. Blocking wait() is still future.
struct TExitRec
    {
    char iName[32];
    TUint iId;        // 0 when the object was already gone
    TUint8 iExitType; // EExitPending sentinel when unknown
    TInt iExitReason;
    TBool iKnown;     // EFalse for the already-destroyed orphan path
    TBool iUsed;
    };
static TExitRec sExits[16];
static TUint sExitNext;

static void ExitRecRecord(const char* aName, TUint aId, TUint8 aType,
                          TInt aReason, TBool aKnown)
    {
    TExitRec& e = sExits[sExitNext % 16];
    ++sExitNext;
    TUint k = 0;
    while (aName && aName[k] && k < sizeof(e.iName) - 1)
        { e.iName[k] = aName[k]; ++k; }
    e.iName[k] = 0;
    e.iId = aId;
    e.iExitType = aType;
    e.iExitReason = aReason;
    e.iKnown = aKnown;
    e.iUsed = ETrue;
    }

void E32PrintExits()
    {
    NKern::ThreadEnterCS();
    TBool any = EFalse;
    for (TUint n = 0; n < 16; ++n)
        {
        const TExitRec& e = sExits[(sExitNext + n) % 16];  // oldest first
        if (!e.iUsed)
            continue;
        any = ETrue;
        if (e.iKnown)
            KPrintf("  %s id=%u type=%d reason=%d\r\n",
                e.iName[0] ? e.iName : "?", e.iId,
                (TInt)e.iExitType, e.iExitReason);
        else
            KPrintf("  %s id=? type=? reason=? (object gone)\r\n",
                e.iName[0] ? e.iName : "?");
        }
    if (!any)
        KPrintf("no exits recorded\r\n");
    NKern::ThreadLeaveCS();
    }
TInt E32Run(const TUint8* aFileBuf, TUint aFileLen, const char* aName,
            const char* aCmdLine)
    {
    TE32Info info;
    TInt r = E32Validate(aFileBuf, aFileLen, info);
    KPrintf("e32run: validate r=%d\r\n", r);
    if (r != KErrNone)
        return r;
    if (info.iUid1 != 0x1000007au)
        return KErrNotSupported;  // EXEC must never start a DLL as a process
    // Single-owner guard: key on the genuine EFILE UID3 (covers every alias)
    // plus a name fallback; EWSRV shares the UID3 and is excluded.  The
    // published-name/process probes run unlocked (container mutex is legal),
    // the check-and-claim itself is atomic under NKern CS below.
    const TBool isEfile = PortIsEfileImage(info, aName);
    TBool efileBusy = EFalse;
    if (isEfile)
        efileBusy = PortEfileProcessRunning() || PortFileServerPublished();
    // All three per-process tables must have room: sProcs (CWD), sExecs
    // (code-seg reaper) and the M-03 PD pool. Failing before ProcessCreate
    // avoids untracked processes and leaked segs.
    if (!ProcExtAvailable() || !ExecRecAvailable() || !PdAvail()) return KErrOverflow;
    // Resolve DLL imports before creating the process, so a missing
    // export fails fast without a stillborn process left behind.
    TInt dllR = KErrNone;
    if (info.iImpN > 0)
        {
        dllR = E32LoadDll();
        KPrintf("e32run: E32LoadDll r=%d impN=%d sDllExpN=%d\r\n", dllR, info.iImpN, sDllExpN);
        if (dllR != KErrNone && !(dllR == KErrNotFound && info.iImpN == 2))
            return dllR;
        if (dllR == KErrNone && sDllExpN < info.iImpN)
            return KErrNotSupported;
        }
    TUint nlen = 0;
    while (aName[nlen] && nlen < 255)
        ++nlen;
    if (!nlen)
        return KErrArgument;
    TUint dot = nlen;
    while (dot > 0 && aName[dot - 1] != '.')
        --dot;
    TUint rootLen = dot ? dot - 1 : nlen;
    TUint extOff = dot ? dot - 1 : nlen;

    TProcessCreateInfo* ppinfo = (TProcessCreateInfo*)Kern::AllocZ(sizeof(TProcessCreateInfo));
    KPrintf("e32run: alloc %x\r\n", ppinfo);
    if (!ppinfo)
        return KErrNoMemory;
    new (ppinfo) TProcessCreateInfo();  // run ctors (TBuf8 lengths!)
    TProcessCreateInfo& pinfo = *ppinfo;
    pinfo.iFileName.Copy(TPtrC8((const TUint8*)aName, (TInt)nlen));
    KPrintf("e32run: named\r\n");
    pinfo.iRootNameOffset = 0;
    pinfo.iRootNameLength = (TInt)rootLen;
    pinfo.iExtOffset = (TInt)extOff;
    pinfo.iUids.iUid[0].iUid = info.iUid1;
    pinfo.iUids.iUid[1].iUid = info.iUid2;
    pinfo.iUids.iUid[2].iUid = info.iUid3;
    pinfo.iAttr = ECodeSegAttGlobal;
    pinfo.iCodeSize = info.iCodeSize;
    pinfo.iTextSize = info.iCodeSize;
    pinfo.iDataSize = info.iDataSize;
    pinfo.iBssSize = info.iBssSize;
    pinfo.iTotalDataSize = info.iDataSize + info.iBssSize;
    pinfo.iEntryPtVeneer = info.iEntry;
    pinfo.iFileEntryPoint = info.iEntry;
    pinfo.iDepCount = 0;
    pinfo.iCodeLoadAddress = 0;  // RAM-loaded: kernel allocates
    pinfo.iHeapSizeMin = info.iHeapMin;
    pinfo.iHeapSizeMax = info.iHeapMax;
    pinfo.iStackSize = info.iStackSize;
    // Header priority passes through if the kernel accepts it, else the
    // standard foreground default (a 0/garbage header must not kill exec).
    switch (info.iPriority)
        {
    case 150: case 250: case 350: case 450:
    case 650: case 750: case 850: case 950:
        pinfo.iPriority = (TProcessPriority)info.iPriority;
        break;
    default:
        pinfo.iPriority = EPriorityForeground;
        break;
        }
    pinfo.iOwnerType = EOwnerProcess;

    KPrintf("e32run: setup done, entering CS...\r\n");
    TssInitOnce();
    NKern::ThreadEnterCS();
    if (isEfile)
        {
        if (sPortEfileOwner || efileBusy)
            {
            KPrintf("e32run: single-owner: %s uid3=%08x already running or "
                    "!FileServer published; launch refused\r\n",
                    aName, info.iUid3);
            NKern::ThreadLeaveCS();
            Kern::Free(ppinfo);
            return KErrAlreadyExists;
            }
        sPortEfileOwner = ETrue;  // atomic claim: check and set under CS
        KPrintf("e32run: single-owner: %s uid3=%08x claimed\r\n",
                aName, info.iUid3);
        }
    DProcess* proc = NULL;
    HBuf* cmd = NULL;
    if (aCmdLine && *aCmdLine)
        {
        // The kernel HBuf is byte-oriented, but Unicode command lines
        // contain UTF-16 bytes (the length executive divides by two).
        TUint clen = 0;
        while (aCmdLine[clen] && clen < 256)
            ++clen;
        TUint8 unicode[512];
        for (TUint i = 0; i < clen; ++i)
            {
            unicode[2 * i] = (TUint8)aCmdLine[i];
            unicode[2 * i + 1] = 0;
            }
        cmd = HBuf::New(TPtrC8(unicode, (TInt)clen * 2));
        if (!cmd)
            {
            if (isEfile) sPortEfileOwner = EFalse;
            NKern::ThreadLeaveCS(); Kern::Free(ppinfo); return KErrNoMemory;
            }
        KPrintf("e32run: cmdline %d chars\r\n", clen);
        }
    TInt h = 0;
    KPrintf("e32run: creating process...\r\n");
    r = Kern::ProcessCreate(proc, pinfo, cmd, &h);
    if (!proc && cmd)
        delete cmd;  // NewProcess failed: ownership never transferred
    KPrintf("e32run: ProcessCreate r=%d run=%x proc=%x\r\n", r, pinfo.iCodeRunAddress, proc);
    if (r != KErrNone && isEfile) sPortEfileOwner = EFalse;  // failed launch
    if (r == KErrNone && proc)
        {
        sSvcLastPid = (TUint)proc->iId;  // S-02: hand pid back to the launch exec
        // G-01/B1: the port loader never copied the E32 image capability set
        // into the DProcess, so ExecHandler::ServerCreateWithOptions
        // (kernel/eka/kernel/sipc.cpp:837) panics the thread when a genuine
        // server publishes a protected "!..." name (sfile "!FileServer",
        // ewsrv "!E32WindowServer").  This port has no platform-security
        // model: grant ProtServ to every E32 process we load so the leading
        // '!' is accepted.  kernelhwsrv/ is untouched.
        proc->iS.iCaps[0] |= (TUint32)(1u << (TUint)ECapabilityProtServ);
        // M-03 step 2: allocate the process page directory before Resume().
        // Static 4K-aligned pool makes this infallible after the PdAvail()
        // pre-check; log PD physical so CR3 switching can be verified later.
        TInt pdR = PdCreate(proc, pinfo.iCodeRunAddress, (TUint)info.iCodeSize,
                            pinfo.iDataRunAddress, (TUint)(info.iDataSize + info.iBssSize),
                            0, (TUint)info.iStackSize);
        KPrintf("e32run: PdCreate r=%d PD=%x\r\n", pdR, PdPhys(proc));
        // Genuine EWSRV's edisp screen driver writes the VBE LFB directly;
        // expose the framebuffer to user mode (coarse 4M PDE U/S=1).
        TUint32 lfb = display_lfb();
        if (lfb && display_fb_size())
            PdMapUser(proc, lfb, lfb, display_fb_size());
        }
    if (r == KErrNone)
        {
        TUint8* codeRun = (TUint8*)pinfo.iCodeRunAddress;
        memcpy(codeRun, aFileBuf + info.iCodeOff, (TUint)info.iCodeSize);
        TUint32 codeDelta = pinfo.iCodeRunAddress - info.iCodeBase;
        TUint32 dataDelta = 0;
        if (info.iDataSize || info.iBssSize)
            {
            // EXE data lives in the process data chunk (see
            // CreateDataBssStackArea); copy initialized part, zero BSS.
            TUint8* dataRun = (TUint8*)pinfo.iDataRunAddress;
            if (info.iDataSize)
                memcpy(dataRun, aFileBuf + info.iDataOff,
                       (TUint)info.iDataSize);
            if (info.iBssSize)
                memclr(dataRun + info.iDataSize, info.iBssSize);
            dataDelta = pinfo.iDataRunAddress - info.iDataBase;
            }
        if (info.iCodeRelocOff)
            ApplyRelocs(aFileBuf, aFileLen, info.iCodeRelocOff, codeRun,
                         codeDelta, dataDelta,
                         info.iCodeBase, (TUint32)info.iCodeSize);
        if (info.iDataRelocOff)
            ApplyRelocs(aFileBuf, aFileLen, info.iDataRelocOff,
                        (TUint8*)pinfo.iDataRunAddress, codeDelta, dataDelta,
                        info.iCodeBase, (TUint32)info.iCodeSize);
        if (info.iImpN > 0)
            {
            if (dllR == KErrNone)
                {
                // PE2: the IAT slot holds a 1-based ordinal in the file;
                // bind that ordinal's genuine EUSER export address.  ELF:
                // ordinals are positional (slot k -> ord k+1).
                for (TInt k = 0; k < info.iImpN; ++k)
                    {
                    TInt ord;
                    if (info.iPe2)
                        ord = (TInt)Rd32(aFileBuf + info.iCodeOff + info.iSlots[k]);
                    else
                        ord = k + 1;
                    if (ord < 1 || ord > sDllExpN)
                        {
                        KPrintf("e32run: ordinal %d out of range (have %d)\r\n",
                            ord, sDllExpN);
                        continue;
                        }
                    // PTRINJ proof: rebind ONLY EWSRV's ord-1868 IAT slot to a
                    // transparent wrapper that calls the genuine export and
                    // logs EAX:EDX.  Gated on the marker so no other boot or
                    // process changes behaviour.
                    if (ord == 1868 && info.iPe2 && IsEwsrvProcess(aName) &&
                        PointerInjectionActive())
                        {
                        sPortOrigPos = sDllExp[ord - 1];
                        // Ring-3 wrapper lives in kernel .text (U/S=0 by default);
                        // make its 4K page user-executable via the port PD API.
                        PdMapUser(0, (TUint)(TLinAddr)&PortEwsrvPosHook & ~0xfffu,
                                  0, 4096);
                        *(TUint32*)(codeRun + info.iSlots[k]) =
                            (TUint32)(TLinAddr)&PortEwsrvPosHook;
                        KPrintf("e32run: PTRINJ EWSRV ord 1868 -> PosHook %08x "
                                "(genuine %08x)\r\n",
                                (TUint)(TLinAddr)&PortEwsrvPosHook, sPortOrigPos);
                        continue;
                        }
                    *(TUint32*)(codeRun + info.iSlots[k]) = sDllExp[ord - 1];
                    KPrintf("e32run: bind slot %d ord %d -> %08x\r\n",
                        info.iSlots[k], ord, sDllExp[ord - 1]);
                    }
                KPrintf("e32run: imports bound dll (%d ordinals)\r\n",
                    info.iImpN);
                }
            else if (info.iImpN == 2)
                {
                *(TUint32*)(codeRun + info.iSlots[0]) = (TUint32)sTrampExit;
                *(TUint32*)(codeRun + info.iSlots[1]) = (TUint32)sTrampPutc;
                KPrintf("e32run: imports bound trampoline (dll r=%d)\r\n",
                    dllR);
                }
            }
        KPrintf("e32run: sections in (deltas %x/%x), Loaded()...\r\n",
            codeDelta, dataDelta);
        // Entry point must be the absolute runtime address (code run address
        // + entry offset), not just the header entry offset.  The kernel uses
        // iEntryPtVeneer/iFileEntryPoint directly as the thread's initial EIP.
        pinfo.iEntryPtVeneer = pinfo.iCodeRunAddress + info.iEntry;
        pinfo.iFileEntryPoint = pinfo.iCodeRunAddress + info.iEntry;
        if (info.iBssSize >= 4)
            KPrintf("e32run: bss chk %x\r\n",
                *(TUint32*)(pinfo.iDataRunAddress + info.iDataSize));
        r = proc->Loaded(pinfo);
        KPrintf("e32run: Loaded r=%d\r\n", r);
        if (r == KErrNone && info.iBssSize >= 4)
            KPrintf("e32run: bss postload %x\r\n",
                *(TUint32*)(pinfo.iDataRunAddress + info.iDataSize));
        }
    if (r == KErrNone)
        {
        KPrintf("e32run: Resume()...\r\n");
        ProcExtCapture(proc);
        if (info.iBssSize >= 4)
            KPrintf("e32run: bss preresume %x run=%x\r\n",
                *(TUint32*)(pinfo.iDataRunAddress + info.iDataSize),
                pinfo.iDataRunAddress);
        NKern::LockSystem();
        TagProcessAddressSpace(proc);  // M-03: CR3 reload hook for this process
        proc->Resume();
        NKern::UnlockSystem();
        KPrintf("e32run: resumed\r\n");
        // Release our handle: the process lives on its threads. When they
        // die with no handles left, the object is destroyed (no zombies).
        Kern::CloseHandle(NULL, h);
        // Remember (process, code seg, name) for the post-mortem reaper: the
        // kernel leaves its creator ref on the seg (F32 would own this
        // lifecycle); E32Reap drops it once the process is dead.
        ExecRecRemember(proc, (DCodeSeg*)pinfo.iHandle, aName);
        }
    NKern::ThreadLeaveCS();
    (void)pinfo;
    Kern::Free(ppinfo);
    return r;
    }

// Kill a user process by container id (shell `kill`, exec 269 — bound to
// PortKillExec by PortTrapExecInstall). Refuse the kernel process and the
// caller's own process by object identity (pointer comparison, not numeric
// id). exec 269 is reachable from ring 3 (KILLPROC), so a user process
// passing the kernel's pid must not be able to Die() ekern: the self-only
// check used to let that through and fault the kernel.
// Locking mirrors OpenById (sexec.cpp): Open under the container lock,
// release it, then Die (container+process nesting faults otherwise).
TUint E32LastPid()
    {
    // sSvcLastPid is written inside E32Run before it returns, so the shell
    // can capture the pid of the child it just launched (Ctrl+C abort).
    return sSvcLastPid;
    }

TInt E32Kill(TUint aId)
    {
    NKern::ThreadEnterCS();
    DProcess* self = Kern::CurrentThread().iOwningProcess;
    DObjectCon* c = K::Containers[EProcess];
    c->Wait();
    DProcess* found = NULL;
    for (TInt i = 0; i < c->Count(); ++i)
        {
        DProcess* p = (DProcess*)(*c)[i];
        if ((TUint)p->iId == aId)
            { found = p; break; }
        }
    TInt r = KErrNotFound;
    if (found && (found == self || found == K::TheKernelProcess))
        {
        r = KErrAccessDenied;
        c->Signal();
        }
    else if (found && found->Open() == KErrNone)
        {
        c->Signal();
        TBuf<8> cat;
        cat.Copy(_L8("Kill"));
        found->Die(EExitKill, 0, cat);
        found->Close(NULL);
        r = KErrNone;
        }
    else
        c->Signal();
    NKern::ThreadLeaveCS();
    return r;
    }

// Post-mortem code-seg reaper (plays the F32 part our loader bypasses).
// Genuine F32 owns a code-seg handle per load and drops it after process
// death; our E32Run leaves the kernel creator ref, so the seg (plus its
// RAM region, plus its attach-open on data processes) lives forever.
// For entries whose process is threadless and past exit (or already gone
// from the container), drop one seg ref via CheckedClose under the code-
// seg lock (mirrors DProcess::Release). Cascade: seg destroyed ->
// attach dropped -> zombie process destroyed. Only touches OUR table,
// only dead processes, never live ones. Returns number reaped.
struct TExecRec
    {
    DProcess* iProc;
    DCodeSeg* iSeg;
    char iName[32];  // basename at exec, for fault attribution after death
    };
static TExecRec sExecs[16];

static TBool ExecRecAvailable()
    {
    for (TInt i = 0; i < 16; ++i)
        if (!sExecs[i].iProc) return ETrue;
    return EFalse;
    }

static void ExecRecRemember(DProcess* aProc, DCodeSeg* aSeg, const char* aName)
    {
    for (TInt i = 0; i < 16; ++i)
        if (!sExecs[i].iProc)
            {
            sExecs[i].iProc = aProc;
            sExecs[i].iSeg = aSeg;
            // Basename only: "C:\DIR\CRASH.E32" -> "CRASH.E32".
            const char* base = aName ? aName : "";
            for (const char* p = base; *p; ++p)
                if (*p == '\\' || *p == '/' || *p == ':')
                    base = p + 1;
            TUint k = 0;
            while (base[k] && k < sizeof(sExecs[i].iName) - 1)
                { sExecs[i].iName[k] = base[k]; ++k; }
            sExecs[i].iName[k] = 0;
            return;
            }
    // Table full of live processes: skip (leaks that seg; documented).
    }

TInt E32Reap()
    {
    NKern::ThreadEnterCS();
    DObjectCon* c = K::Containers[EProcess];
    TInt n = 0;
    DCodeSeg::Wait();
    for (TInt i = 0; i < 16; ++i)
        {
        DProcess* p = sExecs[i].iProc;
        DCodeSeg* s = sExecs[i].iSeg;
        if (!p)
            continue;
        // Liveness: still in the process container?
        c->Wait();
        TBool present = EFalse;
        DProcess* live = NULL;
        for (TInt j = 0; j < c->Count(); ++j)
            {
            DProcess* q = (DProcess*)(*c)[j];
            if (q == p)
                { present = ETrue; live = q; break; }
            }
        TBool dead = EFalse;
        if (!present)
            dead = ETrue;  // already destroyed; seg orphaned (creator ref)
        else if (live->iThreadQ.IsEmpty() && live->iExitType != EExitPending &&
                 live->CodeSeg() == NULL)
            dead = ETrue;  // torn down past seg detach; Release will not
                           // touch the seg again (RemoveCodeSeg already ran)
        if (dead && present)
            KPrintf("exit: id=%u type=%d reason=%d prog=%s\r\n",
                live->iId, live->iExitType, live->iExitReason,
                sExecs[i].iName[0] ? sExecs[i].iName : "?");
        else if (dead)
            KPrintf("exit: id=? type=? reason=? prog=%s (object gone)\r\n",
                sExecs[i].iName[0] ? sExecs[i].iName : "?");
        if (dead)
            ExitRecRecord(sExecs[i].iName,
                present ? live->iId : 0,
                present ? live->iExitType : (TUint8)EExitPending,
                present ? live->iExitReason : 0,
                present);
        c->Signal();
        if (dead && s)
            {
            s->CheckedClose();  // drop kernel creator ref; may cascade
            ++n;
            }
        if (dead)
            {
            PdDestroy(p);  // M-03: release the PD side-table row
            ProcExtRemove(p);
            sExecs[i].iProc = NULL; sExecs[i].iSeg = NULL;
            sExecs[i].iName[0] = 0;
            }
        }
    DCodeSeg::Signal();
    NKern::ThreadLeaveCS();
    return n;
    }

TBool E32ProcessDead(TUint aId)
{
    NKern::ThreadEnterCS();
    DCodeSeg::Wait();
    for (TInt i = 0; i < 16; ++i)
        {
        if (sExecs[i].iProc && sExecs[i].iProc->iId == aId)
            {
            DProcess* p = sExecs[i].iProc;
            TBool dead = p->iThreadQ.IsEmpty() && p->iExitType != EExitPending &&
                         p->CodeSeg() == NULL;
            DCodeSeg::Signal();
            NKern::ThreadLeaveCS();
            return dead;
            }
        }
    DCodeSeg::Signal();
    NKern::ThreadLeaveCS();
    return ETrue;  // not in exec table = not running = dead
}

TBool E32FindExit(TUint aId, TUint8& aType, TInt& aReason, char* aName)
{
    NKern::ThreadEnterCS();
    for (TUint n = 0; n < 16; ++n)
        {
        const TExitRec& e = sExits[(sExitNext + n) % 16];
        if (!e.iUsed) continue;
        if (e.iId == aId)
            {
            aType = e.iExitType;
            aReason = e.iExitReason;
            TUint k = 0;
            while (e.iName[k] && k < 31) { aName[k] = e.iName[k]; ++k; }
            aName[k] = 0;
            NKern::ThreadLeaveCS();
            return ETrue;
            }
        }
    NKern::ThreadLeaveCS();
    return EFalse;
}

// --- S-02: port control exec (slot 270) + exec-81 trap installer -----------
// The genuine slow exec table is generated in kernelhwsrv (which is
// read-only here), so PortTrapExecInstall() repurposes the test-only slot 270
// at boot for port operations. kernelhwsrv source is not touched. Flags=0:
// the callees enter CS themselves (as the shell's RunImage path does).
extern const TUint32 EpocSlowExecTable[];
namespace { TUint8 sSvcImg[512 * 1024]; }  // fit large E32s (DOOM ~338 KiB) + margin

// exec 270: ctl(op, arg) — op 2: FBO_FBINFO; op 3: FBO_FBBLIT; op 4: PTRINJ
// sink.  Ops 0/1 (dead/reap) and launch slot 268 stay retired with svcmgr;
// kill slot 269 is reinstalled by PortTrapExecInstall (cp4 peer kill).
static TInt PortCtlExec(TInt aOp, TInt aArg)
    {
    // Append-only capability: bulk framebuffer blit / info.  These are new
    // operations on this existing genuine slow exec; slots 274/275 cannot be
    // added safely (they overlap EpocFastExecTable -- see qemu_fbblit.h).
    if (aOp == (TInt)FBO_FBINFO) return fbblit_info_exec(aArg);
    if (aOp == (TInt)FBO_FBBLIT) return fbblit_blit_exec(aArg);
    // PTRINJ proof sink: called from the ring-3 ord-1868 Pos wrapper via
    // int 0x21; runs in ring 0 so KPrintf (COM1 + VGA) is legal here.
    if (aOp == 4)
        {
        KPrintf("PTRBRIDGE: TRawEvent::Pos x=%d y=%d\r\n",
                (TInt)(((TUint)aArg >> 16) & 0xffffu),
                (TInt)((TUint)aArg & 0xffffu));
        return KErrNone;
        }
    return KErrArgument;
    }

// --- PTRINJ-gated EUSER ord 1868 (TRawEvent::Pos) wrapper, EWSRV only --------
// Direct guest proof that genuine CEvent::RunL() actually calls EUSER ord 1868
// and that its 8-byte TPoint sret comes back in EAX:EDX.  Gated on the EWSRV
// process name AND \PTRINJ.TXT, so every other boot is byte-for-byte unchanged.
// kernelhwsrv/ and the genuine EWSRV image on disk are untouched; only this
// port-side loader rebinds one PE2 IAT slot at load time, then calls the
// genuine export exactly as before.
//
// Genuine Pos() (EUSER.DLL @1868, disasm @0x1000ac40) is cdecl with one stack
// arg: this=[ebp+8], x=[this+8] -> EAX, y=[this+0xc] -> EDX.  The wrapper is
// transparent: it re-pushes the caller's `this`, calls the genuine export,
// records EAX:EDX, emits slow exec 270 op 4 (logged from ring 0 by
// PortCtlExec), restores EAX:EDX and returns.  Only caller-saved EAX/ECX/EDX
// are touched; EBX/ESI/EDI/EBP and the stack are balanced (cdecl caller still
// cleans its own `this`).
TUint32 sPortOrigPos = 0;   // genuine ord-1868 address (set at bind)
// Written in AT&T syntax explicitly: GAS's Intel mode silently biases
// ESP-relative push displacements by +4, which would push the wrong stack slot.
asm(
    ".text\n"
    ".att_syntax noprefix\n"
    ".globl PortEwsrvPosHook\n"
    "PortEwsrvPosHook:\n"
    "  pushl 4(%esp)\n"                   // re-push this for genuine cdecl call
    "  call *sPortOrigPos\n"              // genuine TRawEvent::Pos(); EAX=x, EDX=y
    "  addl $4, %esp\n"                   // drop copied 'this' (caller cleans its own)
    "  pushl %eax\n"                      // preserve x across the logging exec
    "  pushl %edx\n"                      // preserve y
    "  movl $4, %ecx\n"                   // exec 270 op 4 = log Pos result
    "  movl 4(%esp), %edx\n"              // x
    "  shll $16, %edx\n"
    "  orl (%esp), %edx\n"                // packed (x<<16)|y
    "  movl $270, %eax\n"                 // slow exec: PortCtlExec
    "  int $0x21\n"                       // ring 0 -> KPrintf (QEMU serial)
    "  popl %edx\n"                       // y  (genuine Pos return)
    "  popl %eax\n"                       // x  (genuine Pos return)
    "  ret\n"
    ".intel_syntax noprefix\n"
    );

static TBool sPtrInjChecked = EFalse;
static TBool sPtrInjActive = EFalse;
static TBool PointerInjectionActive()
    {
    if (sPtrInjChecked) return sPtrInjActive;
    sPtrInjChecked = ETrue;
    TUint8 buf[64];
    TUint got = 0;
    char prev = FatCurLetter();
    if (!prev) FatUse('C');
    TInt r = FatReadPath("\\PTRINJ.TXT", buf, sizeof(buf), got);
    if (prev) FatUse(prev);
    sPtrInjActive = (r == KErrNone && got > 0);
    KPrintf("e32run: PTRINJ gate r=%d got=%u active=%d\r\n",
            r, got, sPtrInjActive);
    return sPtrInjActive;
    }

static TBool IsEwsrvProcess(const char* n)
    {
    return n && n[0] == 'E' && n[1] == 'W' && n[2] == 'S' &&
           n[3] == 'R' && n[4] == 'V';
    }

// Genuine FsRegisterThread() marks the server process AND the calling thread
// system-permanent/critical (sexec.cpp:2228-2229).  In a full Symbian boot
// that file server never exits, but this port cannot supply ESTART/media
// drivers, so its E32Main must return and the thread must exit.  A normal exit
// of a system-permanent thread faults K::ESystemThreadPanic (4) in
// DThread::Exit (sthread.cpp:738), which is the residual KERN 4.  Clear those
// two bits after the genuine registration, mirroring the existing UserPanic/
// UserSetCritical port shims in genuine/efile/efile_glue.cpp.
typedef TInt (*TFsRegisterFn)(TInt);
static TFsRegisterFn sOrigFsRegister = 0;
static TInt PortFsRegisterThread(TInt aUnused)
    {
    TInt r = sOrigFsRegister(aUnused);
    DThread* t = TheCurrentThread;
    if (t)
        {
        DProcess* p = t->iOwningProcess;
        if (p)
            p->iFlags &= ~(TUint)(KThreadFlagProcessCritical | KProcessFlagSystemPermanent);
        t->iFlags &= ~(TUint)(KThreadFlagSystemPermanent | KThreadFlagSystemCritical);
        KPrintf("FsRegister: cleared system-permanent/critical flags r=%d\r\n", r);
        }
    return r;
    }

// exec 269: kill(pid, 0) -> E32Kill result.  Restored for cp4_stress:
// user/killproc.c does exec2(269, pid, 0) to kill a peer process mid-test;
// with the slot uninstalled the call lands on genuine slot 269
// (KernMsgTest) and the kill silently does nothing.  Body is the pre-dd22ae2
// SvcMgrKillExec verbatim.  Slot 268 (launch) stays retired with svcmgr.
static TInt PortKillExec(TInt aPid, TInt)
    { return E32Kill((TUint)aPid); }

// --- exec 127/128 trace wrappers (server publish / session create) -----------
// EUSER ord 211 RSessionBase::CreateSession forwards to Exec::SessionCreate,
// slow exec 128 (EExecSessionCreate; us_ksvr.cpp:2290).  The genuine handler
// returns the new session handle in EAX (sipc.cpp:1355 -> DSession::MakeHandle
// -> K::MakeHandle); a zero/negative return makes SetReturnedHandle store 0 /
// KNullHandle in the client object (e32cmn.inl:3037-3060).  Wrap slots 127/128
// so every publish/connect logs its 8-bit name and result; kernelhwsrv/ and
// the genuine table flags stay untouched (only the function words are swapped,
// exactly like the slot-81 FsRegisterThread wrapper above).
static void PortDumpExecName(const char* aTag, TInt aR, const TDesC8& aName)
    {
    TKName n;
    n.Zero();
    Kern::KUDesGet(n, aName);
    char buf[256];
    TInt len = n.Length();
    if (len > (TInt)n.MaxLength()) len = (TInt)n.MaxLength();
    if (len > (TInt)sizeof(buf) - 1) len = (TInt)sizeof(buf) - 1;
    for (TInt i = 0; i < len; ++i)
        buf[i] = (char)n[i];
    buf[len] = 0;
    TUint pid = 0;
    if (TheCurrentThread && TheCurrentThread->iOwningProcess)
        pid = (TUint)TheCurrentThread->iOwningProcess->iId;
    KPrintf("%s: r=%d pid=%u name=<%s>\r\n", aTag, aR, pid, buf);
    }

typedef TInt (*TServerCreateOptFn)(const TDesC8&, TInt, TInt, TInt);
static TServerCreateOptFn sOrigServerCreateOpt = 0;
static TInt PortServerCreateOpt(const TDesC8& aName, TInt a2, TInt a3, TInt a4)
    {
    TInt r = sOrigServerCreateOpt(aName, a2, a3, a4);
    PortDumpExecName("exec127 ServerCreateWithOptions", r, aName);
    return r;
    }

typedef TInt (*TSessionCreateFn)(const TDesC8&, TInt, const TAny*, TInt);
static TSessionCreateFn sOrigSessionCreate = 0;
static TInt PortSessionCreate(const TDesC8& aName, TInt aSlots,
                              const TAny* aPol, TInt aMode)
    {
    TInt r = sOrigSessionCreate(aName, aSlots, aPol, aMode);
    PortDumpExecName("exec128 SessionCreate", r, aName);
    // Port-name fallback: g++ -m32 builds emit _L() literals with 32-bit
    // TText, and the prebuilt EUSER.DLL (true 2-byte TText) truncates them at
    // the first NUL word, so port-built servers register a "!"-prefixed name
    // as plain "!" (see the exec127 trace).  A hand-built full-length client
    // name ("!E32WindowServer", user/ws_e32_client.c) then misses with
    // KErrNotFound and SetReturnedHandle stores KNullHandle.  Retry once with
    // the port-registered "!" form so the session handle propagates; the
    // genuine handler is called through unchanged both times.
    if (r == KErrNotFound && aName.Length() > 1 && aName[0] == (TUint8)'!')
        {
        TBuf8<8> bang;
        bang.Copy(TPtrC8((const TUint8*)"!", 1));
        r = sOrigSessionCreate(bang, aSlots, aPol, aMode);
        PortDumpExecName("exec128 SessionCreate retry '!'", r, bang);
        }
    return r;
    }

void PortTrapExecInstall()
    {
    TUint32* tab = (TUint32*)EpocSlowExecTable;
    if (tab[0] < 271u)  // guard: table must hold the slot-270 install
        { KPrintf("PortTrap: exec table count %u, install skipped\r\n", tab[0]); return; }
    // Wrap the genuine FsRegisterThread so an exiting file-server thread does
    // not fault K::ESystemThreadPanic when this port tears EFILE down.
    if (!sOrigFsRegister)
        {
        sOrigFsRegister = (TFsRegisterFn)tab[4 + 2 * 81];
        tab[4 + 2 * 81] = (TUint32)(TLinAddr)&PortFsRegisterThread;
        KPrintf("PortTrap: FsRegisterThread wrapper installed (slot 81)\r\n");
        }
    // Slot 269: restore the pre-dd22ae2 peer-kill path (cp4_stress asserts it
    // via killproc's exec2(269, pid, 0)).  Slot 268 stays retired with svcmgr.
    tab[3 + 2 * 269u] = 0;  // flags: no claim/preprocess/extra args
    tab[4 + 2 * 269u] = (TUint32)(TLinAddr)&PortKillExec;
    KPrintf("PortTrap: kill exec installed (slot 269 -> E32Kill)\r\n");
    // Slot 270 stays installed, slimmed to the port ops 2/3/4 (fbblit +
    // PTRINJ).  svcmgr's dead/reap ops 0/1 and launch slot 268 are retired
    // with svcmgr.
    tab[3 + 2 * 270u] = 0;  // flags: no claim/preprocess/extra args
    tab[4 + 2 * 270u] = (TUint32)(TLinAddr)&PortCtlExec;
    KPrintf("PortTrap: ctl exec installed (slot 270, ops 2/3/4)\r\n");
    // Slots 127/128: trace wrappers around the genuine publish/session-create
    // handlers (flags words stay genuine).  Diagnostic for the ord-211
    // CreateSession handle-return path; the genuine handlers are called
    // through unchanged and their EAX result is returned verbatim.
    if (!sOrigServerCreateOpt)
        {
        sOrigServerCreateOpt = (TServerCreateOptFn)tab[4 + 2 * 127u];
        tab[4 + 2 * 127u] = (TUint32)(TLinAddr)&PortServerCreateOpt;
        KPrintf("PortTrap: exec 127 (ServerCreateWithOptions) trace wrapper installed\r\n");
        }
    if (!sOrigSessionCreate)
        {
        sOrigSessionCreate = (TSessionCreateFn)tab[4 + 2 * 128u];
        tab[4 + 2 * 128u] = (TUint32)(TLinAddr)&PortSessionCreate;
        KPrintf("PortTrap: exec 128 (SessionCreate) trace wrapper installed\r\n");
        }
    }

// --- G-01: genuine Symbian startup chain (boot list) --------------------------
// Init3 spawns GenuineBootLaunch() on a supervisor DThread (see the Asic) after
// PortTrapExecInstall(). The genuine chain EUSER.DLL (already resident) ->
// EFILE.E32 / EFSRV.E32 (genuine !FileServer) -> ESTART.E32 -> EWSRV.E32 ->
// ESHELL.E32 is probed on the boot volume by FAT presence, in genuine order;
// every component staged under its genuine name is launched through the same
// FatReadPath + E32Run path the shell's RunImage uses. Detection is by FAT
// presence, so disks without the genuine names simply skip those entries.
namespace {
struct GenuineBootEnt
    {
    const char* iPath;    // FAT path probed on the boot volume
    TBool iFileServer;    // ETrue: file-server slot (only one is launched)
    TBool iGraphics;      // ETrue: bring the VBE LFB up before the launch
    };
const GenuineBootEnt kGenuineBootList[] =
    {
    { "\\EFILE.E32",   ETrue,  EFalse },  // genuine !FileServer host (sfile)
    { "\\EFSRV.E32",   ETrue,  EFalse },  // alternate file-server name
    { "\\ESTART.E32",  EFalse, EFalse },  // genuine estart (estartmain.cpp)
    { "\\E32STRT.E32", EFalse, EFalse },  // alternate estart name
    { "\\EWSRV.E32",   EFalse, ETrue  },  // genuine window server (ws_main.cpp)
    { "\\ESHELL.E32",  EFalse, EFalse },  // genuine shell (etshell ts_main.cpp)
    };
enum { KGenuineBootCount = (TInt)(sizeof(kGenuineBootList) / sizeof(kGenuineBootList[0])) };

TInt GenuineBootTry(const GenuineBootEnt& aEnt)
    {
    char prev = FatCurLetter();
    if (!prev) FatUse('C');
    TUint got = 0;
    TInt r = FatReadPath(aEnt.iPath, sSvcImg, sizeof(sSvcImg), got);
    if (prev) FatUse(prev);
    if (r != KErrNone)
        return r;  // not staged on this boot volume
    if (aEnt.iGraphics)
        display_ensure_graphics();  // E32Run maps the LFB only while it is up
    const char* base = aEnt.iPath;
    for (const char* p = aEnt.iPath; *p; ++p)
        if (*p == '\\' || *p == '/' || *p == ':')
            base = p + 1;
    char name[32];
    TUint k = 0;
    while (base[k] && k < sizeof(name) - 1)
        { name[k] = base[k]; ++k; }
    name[k] = 0;
    r = E32Run(sSvcImg, got, name, 0);
    KPrintf("GenuineBoot: %s r=%d pid=%u\r\n", name, r, sSvcLastPid);
    return r;
    }
}  // namespace

void GenuineBootLaunch()
    {
    KPrintf("GenuineBoot: probing boot volume for the genuine startup chain\r\n");
    TBool genuineFs = EFalse;
    TBool serverUp = PortFileServerPublished();  // e.g. a shim won the boot race
    if (serverUp)
        KPrintf("GenuineBoot: !FileServer already published, skipping "
                "genuine file-server slot\r\n");
    for (TInt i = 0; i < KGenuineBootCount; ++i)
        {
        const GenuineBootEnt& e = kGenuineBootList[i];
        if (e.iFileServer && (genuineFs || serverUp))
            continue;  // one file server only
        if (GenuineBootTry(e) == KErrNone && e.iFileServer)
            genuineFs = ETrue;
        }
    // Opt-in genuine pointer bridge (variant/qemu_display.cpp).  Only active
    // when the test stages \PTRINJ.TXT; queues one real TRawEvent(EPointerMove)
    // via Kern::AddEvent so genuine EWSRV's UserSvr::RequestEvent -> Pos()
    // path is exercised.  Runs on this supervisor DThread (kernel unlocked).
    GenuinePointerBridge();

    if (genuineFs || serverUp)
        {
        KPrintf("GenuineBoot: file server up, svcmgr fallback skipped\r\n");
        return;
        }
    }

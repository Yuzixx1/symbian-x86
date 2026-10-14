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

extern "C" TUint8 qemu_tss[];  // 104B bootstrap TSS (bootcpu_gas.S BSS)
static TUint8 sTssStack[4096];

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
    0xCD, 0x21,                    // int 0x21 (never returns)
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
	0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5C06D,0x7CDCEFB7,0x0BDBDF21,
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
    if (aRelOff + 8 < aRelOff || aRelOff + 8 > aLen)
        return KErrCorrupt;
    TUint32 size = Rd32(aFile + aRelOff);
    TInt n = Rd32s(aFile + aRelOff + 4);
    if (size < 8 || (size & 3) || n < 0)
        return KErrCorrupt;
    if (aRelOff + size < aRelOff || aRelOff + size > aLen)
        return KErrCorrupt;
    TUint p = aRelOff + 8, end = aRelOff + size;
    TInt count = 0;
    while (p < end)
        {
        if (p + 8 < p || p + 8 > end)
            return KErrCorrupt;
        TUint32 page = Rd32(aFile + p), bs = Rd32(aFile + p + 4);
        if ((page & 0xFFF) || bs < 8 || (bs & 3))
            return KErrCorrupt;
        if (p + bs < p || p + bs > end)
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
    if (count != n)
        return KErrCorrupt;
    return KErrNone;
    }

// Mirror of svRelocateSection (sf_lepoc.cpp): add code/data delta per type.
static void ApplyRelocs(const TUint8* aFile, TUint aRelOff, TUint8* aRun,
                        TUint32 aCodeDelta, TUint32 aDataDelta,
                        TUint32 aCodeBase, TUint32 aCodeSize)
    {
    TUint32 size = Rd32(aFile + aRelOff);
    TUint p = aRelOff + 8, end = aRelOff + size;
    while (p < end)
        {
        TUint32 page = Rd32(aFile + p), bs = Rd32(aFile + p + 4);
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
        return KErrNotSupported;
    if (codeOff > aLen || codeOff < 156)
        return KErrCorrupt;
    if (cpu != kCpuX86)
        return KErrNotSupported;
    if (UidChecksum(aBuf) != uidChk)
        return KErrCorrupt;
    if (sig != kEpoc)
        return KErrCorrupt;
    // Header CRC: recompute over a copy with the init constant in the field.
    // (Caller guarantees aLen >= codeOff; use the stack for the 156B copy.)
    if (codeOff > 512)
        return KErrCorrupt;
    TUint8 tmp[512];
    for (TUint i = 0; i < codeOff; ++i)
        tmp[i] = aBuf[i];
    tmp[20] = (TUint8)(kCrcInit & 0xFF);
    tmp[21] = (TUint8)((kCrcInit >> 8) & 0xFF);
    tmp[22] = (TUint8)((kCrcInit >> 16) & 0xFF);
    tmp[23] = (TUint8)((kCrcInit >> 24) & 0xFF);
    if (Crc32(tmp, codeOff) != hdrCrc)
        return KErrCorrupt;
    const TUint32 kDllUid = 0x10000079u;
    TBool isDll = (flags & 0x00000001u) != 0;
    if (isDll ? (uid1 != kDllUid) : (uid1 != kExeUid))
        return KErrCorrupt;  // uid1 must agree with the DLL flag (genuine)
    if (modver >= 0x80000000u || (modver & 0xFFFFu) > 0x8000u)
        return KErrNotSupported;
    if (comp != 0)
        return KErrNotSupported;  // uncompressed only for now
    if (heapMin < 0 || heapMax < heapMin || stackSize < 0 || bssSize < 0)
        return KErrCorrupt;
    if (flags & 0x000000E0u)  // x86: no entry-point-type bits
        return KErrNotSupported;
    if (flags & 0x00000018u)  // x86: no ABI bits
        return KErrNotSupported;
    TUint32 impFmt = flags & 0xF0000000u;
    if (impFmt != 0 && impFmt != 0x10000000u && impFmt != 0x20000000u)
        return KErrNotSupported;
    if (entry >= (TUint32)codeSize)
        return KErrCorrupt;
    if (entry + 12 + 4 > (TUint32)codeSize)
        return KErrCorrupt;
    if ((codeBase & 3) || (dataBase & 3))
        return KErrCorrupt;
    if (dllRefs < 0 || dllRefs > 1)
        return KErrCorrupt;
    if (dllRefs == 0 && impOff)
        return KErrCorrupt;  // refs and section must agree
    TUint32 codeEnd = codeOff + (TUint32)codeSize;
    if (codeEnd < codeOff || codeEnd > aLen)
        return KErrCorrupt;
    if (dataOff == 0)
        {
        if (dataSize != 0)
            return KErrCorrupt;
        }
    else
        {
        TUint32 dataEnd = dataOff + (TUint32)dataSize;
        if (dataEnd < dataOff || dataOff < codeEnd || dataEnd > aLen)
            return KErrCorrupt;
        }
    if (expCount < 0 || expCount > 65535)
        return KErrCorrupt;
    if (!isDll && expCount != 0)  // EXEs carry no exports here
        return KErrNotSupported;
    if (isDll && expCount <= 0)
        return KErrCorrupt;  // a DLL must export something
    if (expCount > 0)
        {
        TUint32 expEnd = expOff + (TUint32)expCount * 4;
        if (expEnd < expOff || expOff < codeOff || expEnd > codeEnd)
            return KErrCorrupt;  // export dir must lie in the code part
        for (TInt i = 0; i < expCount; ++i)
            {
            TUint32 target = Rd32(aBuf + expOff + (TUint)i * 4);
            if (target >= (TUint32)codeSize)
                return KErrCorrupt;  // export target outside code
            }
        }
    if (textSize < 0 || textSize > codeSize)
        return KErrCorrupt;
    // EUSER-ordinal imports (ELF-style code-offset list, positional:
    // list entry k serves ordinal k+1: 1=Exit, 2=PutC, 3=CmdLen, ...).
    // Mirrors E32ImageHeaderV::ValidateImports bounds discipline.
    aInfo.iImpOff = 0; aInfo.iImpN = 0;
    if (impOff)
        {
        if (dllRefs != 1)
            return KErrCorrupt;
        if (impOff + 4 < impOff || impOff + 4 > aLen)
            return KErrCorrupt;
        TUint32 isize = Rd32(aBuf + impOff);
        if (isize < 22 || impOff + 4 + isize < impOff + 4 ||
            impOff + 4 + isize > aLen)
            return KErrCorrupt;
        const TUint8* sec = aBuf + impOff + 4;
        TUint32 nameOff = Rd32(sec), n = (TUint32)Rd32s(sec + 4);
        if (n < 2 || n > 4)
            return KErrNotSupported;  // ordinals 1..4 served here
        if (isize < 14 + 4 * n)
            return KErrCorrupt;
        // name+NUL must lie within header+iSize (offsets count the header).
        if (nameOff + 6 < nameOff || nameOff + 6 > isize + 4)
            return KErrCorrupt;
        const TUint8* nm = aBuf + impOff + nameOff;
        if (nm[0] != 'E' || nm[1] != 'U' || nm[2] != 'S' ||
            nm[3] != 'E' || nm[4] != 'R' || nm[5] != 0)
            return KErrNotSupported;  // unknown provider DLL
        for (TUint k = 0; k < n; ++k)
            {
            TUint32 s = Rd32(sec + 8 + 4 * k);
            if ((s & 3) || s < (TUint32)textSize ||
                s + 4 < s || s + 4 > (TUint32)codeSize)
                return KErrCorrupt;  // IAT slots live past .text
            for (TUint j = 0; j < k; ++j)
                if (Rd32(sec + 8 + 4 * j) == s)
                    return KErrCorrupt;  // slots must not alias
            aInfo.iSlots[k] = s;
            }
        aInfo.iImpOff = impOff; aInfo.iImpN = (TInt)n;
        }
    TInt rr = ValidateRelocs(aBuf, aLen, codeRelocOff, (TUint)codeSize);
    if (rr != KErrNone)
        return rr;
    if (dataRelocOff && (!dataOff || !dataSize))
        return KErrCorrupt;
    rr = ValidateRelocs(aBuf, aLen, dataRelocOff, (TUint)dataSize);
    if (rr != KErrNone)
        return rr;
    // Export-desc tail: offsetof(iExportDescSize)=152, NoHoles tail=4.
    if (codeOff != 156)
        return KErrNotSupported;
    if (aBuf[152] != 0 || aBuf[153] != 0 || aBuf[154] != 0x00)
        return KErrNotSupported;

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
    TUint32 sDllExp[8];
    TInt sDllExpN = 0;
    TUint8 sDllFile[65536];
    }

static TInt E32LoadDll()
    {
    if (sDllLoaded)
        return KErrNone;
    TUint got = 0;
    TInt r = FatReadPath("\\EUSER.DLL", sDllFile, sizeof(sDllFile), got);
    if (r != KErrNone)
        return r;
    TE32Info di;
    r = E32Validate(sDllFile, got, di);
    if (r != KErrNone)
        return r;
    if (!(di.iUid1 == 0x10000079u) || di.iExpCount < 2 || di.iExpCount > 8)
        return KErrCorrupt;
    // Resident pilot DLLs have no writable state or dependencies. Do not
    // silently load only the code half of a more complicated library.
    if (di.iDataSize || di.iBssSize || di.iDllRefs || di.iDataRelocOff ||
        di.iCodeRelocOff || sDllFile[di.iCodeOff + di.iEntry] != 0xC3)
        return KErrNotSupported;
    TUint8* run = (TUint8*)Kern::Alloc((TInt)di.iCodeSize);
    if (!run)
        return KErrNoMemory;
    memcpy(run, sDllFile + di.iCodeOff, (TUint)di.iCodeSize);
    TUint32 codeDelta = (TUint32)run - di.iCodeBase;
    TUint32 dataDelta = 0;
    if (di.iCodeRelocOff)
        ApplyRelocs(sDllFile, di.iCodeRelocOff, run, codeDelta, dataDelta,
                     di.iCodeBase, (TUint32)di.iCodeSize);
    for (TInt i = 0; i < di.iExpCount; ++i)
        sDllExp[i] = (TUint32)(run + Rd32(sDllFile + di.iExpOff + (TUint)i * 4));
    sDllExpN = di.iExpCount;
    sDllCode = run;
    sDllLoaded = ETrue;
    KPrintf("e32run: EUSER.DLL resident code=%x exports=%d\r\n", run, sDllExpN);
    return KErrNone;
    }

// Run an import-free, uncompressed EXE as a genuine user process.
// Loader copy: kernel allocates the code region (AllocRegion ignores the
// linked base); we copy the file's code section to the returned run
// address (position-independent code needs no fixups). Thread starts
// suspended; Loaded() marks ready, Resume() starts it.
static void ExecRecRemember(DProcess* aProc, DCodeSeg* aSeg, const char* aName);
static TBool ExecRecAvailable();

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
    // Both per-process tables must have room: sProcs (CWD) and sExecs
    // (code-seg reaper). Failing before ProcessCreate avoids untracked
    // processes and leaked segs.
    if (!ProcExtAvailable() || !ExecRecAvailable()) return KErrOverflow;
    // Resolve DLL imports before creating the process, so a missing
    // export fails fast without a stillborn process left behind.
    TInt dllR = KErrNone;
    if (info.iImpN > 0)
        {
        dllR = E32LoadDll();
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
            { NKern::ThreadLeaveCS(); Kern::Free(ppinfo); return KErrNoMemory; }
        KPrintf("e32run: cmdline %d chars\r\n", clen);
        }
    TInt h = 0;
    KPrintf("e32run: creating process...\r\n");
    r = Kern::ProcessCreate(proc, pinfo, cmd, &h);
    if (!proc && cmd)
        delete cmd;  // NewProcess failed: ownership never transferred
    KPrintf("e32run: ProcessCreate r=%d run=%x proc=%x\r\n", r, pinfo.iCodeRunAddress, proc);
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
            ApplyRelocs(aFileBuf, info.iCodeRelocOff, codeRun,
                         codeDelta, dataDelta,
                         info.iCodeBase, (TUint32)info.iCodeSize);
        if (info.iDataRelocOff)
            ApplyRelocs(aFileBuf, info.iDataRelocOff,
                        (TUint8*)pinfo.iDataRunAddress, codeDelta, dataDelta,
                        info.iCodeBase, (TUint32)info.iCodeSize);
        if (info.iImpN > 0)
            {
            // Ordinals are positional (ELF list order = ordinal). Prefer the
            // resident EUSER.DLL file; ordinals 1-2 fall back to kernel
            // trampolines when it is absent so older disks still run.
            if (dllR == KErrNone && sDllExpN >= info.iImpN)
                {
                for (TInt k = 0; k < info.iImpN; ++k)
                    *(TUint32*)(codeRun + info.iSlots[k]) = sDllExp[k];
                KPrintf("e32run: imports bound dll (%d ordinals)\r\n",
                    info.iImpN);
                }
            else
                {
                *(TUint32*)(codeRun + info.iSlots[0]) = (TUint32)sTrampExit;
                *(TUint32*)(codeRun + info.iSlots[1]) = (TUint32)sTrampPutc;
                KPrintf("e32run: imports bound trampoline (dll r=%d)\r\n",
                    dllR);
                }
            }
        KPrintf("e32run: sections in (deltas %x/%x), Loaded()...\r\n",
            codeDelta, dataDelta);
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

// Kill a user process by container id (shell `kill`). Refuses the kernel
// and the shell's own process (pointer comparison, not numeric id).
// Locking mirrors OpenById (sexec.cpp): Open under the container lock,
// release it, then Die (container+process nesting faults otherwise).
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
    if (found && found == self)
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
            ProcExtRemove(p);
            sExecs[i].iProc = NULL; sExecs[i].iSeg = NULL;
            sExecs[i].iName[0] = 0;
            }
        }
    DCodeSeg::Signal();
    NKern::ThreadLeaveCS();
    return n;
    }

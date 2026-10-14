// rom_image.h — minimal synthesized ROM image (Phase 4).
// Stands in for rombuild output (TRomHeader + kernel image headers).
// Layout policy: field ORDER and offsets of the covered prefix match the
// genuine headers (e32rom.h class TRomHeader, kernboot.h SSuperPageBase)
// so future genuine startup code can read them; trailing genuine fields
// are deferred to full-ROM work and live in an explicit QEMU extension.
#ifndef __QEMU_ROM_IMAGE_H__
#define __QEMU_ROM_IMAGE_H__

#include "qemu_assp.h"  // TUint/TInt/TUint8

typedef unsigned short TUint16;
typedef unsigned long long TUint64;

// --- Compatible prefix of genuine TRomHeader (offsets in comments) ---
struct QemuRomHeader
    {
    TUint8  iJump[124];          // +0
    TUint   iRestartVector;      // +124
    TUint64 iTime;               // +128 (build time, microseconds)
    TUint   iTimeHi;             // +136
    TUint   iRomBase;            // +140 linear base of ROM
    TUint   iRomSize;            // +144
    TUint   iRomRootDirectoryList; // +148
    TUint   iKernDataAddress;    // +152
    TUint   iKernelLimit;        // +156 top of kernel region (direct model)
    TUint   iPrimaryFile;        // +160
    TUint   iSecondaryFile;      // +164
    TUint   iCheckSum;           // +168 (32-bit word-sum == KRomCheckTarget)
    // --- QEMU extension (NOT genuine offsets; genuine has iHardware/... here)
    TUint   iQemuMagic;          // 'QROM'
    TUint   iVersion;            // EKA2 version tag
    TUint   iDebugPort;          // COM1 base (cf. TRomHeader::iDebugPort)
    TUint   iUncompressedSize;
    };

// --- Compatible prefix of genuine SSuperPageBase (offsets in comments) ---
struct QemuSuperPage
    {
    TInt  iRamDriveSize;     // +0
    TInt  iTotalRomSize;     // +4
    TInt  iTotalRamSize;     // +8
    TUint iPageDir;          // +12 phys addr of page directory
    TInt  iKernelFault;      // +16
    TUint iRamBootData;      // +20
    TUint iRomBootData;      // +24
    TUint iBootTable;        // +28
    TUint iCodeBase;         // +32
    TUint iBootFlags;        // +36
    TUint iBootAlloc;        // +40
    TUint iMachineData;      // +44
    TUint iActiveVariant;    // +48
    TUint iPrimaryEntry;     // +52
    TInt  iInitialHeapSize;  // +56
    TInt  iDebugPort;        // +60
    TUint iCpuId;            // +64 CPU feature flags (cf. iCpuId/EX86Feat_*)
    TUint iKernelLimit;      // +68 direct model: top of kernel region
    TUint iRamBase;          // +72 direct model: linear base of RAM
    TUint iQemuSignature;    // +76 'QSPG' (extension; genuine has cache fields here)
    };

namespace QemuRom
    {
    extern QemuRomHeader RomHeader;
    extern QemuSuperPage SuperPage;
    void Synthesize(TUint aCpuFeatures);
    TInt ValidateRom();    // 0 ok: magic + word-sum checksum
    TInt ValidateSuperPage(); // 0 ok: signature + RAM/PageDir/CpuId sane
    void Report();
    }

#endif

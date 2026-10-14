// qemu_variant_rom.cpp — QEMU ROM/SuperPage synthesis for the full Kern image.
//
// The genuine bootstrap + P::CreateVariant + P::KernelInfo consume a TRomHeader
// (variant list, kernel data/heap geometry) and a TSuperPage (RAM geometry,
// primary kernel entry, root dir list). Real baseports get these from estrt/
// rombuild; here they are synthesized from the genuine struct layouts
// (<e32rom.h>, <kernel/kernboot.h>) around link-time addresses. No genuine
// code is modified; this file is variant data + two tiny entry points.
//
// Layout (all inside one contiguous blob so M::IsRomAddress() covers it):
//   QRomBlob: TRomHeader + variant TRomEntry/TRomImageHeader +
//             ekern TRomEntry/TRomImageHeader + TRomRootDirectoryList.

#include <e32rom.h>
#include <e32uid.h>
#include <kernel/kernboot.h>
#include "qemu_variant.h"

static inline void Out8(unsigned short aPort, unsigned char aVal)
    { __asm__ __volatile__("outb %0,%1" : : "a"(aVal), "Nd"(aPort)); }

// ------------------------------------------------------------------ geometry
// (Shared constants in qemu_variant.h: kQemuKernDataAddress et al.)
static const unsigned long kKernDataAddress = kQemuKernDataAddress;

// ------------------------------------------------------------------ ROM blob
// (QRomBlob layout shared via qemu_variant.h.)
extern "C" TUint8 qemu_rom_header_reserve[4096];
extern "C" TUint8 qemu_superpage_reserve[16384];

static QRomBlob* RomBlob()
    { return (QRomBlob*)qemu_rom_header_reserve; }

// Variant entry points (defined in qemu_variant_asic.cpp).
extern "C" TInt QemuVariantEntry(TInt aReason);
extern "C" TAny* QemuVariantInitialise(TInt aMode);

// Dummy extension entry: priority 0 at Init0, KErrNone at Init1. Product ROMs
// always carry extensions; the extension array panics on zero entries.
extern "C" TInt QemuExtEntry(TInt /*aReason*/)
    {
    return 0;
    }

// Runs from HwInit0 (before _E32Startup_Body fills RomHeaderAddress, which
// just points at this same blob). Absolute addresses are link-time constants.
extern "C" void QemuRomInit()
    {
    QRomBlob* r = RomBlob();
    for (TUint i = 0; i < sizeof(*r); ++i)
        ((TUint8*)r)[i] = 0;

    // --- TRomHeader (fields consumed: shadowed uses listed) ---
    // iTime must be a VALID TTimeK (microseconds since 0000-01-01): cold boot
    // sets the system clock from the ROM timestamp (P::InitSystemTime) and
    // panics on overflow. 2026-10-05 00:00:00 nominal Gregorian.
    r->hdr.iTime = 63958377600000000LL;
    r->hdr.iTimeHi = 0x0000E339u;
    r->hdr.iRomBase = (TLinAddr)r;
    r->hdr.iRomSize = sizeof(*r);
    r->hdr.iUncompressedSize = sizeof(*r);      // M::IsRomAddress coverage
    r->hdr.iKernDataAddress = kKernDataAddress; // P::KernelInfo stack/heap
    r->hdr.iKernelLimit = kKernDataAddress + kQemuKernRegionSize;
    r->hdr.iTotalSvDataSize = 0x10000;          // kernel data footprint
    r->hdr.iVariantFile = (TLinAddr)&r->varEntry;
    r->hdr.iExtensionFile = (TLinAddr)&r->extEntry;  // one dummy extension
    r->hdr.iPrimaryFile = (TLinAddr)&r->kernEntry;
    r->hdr.iUserDataAddress = kQemuUserDataAddress; // M::Init1 user pool base
    r->hdr.iTotalUserDataSize = 0;

    // --- variant ROM entry + image ---
    r->varEntry.iSize = (TInt)sizeof(r->varImg);
    r->varEntry.iAddressLin = (TUint32)&r->varImg;
    TRomImageHeader* v = &r->varImg;
    v->iUid1 = (TUint32)KExecutableImageUidValue;
    v->iUid2 = 0x1000008Bu;
    v->iUid3 = 0x71000001u;                     // QEMU variant image uid
    v->iEntryPoint = (TUint32)&QemuVariantEntry;
    v->iDataSize = 0;                           // no .data relocation
    v->iExportDirCount = 2;
    v->iExportDir = (TUint32)&r->varExportDir;
    v->iHardwareVariant = 0;                    // layer 0: always compatible
    v->iNextExtension = 0;
    // NB: ROM export ordinal 1 lives at index 0 (cf. A::CreateVariant).
    r->varExportDir[0] = (TUint32)&QemuVariantInitialise;
    r->varExportDir[1] = 0;

    // --- dummy extension ROM entry + image (see qemu_variant.h) ---
    r->extEntry.iSize = (TInt)sizeof(r->extImg);
    r->extEntry.iAddressLin = (TUint32)&r->extImg;
    TRomImageHeader* e = &r->extImg;
    e->iUid1 = (TUint32)KExecutableImageUidValue;
    e->iUid2 = 0x1000008Bu;
    e->iUid3 = 0x71000002u;
    e->iEntryPoint = (TUint32)&QemuExtEntry;
    e->iDataSize = 0;
    e->iHardwareVariant = 0;
    e->iNextExtension = 0;

    // --- ekern.exe ROM entry + image (consumed by Epoc::RomProcessInfo) ---
    r->kernEntry.iSize = (TInt)sizeof(r->kernImg);
    r->kernEntry.iAddressLin = (TUint32)&r->kernImg;
    TRomImageHeader* k = &r->kernImg;
    k->iUid1 = (TUint32)KExecutableImageUidValue;
    k->iUid2 = 0x1000008Bu;
    k->iUid3 = 0x100041AFu;                     // genuine ekern.exe UID3
    k->iEntryPoint = 0;                         // kernel proc: unused veneer
    k->iCodeAddress = 0x100000u;                // link base (cf. link.ld)
    k->iCodeSize = 0x40000;
    k->iTextSize = 0x40000;
    k->iDataAddress = kKernDataAddress;
    k->iDataSize = 0;
    k->iBssSize = 0;
    k->iHeapSizeMin = kQemuInitialHeapSize;
    k->iHeapSizeMax = 0x100000;                 // 1MB kernel heap max
    k->iStackSize = 0x4000;
    k->iDataBssLinearBase = kKernDataAddress;
    k->iTotalDataSize = 0x10000;
    k->iPriority = (TProcessPriority)0;
    k->iFlags = KRomImageFlagsKernelMask | KRomImageFlagFixedAddressExe;
    k->iHardwareVariant = 0;
    k->iNextExtension = 0;
    // iDllRefTable/iExportDir/iS/iModuleVersion/iExceptionDescriptor stay 0.

    // --- ROM root directory list (FindRomRootDirectory) ---
    r->rootList.iNumRootDirs = 1;
    r->rootList.iRootDir[0].iHardwareVariant = 0;
    r->rootList.iRootDir[0].iAddressLin = (TLinAddr)&r->rootDir;
    r->rootDir.iSize = 0;  // empty directory: no files yet (storage phase)
    }

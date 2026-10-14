// rom_image.cpp — synthesize + validate the minimal ROM/SuperPage.
#include "rom_image.h"
#include "../inc/qemu_config.h"

extern "C" TUint qemu_page_dir[];

QemuRomHeader QemuRom::RomHeader;
QemuSuperPage QemuRom::SuperPage;

static const TUint KRomCheckTarget = 0x51EB0001u;
static const TUint KQemuRomMagic = 0x51524F4Du;   // 'QROM'
static const TUint KQemuSpSignature = 0x51535047u; // 'QSPG'

void QemuRom::Synthesize(TUint aCpuFeatures)
    {
    // ROM header: identity load @1M, kernel data fictional-but-plausible
    for (unsigned i = 0; i < sizeof(RomHeader.iJump); ++i)
        RomHeader.iJump[i] = 0x90;               // NOP sled (genuine: branch stub)
    RomHeader.iRestartVector = QEMU_KERNEL_LOAD + 0x100;
    RomHeader.iTime = 0x0123456789ABCDEFULL;
    RomHeader.iTimeHi = 0x01234567u;
    RomHeader.iRomBase = QEMU_KERNEL_LOAD;
    RomHeader.iRomSize = 0x00200000u;             // 2M image
    RomHeader.iRomRootDirectoryList = 0;
    RomHeader.iKernDataAddress = 0x00200000u;     // kernel data @2M
    RomHeader.iKernelLimit = 0x00400000u;         // kernel region top @4M
    RomHeader.iPrimaryFile = QEMU_KERNEL_LOAD + 0x200;
    RomHeader.iSecondaryFile = 0;
    RomHeader.iQemuMagic = KQemuRomMagic;
    RomHeader.iVersion = 0x00060001u;             // EKA2 v6.1 tag
    RomHeader.iDebugPort = QEMU_COM1_BASE;
    RomHeader.iUncompressedSize = RomHeader.iRomSize;
    // checksum over prefix words excluding iCheckSum itself
    const TUint* w = (const TUint*)&RomHeader;
    TUint sum = 0;
    for (unsigned i = 0; i < sizeof(RomHeader) / 4; ++i)
        {
        if ((const void*)&w[i] == (const void*)&RomHeader.iCheckSum) continue;
        sum += w[i];
        }
    RomHeader.iCheckSum = KRomCheckTarget - sum;

    // SuperPage: direct model, identity-mapped RAM
    SuperPage.iRamDriveSize = 0;
    SuperPage.iTotalRomSize = (TInt)RomHeader.iRomSize;
    SuperPage.iTotalRamSize = (TInt)QEMU_RAM_SIZE;
    SuperPage.iPageDir = (TUint)qemu_page_dir;
    SuperPage.iKernelFault = 0;
    SuperPage.iRamBootData = 0;
    SuperPage.iRomBootData = 0;
    SuperPage.iBootTable = 0;
    SuperPage.iCodeBase = RomHeader.iRomBase;
    SuperPage.iBootFlags = 0x00000001u;           // running-from-RAM marker
    SuperPage.iBootAlloc = 0;
    SuperPage.iMachineData = 0;
    SuperPage.iActiveVariant = 0;
    SuperPage.iPrimaryEntry = RomHeader.iPrimaryFile;
    SuperPage.iInitialHeapSize = 24 * 1024;
    SuperPage.iDebugPort = (TInt)QEMU_COM1_BASE;
    SuperPage.iCpuId = aCpuFeatures;
    SuperPage.iKernelLimit = RomHeader.iKernelLimit;
    SuperPage.iRamBase = QEMU_RAM_BASE;
    SuperPage.iQemuSignature = KQemuSpSignature;
    }

TInt QemuRom::ValidateRom()
    {
    if (RomHeader.iQemuMagic != KQemuRomMagic) return -1;
    const TUint* w = (const TUint*)&RomHeader;
    TUint sum = 0;
    for (unsigned i = 0; i < sizeof(RomHeader) / 4; ++i) sum += w[i];
    if (sum != KRomCheckTarget) return -2;
    if (RomHeader.iRomBase != QEMU_KERNEL_LOAD) return -3;
    if (RomHeader.iKernDataAddress == 0 || RomHeader.iKernelLimit <= RomHeader.iKernDataAddress) return -4;
    return 0;
    }

TInt QemuRom::ValidateSuperPage()
    {
    if (SuperPage.iQemuSignature != KQemuSpSignature) return -1;
    if (SuperPage.iTotalRamSize != (TInt)QEMU_RAM_SIZE) return -2;
    if ((SuperPage.iPageDir & 0xFFFu) != 0) return -3;  // must be page-aligned
    if (SuperPage.iCpuId == 0) return -4;
    if (SuperPage.iRamBase != QEMU_RAM_BASE) return -5;
    return 0;
    }

void QemuRom::Report()
    {
    QemuUart::Puts("EKA2-QEMU ROM: base=0x");
    QemuUart::PutHex(RomHeader.iRomBase);
    QemuUart::Puts(" kernData=0x");
    QemuUart::PutHex(RomHeader.iKernDataAddress);
    QemuUart::Puts(" kernLimit=0x");
    QemuUart::PutHex(RomHeader.iKernelLimit);
    QemuUart::Puts("\r\nEKA2-QEMU SuperPage: ram=0x");
    QemuUart::PutHex((TUint)SuperPage.iTotalRamSize);
    QemuUart::Puts(" pageDir=0x");
    QemuUart::PutHex(SuperPage.iPageDir);
    QemuUart::Puts(" cpuId=0x");
    QemuUart::PutHex(SuperPage.iCpuId);
    QemuUart::Puts("\r\n");
    }

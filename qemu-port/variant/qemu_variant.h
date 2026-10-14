// qemu_variant.h — shared QEMU-variant declarations for the full Kern image.
//
// Genuine struct layouts (<e32rom.h>); everything else is our variant.

#ifndef __QEMU_VARIANT_H__
#define __QEMU_VARIANT_H__

#include <e32rom.h>

// Contiguous ROM synth blob (variant/qemu_variant_rom.cpp). Lives at
// qemu_rom_header_reserve so M::IsRomAddress() covers every image header.
struct QRomBlob
    {
    TRomHeader hdr;
    TRomEntry varEntry;
    TRomImageHeader varImg;
    TUint32 varExportDir[2];
    TRomEntry extEntry;      // dummy extension: product ROMs always have
    TRomImageHeader extImg;  // >=1, and zero extensions panic (RArray(0))
    TRomEntry kernEntry;
    TRomImageHeader kernImg;
    TRomRootDirectoryList rootList;  // iRootDir[1]
    TRomDir rootDir;                 // empty but valid (storage phase fills)
    };

// Identity-mapped RAM geometry (QEMU -m 128, link base 0x100000).
static const unsigned long kQemuKernDataAddress = 0x1000000ul;
static const unsigned long kQemuKernRegionSize  = 0x200000ul;
static const unsigned long kQemuUserDataAddress = 0x2000000ul;
static const unsigned long kQemuTotalRamSize    = 128ul * 1024ul * 1024ul;
static const int kQemuInitialHeapSize = 0x40000;

extern "C"
    {
    // variant/qemu_variant_rom.cpp
    void QemuRomInit();
    // variant/qemu_variant_asic.cpp
    TInt QemuVariantEntry(TInt aReason);
    TAny* QemuVariantInitialise(TInt aMode);
    // variant/qemu_variant_int.cpp (plain-C dispatcher, ISR-driven)
    void X86IrqDispatcher();
    // Binds the PIT tick + PS/2 ISRs. Called from Asic::Init1.
    void QemuIrqBindTickKbd();
    // RTC seconds since 2000 (variant/qemu_variant_asic.cpp); -1 if bad.
    TInt QemuRtcSeconds();
    // Set RTC from seconds since 2000; negative rejected. Returns Symbian error.
    TInt QemuSetRtcSeconds(TInt aSeconds);
    }

// PCI bus scan (variant/qemu_pci.cpp).
void PciList();
unsigned int PciRead32(unsigned int aBus, unsigned int aDev,
                       unsigned int aFn, unsigned int aReg);
void PciWrite32(unsigned int aBus, unsigned int aDev,
                unsigned int aFn, unsigned int aReg, unsigned int aVal);
TBool PciFindClass(unsigned int aClass, unsigned int aSub,
                   unsigned int* aBus, unsigned int* aDev, unsigned int* aFn);

// ATA PIO driver (variant/qemu_ata.cpp). Devices 0..3 = pri-master,
// pri-slave, sec-master, sec-slave. LBA28, polled.
TInt AtaInit();
TInt AtaCount();
TBool AtaPresent(int aDev);
TUint32 AtaSectors(int aDev);
const char* AtaModel(int aDev);
TInt AtaRead(int aDev, TUint32 aLba, TUint aCount, TUint8* aBuf);
TInt AtaWrite(int aDev, TUint32 aLba, TUint aCount, const TUint8* aBuf);

// AHCI SATA driver (variant/qemu_ahci.cpp). Ports present as Fat devices
// 4..7 behind the Ata* dispatch below. DMA, polled.
TInt AhciInit();
TInt AhciCount();
TBool AhciPresent(int aDev);   // 0..3 = AHCI ports 0..3
TUint32 AhciSectors(int aDev);
const char* AhciModel(int aDev);
TInt AhciRead(int aDev, TUint32 aLba, TUint aCount, TUint8* aBuf);
TInt AhciWrite(int aDev, TUint32 aLba, TUint aCount, const TUint8* aBuf);
void AhciInfo();

// xHCI USB3, polled command/event/transfer rings and one direct-root-port
// HID boot keyboard. No hubs, hotplug, multiple devices or mouse yet.
TInt XhciInit();
void XhciInfo();
TInt XhciStart();
TInt XhciNoOp();
TInt XhciPortReset(int aPort); // zero-based root port
TInt XhciEnableSlot(int aPort);
TInt XhciAddress(int aPort);
TInt XhciGetDescriptor(int aPort, TUint aType, TUint aLength);
TInt XhciKeyboardInit(int aPort);
void XhciKeyboardPoll();
TBool XhciKeyboardReady();
TInt XhciAutoKeyboard();
void XhciKeyboardInfo();
void XhciPorts();

// FAT32 on ATA (variant/fat32.cpp). Up to 8 mounted volumes (C:, D:, ...)
// across drives/partitions, each with its own CWD. Mount, list/read/write.
TInt FatMount(int aDev, int aPart);   // aPart<0: first FAT32 partition
TInt UserFilesInit();
TInt UserFilesChannelCount();
TInt UserConInit();          // transitional console-input service
TBool ConInputLeased();      // a user program owns the input stream
TBool ConOwnerAlive();       // lease owner still has a live thread
void ConForceRelease();      // reclaim a dead owner's lease
int ConPollChar();           // shared stream; caller must hold the lease
class DProcess;
TBool ProcExtAvailable(); // caller serializes process launch/reap
void ProcExtCapture(DProcess* aProc);
void ProcExtRemove(DProcess* aProc);
void UserFilesLock();
void UserFilesUnlock();
TInt FatParts(int aDev);              // list partitions, returns FAT32 count
char FatCurLetter();                  // active volume letter, 0 if none
TInt FatUse(char aLetter);            // activate a mounted volume
void FatList();                       // list mounted volumes (* = active)
void FatPrompt();                     // "C:/path> " or "EKA2> "
TInt FatDir();
TInt FatDirWide();                  // compact DOS-style /W listing
TInt FatDirPath(const char* aPath, TBool aWide); // temporary directory view
TInt FatCdPath(const char* aPath);     // persistent directory change
TInt FatTypePath(const char* aPath);
TInt FatReadPath(const char* aPath, TUint8* aBuf, TUint aMax, TUint& aOutLen);
TInt FatWritePath(const char* aPath, const TUint8* aData, TUint aLen);
TInt FatExistsPath(const char* aPath);
TInt FatDelPath(const char* aPath);
TInt FatMkdirPath(const char* aPath);
TInt FatRenamePath(const char* aOld, const char* aNew);
TInt FatType(const char* aName);
TInt FatRead(const char* aName, TUint8* aBuf, TUint aMax, TUint& aOutLen);
TInt FatExists(const char* aName); // KErrNone if file, KErrAlreadyExists if dir
TInt FatWrite(const char* aName, const TUint8* aData, TUint aLen);
TInt FatRename(const char* aOld, const char* aNew); // same-dir 8.3 metadata rename
TInt FatDel(const char* aName);
TInt FatMkdir(const char* aName);
TInt FatCd(const char* aName);        // ".." = up, NULL/empty = root
TInt FatFormat(int aDev);             // whole-disk single FAT32 partition
void FatGetCwdPath(char* aOut, TUint aMax);  // active volume CWD: "\" or "\A\B"

// USB HID on UHCI (variant/qemu_usb.cpp). Polled; `usb` enumerates.
TInt UsbInit();
void UsbPoll();   // feed USB keyboards into the shell input ring
void UsbList();   // list UHCI devices
void UsbDebugPoll();  // one-shot interrupt-IN dump (bring-up debug)

// OHCI backend (variant/qemu_usb_ohci.cpp). Same polled discipline.
TInt OhciPresent();
TInt OhciInit();
int OhciPorts();
TInt OhciPortReset(int aPort, TBool& aLow);
TInt OhciControl(TUint8 aAddr, TUint8 aMaxPkt, TBool aLow,
                 TUint8 aReqType, TUint8 aReq, TUint16 aValue, TUint16 aIndex,
                 TUint16 aLen, TUint8* aData, TBool aIn);
TInt OhciIntPoll(TUint8 aAddr, TUint8 aEp, TUint8& aToggle,
                 TUint8* aDst, TUint aLen, TBool aLow, TUint8 aMaxPkt);

// E32 image validation (variant/e32load.cpp). Stage 3a: validate + report.
struct TE32Info
    {
    TUint32 iUid1, iUid2, iUid3;
    TUint32 iEntry;
    TUint32 iCodeBase, iDataBase;
    TInt iCodeSize, iDataSize, iBssSize;
    TInt iHeapMin, iHeapMax, iStackSize;
    TInt iDllRefs;
    TUint32 iCodeOff, iDataOff;  // file offsets for the loader copy step
    TUint16 iPriority;           // header process priority (may be mapped)
    TUint32 iCodeRelocOff;       // file offset of code reloc section (0=none)
    TUint32 iDataRelocOff;       // file offset of data reloc section (0=none)
    TUint32 iImpOff;             // file offset of import section (0=none)
    TInt iImpN;                  // import count (0, or 2..4 EUSER ordinals)
    TUint32 iSlots[4];             // code offsets of the IAT slots (ord k+1)
    TUint32 iExpOff;             // file offset of export directory (0=none)
    TInt iExpCount;              // export count (DLLs only)
    };
TInt E32Validate(const TUint8* aBuf, TUint aLen, TE32Info& aInfo);
TInt E32Run(const TUint8* aFileBuf, TUint aFileLen, const char* aName,
            const char* aCmdLine = 0);  // shell tail, stored as UTF-16 bytes
TInt E32Kill(TUint aId);     // die by container id (never the kernel)
TInt E32Reap();              // post-mortem code-seg cleanup, returns count
void E32PrintExits();        // ring of recent exit records (name/id/type/reason)
void VgaTeletype(char aC);
void ShellSpawnFull();

#endif // __QEMU_VARIANT_H__

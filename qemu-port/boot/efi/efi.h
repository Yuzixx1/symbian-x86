/* efi.h — minimal UEFI definitions for the freestanding loader.
 * Only what loader.c uses (X64, MS ABI). Values from the UEFI spec. */
#ifndef __EFI_MIN_H__
#define __EFI_MIN_H__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
typedef u64 UINTN;
typedef u64 EFI_STATUS;
typedef void *EFI_HANDLE;
typedef u16 CHAR16;   /* with -fshort-wchar, wchar_t matches */

#define EFI_SUCCESS 0
#define EFI_ERROR_MASK 0x8000000000000000ULL
#define EFI_BUFFER_TOO_SMALL (EFI_ERROR_MASK | 5)

typedef struct {
    u32 Data1;
    u16 Data2;
    u16 Data3;
    u8 Data4[8];
} EFI_GUID;

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    {0x5B1B31A1, 0x9562, 0x11d2, {0x8E,0x3F,0x00,0xA0,0xC9,0x69,0x72,0x3B}}
#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    {0x964E5B22, 0x6459, 0x11d2, {0x8E,0x39,0x00,0xA0,0xC9,0x69,0x72,0x3B}}

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_ EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_ {
    void *Reset;
    EFI_STATUS (*OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
                               const CHAR16 *String);
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
};

typedef struct {
    u32 Type;
    u32 Pad;
    u64 PhysicalStart;
    u64 VirtualStart;
    u64 NumberOfPages;
    u64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

#define EFI_CONVENTIONAL_MEMORY 7
#define EFI_LOADER_DATA 2

typedef struct EFI_BOOT_SERVICES_ EFI_BOOT_SERVICES;
struct EFI_BOOT_SERVICES_ {
    char _pad0[24];  /* table header */
    void *RaiseTPL;
    void *RestoreTPL;
    EFI_STATUS (*AllocatePages)(int Type, int MemoryType, UINTN Pages,
                                u64 *Memory);
    EFI_STATUS (*FreePages)(u64 Memory, UINTN Pages);
    EFI_STATUS (*GetMemoryMap)(UINTN *MemoryMapSize,
                               EFI_MEMORY_DESCRIPTOR *MemoryMap,
                               UINTN *MapKey, UINTN *DescriptorSize,
                               u32 *DescriptorVersion);
    EFI_STATUS (*AllocatePool)(int PoolType, UINTN Size, void **Buffer);
    EFI_STATUS (*FreePool)(void *Buffer);
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    EFI_STATUS (*HandleProtocol)(EFI_HANDLE Handle, EFI_GUID *Protocol,
                                 void **Interface);
    void *__reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;
    EFI_STATUS (*ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    void *GetNextMonotonicCount;
    void *Stall;
    EFI_STATUS (*SetWatchdogTimer)(UINTN Timeout, u64 WatchdogCode,
                                   UINTN DataSize, CHAR16 *WatchdogData);
};

#define EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL 0x00000001

typedef struct {
    u32 Revision;
    EFI_HANDLE ParentHandle;
    void *SystemTable;
    EFI_HANDLE DeviceHandle;
    void *FilePath;
    void *Reserved;
    u32 LoadOptionsSize;
    void *LoadOptions;
    void *ImageBase;
    u64 ImageSize;
    int ImageCodeType;
    int ImageDataType;
    void *Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_ EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct EFI_FILE_PROTOCOL_ EFI_FILE_PROTOCOL;
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_ {
    u64 Revision;
    EFI_STATUS (*OpenVolume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
                             EFI_FILE_PROTOCOL **Root);
};
struct EFI_FILE_PROTOCOL_ {
    u64 Revision;
    EFI_STATUS (*Open)(EFI_FILE_PROTOCOL *This, EFI_FILE_PROTOCOL **NewHandle,
                       const CHAR16 *FileName, u64 OpenMode, u64 Attributes);
    EFI_STATUS (*Close)(EFI_FILE_PROTOCOL *This);
    void *Delete;
    EFI_STATUS (*Read)(EFI_FILE_PROTOCOL *This, UINTN *BufferSize,
                       void *Buffer);
    void *Write;
    void *GetPosition;
    void *SetPosition;
    void *GetInfo;
    void *SetInfo;
    void *Flush;
};
#define EFI_FILE_MODE_READ 0x0000000000000001ULL

typedef struct EFI_SYSTEM_TABLE_ EFI_SYSTEM_TABLE;
struct EFI_SYSTEM_TABLE_ {
    char _pad[24];  /* table header */
    CHAR16 *FirmwareVendor;
    u32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
};

#endif

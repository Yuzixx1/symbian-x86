/* loader.c — freestanding X64 UEFI loader for the genuine EKA2 kernels.
 *
 * Built as BOOTX64.EFI (PE32+, MS ABI). Job: load EKA2/EKERN.ELF from the
 * same FAT volume (the efiboot image carries its own kernel copy, so only
 * SimpleFileSystem is needed), place PT_LOADs at linked addresses below
 * 4G, synthesize the Multiboot1 info block the kernel expects, exit boot
 * services, drop from long mode to 32-bit protected mode (no paging, like
 * the BIOS path leaves it), and jump to _mb_entry with EAX=2BADB002,
 * EBX=MBI. The kernel cannot tell UEFI from BIOS.
 *
 * Compile: -mabi=ms so every call uses the firmware's convention.
 */
#include "efi.h"

#define KERNEL_PATH L"\\EKA2\\EKERN.ELF"
#define LOAD_BASE 0x100000ULL
#define MBI_ADDR 0x8000ULL
#define SCRATCH_ESP 0x9000ULL
#define MB_MAGIC 0x2BADB002ULL

static EFI_SYSTEM_TABLE *ST;
static EFI_BOOT_SERVICES *BS;

/* Direct COM1 diagnostics (no firmware calls): the only output used past
   entry. 115200 8N1; SeaBIOS/OVMF leave the UART initialized. */
static void sprout(char c)
    {
    unsigned char s;
    do
        {
        __asm__ __volatile__("inb %1,%0" : "=a"(s) : "Nd"((unsigned short)0x3FD));
        }
    while (!(s & 0x20));
    __asm__ __volatile__("outb %0,%1" : : "a"((unsigned char)c), "Nd"((unsigned short)0x3F8));
    }

static void sprints(const char *s)
    {
    while (*s)
        {
        if (*s == '\n')
            sprout('\r');
        sprout(*s++);
        }
    }

static void sprinthex(u64 v)
    {
    char b[16];
    int n = 0;
    if (!v)
        sprints("0");
    else
        {
        while (v && n < 16)
            {
            int d = (int)(v & 15);
            b[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
            v >>= 4;
            }
        while (n--)
            {
            char c = b[n];
            sprout(c);
            }
        }
    sprints("\r\n");
    }

static void print(const CHAR16 *s)
    {
    while (*s)
        {
        CHAR16 c = *s++;
        if (c == '\n')
            sprout('\r');
        if (c < 128)
            sprout((char)c);
        }
    }

static void print_hex(u64 v)
    {
    sprinthex(v);
    }

static void die(const CHAR16 *msg, u64 code)
    {
    print(msg);
    print_hex(code);
    for (;;)
        {
        __asm__ __volatile__("cli; hlt");
        }
    }

/* GDT for the 64->32 drop: null, 32-bit code, 32-bit data (flat 4G). */
static u64 gdt32[3];
static struct { u16 limit; u64 base; } __attribute__((packed)) gdt_ptr;

static void make_gdt32(void)
    {
    gdt32[0] = 0;
    /* code: base 0, limit 4G, G=1, D=1, P=1, exec+read, DPL0 */
    gdt32[1] = 0x00CF9A000000FFFFULL;
    /* data: base 0, limit 4G, G=1, D=1, P=1, read+write, DPL0 */
    gdt32[2] = 0x00CF92000000FFFFULL;
    gdt_ptr.limit = sizeof(gdt32) - 1;
    gdt_ptr.base = (u64)&gdt32[0];
    }

/* Multiboot1 info block the kernel expects (flags + mem only). */
struct mbi {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
};

extern void drop_to_pm(u64 entry, u64 mbi, void *gdt_ptr_addr);

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
    {
    ST = SystemTable;
    BS = SystemTable->BootServices;
    print(L"EKA2 UEFI loader...\r\n");
    print(L"base: ");
    print_hex((u64)&efi_main);

    BS->SetWatchdogTimer(0, 0, 0, 0);

    /* Open our own volume. */
    EFI_LOADED_IMAGE_PROTOCOL *li = 0;
    EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    if (BS->HandleProtocol(ImageHandle, &li_guid, (void **)&li) || !li)
        die(L"no LoadedImage: ", 1);
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = 0;
    EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    if (BS->HandleProtocol(li->DeviceHandle, &fs_guid, (void **)&fs) || !fs)
        die(L"no SimpleFS: ", 2);
    EFI_FILE_PROTOCOL *root = 0;
    if (fs->OpenVolume(fs, &root) || !root)
        die(L"no volume: ", 3);

    /* Load the kernel file. */
    EFI_FILE_PROTOCOL *kf = 0;
    if (root->Open(root, &kf, KERNEL_PATH, EFI_FILE_MODE_READ, 0) || !kf)
        die(L"no " KERNEL_PATH L": ", 4);
    UINTN fsize = 0;
    /* Size via reads: read 0-length probe is unreliable; read in chunks. */
    UINTN cap = 1 << 20;
    u8 *buf = 0;
    if (BS->AllocatePool(EFI_LOADER_DATA, cap, (void **)&buf) || !buf)
        die(L"no pool: ", 5);
    UINTN total = 0;
    for (;;)
        {
        if (total + 65536 > cap)
            die(L"kernel >1M file: ", 6);
        UINTN n = 65536;
        EFI_STATUS st = kf->Read(kf, &n, buf + total);
        if (st)
            die(L"read err: ", (u64)st);
        if (!n)
            break;
        total += n;
        }
    fsize = total;
    /* ELF32 parse. */
    if (total < 52 || buf[0] != 0x7F || buf[1] != 'E' ||
        buf[2] != 'L' || buf[3] != 'F')
        die(L"bad ELF: ", 7);
    u32 phoff = buf[28] | ((u32)buf[29] << 8) | ((u32)buf[30] << 16) |
                ((u32)buf[31] << 24);
    u32 phnum = buf[44] | ((u32)buf[45] << 8);
    if (phnum > 8 || phoff > total)
        die(L"bad PHDR: ", 8);
    u64 lo = ~0ULL, hi = 0;
    for (u32 i = 0; i < phnum; ++i)
        {
        u8 *ph = buf + phoff + i * 32;
        u32 type = ph[0] | ((u32)ph[1] << 8) | ((u32)ph[2] << 16) |
                   ((u32)ph[3] << 24);
        if (type != 1)
            continue;
        u32 vaddr = ph[8] | ((u32)ph[9] << 8) | ((u32)ph[10] << 16) |
                    ((u32)ph[11] << 24);
        u32 memsz = ph[20] | ((u32)ph[21] << 8) | ((u32)ph[22] << 16) |
                    ((u32)ph[23] << 24);
        if ((u64)vaddr < lo)
            lo = vaddr;
        if ((u64)vaddr + memsz > hi)
            hi = (u64)vaddr + memsz;
        }
    if (hi <= lo || hi > 0x100000000ULL)
        die(L"bad span: ", 9);
    u64 pages = (hi - LOAD_BASE + 0xFFF) / 0x1000;
    u64 at = LOAD_BASE;
    if (BS->AllocatePages(2 /*AllocateAddress*/, EFI_LOADER_DATA, pages, &at)
        || at != LOAD_BASE)
        die(L"no low RAM: ", 10);
    for (u32 i = 0; i < phnum; ++i)
        {
        u8 *ph = buf + phoff + i * 32;
        u32 type = ph[0] | ((u32)ph[1] << 8) | ((u32)ph[2] << 16) |
                   ((u32)ph[3] << 24);
        if (type != 1)
            continue;
        u32 off = ph[4] | ((u32)ph[5] << 8) | ((u32)ph[6] << 16) |
                  ((u32)ph[7] << 24);
        u32 vaddr = ph[8] | ((u32)ph[9] << 8) | ((u32)ph[10] << 16) |
                    ((u32)ph[11] << 24);
        u32 filesz = ph[16] | ((u32)ph[17] << 8) | ((u32)ph[18] << 16) |
                     ((u32)ph[19] << 24);
        u32 memsz = ph[20] | ((u32)ph[21] << 8) | ((u32)ph[22] << 16) |
                    ((u32)ph[23] << 24);
        if (off + filesz > total)
            die(L"seg past EOF: ", 11);
        u8 *d = (u8 *)(u64)vaddr;
        for (u32 k = 0; k < filesz; ++k)
            d[k] = buf[off + k];
        for (u32 k = filesz; k < memsz; ++k)
            d[k] = 0;
        }
    u32 entry = buf[24] | ((u32)buf[25] << 8) | ((u32)buf[26] << 16) |
                ((u32)buf[27] << 24);
    print(L"kernel entry: ");
    print_hex(entry);
    BS->FreePool(buf);
    kf->Close(kf);
    root->Close(root);
    print(L"files closed\r\n");

    /* Memory map (also feeds the MBI numbers). Robust sequence: size the
       buffer generously, re-query fresh inside the exit retry loop. */
    UINTN msize = 0, mkey = 0, dsize = 0;
    u32 dver = 0;
    EFI_MEMORY_DESCRIPTOR *map = 0;
    UINTN map_buf = 0;
    u64 low_kb = 0, high_kb = 0;
    print(L"sizing map\r\n");
    // One generously sized static buffer. Diagnostics use direct serial
    // (firmware ConOut calls between map and exit invalidate the MapKey).
    static u8 bigmap[32768];
    {
    int tries = 0;
    for (;;)
        {
        msize = sizeof(bigmap);
        map = (EFI_MEMORY_DESCRIPTOR *)bigmap;
        EFI_STATUS q = BS->GetMemoryMap(&msize, map, &mkey, &dsize, &dver);
        if (q)
            {
            print(L"map fill: ");
            print_hex((u64)q);
            if (++tries > 2)
                die(L"map fill", (u64)q);
            continue;
            }
        map_buf = msize;
        low_kb = 0;
        high_kb = 0;
        {
        u8 *p = (u8 *)map;
        UINTN n = msize / dsize;
        for (UINTN i = 0; i < n; ++i)
            {
            EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *)(p + i * dsize);
            if (d->Type != EFI_CONVENTIONAL_MEMORY)
                continue;
            u64 base = d->PhysicalStart, len = d->NumberOfPages * 4096;
            if (base < 0xA0000)
                {
                u64 top = base + len;
                if (top > 0xA0000)
                    top = 0xA0000;
                if (top > base)
                    low_kb += (top - base) / 1024;
                }
            if (base + len > 0x100000 && base < 0x100000000ULL)
                {
                u64 s = base < 0x100000 ? 0x100000 : base;
                u64 e = base + len;
                if (e > 0x100000000ULL)
                    e = 0x100000000ULL;
                if (e > s)
                    high_kb += (e - s) / 1024;
                }
            }
        }
        if (low_kb > 640)
            low_kb = 640;
        EFI_STATUS es = BS->ExitBootServices(ImageHandle, mkey);
        if (!es)
            break;
        if (++tries > 4)
            die(L"no exit", (u64)es);
        /* else loop: re-query with a bigger buffer (map may have grown) */
        }
    }
    (void)map_buf;
    print(L"no bs\r\n");
    /* No boot services past this point. */

    struct mbi *mbi = (struct mbi *)MBI_ADDR;
    mbi->flags = 1;
    mbi->mem_lower = (u32)low_kb;
    mbi->mem_upper = (u32)high_kb;

    make_gdt32();
    print(L"jumping...\r\n");
    drop_to_pm((u64)entry, MBI_ADDR, &gdt_ptr);
    die(L"returned?!: ", 15);
    return 0;
    }

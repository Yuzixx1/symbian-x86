/*
 * probe.cpp -- run ACTUAL genuine F32 machine code from kernelhwsrv on the
 * ported x86 kernel: FileSystem_FAT::Log2 / Count1Bits (fs_utils) and
 * TFatBootSector::Internalize / IsValid / FatType / FirstDataSector
 * (sfat32/sl_bpb32.cpp).  Only local EUSER descriptor primitives are supplied
 * (see euser_local.cpp); the F32 logic itself is unmodified genuine source.
 */
extern "C" {
#include "runtime.h"
}

extern "C" void (*f32probe_printf_slot)(const char*);  /* ld --defsym @0x408000 */

/* Genuine F32 entry points (Itanium-mangled gcc symbols). */
extern "C" unsigned long f32_Log2(unsigned long) __asm__("_Z4Log2m");
extern "C" unsigned long f32_Count1Bits(unsigned long) __asm__("_Z10Count1Bitsm");
extern "C" void bpb_ctor(void*) __asm__("_ZN14TFatBootSectorC1Ev");
extern "C" void bpb_internalize(void*, const void*) __asm__("_ZN14TFatBootSector11InternalizeERK6TDesC8");
extern "C" bool bpb_isvalid(const void*) __asm__("_ZNK14TFatBootSector7IsValidEv");
extern "C" int bpb_fattype(const void*) __asm__("_ZNK14TFatBootSector7FatTypeEv");
extern "C" unsigned long bpb_firstdata(const void*) __asm__("_ZNK14TFatBootSector15FirstDataSectorEv");

struct GDesC8 { unsigned iTypeLength; const unsigned char* iPtr; };

static void marker(const char* text) {
    unsigned fn = *(unsigned*)(void*)&f32probe_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* genuine EUSER.DLL ord 915 */
    print(text);                             /* DebugPrint fallback */
}

static void num(const char* tag, unsigned long v) {
    char b[80];
    unsigned n = 0;
    while (tag[n] && n < 64) { b[n] = tag[n]; ++n; }
    if (!v) b[n++] = '0';
    else {
        char d[16]; unsigned m = 0;
        while (v) { d[m++] = char('0' + (v % 10)); v /= 10; }
        while (m) b[n++] = d[--m];
    }
    b[n] = 0;
    marker(b);
}

/* A minimal FAT32 boot sector (512 bytes) matching the volume layout used by
 * the project's FAT disk builder: 512 B/sector, 8 sectors/cluster, 32 reserved
 * sectors, 2 FATs, LBA 2048, 49152 sectors in the volume. */
static unsigned char boot[512];

static void put16(unsigned o, unsigned v) { boot[o] = (unsigned char)v; boot[o+1] = (unsigned char)(v >> 8); }
static void put32(unsigned o, unsigned v) {
    for (unsigned i = 0; i < 4; ++i) boot[o+i] = (unsigned char)(v >> (8*i));
}

extern "C" int app_main(void) {
    marker("F32PROBE: start");

    /* Genuine fs_utils code (unmodified filesystem_utils.cpp). */
    num("F32PROBE: genuine Log2(256)=", f32_Log2(256));
    num("F32PROBE: genuine Count1Bits(0xF0F0)=", f32_Count1Bits(0xF0F0u));

    boot[0] = 0xEB; boot[1] = 0x58; boot[2] = 0x90;
    const char oem[] = "MSWIN4.1";
    for (unsigned i = 0; i < 8; ++i) boot[3+i] = (unsigned char)oem[i];
    put16(11, 512);          /* bytes per sector */
    boot[13] = 1;            /* sectors per cluster -> >65525 clusters */
    put16(14, 32);           /* reserved sectors */
    boot[16] = 2;            /* number of FATs */
    put16(17, 0);            /* root dir entries (FAT32 = 0) */
    put16(19, 0);            /* total sectors 16 (= 0 for FAT32) */
    boot[21] = 0xF8;         /* media descriptor */
    put16(22, 0);            /* FAT size 16 */
    put16(24, 32); put16(26, 64);
    put32(28, 2048);         /* hidden sectors */
    put32(32, 1048576);      /* huge sectors -> >65525 clusters => FAT32 */
    put32(36, 512);          /* FAT size 32 */
    put32(44, 2);            /* root cluster */
    boot[510] = 0x55; boot[511] = 0xAA;

    GDesC8 d;
    d.iTypeLength = 512u;   /* genuine iTypeLength: length in low 28 bits */
    d.iPtr = boot;
    static unsigned char bpb_store[512] __attribute__((aligned(4)));
    bpb_ctor(bpb_store);
    bpb_internalize(bpb_store, &d);
    marker(bpb_isvalid(bpb_store) ? "F32PROBE: genuine FAT32 BPB IsValid=1"
                                  : "F32PROBE: genuine FAT32 BPB IsValid=0");
    num("F32PROBE: genuine FAT type=", (unsigned long)bpb_fattype(bpb_store));
    num("F32PROBE: genuine FirstDataSector=", bpb_firstdata(bpb_store));
    marker("F32PROBE: done");
    return 0;
}

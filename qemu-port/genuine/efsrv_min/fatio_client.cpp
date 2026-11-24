/*
 * fatio_client.cpp -- genuine EFSRV IPC client for the B5 on-disk FAT I/O
 * integration proof (tools/r1_genuine_fat_io_test.py).
 *
 * Uses the genuine TFsMessage opcode numbers and the same descriptor shapes
 * the already-proven /tmp IPC probe used against R1Fs::FileServer:
 *   SessionPath=14, FileOpen=30, FileCreate=31, FileRead=34, FileWrite=35,
 *   FileClose=28.  This client does NOT use any port file_*() helper; every
 *   byte travels through genuine !FileServer IPC to the disk-backed backend.
 *
 * Flow:
 *   1. Open the pre-staged C:\R1TEST\SEED.TXT (on the real FAT volume) and
 *      read it.  The old in-memory backend has no such file -> it fails here,
 *      so this is an in-guest discriminator for real disk reads.
 *   2. Create C:\R1TEST\FATIO.TXT, write a unique marker, close (commit).
 *   3. Reopen, read back, and verify the marker byte-for-byte.
 *
 * C++98 / -ffreestanding.  Markers go out through the port print() bridge.
 */
extern "C" {
#include "runtime.h"
#include "ipc.h"
}

/* Descriptor headers: type nibble in the top bits of type_length.
   1 = EPtrC (const), 2 = EPtr (writable). */
struct C16 { unsigned type_length; const unsigned short* pointer; };
struct C8  { unsigned type_length; const unsigned char* pointer; };
struct W8  { unsigned type_length, maximum; unsigned char* pointer; };
struct W16 { unsigned type_length, maximum; unsigned short* pointer; };

static void emit(const char* tag, int v) {
    char b[100]; unsigned n = 0;
    while (tag[n] && n < 70) { b[n] = tag[n]; ++n; }
    unsigned long x = (unsigned long)(v < 0 ? -v : v);
    if (v < 0) b[n++] = '-';
    char d[16]; unsigned m = 0;
    if (!x) d[m++] = '0';
    while (x) { d[m++] = char('0' + x % 10); x /= 10; }
    while (m) b[n++] = d[--m];
    b[n] = 0;
    print(b);
}

/* Zero a byte buffer so a short read leaves a NUL terminator. */
static void zero(unsigned char* p, unsigned n) { while (n--) *p++ = 0; }

/* Match exactly expect + terminating NUL (readback buffer is pre-zeroed). */
static bool equals(const unsigned char* buf, const char* expect) {
    unsigned i = 0;
    for (; expect[i]; ++i) if (buf[i] != (unsigned char)expect[i]) return false;
    return buf[i] == 0;
}

static unsigned short seedPath[] = {
    'C',':','\\','R','1','T','E','S','T','\\','S','E','E','D','.','T','X','T'
};
static unsigned short fatPath[] = {
    'C',':','\\','R','1','T','E','S','T','\\','F','A','T','I','O','.','T','X','T'
};

static const char kSeed[]   = "SEED-DATA-99887";
static const char kMarker[] = "FATIO-MARKER-42A7";

/* Open/Create (opcode 30/31). slot0 = const 16-bit path, slot1 = mode,
   slot3 = writable 8-bit handle reply. flags: EDesC16=7 slot0, EDes8=4 slot3. */
static int open_or_create(int s, int opcode, const unsigned short* path,
                          unsigned n16, unsigned mode, int& handle) {
    static unsigned char reply[8];
    zero(reply, sizeof(reply));
    C16 d = { (1u << 28) | n16, path };
    W8  h = { 0x20000000u, 4, reply };
    struct IpcArgs a = {{(unsigned)&d, mode, 0, (unsigned)&h}, 7u | (4u << 9)};
    int r = ipc_call_args(s, opcode, &a);
    if (!r) handle = ((int*)reply)[0];
    return r;
}

/* FileWrite (opcode 35). slot0 = const 8-bit data, slot1 = length,
   slot3 = handle as a raw int. flags: EDesC8=6 slot0. */
static int write_file(int s, int handle, const char* data, unsigned length) {
    C8 d = { (1u << 28) | length, (const unsigned char*)data };
    struct IpcArgs a = {{(unsigned)&d, length, 0, (unsigned)handle}, 6u};
    return ipc_call_args(s, 35, &a);
}

/* FileRead (opcode 34). slot0 = writable 8-bit buffer, slot1 = requested-1,
   slot3 = handle. flags: EDes8=4 slot0. */
static int read_file(int s, int handle, unsigned char* buf, unsigned cap) {
    zero(buf, cap);
    W8 b = { 0x20000000u, cap, buf };
    struct IpcArgs a = {{(unsigned)&b, cap - 1, 0, (unsigned)handle}, 4u};
    return ipc_call_args(s, 34, &a);
}

static int close_file(int s, int handle) {
    struct IpcArgs a = {{0, 0, 0, (unsigned)handle}, 0};
    return ipc_call_args(s, 28, &a);
}

extern "C" int app_main(void) {
    print("FATIOCLIENT: start");
    int s = ipc_connect("R1FileServer");
    if (s < 0) { print("FATIOCLIENT: connect failed"); return 1; }
    print("FATIOCLIENT: connected");

    static unsigned char buf[256];

    /* 1. Seed read: this file exists ONLY on the FAT volume. */
    int hseed = -1;
    int r = open_or_create(s, 30, seedPath,
                           sizeof(seedPath)/sizeof(seedPath[0]), 0, hseed);
    emit("FATIOCLIENT: open seed r", r);
    if (r) { print("FATIOCLIENT: seed open FAILED"); ipc_close(s); return 2; }
    r = read_file(s, hseed, buf, sizeof(buf));
    emit("FATIOCLIENT: read seed r", r);
    print((const char*)buf);
    if (r || !equals(buf, kSeed)) { print("FATIOCLIENT: seed MISMATCH"); }
    else { print("FATIOCLIENT: seed MATCH"); }
    emit("FATIOCLIENT: close seed r", close_file(s, hseed));

    /* 2. Create + write + close (commit to real FAT). */
    int hw = -1;
    r = open_or_create(s, 31, fatPath,
                       sizeof(fatPath)/sizeof(fatPath[0]), 0x203, hw);
    emit("FATIOCLIENT: create r", r);
    if (r) { print("FATIOCLIENT: create FAILED"); ipc_close(s); return 3; }
    r = write_file(s, hw, kMarker, sizeof(kMarker) - 1);
    emit("FATIOCLIENT: write r", r);
    if (r) { print("FATIOCLIENT: write FAILED"); ipc_close(s); return 4; }
    emit("FATIOCLIENT: close write r", close_file(s, hw));

    /* 3. Reopen + read back (from the FAT cluster chain). */
    int hr = -1;
    r = open_or_create(s, 30, fatPath,
                       sizeof(fatPath)/sizeof(fatPath[0]), 0, hr);
    emit("FATIOCLIENT: reopen r", r);
    if (r) { print("FATIOCLIENT: reopen FAILED"); ipc_close(s); return 5; }
    r = read_file(s, hr, buf, sizeof(buf));
    emit("FATIOCLIENT: readback r", r);
    print((const char*)buf);
    if (r || !equals(buf, kMarker)) { print("FATIOCLIENT: marker MISMATCH"); }
    else { print("FATIOCLIENT: marker MATCH"); print("FATIOCLIENT: round trip OK"); }
    emit("FATIOCLIENT: close read r", close_file(s, hr));

    ipc_close(s);
    print("FATIOCLIENT: done");
    return 0;
}

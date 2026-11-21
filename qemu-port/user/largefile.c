/*
 * largefile.c -- E32 proof of genuine large-file streaming (append-only).
 *
 * The staged C:\BIGFILE.BIN is 2 MiB + 37 bytes and is filled with a pattern
 * that depends only on the absolute byte offset: byte[i] = ((i / 4096) & 0xff).
 * We open it through file_open(), query file_size(), then read at the start,
 * the middle and the very end using file_read_at(), and also exercise
 * file_seek() + file_read().  Everything goes through the PortBigFile logical
 * device, which walks the FAT cluster chain instead of taking a 64 KiB
 * snapshot -- the same path DOOM uses for a multi-MB WAD.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*largefile_printf_slot)(const char*);

static void marker(const char* text)
{
    unsigned fn = *(volatile unsigned*)(void*)&largefile_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* Freestanding decimal formatter (no hosted libc). */
static void marker_n(const char* prefix, unsigned value, const char* suffix)
{
    char buf[160];
    unsigned n = 0;
    char digits[12];
    int d = 0;
    while (*prefix && n < sizeof(buf) - 16u) buf[n++] = *prefix++;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10u); value /= 10u; }
    while (d) buf[n++] = digits[--d];
    while (*suffix && n < sizeof(buf) - 1u) buf[n++] = *suffix++;
    buf[n] = 0;
    marker(buf);
}

static int pattern_ok(const unsigned char* data, unsigned len, unsigned offset)
{
    for (unsigned i = 0; i < len; ++i)
        if (data[i] != (unsigned char)(((offset + i) / 4096u) & 0xffu))
            return 0;
    return 1;
}

#define EXPECTED_SIZE 2097189u   /* 2 MiB + 37 */

int app_main(void)
{
    unsigned char buf[64];
    const unsigned mid = EXPECTED_SIZE / 2u;
    const unsigned tail = EXPECTED_SIZE - 32u;

    marker("LF: LARGE FILE TEST START");

    int h = file_open("C:\\BIGFILE.BIN");
    if (h < 0)
        {
        marker("LF: open FAILED");
        return 1;
        }

    long size = file_size(h);
    if (size != (long)EXPECTED_SIZE)
        {
        marker_n("LF: size FAILED got ", (unsigned)size, "");
        file_close(h);
        return 2;
        }

    if (file_read_at(h, 0, buf, sizeof(buf)) != (int)sizeof(buf) ||
        !pattern_ok(buf, sizeof(buf), 0))
        {
        marker("LF: read at 0 FAILED");
        file_close(h);
        return 3;
        }

    if (file_read_at(h, mid, buf, sizeof(buf)) != (int)sizeof(buf) ||
        !pattern_ok(buf, sizeof(buf), mid))
        {
        marker("LF: read at middle FAILED");
        file_close(h);
        return 4;
        }

    if (file_read_at(h, tail, buf, 32) != 32 ||
        !pattern_ok(buf, 32, tail))
        {
        marker("LF: read at end FAILED");
        file_close(h);
        return 5;
        }

    if (file_seek(h, mid) < 0 ||
        file_read(h, buf, 16) != 16 ||
        !pattern_ok(buf, 16, mid))
        {
        marker("LF: seek+read FAILED");
        file_close(h);
        return 6;
        }

    marker_n("LF: size=", EXPECTED_SIZE, " read at 0/");
    marker_n("LF: middle=", mid, " OK");
    marker("LF: result PASS");
    marker("LF: DONE");

    file_close(h);
    return 0;
}

/*
 * wadprobe.c -- tiny PE2 E32 in-guest diagnostic for the DOOM IWAD path.
 *
 * For each candidate spelling of the staged Freedoom IWAD, open it through
 * the very same runtime.h large-file API DOOM's fopen() shim uses, print the
 * decimal file_open() return code, and -- when the open succeeds -- print
 * file_size() and the first bytes read via file_read_at().  Running this on
 * the real r1_doom_test FAT image gives ground truth about which path the
 * kernel's PortBigFile / R1Fat layer actually accepts.
 *
 * Markers use the genuine EUSER.DLL ordinal-915 PE2 import (DebugPrint
 * fallback via print()), exactly like tools' other E32 diagnostics.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user/user.mk. */
extern void (*wadprobe_printf_slot)(const char *);

static void marker(const char *text)
{
    unsigned fn = *(volatile unsigned *)(void *)&wadprobe_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char *))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* Freestanding append helpers (no hosted libc). */
struct out { char buf[240]; unsigned n; };

static void o_reset(struct out *o) { o->n = 0; o->buf[0] = 0; }

static void o_str(struct out *o, const char *s)
{
    while (*s && o->n < sizeof(o->buf) - 1u) o->buf[o->n++] = *s++;
    o->buf[o->n] = 0;
}

static void o_int(struct out *o, int v)
{
    char digits[12];
    unsigned d = 0, u;
    if (v < 0) { o_str(o, "-"); u = (unsigned)(-(v + 1)) + 1u; }
    else u = (unsigned)v;
    if (!u) digits[d++] = '0';
    while (u) { digits[d++] = (char)('0' + u % 10u); u /= 10u; }
    while (d) { if (o->n < sizeof(o->buf) - 1u) o->buf[o->n++] = digits[--d]; }
    o->buf[o->n] = 0;
}

static void o_hexbyte(struct out *o, unsigned char b)
{
    const char *h = "0123456789abcdef";
    if (o->n + 2u < sizeof(o->buf))
        { o->buf[o->n++] = h[(b >> 4) & 15u]; o->buf[o->n++] = h[b & 15u]; }
    o->buf[o->n] = 0;
}

static void emit(struct out *o) { marker(o->buf); o_reset(o); }

/* Print "WP <path> open=<code>" and, if open succeeded, size + first bytes. */
static void probe(struct out *o, const char *path)
{
    unsigned char bytes[16];
    int h, got, i;
    long size;

    o_reset(o);
    o_str(o, "WP [");
    o_str(o, path);
    o_str(o, "] open=");
    h = file_open(path);
    o_int(o, h);
    emit(o);

    if (h < 0) return;

    size = file_size(h);
    o_reset(o);
    o_str(o, "WP [");
    o_str(o, path);
    o_str(o, "] size=");
    o_int(o, (int)size);
    emit(o);

    got = file_read_at(h, 0, bytes, sizeof(bytes));
    o_reset(o);
    o_str(o, "WP [");
    o_str(o, path);
    o_str(o, "] read=");
    o_int(o, got);
    o_str(o, " bytes=");
    if (got > 0)
        for (i = 0; i < (unsigned)got && i < sizeof(bytes); ++i)
            { o_hexbyte(o, bytes[i]); o_str(o, " "); }
    emit(o);

    file_close(h);
}

int app_main(void)
{
    struct out o;
    o_reset(&o);

    marker("WP: WAD PROBE START");

    /* The exact spellings DOOM tries, plus a few control cases. */
    probe(&o, "X:/FDOOM1.WAD");
    probe(&o, "X:FDOOM1.WAD");
    probe(&o, "FDOOM1.WAD");
    probe(&o, "/FDOOM1.WAD");
    probe(&o, "C:/FDOOM1.WAD");
    probe(&o, "X:\\FDOOM1.WAD");
    probe(&o, "X:/fdoom1.wad");
    probe(&o, "X:/EUSER.DLL");          /* control: known small file on X: */
    probe(&o, "C:\\BIGFILE.BIN");       /* control: known largefile-test file */

    marker("WP: DONE");
    return 0;
}

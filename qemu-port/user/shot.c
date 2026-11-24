/*
 * shot.c -- PE2 E32 SCREENSHOT tool for the R1 QEMU port.
 *
 * MIGRATED 2026-11-24 off the winsrv path: no winsrv GUI host header, no
 * GUI slow execs (the old winsrv display-list slots are gone from this
 * file).  The
 * winsrv-migrated SHOTE32.E32 (user/shot_e32.c, genuine
 * EWSRV RConsole + exec 270) supersedes the old flow; this SHOT.E32 keeps the
 * same plain-C runtime.h shape as FBBLITTEST.E32 and now uses:
 *
 *   1. Framebuffer geometry from slow exec 270 op 2 (FBO_FBINFO).  Exec 270
 *      is the SvcMgr-ctl multiplexed exec (variant/qemu_fbblit.*) -- NOT part
 *      of the winsrv deletion set (it retires with svcmgr, a separate step).
 *      Op 2 lazily brings the VBE LFB up; E32Run's PdMapUser()
 *      (variant/e32load.cpp:936) mapped the LFB U/S=1 into this process, so
 *      ring-3 can read the real framebuffer back directly.
 *   2. The 64x64 capture patch is painted with ONE exec 270 op 3 bulk blit
 *      (FBO_FBBLIT, ring-0 LFB write) instead of a 64-command winsrv
 *      display list.  32bpp source = LFB-native 0x00RRGGBB words.
 *   3. SCREENDUMP: the same 64x64 rect is read straight back from the mapped
 *      LFB (no kernel readback exec needed), converted to raw RGBA and saved
 *      to SCREEN.RAW through PortFile IPC (runtime.c file_save).  If
 *      portfilesrv is not running the capture still succeeds and is reported
 *      as such.
 *
 * Serial markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf),
 * bound by E32Run from the PE2 IAT slot shot_printf_slot at 0x408000, with
 * the port DebugPrint executive as a fallback (same pattern as fbblittest.c).
 */
#include "runtime.h"
#include "../variant/qemu_fbblit.h"   /* exec 270 ops 2/3 ABI (variant/, svcmgr-owned) */

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int shot_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&shot_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);  /* EUSER ord 915 */
    else print(text);                                  /* no EUSER bound */
}

/* ---- tiny formatters (no hosted libc) ---- */
static unsigned append(char* p, unsigned k, const char* s)
{
    while (*s) p[k++] = *s++;
    return k;
}
static unsigned dec(char* p, unsigned k, unsigned v)
{
    char t[12];
    int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10u); v /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}
static unsigned hex32(char* p, unsigned k, unsigned v)
{
    const char* h = "0123456789abcdef";
    k = append(p, k, "0x");
    for (int i = 28; i >= 0; i -= 4) p[k++] = h[(v >> i) & 0xfu];
    return k;
}

/* ---- capture geometry / private patch buffer ---- */
#define CAP_W       64u
#define CAP_H       64u
#define CAP_SIZE    (CAP_W * CAP_H * 4u)   /* 16384 raw RGBA bytes */

/* Patch destination on the LFB (safe inside 320x200 and 1024x768). */
#define CAP_X       16u
#define CAP_Y       96u

static unsigned s_words[CAP_W * CAP_H];   /* 32bpp 0x00RRGGBB, LFB-native */
static unsigned char s_pixels[CAP_SIZE];  /* raw RGBA screendump          */

static void zero8(void* p, unsigned n)
{
    unsigned char* b = (unsigned char*)p;
    for (unsigned i = 0; i < n; ++i) b[i] = 0;
}

/* Render the 64x64 patch.  The colour ramp is seeded with the discovered fb
 * base/size, tying the dump to this boot's actual LFB rather than to a
 * constant fixture (same idea as the old winsrv display-list version). */
static void build_patch(unsigned fbBase, unsigned fbSize)
{
    unsigned y, x;
    for (y = 0; y < CAP_H; ++y)
        {
        unsigned col = (((y * 4u) + (fbBase & 0xffu)) << 16)
                     | ((0x30u + y * 2u) << 8)
                     | (0x80u + ((fbSize >> 8) & 0x3fu));
        for (x = 0; x < CAP_W; ++x)
            s_words[y * CAP_W + x] = col;
        }
    /* white cross-hair */
    for (x = 24u; x < 40u; ++x)
        { s_words[31u * CAP_W + x] = 0x00FFFFFFu; s_words[32u * CAP_W + x] = 0x00FFFFFFu; }
    for (y = 24u; y < 40u; ++y)
        { s_words[y * CAP_W + 31u] = 0x00FFFFFFu; s_words[y * CAP_W + 32u] = 0x00FFFFFFu; }
}

int app_main(void)
{
    char line[160];
    unsigned k, match;
    int r, rblit;

    marker("SHOT: START");

    /* 1. framebuffer geometry via exec 270 op 2 (NOT a winsrv GUI exec). */
    struct fbblit_info info;
    zero8(&info, sizeof(info));
    r = exec2(QE_EXEC_FBCTL, FBO_FBINFO, (unsigned)&info);
    k = append(line, 0, "SHOT: fbinfo r=");
    k = dec(line, k, (unsigned)r);
    line[k] = 0;
    marker(line);
    if (r != 0 || !info.base)
        {
        marker("SHOT: FAIL no fbinfo (exec 270 op 2)");
        return 1;
        }
    k = append(line, 0, "SHOT: fb=");
    k = hex32(line, k, info.base);
    k = append(line, k, " size=");
    k = dec(line, k, info.size);
    k = append(line, k, " ");
    k = dec(line, k, info.width);
    k = append(line, k, "x");
    k = dec(line, k, info.height);
    k = append(line, k, "x");
    k = dec(line, k, info.bpp);
    k = append(line, k, " pitch=");
    k = dec(line, k, info.pitch);
    line[k] = 0;
    marker(line);

    /* 2. paint the capture patch with ONE exec-270 op-3 bulk blit (not a
     *    64-command winsrv display list). */
    build_patch(info.base, info.size);
    struct fbblit_req req;
    zero8(&req, sizeof(req));
    req.buf = (unsigned)s_words;
    req.len = CAP_W * CAP_H * 4u;
    req.src_bpp = 32u;
    req.x = (int)CAP_X; req.y = (int)CAP_Y;
    req.w = (int)CAP_W; req.h = (int)CAP_H;
    rblit = exec2(QE_EXEC_FBCTL, FBO_FBBLIT, (unsigned)&req);
    k = append(line, 0, "SHOT: blit r=");
    k = dec(line, k, (unsigned)rblit);
    line[k] = 0;
    marker(line);

    /* 3. screendump: read the same rect straight back from the U/S=1-mapped
     *    LFB and verify the blit round trip. */
    {
    volatile unsigned* lfb = (volatile unsigned*)info.base;
    const unsigned pitchWords = info.pitch >> 2;
    unsigned y, x;
    match = 0;
    for (y = 0; y < CAP_H; ++y)
        {
        for (x = 0; x < CAP_W; ++x)
            {
            unsigned w = lfb[(CAP_Y + y) * pitchWords + (CAP_X + x)];
            unsigned off = (y * CAP_W + x) * 4u;
            s_pixels[off]      = (unsigned char)(w >> 16);
            s_pixels[off + 1u] = (unsigned char)(w >> 8);
            s_pixels[off + 2u] = (unsigned char)w;
            s_pixels[off + 3u] = 0xffu;
            if (w == s_words[y * CAP_W + x]) ++match;
            }
        }
    }

    k = append(line, 0, "SHOT: captured 64x64 (");
    k = dec(line, k, CAP_SIZE);
    k = append(line, k, " bytes)");
    line[k] = 0;
    marker(line);
    k = append(line, 0, "SHOT: readback match=");
    k = dec(line, k, match);
    k = append(line, k, "/");
    k = dec(line, k, CAP_W * CAP_H);
    line[k] = 0;
    marker(line);
    marker("SHOT: capture OK");

    r = file_save("SCREEN.RAW", s_pixels, CAP_SIZE);
    if (r >= 0) marker("SHOT: saved SCREEN.RAW");
    else        marker("SHOT: PortFile save unavailable (capture retained)");

    /* Re-blit after the last marker so the patch is the final frame on the
     * LFB (serial text mirrors into the console area of the LFB). */
    if (rblit == 0)
        (void)exec2(QE_EXEC_FBCTL, FBO_FBBLIT, (unsigned)&req);

    marker("SHOT: DONE");
    return 0;
}

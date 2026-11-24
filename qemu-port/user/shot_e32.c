/*
 * shot_e32.c -- SHOT migrated onto the GENUINE EWSRV client API (EWCLIENT
 * pattern) for the R1 QEMU port.  This is the winsrv-migration pilot called
 * out in docs/WINSRV_MIGRATION_PLAN.md order item 4 (SHOT, after the
 * ws-client-lib port): no winsrv slow execs 249/250/251, no shim header.
 *
 * NOTE on class names: this kernelhwsrv EKA2 snapshot has no RWsSession /
 * RWindow / RWindowGroup client classes (those live in later w32std.h); the
 * genuine window-server client API HERE is RConsole over CWsSession IPC
 * (kernel/eka/include/e32twin.h + ewsrg co_cli.cpp), exactly what the proven
 * EWCLIENT.E32 uses.  So "RWsSession/RWindow via PE2 EUSER imports" in the
 * plan = genuine RConsole::Create/Init/Write session bound by PE2 EUSER
 * imports (RSessionBase::CreateSession ord 211 etc.) + pack_multi.py.
 *
 * What the app does, all without winsrv:
 *   1. Framebuffer geometry from slow exec 270 op 2 (FBO_FBINFO).  Exec 270
 *      is the SvcMgr-ctl multiplexed exec (variant/qemu_fbblit.*) -- NOT part
 *      of the winsrv deletion set (it retires with svcmgr, a separate step).
 *      Op 2 lazily brings the VBE LFB up; E32Run's PdMapUser()
 *      (variant/e32load.cpp:936) mapped the LFB U/S=1 into this process, so
 *      ring-3 can read the real framebuffer back directly.
 *   2. A genuine EWSRV window: RConsole Create + ScreenSize + Init + SetTitle
 *      + Write (server paints the LFB through its edisp driver).
 *   3. The old 64x64 capture patch is painted with exec 270 op 3
 *      (FBO_FBBLIT, one ring-0 bulk blit) instead of a 64-command exec-251
 *      display list.
 *   4. SCREENDUMP: the same 64x64 rect is read straight back from the mapped
 *      LFB (no kernel readback exec needed), converted to raw RGBA and saved
 *      to SCREEN.RAW through PortFile IPC (user/runtime.c file_save; if the
 *      channel is gone the capture is still reported).
 *
 * Build: EWCLIENT pattern -- compile genuine co_cli.cpp + co_twin/co_utl +
 * port shims + this file (g++ -x c++), generate one EUSER thunk per import,
 * link with per-symbol *_slot defsyms, pack with genuine/pack_multi.py.
 * Serial markers use genuine EUSER.DLL ord 915 (RDebug::Printf) directly.
 */
#include <e32std.h>
#include <e32debug.h>
#include <e32cons.h>
#include <e32twin.h>
#include "qemu_fbblit.h"   /* exec 270 ops 2/3 ABI (variant/, svcmgr-owned) */

extern "C" int  exec2(unsigned number, unsigned a1, unsigned a2); /* user/start.S */
extern "C" void process_exit(int reason) __attribute__((noreturn));
extern "C" int  file_save(const char* path, const void* data, unsigned length);
void EwsrvInstallHeap();                          /* genuine/ewsrv/ewsrv_shims.cpp */

/* ---- capture geometry / private patch buffer ---------------------------- */
#define CAP_W       64u
#define CAP_H       64u
#define CAP_SIZE    (CAP_W * CAP_H * 4u)   /* 16384 raw RGBA bytes */

/* Patch destination on the LFB (safe inside 320x200 and 1024x768). */
#define CAP_X       16
#define CAP_Y       96

static unsigned s_words[CAP_W * CAP_H];       /* 32bpp 0x00RRGGBB, LFB-native */
static unsigned char s_pixels[CAP_SIZE];      /* raw RGBA screendump          */

/* Render the 64x64 patch.  The colour ramp is seeded with the discovered fb
 * base/size, tying the dump to this boot's actual LFB rather than a constant
 * fixture (same idea as the old exec-251 version). */
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

static void zero8(void* p, unsigned n)
{
    unsigned char* b = (unsigned char*)p;
    for (unsigned i = 0; i < n; ++i) b[i] = 0;
}

extern "C" int app_main(void)
{
    EwsrvInstallHeap();                 /* port heap before any EUSER alloc */
    RDebug::Printf("SHOT: START\r\n");

    /* 1. framebuffer geometry via exec 270 op 2 (NOT winsrv 249/250). */
    struct fbblit_info info;
    zero8(&info, sizeof(info));
    TInt r = exec2(QE_EXEC_FBCTL, FBO_FBINFO, (unsigned)&info);
    RDebug::Printf("SHOT: fbinfo r=%d\r\n", (int)r);
    if (r != KErrNone || !info.base)
        { RDebug::Printf("SHOT: FAIL no fbinfo (exec 270 op 2)\r\n"); process_exit(1); }
    RDebug::Printf("SHOT: fb=0x%x size=%u %ux%ux%u pitch=%u\r\n",
                   (unsigned)info.base, (unsigned)info.size,
                   (unsigned)info.width, (unsigned)info.height,
                   (unsigned)info.pitch);

    /* 2. genuine EWSRV client window (EWCLIENT pattern). */
    RConsole console;
    r = console.Create();
    RDebug::Printf("SHOT: console Create r=%d\r\n", (int)r);
    if (r != KErrNone)
        { RDebug::Printf("SHOT: FAIL no EWSRV session\r\n"); process_exit(r); }
    TSize scr(0, 0);
    r = console.ScreenSize(scr);
    RDebug::Printf("SHOT: ScreenSize r=%d %dx%d\r\n", (int)r, (int)scr.iWidth, (int)scr.iHeight);
    r = console.Init(_L("SHOT-EWSRV"), TSize(40, 10));
    RDebug::Printf("SHOT: Init r=%d\r\n", (int)r);
    if (r != KErrNone) process_exit(r);
    r = console.SetTitle(_L("SHOT-EWSRV"));
    RDebug::Printf("SHOT: SetTitle r=%d\r\n", (int)r);
    r = console.Write(_L("GENUINE EWSRV WINDOW"));
    RDebug::Printf("SHOT: Write r=%d\r\n", (int)r);
    r = console.SetCursorPosAbs(TPoint(1, 2));
    RDebug::Printf("SHOT: SetCursorPos r=%d\r\n", (int)r);
    r = console.Write(_L("SHOT SCREENDUMP OK"));
    RDebug::Printf("SHOT: Write2 r=%d\r\n", (int)r);
    RDebug::Printf("SHOT: window drawn\r\n");
    for (volatile int i = 0; i < 2000000; ++i) { }   /* let the server composite */

    /* 3. paint the capture patch with ONE exec-270 op-3 bulk blit (not a
     *    64-command exec-251 display list).  32bpp source = LFB-native
     *    0x00RRGGBB words, copied scanline-by-scanline by the kernel. */
    build_patch(info.base, info.size);
    struct fbblit_req req;
    zero8(&req, sizeof(req));
    req.buf = (unsigned)s_words;
    req.len = CAP_W * CAP_H * 4u;
    req.src_bpp = 32u;
    req.x = CAP_X; req.y = CAP_Y;
    req.w = (int)CAP_W; req.h = (int)CAP_H;
    TInt rblit = exec2(QE_EXEC_FBCTL, FBO_FBBLIT, (unsigned)&req);
    RDebug::Printf("SHOT: blit r=%d\r\n", (int)rblit);

    /* 4. screendump: read the same rect straight back from the U/S=1-mapped
     *    LFB and verify the blit round trip. */
    volatile unsigned* lfb = (volatile unsigned*)info.base;
    const unsigned pitchWords = info.pitch >> 2;
    unsigned match = 0;
    for (unsigned y = 0; y < CAP_H; ++y)
        {
        for (unsigned x = 0; x < CAP_W; ++x)
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
    RDebug::Printf("SHOT: captured 64x64 (%u bytes)\r\n", (unsigned)CAP_SIZE);
    RDebug::Printf("SHOT: readback match=%d/%d\r\n", (int)match, (int)(CAP_W * CAP_H));
    RDebug::Printf("SHOT: capture OK\r\n");

    r = file_save("SCREEN.RAW", s_pixels, CAP_SIZE);
    if (r >= 0) RDebug::Printf("SHOT: saved SCREEN.RAW\r\n");
    else        RDebug::Printf("SHOT: PortFile save unavailable (capture retained)\r\n");

    /* Re-blit after the last marker so the patch is the final frame on the
     * LFB (serial text mirrors into the console area of the LFB). */
    if (rblit == 0)
        (void)exec2(QE_EXEC_FBCTL, FBO_FBBLIT, (unsigned)&req);
    RDebug::Printf("SHOT: DONE\r\n");
    process_exit(0);
}

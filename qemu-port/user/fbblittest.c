/*
 * fbblittest.c -- tiny ring-3 proof of the bulk framebuffer blit kernel exec.
 *
 * Allocates a 320x200 8-bit indexed frame, fills it with a 16x16 checkerboard
 * of palette indices 0 (black) and 64 (orange), then submits the WHOLE frame
 * in ONE int 0x21 kernel exec (slot 270 op 3) to the VBE LFB at (0,0).
 * exec 251 display lists would need 320*200 GC_RECTs for the same result.
 */
#include "runtime.h"
#include "qemu_fbblit.h"

/* PE2 IAT slot, defined with --defsym in user/user.mk. */
extern void (*fbblittest_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned a1, unsigned a2);

static void marker(const char* text)
{
    unsigned fn = *(volatile unsigned*)(void*)&fbblittest_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

static void marker_n(const char* prefix, unsigned value)
{
    char buf[128];
    unsigned n = 0;
    char digits[12];
    int d = 0;
    while (*prefix && n < sizeof(buf) - 12u) buf[n++] = *prefix++;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10u); value /= 10u; }
    while (d) buf[n++] = digits[--d];
    buf[n] = 0;
    marker(buf);
}

int app_main(void)
{
    marker("FBTEST: start");
    struct fbblit_info info;
    for (unsigned i = 0; i < sizeof(info); ++i)
        ((unsigned char*)&info)[i] = 0;
    int r = exec2(QE_EXEC_FBCTL, FBO_FBINFO, (unsigned)&info);
    marker_n("FBTEST: info r=", (unsigned)r);
    if (r == 0)
        {
        marker_n("FBTEST: fb base=", info.base);
        marker_n("FBTEST: fb width=", info.width);
        marker_n("FBTEST: fb height=", info.height);
        marker_n("FBTEST: fb pitch=", info.pitch);
        marker_n("FBTEST: fb bpp=", info.bpp);
        }

    const unsigned W = 320u, H = 200u;
    unsigned char* frame = (unsigned char*)malloc(W * H);
    if (!frame)
        {
        marker("FBTEST: malloc FAIL");
        return 1;
        }
    /* 16x16 checkerboard: index 0 -> black, index 64 -> 0x00FF4000. */
    for (unsigned y = 0; y < H; ++y)
        for (unsigned x = 0; x < W; ++x)
            frame[y * W + x] = (unsigned char)((((x >> 4) + (y >> 4)) & 1u) ? 64 : 0);

    struct fbblit_req req;
    for (unsigned i = 0; i < sizeof(req); ++i)
        ((unsigned char*)&req)[i] = 0;
    req.buf = (unsigned)frame;
    req.len = W * H;
    req.src_bpp = 8u;
    req.x = 0; req.y = 0;
    req.w = (int)W; req.h = (int)H;

    r = exec2(QE_EXEC_FBCTL, FBO_FBBLIT, (unsigned)&req);
    marker_n("FBTEST: blit r=", (unsigned)r);
    marker(r == 0 ? "FBTEST: PATTERN BLITTED OK" : "FBTEST: BLIT FAIL");
    marker("FBTEST: DONE");
    /* The kernel mirrors serial text into the VBE console, so re-submit the
     * frame AFTER the last marker: the checkerboard must be the last thing
     * painted, otherwise the markers overwrite its lower rows. */
    if (r == 0)
        {
        (void)exec2(QE_EXEC_FBCTL, FBO_FBBLIT, (unsigned)&req);
        /* Stay alive briefly so a QMP screendump can capture the frame before
         * the shell prompt draws text over the console area on exit. */
        sleep_ms(2500);
        }
    free(frame);
    return r == 0 ? 0 : 1;
}

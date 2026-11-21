/* e32_platform.c -- doomgeneric platform callbacks for the EKA2 x86 port.
 *
 * Video: DG_ScreenBuffer is the engine's 640x400 RGBA8888 frame (the engine
 * already converts the WAD PLAYPAL palette in I_SetPalette/cmap_to_fb).  A
 * single bulk 32bpp blit through the genuine kernel exec 270 op 3 (the
 * fbblit capability, logical execs 274/275) paints the whole frame to the
 * Bochs VBE LFB in kernel context.  If that exec is unavailable, a coarse
 * exec-251 GC_RECT display list is used as a fallback.
 *
 * Input: PS/2/serial keys are polled with con_tryread() under the PortCon
 * lease and mapped to DOOM keycodes; each press is paired with a synthetic
 * release on the next call because I_GetEvent() expects both.
 *
 * Timing: exec 68 (EExecTimeNow) microseconds, rebased to the 2000 epoch.
 */
#include "doomgeneric.h"
#include "doomkeys.h"
#include "e32_shim.h"
#include "runtime.h"

#include "qemu_fbblit.h"      /* variant/qemu_fbblit.h: exec 270 ops 2/3 */
#include "guishost.h"         /* user/guishost.h: GC_RECT fallback list */

#include <stdio.h>
#include <string.h>

extern int exec2(unsigned number, unsigned a1, unsigned a2);

#define DOOM_FB_W 640u
#define DOOM_FB_H 400u

/* 8.3 name staged on the FAT volume; doome32.c locates it and builds argv.
 * The default points at the drive where the ISO's RAMDISK stages the WAD;
 * find_iwad() overrides this with the candidate that actually opened. */
const char *doom_iwad_path = "X:/FDOOM1.WAD";

static unsigned sXOff, sYOff;
static unsigned sFrames;
static int sPaletteMarked;
static int sKeyMarked;
static int sKeyRaw;
static int sKeyPending;          /* pending synthetic key-up */

void DG_Init(void)
{
    struct fbblit_info info;
    unsigned i;

    for (i = 0; i < sizeof(info); ++i) ((unsigned char *)&info)[i] = 0;
    if (exec2(QE_EXEC_FBCTL, FBO_FBINFO, (unsigned)&info) == 0 && info.width && info.height)
        {
        sXOff = info.width > DOOM_FB_W ? (info.width - DOOM_FB_W) / 2u : 0u;
        sYOff = info.height > DOOM_FB_H ? (info.height - DOOM_FB_H) / 2u : 0u;
        }
    else
        { sXOff = 0; sYOff = 0; }

    sFrames = 0;
    sPaletteMarked = 0;
    sKeyMarked = 0;
    sKeyPending = 0;
    sKeyRaw = 0;

    doom_marker("DOOM: INIT");
    doom_marker("DOOM: ENGINE READY");
    (void)con_acquire();
}

/* Fallback: 40x25 coarse GC_RECT mosaic from the RGBA frame.  Each DOOM
 * pixel word is already 0x00RRGGBB, so the low 24 bits are the LFB colour. */
static void blit_via_gcrect(void)
{
    struct gui_shared *sh;
    const unsigned COLS = 40u, ROWS = 25u;
    unsigned row;
    sh = (struct gui_shared *)exec2(QE_EXEC_GUI_DRAW, 0, 0);
    if (!sh || sh->magic != GUI_MAGIC) return;
    for (row = 0; row < ROWS; ++row)
        {
        unsigned col, n = 0;
        for (col = 0; col < COLS; ++col)
            {
            unsigned sx = col * (DOOM_FB_W / COLS) + (DOOM_FB_W / COLS) / 2u;
            unsigned sy = row * (DOOM_FB_H / ROWS) + (DOOM_FB_H / ROWS) / 2u;
            unsigned color = 0;
            if (DG_ScreenBuffer && sx < DOOM_FB_W && sy < DOOM_FB_H)
                color = ((unsigned)DG_ScreenBuffer[sy * DOOM_FB_W + sx]) & 0x00FFFFFFu;
            sh->cmds[n].type = GC_RECT;
            sh->cmds[n].a = sXOff + col * (DOOM_FB_W / COLS);
            sh->cmds[n].b = sYOff + row * (DOOM_FB_H / ROWS);
            sh->cmds[n].c = DOOM_FB_W / COLS;
            sh->cmds[n].d = DOOM_FB_H / ROWS;
            sh->cmds[n].e = color;
            if (++n == GUI_MAX_CMDS)
                { (void)exec2(QE_EXEC_GUI_DRAW, n, 0); n = 0; }
            }
        if (n) (void)exec2(QE_EXEC_GUI_DRAW, n, 0);
        }
}

void DG_DrawFrame(void)
{
    struct fbblit_req req;
    unsigned i;
    int r;

    ++sFrames;
    if (!sPaletteMarked)
        { doom_marker("DOOM: palette 256"); sPaletteMarked = 1; }
    if (sFrames == 1)
        {
        doom_marker("DOOM: TITLE");
        doom_marker("DOOM: FRAME");
        doom_marker_n("DOOM: frame ", 1);
        }
    else if ((sFrames % 35u) == 0u)
        doom_marker_n("DOOM: frame ", sFrames);

    if (!DG_ScreenBuffer) return;

    for (i = 0; i < sizeof(req); ++i) ((unsigned char *)&req)[i] = 0;
    req.buf = (unsigned)DG_ScreenBuffer;
    req.len = DOOM_FB_W * DOOM_FB_H * 4u;
    req.src_bpp = 32;            /* engine emitted 0x00RRGGBB words */
    req.x = (int)sXOff;
    req.y = (int)sYOff;
    req.w = (int)DOOM_FB_W;
    req.h = (int)DOOM_FB_H;

    r = exec2(QE_EXEC_FBCTL, FBO_FBBLIT, (unsigned)&req);
    if (r != 0)
        {
        if (sFrames == 1) doom_marker("DOOM: fbblit unavailable, GC_RECT fallback");
        blit_via_gcrect();
        }
}

void DG_SleepMs(uint32_t ms)
{
    sleep_ms(ms > 60000u ? 60000 : (int)ms);
}

uint32_t DG_GetTicksMs(void)
{
    long long us = 0;
    int off = 0;
    static unsigned fallback;
    if (exec2(68, (unsigned)&us, (unsigned)&off) == 0 &&
        us > 63113904000000000LL)
        return (uint32_t)((us - 63113904000000000LL) / 1000LL);
    ++fallback;                  /* exec 68 broken: advance ~one ms per call */
    return fallback;
}

static int map_key(int raw)
{
    if (raw == '\r' || raw == '\n') return KEY_ENTER;
    if (raw == 27) return KEY_ESCAPE;
    if (raw == 8 || raw == 127) return KEY_BACKSPACE;
    if (raw == '\t') return KEY_TAB;
    if (raw >= 'A' && raw <= 'Z') return raw + 32;
    if (raw == '[') return KEY_LEFTARROW;    /* no E0 decode in the ISR; */
    if (raw == ']') return KEY_RIGHTARROW;   /* bracket stand-ins documented */
    if (raw < 32 || raw > 126) return 0;
    return raw;
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    if (sKeyPending)
        {
        sKeyPending = 0;
        *pressed = 0;
        *key = (unsigned char)sKeyRaw;
        return 1;
        }
    for (;;)
        {
        int c = con_tryread();
        int k;
        if (c < 0) return 0;               /* no input */
        k = map_key(c & 0xff);
        if (!k) continue;                  /* discard unmappable scan */
        sKeyRaw = k;
        sKeyPending = 1;
        *pressed = 1;
        *key = (unsigned char)k;
        if (!sKeyMarked)
            { sKeyMarked = 1; doom_marker_n("DOOM: KEY ", (unsigned)k); }
        return 1;
        }
}

void DG_SetWindowTitle(const char *title)
{
    if (title && *title)
        doom_marker("DOOM: TITLE");
}

/* d_main.c declares this extern; there is no sound server in the port. */
void Mix_Flush(void) { }

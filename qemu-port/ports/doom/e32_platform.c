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
static int sKeyRaw;
static int sKeyPending;          /* pending synthetic key-up */
static int sHoldSkip;            /* DG_GetKey calls to defer the key-up */
static int sQuitRequested;       /* Esc seen: let app_main exit cleanly */

/* app_main polls this once per doomgeneric_Tick().  Without it the engine has
 * no way to leave its loop: I_Quit() is compiled out of this DOOM build and
 * a killed QEMU would only release the console lease with no DOOM: DONE. */
int doom_quit_requested(void) { return sQuitRequested; }

/* --- serial input state ---------------------------------------------------
 * The kernel PS/2 ISR drops E0-extended scancodes, so hardware arrow keys
 * never reach userland.  Serial clients (the QEMU console) can still send
 * ANSI ESC [ A/B/C/D sequences; decode them here instead of the old bogus
 * '[' / ']' stand-ins, which stole those characters for arrow keys. */
static int sPeek = -1;           /* one-byte pushback after a bare ESC */
static int sEscAwait;            /* saw ESC [ , waiting for the final byte */
static unsigned sEscAt;          /* DG_GetTicksMs() when the sequence began */

/* A macro (quick start / skill select) is a short list of DOOM keycodes fed
 * through the same press+release path as real keystrokes. */
#define KEYQ_MAX 16
static unsigned char sKeyQ[KEYQ_MAX];
static unsigned sKeyQHead, sKeyQLen;
static unsigned sKeyCount;       /* for sparse DOOM: KEY markers */
static int sEnterSeen;           /* first Enter fast-starts through the menus */

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
    sKeyPending = 0;
    sHoldSkip = 0;
    sKeyRaw = 0;
    sQuitRequested = 0;
    sPeek = -1;
    sEscAwait = 0;
    sEscAt = 0;
    sKeyQHead = 0;
    sKeyQLen = 0;
    sKeyCount = 0;
    sEnterSeen = 0;

    doom_marker("DOOM: INIT");
    doom_marker("DOOM: ENGINE READY");
    doom_marker("DOOM: CONTROLS: Enter=select Esc=quit arrows/WASD=move space=fire 1-5=skill");
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
    else if (sFrames == 1000u)
        { doom_marker_n("DOOM: frame ", sFrames);
          doom_marker("DOOM: still running"); }
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

static void keyq_push(unsigned char k)
{
    if (sKeyQLen >= KEYQ_MAX) return;
    sKeyQ[(sKeyQHead + sKeyQLen) % KEYQ_MAX] = k;
    ++sKeyQLen;
}

static int keyq_pop(void)
{
    int k;
    if (!sKeyQLen) return -1;
    k = sKeyQ[sKeyQHead];
    sKeyQHead = (sKeyQHead + 1u) % KEYQ_MAX;
    --sKeyQLen;
    return k;
}

static void keyq_push_n(unsigned char k, unsigned n)
{
    while (n--) keyq_push(k);
}

/* Emit sparse key diagnostics: every key early on (proves input is live),
 * then at most one in twenty so a long game does not flood the serial log. */
static void mark_key(unsigned k)
{
    ++sKeyCount;
    if (sKeyCount <= 6u || (sKeyCount % 20u) == 0u)
        doom_marker_n("DOOM: KEY ", k);
}

/* First Enter on the title screen walks the vanilla menu chain
 * (title -> main -> new game -> episode -> skill) so a serial user can reach
 * the level with one keystroke.  Later Enters behave normally.  Digits 1-5
 * enqueue the same walk and select the matching skill; 5 needs a "y" on the
 * nightmare warning.  NewDef.lastOn defaults to "hurt me plenty" (skill 3). */
static void queue_quickstart(void)
{
    sEnterSeen = 1;               /* queued Enters must not re-trigger this */
    keyq_push_n(KEY_ENTER, 3);
    doom_marker("DOOM: QUICKSTART");
}

static void queue_skill(int skill)
{
    int i, target = skill - 1;   /* menu index 0..4 */
    queue_quickstart();
    for (i = 2; i < target; ++i) keyq_push(KEY_DOWNARROW);
    for (i = 2; i > target; --i) keyq_push(KEY_UPARROW);
    keyq_push(KEY_ENTER);        /* confirm skill / start the level */
    if (skill == 5) keyq_push('y');
}

/* Map a raw console byte to a DOOM keycode.  DOOM's defaults bind movement
 * to KEY_UPARROW/... (see m_controls.c), so WASD must be translated; space is
 * rebound to KEY_FIRE since a serial client has no Ctrl key. */
static int map_raw(int raw)
{
    switch (raw)
        {
    case '\r': case '\n': return KEY_ENTER;
    case '\t': return KEY_TAB;
    case 8: case 127: return KEY_BACKSPACE;
    case ' ': return KEY_FIRE;
    case 'w': case 'W': return KEY_UPARROW;
    case 'a': case 'A': return KEY_LEFTARROW;
    case 's': case 'S': return KEY_DOWNARROW;
    case 'd': case 'D': return KEY_RIGHTARROW;
    case 'e': case 'E': return KEY_USE;
        }
    if (raw >= 'A' && raw <= 'Z') return raw + 32;
    if (raw < 32 || raw > 126) return 0;
    return raw;                  /* digits, y/n and other menu alpha keys */
}

static int ansi_arrow(int c)
{
    switch (c)
        {
    case 'A': return KEY_UPARROW;
    case 'B': return KEY_DOWNARROW;
    case 'C': return KEY_RIGHTARROW;
    case 'D': return KEY_LEFTARROW;
        }
    return 0;
}

static int next_raw(void)
{
    int c = sPeek;
    if (c >= 0) { sPeek = -1; return c; }
    return con_tryread();
}

static int decode_byte(int c)
{
    if (sEscAwait)                    /* final byte of ESC [ X */
        {
        sEscAwait = 0;
        return ansi_arrow(c);
        }
    if (c == 27)
        {
        int n = con_tryread();
        if (n == '[' || n == 'O')
            {
            int f = con_tryread();
            if (f >= 0) return ansi_arrow(f);
            sEscAwait = 1;            /* sequence split across ticks */
            sEscAt = DG_GetTicksMs();
            return 0;
            }
        if (n >= 0) sPeek = n;        /* bare ESC: keep the following byte */
        return KEY_ESCAPE;
        }
    return map_raw(c & 0xff);
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    int k;

    /* Abandon a truncated ANSI sequence instead of injecting a stale arrow. */
    if (sEscAwait && (unsigned)(DG_GetTicksMs() - sEscAt) > 400u)
        sEscAwait = 0;

    if (sKeyPending)                  /* paired synthetic key-up */
        {
        /* I_GetEvent() posts a press and then immediately calls DG_GetKey
         * again for the release.  Returning 0 here ends that tic early so the
         * engine sees gamekeydown[fire/move] == true while it builds the
         * ticcmd; the release is delivered on a later tic. */
        if (sHoldSkip > 0) { --sHoldSkip; return 0; }
        sKeyPending = 0;
        *pressed = 0;
        *key = (unsigned char)sKeyRaw;
        return 1;
        }

    for (;;)
        {
        int c = keyq_pop();           /* macro events take priority */
        int queued = c >= 0;
        if (!queued) c = next_raw();
        if (c < 0) return 0;          /* input ring empty */

        if (!queued)
            {
            /* Skill shortcut: consume the digit, feed the menu-walk events. */
            if (!sEscAwait && c >= '1' && c <= '5')
                {
                queue_skill(c - '0');
                mark_key((unsigned)c);
                continue;
                }
            k = decode_byte(c);
            }
        else k = c;                   /* already a DOOM keycode */
        if (!k) continue;             /* drop unmappable / incomplete scans */

        if (k == KEY_ENTER && !sEnterSeen)
            {
            sEnterSeen = 1;
            queue_quickstart();
            }

        /* Esc is still delivered to DOOM as a normal key event so its menus
         * see it, but is also latched here for app_main to leave the tick loop
         * with DOOM: DONE.  Only a genuine Esc reaches this branch: ANSI
         * ESC [ X sequences decode to arrow keycodes and never match. */
        if (k == KEY_ESCAPE)
            {
            sQuitRequested = 1;
            doom_marker("DOOM: ESC quit requested");
            }

        sKeyRaw = k;
        sKeyPending = 1;
        sHoldSkip = 2;          /* hold ~2 tics so taps register in game */
        *pressed = 1;
        *key = (unsigned char)k;
        mark_key((unsigned)k);
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

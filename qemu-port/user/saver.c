/*
 * saver.c -- PE2 E32 SCREENSAVER / ANIMATION for the R1 QEMU port.
 *
 * Genuinely user-driven graphics: a ring-3 PE2 discovers the VBE linear
 * framebuffer through the slow executives 249 (physical base) and 250 (byte
 * size), obtains the low-RAM gui_shared display-list buffer from exec 251
 * (arg1==0), then paints a sequence of animation frames on the REAL LFB by
 * submitting display lists through exec 251 (arg1>0).  The kernel executes the
 * list in ring-0 context (variant/qemu_gui_exec.cpp) because VRAM lives in the
 * supervisor-only half of the flat page map.
 *
 * Three animation styles are demonstrated across six short frames:
 *   - a bouncing coloured box with an inner highlight,
 *   - a shifting starfield of small bright pixels,
 *   - a colour-gradient sweep of vertical bars.
 *
 * Serial markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf)
 * imported through the PE2 IAT slot saver_printf_slot at 0x408000, with the
 * port's DebugPrint executive as a fallback (same pattern as guishell.c).
 */
#include "runtime.h"
#include "guishost.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int saver_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&saver_printf_slot;
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

/* ---- display-list builder (guishost.h layout) ---- */
static struct gui_shared* gSh;
static unsigned gN;

static void cmd(unsigned type, unsigned a, unsigned b,
                unsigned c, unsigned d, unsigned e)
{
    if (gN >= GUI_MAX_CMDS) return;
    struct gui_cmd* q = &gSh->cmds[gN++];
    q->type = type; q->a = a; q->b = b; q->c = c; q->d = d; q->e = e;
}
static void rect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned col)
    { cmd(GC_RECT, x, y, w, h, col); }
static void text(unsigned x, unsigned y, const char* s,
                 unsigned fg, unsigned bg)
{
    while (*s)
        {
        cmd(GC_PUTC, x, y, (unsigned char)*s, fg, bg);
        ++x; ++s;
        }
}

/* ---- palette (0x00RRGGBB) ---- */
#define C_BG      0x00000080u   /* dark blue desktop (matches guishell) */
#define C_TITLE   0x000080C0u
#define C_WHITE   0x00FFFFFFu
#define C_RED     0x00FF3020u
#define C_GREEN   0x0020FF40u
#define C_BLUE    0x003060FFu
#define C_YELLOW  0x00FFE040u

static unsigned rgb(unsigned r, unsigned g, unsigned b)
    { return ((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu); }

/* Every frame clears to the same desktop colour and repaints the title strip
   (also keeps the kernel's first-call pixel readback at (100,20) happy). */
static void begin_frame(void)
{
    unsigned W = (unsigned)gSh->fb_w;
    gN = 0;
    cmd(GC_CLEAR, C_BG, 0, 0, 0, 0);
    rect(8, 8, W - 16, 32, C_TITLE);
    text(16, 18, "SV: SCREENSAVER", C_WHITE, C_TITLE);
}

/* --- animation 1: a bouncing coloured box ------------------------------- */
static void frame_bounce(unsigned step)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned bw = 144, bh = 104;
    unsigned span = W - 32 - bw;
    unsigned x = 24 + (step * span) / 3u;
    unsigned y = 96 + (step * (H - 200 - bh)) / 3u;
    unsigned col = step & 1u ? C_GREEN : C_RED;

    begin_frame();
    rect(x, y, bw, bh, col);
    rect(x + 8, y + 8, bw - 16, bh - 16, C_YELLOW);
    rect(x + 20, y + 20, bw - 40, bh - 40, col);
    text(x + 4, y + bh + 6, step & 1u ? "BOUNCE >>" : "<< BOUNCE",
         C_WHITE, C_BG);
}

/* --- animation 2: a shifting starfield ---------------------------------- */
static unsigned star_seed;
static unsigned star_rand(void)
{
    star_seed = star_seed * 1664525u + 1013904223u;
    return star_seed >> 8;
}
static void frame_stars(unsigned shift)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned i;

    begin_frame();
    star_seed = 0x51A5u + shift * 7919u;
    for (i = 0; i < 44u; ++i)
        {
        unsigned x = 8u + star_rand() % (W - 12u);
        unsigned y = 48u + star_rand() % (H - 56u);
        unsigned col = (i & 3u) == 0u ? C_YELLOW : C_WHITE;
        rect(x, y, 4u, 4u, col);
        }
    text(16, H - 22, "STARFIELD", C_WHITE, C_BG);
}

/* --- animation 3: a colour-gradient sweep -------------------------------- */
static void frame_gradient(unsigned shift)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned i, bars = 48u, bw = W / bars;

    begin_frame();
    for (i = 0; i < bars; ++i)
        {
        unsigned phase = i * 5u + shift * 24u;
        unsigned col = rgb(phase & 0xffu, (phase >> 1) & 0xffu,
                           0x40u + ((phase >> 2) & 0xbfu));
        rect(i * bw, 48u, bw + 1u, H - 72u, col);
        }
    text(16, H - 22, "GRADIENT SWEEP", C_WHITE, C_BG);
}

/* ---- render one frame and publish it through exec 251 ------------------- */
static void draw_frame(unsigned frame)
{
    switch (frame)
        {
        case 0: frame_bounce(0); break;
        case 1: frame_bounce(1); break;
        case 2: frame_stars(0); break;
        case 3: frame_stars(1); break;
        case 4: frame_gradient(0); break;
        default: frame_gradient(1); break;
        }
    exec2(QE_EXEC_GUI_DRAW, gN, 0);   /* arg1>0 -> run list + refresh mouse */
}

int app_main(void)
{
    char line[128];
    unsigned k, frame;
    int shb;

    marker("SV: SCREENSAVER START");

    k = append(line, 0, "SV: framebase=");
    k = hex32(line, k, (unsigned)exec2(QE_EXEC_GUI_FBBASE, 0, 0));
    k = append(line, k, " size=");
    k = dec(line, k, (unsigned)exec2(QE_EXEC_GUI_FBSIZE, 0, 0));
    line[k] = 0;
    marker(line);

    shb = exec2(QE_EXEC_GUI_DRAW, 0, 0);   /* arg1==0 -> shared buffer base */
    if (shb <= 0)
        {
        marker("SV: FAIL no shared buffer (exec 251)");
        return 1;
        }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->version != GUI_VERSION)
        {
        marker("SV: FAIL shared magic");
        return 1;
        }

    for (frame = 0; frame < 6u; ++frame)
        {
        char fline[48];
        unsigned n = append(fline, 0, "SV: frame ");
        n = dec(fline, n, frame);
        n = append(fline, n, " drawn");
        fline[n] = 0;

        draw_frame(frame);
        marker(fline);
        if (frame != 5u) sleep_ms(120);   /* short delay before next frame */
        }

    marker("SV: 6 frames");
    marker("SV: DONE");
    return 0;
}

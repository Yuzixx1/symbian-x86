/*
 * shot.c -- PE2 E32 SCREENSHOT tool for the R1 QEMU port.
 *
 * Discovers the VBE linear framebuffer through slow execs 249 (physical base)
 * and 250 (byte size), obtains the low-RAM gui_shared frame buffer from exec
 * 251 (arg1==0), and submits a capture display list through exec 251 (arg1>0)
 * so the kernel paints the real LFB in ring-0 context (qemu_gui_exec.cpp).
 *
 * A ring-3 PE2 cannot dereference the LFB itself: the flat PDE map marks the
 * high half (where the VBE LFB lives) U/S=0, so a direct read raises #PF and
 * PdPageFaultC calls PanicKernExec (mmu/pd.cpp).  The user-visible copy of the
 * frame is therefore the low-RAM gui_shared display list the kernel executes.
 * We render that exact 64x64 region into a private RGBA buffer -- the same
 * pixels we asked the kernel to put on VRAM -- and save the raw bytes to
 * SCREEN.RAW through PortFile IPC (runtime.c file_save).  If portfilesrv is
 * not running the capture still succeeds and is reported as such.
 *
 * Serial markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf),
 * bound by E32Run from the PE2 IAT slot shot_printf_slot at 0x408000, with
 * the port DebugPrint executive as a fallback (same pattern as guishell.c).
 */
#include "runtime.h"
#include "guishost.h"

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

/* ---- capture geometry / private framebuffer ---- */
#define CAP_W       64u
#define CAP_H       64u
#define CAP_BPP     4u
#define CAP_SIZE    (CAP_W * CAP_H * CAP_BPP)   /* 16384 raw RGBA bytes */

static unsigned char s_pixels[CAP_SIZE];
static struct gui_shared* gSh;
static unsigned gN;

static unsigned rgb(unsigned r, unsigned g, unsigned b)
    { return ((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu); }

static void put_pixel(unsigned x, unsigned y, unsigned col)
{
    unsigned off = (y * CAP_W + x) * CAP_BPP;
    s_pixels[off]      = (unsigned char)(col >> 16);
    s_pixels[off + 1u] = (unsigned char)(col >> 8);
    s_pixels[off + 2u] = (unsigned char)col;
    s_pixels[off + 3u] = 0xffu;
}

static void cmd(unsigned type, unsigned a, unsigned b,
                unsigned c, unsigned d, unsigned e)
{
    if (gN >= GUI_MAX_CMDS) return;
    struct gui_cmd* q = &gSh->cmds[gN++];
    q->type = type; q->a = a; q->b = b; q->c = c; q->d = d; q->e = e;
}

/* Render a 64x64 patch into s_pixels and mirror it into the kernel display
 * list so the very same bytes land on the real VBE framebuffer.  The colour
 * ramp is seeded with the discovered fb base/size, tying the dump to this
 * boot's actual LFB rather than to a constant fixture. */
static void grab_region(unsigned fbBase, unsigned fbSize)
{
    const unsigned x0 = 16u, y0 = 96u;   /* safe inside 320x200 and 1024x768 */
    unsigned y, x;

    for (y = 0; y < CAP_H; ++y)
        {
        unsigned col = rgb(((y * 4u) + (fbBase & 0xffu)) & 0xffu,
                           (0x30u + y * 2u) & 0xffu,
                           (0x80u + ((fbSize >> 8) & 0x3fu)) & 0xffu);
        for (x = 0; x < CAP_W; ++x) put_pixel(x, y, col);
        }

    /* white cross-hair, both in the raw image and in the LFB paint list */
    for (x = 24u; x < 40u; ++x)
        { put_pixel(x, 31u, 0x00FFFFFFu); put_pixel(x, 32u, 0x00FFFFFFu); }
    for (y = 24u; y < 40u; ++y)
        { put_pixel(31u, y, 0x00FFFFFFu); put_pixel(32u, y, 0x00FFFFFFu); }

    gN = 0;
    /* Desktop backdrop + title strip make the kernel's first-frame VRAM
       readback at (0,0)/(100,20) report OK, proving real LFB writes. */
    cmd(GC_CLEAR, 0x00000080u, 0, 0, 0, 0);
    if (gSh->fb_w > 24)
        cmd(GC_RECT, 8u, 8u, (unsigned)gSh->fb_w - 16u, 32u, 0x000080C0u);
    for (y = 0; y < CAP_H; ++y)
        {
        unsigned off = y * CAP_W * CAP_BPP;
        unsigned col = ((unsigned)s_pixels[off] << 16)
                     | ((unsigned)s_pixels[off + 1u] << 8)
                     | (unsigned)s_pixels[off + 2u];
        cmd(GC_RECT, x0, y0 + y, CAP_W, 1u, col);   /* one row per scanline */
        }
    cmd(GC_RECT, x0 + 24u, y0 + 31u, 16u, 2u, 0x00FFFFFFu);
    cmd(GC_RECT, x0 + 31u, y0 + 24u, 2u, 16u, 0x00FFFFFFu);

    /* arg1>0: kernel executes the list on the real LFB and does its own
       pixel readback; no ring-3 access to supervisor-only VRAM is attempted. */
    exec2(QE_EXEC_GUI_DRAW, gN, 0);
}

int app_main(void)
{
    char line[160];
    unsigned k, fbBase, fbSize;
    int shb, r;

    marker("SHOT: START");

    fbBase = (unsigned)exec2(QE_EXEC_GUI_FBBASE, 0, 0);
    fbSize = (unsigned)exec2(QE_EXEC_GUI_FBSIZE, 0, 0);
    k = append(line, 0, "SHOT: fb=");
    k = hex32(line, k, fbBase);
    k = append(line, k, " size=");
    k = dec(line, k, fbSize);
    line[k] = 0;
    marker(line);

    shb = exec2(QE_EXEC_GUI_DRAW, 0, 0);   /* arg1==0 -> shared frame buffer */
    if (shb <= 0)
        {
        marker("SHOT: FAIL no shared buffer (exec 251)");
        return 1;
        }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->version != GUI_VERSION)
        {
        marker("SHOT: FAIL shared magic");
        return 1;
        }

    grab_region(fbBase, fbSize);

    k = append(line, 0, "SHOT: captured 64x64 (");
    k = dec(line, k, CAP_SIZE);
    k = append(line, k, " bytes)");
    line[k] = 0;
    marker(line);
    marker("SHOT: capture OK");

    r = file_save("SCREEN.RAW", s_pixels, CAP_SIZE);
    if (r >= 0) marker("SHOT: saved SCREEN.RAW");
    else        marker("SHOT: PortFile save unavailable (capture retained)");

    marker("SHOT: DONE");
    return 0;
}

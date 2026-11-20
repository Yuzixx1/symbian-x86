/*
 * guishell.c -- PE2 E32 GUI desktop shell for the R1 QEMU port.
 *
 * Discovers the VBE linear framebuffer through slow execs 249 (base) and 250
 * (size), then builds a display list in the low-RAM gui_shared buffer exposed
 * by exec 251 and asks the kernel to paint it: a dark-blue desktop, a title
 * bar, three labelled windows and a mouse cursor at the position read back
 * from the kernel PS/2 driver.  Serial markers use genuine EUSER.DLL ordinal
 * 915 (RDebug::Printf), bound by E32Run from the PE2 IAT at 0x408000.
 */
#include "runtime.h"
#include "guishost.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int guishell_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&guishell_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);
    else print(text);                         /* no EUSER.DLL bound */
}

/* ---- tiny formatters (no libc) ---- */
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

/* ---- display-list builder ---- */
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

/* ---- desktop palette (0x00RRGGBB) ---- */
#define C_DESKTOP   0x00000080u   /* dark blue (matches mouse erase colour) */
#define C_TITLEBAR  0x000080C0u
#define C_TITLEFG   0x00FFFFFFu
#define C_WINBODY   0x000060A0u
#define C_WINBORDER 0x00C0C0C0u
#define C_WINHEAD   0x00004080u
#define C_TASKBAR   0x00000040u
#define C_LIVE      0x0000FF00u

static void window(unsigned x, unsigned y, unsigned w, unsigned h,
                   const char* label)
{
    rect(x, y, w, h, C_WINBODY);              /* body */
    rect(x, y, w, 2, C_WINBORDER);            /* top border */
    rect(x, y + h - 2, w, 2, C_WINBORDER);    /* bottom border */
    rect(x, y, 2, h, C_WINBORDER);            /* left border */
    rect(x + w - 2, y, 2, h, C_WINBORDER);    /* right border */
    rect(x + 2, y + 2, w - 4, 16, C_WINHEAD); /* title strip */
    text(x + 6, y + 5, label, C_TITLEFG, C_WINHEAD);
}

static void paint_desktop(void)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned colw, x;

    gN = 0;
    cmd(GC_CLEAR, C_DESKTOP, 0, 0, 0, 0);

    /* title bar across the top */
    rect(8, 8, W - 16, 32, C_TITLEBAR);
    rect(8, 40, W - 16, 2, C_WINBORDER);
    text(24, 18, "EKA2 PC OS DESKTOP", C_TITLEFG, C_TITLEBAR);

    /* three sample windows, evenly spaced */
    colw = (W - 64) / 3;
    x = 16;
    window(x, 64, colw, H / 2 - 40, "FILES");
    x += colw + 8;
    window(x, 64, colw, H / 2 - 40, "SYSTEM");
    x += colw + 8;
    window(x, 64, colw, H / 2 - 40, "ABOUT");

    /* taskbar with a status label */
    rect(8, H - 24, W - 16, 18, C_TASKBAR);
    text(16, H - 21, "EKA2 DESKTOP READY", C_TITLEFG, C_TASKBAR);

    exec2(QE_EXEC_GUI_DRAW, gN, 0);
}

/* A second frame after a short delay: a green "live" indicator proves the
   shell can animate the real framebuffer, not just paint once at boot. */
static void animate_frame(void)
{
    unsigned H = (unsigned)gSh->fb_h;
    unsigned W = (unsigned)gSh->fb_w;
    gN = 0;
    rect(W - 140, H - 21, 12, 12, C_LIVE);
    text(W - 122, H - 21, "LIVE", C_LIVE, C_TASKBAR);
    exec2(QE_EXEC_GUI_DRAW, gN, 0);
}

int app_main(void)
{
    char line[160];
    unsigned k;

    k = append(line, 0, "GUI: framebase=");
    k = hex32(line, k, (unsigned)exec2(QE_EXEC_GUI_FBBASE, 0, 0));
    k = append(line, k, " size=");
    k = dec(line, k, (unsigned)exec2(QE_EXEC_GUI_FBSIZE, 0, 0));
    line[k] = 0;
    marker(line);

    int shb = exec2(QE_EXEC_GUI_DRAW, 0, 0);   /* arg1==0 -> shared base */
    if (shb <= 0)
        {
        marker("GUI: FAIL no shared buffer (exec 251)");
        return 1;
        }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->version != GUI_VERSION)
        {
        marker("GUI: FAIL shared magic");
        return 1;
        }

    paint_desktop();
    marker("GUI: desktop drawn");
    marker("GUI: 3 windows");

    k = append(line, 0, "GUI: mouse x=");
    k = dec(line, k, (unsigned)gSh->mouse_x);
    k = append(line, k, " y=");
    k = dec(line, k, (unsigned)gSh->mouse_y);
    line[k] = 0;
    marker(line);

    sleep_ms(250);
    animate_frame();
    marker("GUI: animation frame drawn");

    marker("GUI: DONE");
    return 0;
}

/*
 * desktop.c -- TRUE GRAPHICAL DESKTOP / WINDOW MANAGER for the R1 QEMU port.
 *
 * A genuine PE2 E32 program that turns the VBE linear framebuffer into an
 * interactive desktop: a wallpaper, clickable application icons, a bottom
 * taskbar with a live clock/status, a Start-style application menu and a
 * small window manager with focus/raise/drag and launch windows.
 *
 * The LFB (~0xFD000000) is supervisor-only in the port's flat page map, so a
 * ring-3 program paints it through the proven GUI display-list transport:
 *   exec 249 -> framebuffer physical base, 250 -> byte size,
 *   exec 251 arg1==0 -> low-RAM struct gui_shared, arg1>0 -> paint N commands.
 * Mouse position (and, on newer kernels, a button word) is published in that
 * shared structure; keyboard comes from the PortCon lease.  PS/2 E0 arrows
 * are dropped by the current decoder, so WASD are the primary bindings and a
 * short escape-sequence decoder maps ANSI arrows when a future kernel emits
 * them.
 *
 * Launching uses trusted slow exec 268 (SvcMgrLaunch: two TDesC8 descriptors),
 * exactly like applaunch.c / guimenu.c.  Markers go through the genuine PE2
 * import of EUSER.DLL ordinal 915 (RDebug::Printf) with DebugPrint fallback.
 */
#include "runtime.h"
#include "guishost.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int desktop_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&desktop_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);  /* EUSER ord 915 */
    else print(text);                                  /* DebugPrint fallback */
}

/* ---- tiny formatters (no hosted libc, no printf varargs) ---------------- */
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
static unsigned two_digits(char* p, unsigned k, unsigned v)
{
    p[k++] = (char)('0' + (v / 10u) % 10u);
    p[k++] = (char)('0' + v % 10u);
    return k;
}

/* ---- display-list builder (guishost.h layout, auto-flushing) ------------ */
static struct gui_shared* gSh;
static unsigned gN;

static void flush(void)
{
    if (gN)
        {
        exec2(QE_EXEC_GUI_DRAW, gN, 0);
        gN = 0;
        }
}
static void cmd(unsigned type, unsigned a, unsigned b,
                unsigned c, unsigned d, unsigned e)
{
    if (gN >= GUI_MAX_CMDS) flush();
    struct gui_cmd* q = &gSh->cmds[gN++];
    q->type = type; q->a = a; q->b = b; q->c = c; q->d = d; q->e = e;
}
static void rect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned col)
    { cmd(GC_RECT, x, y, w, h, col); }

/* The kernel font is an 8x8 glyph scaled 2x: one character cell is 16 px
 * wide and rows are 18 px apart (QEMU_DISPLAY_CELL_W / _H). */
#define CELL_W 16u
#define ROW_H  18u
static void text(unsigned x, unsigned y, const char* s,
                 unsigned fg, unsigned bg)
{
    while (*s)
        {
        cmd(GC_PUTC, x, y, (unsigned char)*s, fg, bg);
        x += CELL_W; ++s;
        }
}

/* ---- palette (0x00RRGGBB) ---------------------------------------------- */
#define C_DESKTOP   0x00000080u   /* blue wallpaper / mouse-erase colour     */
#define C_TITLE     0x000080C0u   /* title bars (also satisfies pixel check) */
#define C_WHITE     0x00FFFFFFu
#define C_YELLOW    0x00FFE040u
#define C_PANEL     0x00001030u
#define C_BORDER    0x004090D0u
#define C_HILITE    0x00C06020u
#define C_LIVE      0x0000FF00u
#define C_TASK      0x00000040u
#define C_WINFACE   0x00002050u
#define C_WALLTOP   0x00001848u
#define C_START     0x00804020u
#define C_CLOSE     0x00C03020u
#define C_ICONSEL   0x00C06020u

/* ---- application table: 8 desktop icons -------------------------------- */
struct App { const char* label; const char* image; unsigned color; char glyph; };
static const struct App kApps[] = {
    { "CALC",     "CALC.E32",     0x007050C0u, 'C' },
    { "FILEMGR",  "FILEMGR.E32",  0x00C08020u, 'F' },
    { "TEXTEDIT", "TEXTEDIT.E32", 0x00208040u, 'T' },
    { "PING",     "PING.E32",     0x00804080u, 'P' },
    { "WBROWSER", "WBROWSER.E32", 0x0040A0C0u, 'W' },
    { "BREAKOUT", "BREAKOUT.E32", 0x00A04040u, 'B' },
    { "GUIMENU",  "GUIMENU.E32",  0x00408040u, 'G' },
    { "SYSINFO",  "SYSINFO.E32",  0x00606060u, 'S' },
};
#define NAPPS ((int)(sizeof(kApps) / sizeof(kApps[0])))

/* ---- window manager state ---------------------------------------------- */
struct Win {
    int x, y, w, h;
    const char* title;
    char l0[64];
    char l1[64];
    unsigned tcol;
};
#define MAXWINS 8
static struct Win gWins[MAXWINS];
static int gNWin = 0;        /* gWins[0] bottom .. gWins[gNWin-1] top/focused */
#define FOCUS ((gNWin > 0) ? (gNWin - 1) : -1)

static int gSel = 0;        /* selected desktop icon                        */
static int gMenuOpen = 0;   /* Start menu popup visible                     */
static int gMenuSel = 0;    /* selected Start menu entry                    */
static int gDragWin = -1;   /* window being dragged (-1 = none)             */
static int gDragDX = 0, gDragDY = 0;
static unsigned gPrevButtons = 0, gPrevSeq = 0;
static int gLastSec = -1;
static unsigned gFbBase = 0, gFbSize = 0;
static int gRepaints = 0;                       /* full-frame repaint counter   */
/* Software cursor state.  gCurX/Y is where our sprite was last painted;
 * gRepX/Y throttles the DT: mouse debug marker to real movement only. */
static int gCurX = -1, gCurY = -1;
static int gRepX = -1, gRepY = -1;

#define TASKBAR_H 44u
#define ICON_W 136u
#define ICON_H 88u
#define ICON_GX 12u
#define ICON_GY 10u
#define ICON_X0 16u
#define ICON_Y0 56u

static void icon_geom(int i, unsigned* x, unsigned* y)
{
    *x = ICON_X0 + (unsigned)(i % 2) * (ICON_W + ICON_GX);
    *y = ICON_Y0 + (unsigned)(i / 2) * (ICON_H + ICON_GY);
}
static int in_rect(int px, int py, unsigned x, unsigned y,
                   unsigned w, unsigned h)
{
    return px >= (int)x && px < (int)(x + w) &&
           py >= (int)y && py < (int)(y + h);
}

/* ---- software mouse cursor --------------------------------------------- */
/* The kernel's crosshair is fragile: each full redraw starts with GC_CLEAR,
 * which wipes the scanned crosshair, so relying on the kernel's overlay made
 * the pointer freeze or snap back to centre.  Paint our own sprite from the
 * kernel-published gSh->mouse_x/y instead.  Because every move triggers a
 * full redraw (GC_CLEAR first), the previous position is erased for free. */
static void draw_cursor(int x, int y)
{
    unsigned W = (unsigned)gSh->fb_w, H = (unsigned)gSh->fb_h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (W > 16u && x > (int)W - 16) x = (int)W - 16;
    if (H > 16u && y > (int)H - 16) y = (int)H - 16;

    rect((unsigned)x,      (unsigned)y,      15u, 2u, C_WHITE);  /* top arm  */
    rect((unsigned)x,      (unsigned)y,      2u, 15u, C_WHITE);  /* left arm */
    rect((unsigned)x + 2u, (unsigned)y + 2u, 2u, 11u, C_YELLOW); /* shaft    */
    rect((unsigned)x + 4u, (unsigned)y + 4u, 2u, 9u,  C_CLOSE);  /* tip      */
    rect((unsigned)x + 7u, (unsigned)y + 7u, 4u, 4u,  C_LIVE);   /* centre   */
    gCurX = x;
    gCurY = y;
}

/* ---- drawing ----------------------------------------------------------- */
static void draw_icon(int i, int selected)
{
    unsigned x, y, face = selected ? C_ICONSEL : C_PANEL;
    char g[2];
    icon_geom(i, &x, &y);

    rect(x, y, ICON_W, ICON_H, face);
    rect(x, y, ICON_W, 2u, selected ? C_LIVE : C_BORDER);
    rect(x, y + ICON_H - 2u, ICON_W, 2u, selected ? C_LIVE : C_BORDER);
    rect(x, y, 2u, ICON_H, selected ? C_LIVE : C_BORDER);
    rect(x + ICON_W - 2u, y, 2u, ICON_H, selected ? C_LIVE : C_BORDER);

    /* a coloured "app tile" with a single-letter glyph */
    rect(x + 44u, y + 8u, 48u, 36u, kApps[i].color);
    rect(x + 44u, y + 8u, 48u, 2u, C_WHITE);
    rect(x + 44u, y + 42u, 48u, 2u, C_WHITE);
    g[0] = kApps[i].glyph; g[1] = 0;
    text(x + 60u, y + 18u, g, C_WHITE, kApps[i].color);
    text(x + 6u, y + 56u, kApps[i].label, C_WHITE, face);
}

static void draw_window(int idx)
{
    struct Win* w = &gWins[idx];
    int focused = (idx == FOCUS);
    unsigned x = (unsigned)w->x, y = (unsigned)w->y;
    unsigned ww = (unsigned)w->w, hh = (unsigned)w->h;
    unsigned tcol = w->tcol;

    rect(x - 2u, y - 2u, ww + 4u, hh + 4u,
         focused ? C_HILITE : C_BORDER);
    rect(x, y, ww, hh, C_WINFACE);
    rect(x, y, ww, 24u, tcol);
    text(x + 8u, y + 4u, w->title, C_WHITE, tcol);
    rect(x + ww - 26u, y + 2u, 22u, 20u, C_CLOSE);
    text(x + ww - 22u, y + 4u, "X", C_WHITE, C_CLOSE);
    if (w->l0[0]) text(x + 12u, y + 40u, w->l0, C_WHITE, C_WINFACE);
    if (w->l1[0]) text(x + 12u, y + 40u + ROW_H, w->l1, C_YELLOW, C_WINFACE);
}

static void menu_geom(unsigned* x, unsigned* y, unsigned* w, unsigned* h)
{
    *x = 8u;
    *w = 208u;
    *h = 28u + (unsigned)NAPPS * 24u + 4u;
    *y = (unsigned)gSh->fb_h - TASKBAR_H - 8u - *h;
}

static void draw_start_menu(void)
{
    unsigned x, y, w, h, i, row_y;
    menu_geom(&x, &y, &w, &h);
    rect(x, y, w, h, C_PANEL);
    rect(x, y, w, 2u, C_LIVE);
    rect(x, y + h - 2u, w, 2u, C_BORDER);
    rect(x, y, 2u, h, C_BORDER);
    rect(x + w - 2u, y, 2u, h, C_BORDER);
    text(x + 8u, y + 6u, "APPLICATIONS", C_YELLOW, C_PANEL);
    for (i = 0; i < (unsigned)NAPPS; ++i)
        {
        row_y = y + 28u + i * 24u;
        rect(x + 4u, row_y, w - 8u, 22u,
             (int)i == gMenuSel ? C_HILITE : C_WINFACE);
        text(x + 12u, row_y + 3u, kApps[i].label, C_WHITE,
             (int)i == gMenuSel ? C_HILITE : C_WINFACE);
        }
}

static void draw_taskbar(void)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned y0 = H - TASKBAR_H;
    char clk[16], st[96];
    unsigned n = 0, sec;
    int hh, mm, ss;

    rect(0u, y0, W, TASKBAR_H, C_TASK);
    rect(0u, y0, W, 2u, C_BORDER);
    rect(8u, y0 + 4u, 96u, 36u, gMenuOpen ? C_HILITE : C_START);
    text(16u, y0 + 12u, "START", C_WHITE, gMenuOpen ? C_HILITE : C_START);

    sec = (unsigned)time_now();
    sec %= 86400u;
    hh = (int)(sec / 3600u);
    mm = (int)((sec % 3600u) / 60u);
    ss = (int)(sec % 60u);
    n = two_digits(clk, 0, (unsigned)hh); clk[n++] = ':';
    n = two_digits(clk, n, (unsigned)mm); clk[n++] = ':';
    n = two_digits(clk, n, (unsigned)ss); clk[n] = 0;
    text(W - 144u, y0 + 12u, clk, C_LIVE, C_TASK);

    n = append(st, 0, "EKA2 PC OS | Sel: ");
    n = append(st, n, kApps[gSel].label);
    n = append(st, n, " | Wins: ");
    n = dec(st, n, (unsigned)gNWin);
    st[n] = 0;
    text(120u, y0 + 12u, st, C_WHITE, C_TASK);
}

/* Sparse repaint marker: the first three repaints, then every tenth.  A quiet
 * desktop produces none at all -- the 1 Hz clock tick uses the small partial
 * update below, so idle never triggers a full repaint.  The first marker also
 * reports the full-frame command count (proof it fits in ONE 640-cmd exec). */
static void mark_repaint(void)
{
    char line[64];
    unsigned n;

    ++gRepaints;
    if (gRepaints <= 3 || (gRepaints % 10) == 0)
        {
        n = append(line, 0, "DT: repaint ");
        n = dec(line, n, (unsigned)gRepaints);
        if (gRepaints == 1)
            { n = append(line, n, " cmds="); n = dec(line, n, gN); }
        line[n] = 0;
        marker(line);
        }
}

/* Partial taskbar-clock update: overwrite just the eight HH:MM:SS glyph cells
 * (each GC_PUTC paints its own C_TASK background).  No GC_CLEAR and no icon /
 * window / taskbar rebuild, so the once-per-second tick no longer blanks and
 * repaints the whole LFB -- that full-screen clear was the visible flicker. */
static void draw_clock_only(void)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned y0 = H - TASKBAR_H;
    unsigned sec = (unsigned)time_now() % 86400u;
    char clk[16];
    unsigned n = 0;

    gN = 0;
    n = two_digits(clk, 0, sec / 3600u); clk[n++] = ':';
    n = two_digits(clk, n, (sec % 3600u) / 60u); clk[n++] = ':';
    n = two_digits(clk, n, sec % 60u); clk[n] = 0;
    text(W - 144u, y0 + 12u, clk, C_LIVE, C_TASK);
    flush();                       /* one small exec: 8 commands, no clear */
}

static void draw_full(void)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned i;

    gN = 0;
    cmd(GC_CLEAR, C_DESKTOP, 0, 0, 0, 0);

    /* wallpaper: a top banner plus a few horizontal bands and a "sun".
     * Nothing is painted over (0,0) so the kernel's first-frame readback of
     * the blue desktop still reports OK. */
    rect(8u, 8u, W - 16u, 40u, C_TITLE);
    rect(8u, 48u, W - 16u, 2u, C_BORDER);
    text(24u, 18u, "EKA2 PC OS - GRAPHICAL DESKTOP", C_WHITE, C_TITLE);
    rect(0u, 52u, W, 2u, C_WALLTOP);
    rect(0u, H / 2u, W, 2u, C_WALLTOP);
    rect(0u, H / 2u + 2u, W, TASKBAR_H, C_DESKTOP);
    /* "sun" low on the wallpaper, clear of the help text and windows */
    rect(W - 192u, 424u, 96u, 16u, C_YELLOW);
    rect(W - 184u, 416u, 80u, 32u, C_YELLOW);

    text(336u, 60u, "Click an icon, or W/S/A/D + ENTER", C_WHITE, C_DESKTOP);
    text(336u, 60u + ROW_H, "Click a window to focus/drag it, ESC exits",
         C_YELLOW, C_DESKTOP);

    for (i = 0; i < (unsigned)NAPPS; ++i)
        draw_icon((int)i, (int)i == gSel);

    /* windows bottom-to-top; the focused one is last, i.e. on top */
    for (i = 0; i < (unsigned)gNWin; ++i)
        draw_window((int)i);

    draw_taskbar();
    if (gMenuOpen) draw_start_menu();
    /* software cursor at the live kernel-published PS/2 position */
    draw_cursor(gSh->mouse_x, gSh->mouse_y);
    mark_repaint();   /* count this frame while gN still holds its cmd total */
    flush();          /* ONE exec (<=640 cmds): no half-painted frame is shown */
}

/* ---- trusted slow exec 268 (SvcMgrLaunch) ------------------------------ */
typedef struct Desc { unsigned type_length; const char* ptr; } Desc;
static char s_path[32];
static char s_empty[] = "";

static int svc_launch(const struct App* app)
{
    unsigned n = 0;
    const char* img = app->image;
    s_path[n++] = '\\';
    while (*img && n < sizeof(s_path) - 1u) { s_path[n++] = *img++; }
    s_path[n] = 0;
    Desc p = { 0x10000000u | n, s_path };   /* TDesC8: "\NAME.E32" */
    Desc c = { 0x10000000u, s_empty };      /* empty command line */
    return exec2(268u, (unsigned)&p, (unsigned)&c);
}

static void add_window(const struct App* app, int ret)
{
    unsigned n = 0;
    int i;

    if (gNWin >= MAXWINS)
        {
        /* drop the bottom window to make room */
        for (i = 0; i < gNWin - 1; ++i) gWins[i] = gWins[i + 1];
        --gNWin;
        }
    i = gNWin++;
    gWins[i].w = 480;
    gWins[i].h = 152;
    gWins[i].x = 360 + (i % 4) * 24;
    gWins[i].y = 120 + (i % 4) * 24;
    gWins[i].title = app->label;
    gWins[i].tcol = app->color;
    n = append(gWins[i].l0, 0, "Launched ");
    n = append(gWins[i].l0, n, app->image);
    gWins[i].l0[n] = 0;
    n = append(gWins[i].l1, 0, "close: X/C  r=");
    n = dec(gWins[i].l1, n, (unsigned)ret);
    gWins[i].l1[n] = 0;
}

static void launch(int idx)
{
    const struct App* app = &kApps[idx];
    char line[128];
    unsigned n = 0;
    int ret;

    /* Erase our cursor before the launched app takes over the framebuffer /
     * console; a text-only app may not clear the LFB itself. */
    if (gCurX >= 0)
        {
        gN = 0;
        rect((unsigned)gCurX, (unsigned)gCurY, 15u, 15u, C_DESKTOP);
        flush();
        gCurX = -1; gCurY = -1;
        }

    ret = svc_launch(app);

    n = append(line, 0, "DT: launched ");
    n = append(line, n, app->label);
    n = append(line, n, " r=");
    n = dec(line, n, (unsigned)ret);
    line[n] = 0;
    marker(line);

    add_window(app, ret);
    if (ret <= 0)
        {
        n = append(line, 0, "DT: fallback ");
        n = append(line, n, app->label);
        n = append(line, n, " (use shell)");
        line[n] = 0;
        marker(line);
        }
}

/* ---- window operations ------------------------------------------------- */
static void raise_window(int idx)
{
    struct Win tmp;
    while (idx < gNWin - 1)
        {
        tmp = gWins[idx];
        gWins[idx] = gWins[idx + 1];
        gWins[idx + 1] = tmp;
        ++idx;
        }
}
static void close_window(int idx)
{
    marker("DT: close window");
    for (; idx < gNWin - 1; ++idx) gWins[idx] = gWins[idx + 1];
    if (gNWin > 0) --gNWin;
    gDragWin = -1;
}

/* ---- input ------------------------------------------------------------- */
static int read_key(void)
{
    int c = con_tryread();
    int i, j, d;

    if (c != 27) return c;
    for (i = 0; i < 8; ++i)
        {
        sleep_ms(10);
        d = con_tryread();
        if (d < 0) continue;
        if (d != '[') return 27;
        for (j = 0; j < 8; ++j)
            {
            sleep_ms(10);
            d = con_tryread();
            if (d < 0) continue;
            if      (d == 'A') return 'w';
            else if (d == 'B') return 's';
            else if (d == 'C') return 'd';
            else if (d == 'D') return 'a';
            else return 27;
            }
        return 27;
        }
    return 27;
}

static int move_selection(int key)
{
    int row = gSel / 2, col = gSel % 2;
    if      (key == 'w' || key == 'W') row = (row + 3) % 4;
    else if (key == 's' || key == 'S') row = (row + 1) % 4;
    else if (key == 'a' || key == 'A') col = (col + 1) % 2;
    else if (key == 'd' || key == 'D') col = (col + 1) % 2;
    else return gSel;
    return row * 2 + col;
}

static void emit_select(void)
{
    char line[64];
    unsigned n = append(line, 0, "DT: select ");
    n = append(line, n, kApps[gSel].label);
    line[n] = 0;
    marker(line);
}

/* Returns 1 when anything on screen changed. */
static int handle_click(int mx, int my)
{
    unsigned H = (unsigned)gSh->fb_h;
    unsigned y0 = H - TASKBAR_H;
    unsigned x, y, w, h, row_y;
    int i, k;

    /* Start button (taskbar bottom-left) */
    if (in_rect(mx, my, 8u, y0 + 4u, 96u, 36u))
        {
        gMenuOpen = !gMenuOpen;
        return 1;
        }

    if (gMenuOpen)
        {
        menu_geom(&x, &y, &w, &h);
        if (in_rect(mx, my, x, y, w, h))
            {
            for (i = 0; i < NAPPS; ++i)
                {
                row_y = y + 28u + (unsigned)i * 24u;
                if (in_rect(mx, my, x + 4u, row_y, w - 8u, 22u))
                    {
                    gMenuSel = i;
                    gMenuOpen = 0;
                    launch(i);
                    return 1;
                    }
                }
            return 0;   /* clicked the menu header/background */
            }
        gMenuOpen = 0;  /* click outside the popup closes it */
        }

    /* desktop icons */
    for (i = 0; i < NAPPS; ++i)
        {
        icon_geom(i, &x, &y);
        if (in_rect(mx, my, x, y, ICON_W, ICON_H))
            {
            gSel = i;
            emit_select();
            launch(i);
            return 1;
            }
        }

    /* windows, topmost first */
    for (k = gNWin - 1; k >= 0; --k)
        {
        struct Win* win = &gWins[k];
        unsigned wx = (unsigned)win->x, wy = (unsigned)win->y;
        unsigned ww = (unsigned)win->w, wh = (unsigned)win->h;
        if (!in_rect(mx, my, wx - 2u, wy - 2u, ww + 4u, wh + 4u)) continue;
        raise_window(k);
        k = FOCUS;                     /* index after raising */
        win = &gWins[k];
        if (in_rect(mx, my, wx + ww - 26u, wy + 2u, 22u, 20u))
            { close_window(k); return 1; }
        if (in_rect(mx, my, wx, wy, ww, 24u))
            {
            gDragWin = k;
            gDragDX = mx - win->x;
            gDragDY = my - win->y;
            }
        return 1;
        }
    return 0;
}

static int handle_key(int key)
{
    int next;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned W = (unsigned)gSh->fb_w;

    if (key == 27)
        {
        if (gMenuOpen) { gMenuOpen = 0; return 1; }
        return -1;                       /* exit to shell */
        }
    if (gMenuOpen)
        {
        if (key == 'w' || key == 'W') gMenuSel = (gMenuSel + NAPPS - 1) % NAPPS;
        else if (key == 's' || key == 'S') gMenuSel = (gMenuSel + 1) % NAPPS;
        else if (key == '\n' || key == '\r' || key == ' ')
            { int s = gMenuSel; gMenuOpen = 0; launch(s); }
        else return 0;
        return 1;
        }
    if (key == 'm' || key == 'M')
        { gMenuOpen = 1; gMenuSel = gSel; return 1; }
    if (key == '\n' || key == '\r' || key == ' ')
        { launch(gSel); return 1; }
    if (key == 'c' || key == 'C')
        { if (gNWin > 1) close_window(FOCUS); return 1; }
    if (key == '\t' && gNWin > 1)
        { raise_window(0); return 1; }

    next = move_selection(key);
    if (next != gSel) { gSel = next; emit_select(); return 1; }

    /* keyboard nudge of the focused window with I/J/K/L */
    if (FOCUS >= 0 && (key == 'i' || key == 'I' || key == 'j' ||
                       key == 'J' || key == 'k' || key == 'K' || key == 'l'))
        {
        struct Win* win = &gWins[FOCUS];
        if      (key == 'i' || key == 'I') win->y -= 8;
        else if (key == 'k' || key == 'K') win->y += 8;
        else if (key == 'j' || key == 'J') win->x -= 8;
        else                               win->x += 8;
        if (win->x < 0) win->x = 0;
        if (win->y < 52) win->y = 52;
        if (win->x + win->w > (int)W) win->x = (int)W - win->w;
        if (win->y + win->h > (int)(H - TASKBAR_H))
            win->y = (int)(H - TASKBAR_H) - win->h;
        return 1;
        }
    return 0;
}

/* ---- entry point ------------------------------------------------------- */
int app_main(void)
{
    char line[96];
    unsigned n, shb, buttons, seq, dseq, now_sec;
    int changed, key, idle, clicked, clock_tick, quit = 0;

    gFbBase = (unsigned)exec2(QE_EXEC_GUI_FBBASE, 0, 0);
    gFbSize = (unsigned)exec2(QE_EXEC_GUI_FBSIZE, 0, 0);
    n = append(line, 0, "DT: fb=");
    n = hex32(line, n, gFbBase);
    n = append(line, n, " bytes=");
    n = dec(line, n, gFbSize);
    line[n] = 0;
    marker(line);

    shb = (unsigned)exec2(QE_EXEC_GUI_DRAW, 0, 0);
    if (shb == 0u || shb == (unsigned)-1)
        {
        marker("DT: FAIL no shared buffer (exec 251)");
        return 1;
        }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->version != GUI_VERSION)
        {
        marker("DT: FAIL shared magic");
        return 1;
        }
    if (gSh->fb_w <= 0 || gSh->fb_h <= 0)
        {
        marker("DT: FAIL framebuffer geometry");
        return 1;
        }

    if (con_acquire() < 0)
        {
        marker("DT: console busy");
        return 1;
        }

    /* seed the welcome window */
    gNWin = 1;
    gWins[0].x = 360; gWins[0].y = 100;
    gWins[0].w = 480; gWins[0].h = 152;
    gWins[0].title = "Welcome to EKA2 PC OS";
    gWins[0].tcol = C_TITLE;
    n = append(gWins[0].l0, 0, "PE2 E32 window manager.");
    gWins[0].l0[n] = 0;
    n = append(gWins[0].l1, 0, "Click icons or WASD + Enter.");
    gWins[0].l1[n] = 0;

    gSel = 0; gMenuOpen = 0; gMenuSel = 0;
    gDragWin = -1;
    gPrevButtons = gSh->buttons;
    gPrevSeq = gSh->button_seq;
    gLastSec = -1;

    draw_full();
    marker("DT: DESKTOP READY");
    n = append(line, 0, "DT: icons=");
    n = dec(line, n, (unsigned)NAPPS);
    line[n] = 0;
    marker(line);
    emit_select();

    /* 90 s idle budget at 30 ms/poll: a forgotten desktop cannot wedge input. */
    for (idle = 0; idle < 3000 && !quit; ++idle)
        {
        changed = 0;
        buttons = gSh->buttons;
        seq = gSh->button_seq;
        dseq = seq - gPrevSeq;
        /* A click is a fresh press (rising edge), or a whole press+release
         * pair that happened between two 30 ms polls (seq jumped by >=2). */
        clicked = (buttons != 0u && gPrevButtons == 0u) ||
                  (dseq >= 2u && buttons == 0u);
        if (clicked)
            {
            changed |= handle_click(gSh->mouse_x, gSh->mouse_y);
            idle = 0;
            }
        if (gDragWin >= 0)
            {
            if (buttons != 0u)
                {
                struct Win* win = &gWins[gDragWin];
                win->x = gSh->mouse_x - gDragDX;
                win->y = gSh->mouse_y - gDragDY;
                if (win->x < 0) win->x = 0;
                if (win->y < 52) win->y = 52;
                if (win->x + win->w > gSh->fb_w) win->x = gSh->fb_w - win->w;
                if (win->y + win->h > (int)gSh->fb_h - (int)TASKBAR_H)
                    win->y = (int)gSh->fb_h - (int)TASKBAR_H - win->h;
                changed = 1;
                }
            else gDragWin = -1;
            }
        gPrevButtons = buttons;
        gPrevSeq = seq;

        /* Track the kernel-published PS/2 position every frame.  The ISR
         * writes gSh->mouse_x/y straight into gui_shared, so no DRAW exec is
         * needed to observe movement; a full redraw makes our sprite follow.
         * Calling exec 251 each time also keeps the kernel crosshair fresh. */
        if ((int)gSh->mouse_x != gCurX || (int)gSh->mouse_y != gCurY)
            changed = 1;
        if (gSh->mouse_x != gRepX || gSh->mouse_y != gRepY)
            {
            char ml[64];
            unsigned mn = append(ml, 0, "DT: mouse x=");
            mn = dec(ml, mn, (unsigned)gSh->mouse_x);
            mn = append(ml, mn, " y=");
            mn = dec(ml, mn, (unsigned)gSh->mouse_y);
            ml[mn] = 0;
            marker(ml);
            gRepX = (int)gSh->mouse_x;
            gRepY = (int)gSh->mouse_y;
            }

        key = read_key();
        if (key >= 0)
            {
            int r = handle_key(key);
            if (r < 0) { quit = 1; break; }
            changed |= r;
            idle = 0;
            }
        /* Clock tick is a CHANGE, but only of the 8 clock glyphs.  Painting
         * those in a tiny partial list avoids the full-screen GC_CLEAR that
         * made the whole desktop flash once per second. */
        now_sec = (unsigned)time_now();
        clock_tick = ((int)now_sec != gLastSec);
        if (clock_tick) gLastSec = (int)now_sec;
        if (changed) draw_full();
        else if (clock_tick) draw_clock_only();
        sleep_ms(30);
        }

    marker(quit ? "DT: DONE" : "DT: TIMEOUT");
    con_release();
    return 0;
}

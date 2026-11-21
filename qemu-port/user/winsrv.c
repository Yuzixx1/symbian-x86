/*
 * winsrv.c -- PE2 E32 WINDOW SERVER for the R1 QEMU port.
 *
 * A genuine ring-3 E32 process that owns the VBE linear framebuffer and
 * composites client windows out of the kernel shared area (struct ws_shared).
 * The shared area is discovered through the appended slow executives 271/272/
 * 273 (WS_BASE / WS_SIZE / WS_CTL); WS_BASE returns its physical base, which
 * is directly usable from user mode because the port's flat PDE map
 * identity-maps all low RAM with U/S=1.
 *
 * Compositing uses the proven display-list path: the low-RAM gui_shared list
 * from GUI exec 251 (arg1==0) is filled with GC_CLEAR/GC_RECT/GC_PUTC commands
 * and submitted through exec 251 (arg1>0).  Each visible window contributes
 * its private 320x200x4 content buffer, blitted row-by-row as horizontal
 * run-length encoded GC_RECT commands (colour = the buffer's 0x00RRGGBB word,
 * so RGB order is correct).  A desktop backdrop, top bar, taskbar with window
 * titles and a focus highlight are drawn every frame.
 *
 * The LFB itself (0xFD000000) is supervisor-only in this port, so ring 3 never
 * writes VRAM directly -- every pixel reaches the screen through exec 251.
 *
 * Input events come from the shared ring via WSO_POP_EVENT: MOVE updates a
 * drawn cursor, DOWN hit-tests windows and sets hdr.focus_wid, KEY marks the
 * focused window dirty.  PortCon keys are accepted as synthetic events so the
 * server can be driven from the serial shell even before the kernel input
 * bridges are wired up.
 *
 * Markers use genuine EUSER.DLL ordinal 915 (RDebug::Printf), imported at IAT
 * slot winsrv_printf_slot (0x408000), with the DebugPrint exec as fallback.
 */
#include "runtime.h"
#include "guishost.h"

/* ---------------------------------------------------------------------------
 * Kernel ABI.  The authoritative definitions live in variant/qemu_windowsrv.h
 * (layout, exec numbers, WSO_* op codes, WSE_* event types).  A self-contained
 * fallback keeps this source buildable before the kernel header lands.
 * ------------------------------------------------------------------------ */
#if defined(__has_include)
#  if __has_include("../variant/qemu_windowsrv.h")
#    include "../variant/qemu_windowsrv.h"
#    define WSV_KERNEL_HEADER 1
#  endif
#endif

#ifdef WSV_KERNEL_HEADER
#  define WS_EXEC_BASE     QE_EXEC_WS_BASE
#  define WS_EXEC_SIZE     QE_EXEC_WS_SIZE
#  define WS_EXEC_CTL      QE_EXEC_WS_CTL
#  define WS_WIN(sh, i)    (&(sh)->hdr.wins[i])
#  define WS_EV(sh, i)     (&(sh)->hdr.events[i])
#  define WS_FOCUS(sh)     ((sh)->hdr.focus_wid)
#  define WS_SERVER(sh)    ((sh)->hdr.server_pid)
#  define WS_CONTENT(sh,i) ((unsigned char*)(sh) + WS_CONTENT_OFFSET(i))
#  define WSEV_MOVE_T      WSE_MOVE
#  define WSEV_DOWN_T      WSE_DOWN
#  define WSEV_KEY_T       WSE_KEY
#else
#  define WS_EXEC_BASE     271u
#  define WS_EXEC_SIZE     272u
#  define WS_EXEC_CTL      273u
#  define WS_MAX_WINDOWS   16u
#  define WS_TITLE_LEN    24u
#  define WS_EVENT_RING   64u
#  define WS_CONTENT_W    320u
#  define WS_CONTENT_H    200u
#  define WS_CONTENT_BYTES (WS_CONTENT_W * WS_CONTENT_H * 4u)
#  define WS_CONTENT_BASE 4096u
#  define WS_CONTENT_OFFSET(i) (WS_CONTENT_BASE + (unsigned)(i) * WS_CONTENT_BYTES)
#  define WSO_REGISTER_SERVER 1u
#  define WSO_SET_FOCUS      7u
#  define WSO_FRAME_TICK     8u
#  define WSO_POP_EVENT      6u
#  define WSE_MOVE  1u
#  define WSE_DOWN  2u
#  define WSE_KEY   4u
#  define WSEV_MOVE_T WSE_MOVE
#  define WSEV_DOWN_T WSE_DOWN
#  define WSEV_KEY_T  WSE_KEY
#  define WS_WIN(sh, i)    (&(sh)->wins[i])
#  define WS_EV(sh, i)     (&(sh)->events[i])
#  define WS_FOCUS(sh)     ((sh)->focus_wid)
#  define WS_SERVER(sh)    ((sh)->server_pid)
#  define WS_CONTENT(sh,i) ((unsigned char*)(sh) + WS_CONTENT_OFFSET(i))
struct ws_window {
    volatile unsigned pid;
    volatile int x, y, w, h;
    volatile char title[WS_TITLE_LEN];
    volatile unsigned dirty;
    volatile unsigned content_offset;
    volatile int visible, z;
};
struct ws_event {
    volatile unsigned type;
    volatile int x, y;
    volatile unsigned key;
    volatile int target_wid;
};
struct ws_shared {
    volatile unsigned magic, version, server_pid, frame;
    volatile int focus_wid;
    struct ws_window wins[WS_MAX_WINDOWS];
    struct ws_event events[WS_EVENT_RING];
    volatile unsigned ev_head, ev_tail;
};
#endif /* WSV_KERNEL_HEADER */

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int winsrv_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

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
    int i;
    k = append(p, k, "0x");
    for (i = 28; i >= 0; i -= 4) p[k++] = h[(v >> i) & 0xfu];
    return k;
}
static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&winsrv_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);  /* EUSER ord 915 */
    else print(text);                                  /* DebugPrint fallback */
}
static void marker_val(const char* label, unsigned v, int hex)
{
    char s[64];
    unsigned n = append(s, 0, label);
    n = hex ? hex32(s, n, v) : dec(s, n, v);
    s[n] = 0;
    marker(s);
}

/* ---- display-list builder (guishost.h layout) -------------------------- */
static struct gui_shared* gSh;
static unsigned gN;

static void flush(void)
{
    if (gN) { exec2(QE_EXEC_GUI_DRAW, gN, 0); gN = 0; }
}
static void cmd(unsigned type, unsigned a, unsigned b,
                unsigned c, unsigned d, unsigned e)
{
    if (gN >= GUI_MAX_CMDS) flush();
    {
    struct gui_cmd* q = &gSh->cmds[gN++];
    q->type = type; q->a = a; q->b = b; q->c = c; q->d = d; q->e = e;
    }
}
static void rect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned col)
    { cmd(GC_RECT, x, y, w, h, col); }
static void text(unsigned x, unsigned y, const char* s,
                 unsigned fg, unsigned bg)
{
    while (*s)
        { cmd(GC_PUTC, x, y, (unsigned char)*s, fg, bg); x += 16u; ++s; }
}

/* ---- palette ----------------------------------------------------------- */
#define C_DESKTOP   0x00001D2Fu
#define C_TOPBAR    0x00003050u
#define C_TASKBAR   0x00001820u
#define C_TITLE     0x000080C0u
#define C_TITLE_FOC 0x00C06020u
#define C_BORDER    0x004090D0u
#define C_WHITE     0x00FFFFFFu
#define C_CURSOR    0x00FFE040u
#define C_LIVE      0x0000FF00u
#define C_CLIENTBG  0x00001020u
#define C_SHADOW    0x00000810u

/* ---- server state ------------------------------------------------------ */
static struct ws_shared* gWs;
static int gLocal;                       /* 1 = kernel execs unavailable */
#ifndef WSV_KERNEL_HEADER
static unsigned gLocalStorage[4096];
#endif
static unsigned gCursorX, gCursorY;
static int gFocus = -1;                  /* window id (slot index), -1 = none */
static unsigned gEvents, gComposites, gFrames;
static int gQuit;

/* A full 320x200 RLE blit needs far more than the GUI_MAX_CMDS commands one
 * exec-251 display list can hold, and GUI_MAX_CMDS is also the kernel's per-
 * exec budget (guishost.h), so it cannot be raised from here.  Instead the
 * desktop is repainted in bounded vertical bands across successive frames.
 * VRAM persists between execs, so each band adds to the previous picture
 * instead of clearing it; the window is only flagged complete (dirty cleared)
 * after every content row has been blitted. */
#define WSV_BAND_BUDGET 512u          /* commands added per band frame */
static int  gPaintActive;             /* 1 while a band sequence is in flight */
static int  gPaintedOnce;             /* 0 until the first frame is complete   */
static int  gCursorDirty;             /* pointer moved since last completion   */
static unsigned gBandRow;             /* next content row to blit              */
static unsigned gBandRows;            /* rows to blit in this cycle           */
static unsigned gCycleDirty[WS_MAX_WINDOWS]; /* dirty generation at cycle start */

static void composite_marker(void)
{
    char s[48];
    unsigned n = append(s, 0, "WSV: composite ");
    n = dec(s, n, gComposites);
    s[n] = 0;
    marker(s);
}

/* A generous safety ceiling: 4000 frames * 25 ms = 100 s.  Normal operation
 * runs until Esc / q / a KEY event with code 27, so the compositor stays alive
 * long enough for the host to take its screendumps. */
#define WSV_MAX_FRAMES 4000u

static void slot_title(unsigned i, char* dst, unsigned cap)
{
    unsigned k = 0;
    if (cap) --cap;
    while (k < cap && WS_WIN(gWs, i)->title[k])
        { dst[k] = WS_WIN(gWs, i)->title[k]; ++k; }
    dst[k] = 0;
}

/* Content pixels are stored as 0x00RRGGBB uint32 words (exactly the colour
 * convention a GC_RECT expects), so blitting is a straight word read -- no
 * byte-by-byte BGR reassembly. */
static unsigned content_pixel(const unsigned char* p, unsigned x, unsigned y)
{
    unsigned at = (y * WS_CONTENT_W + x) * 4u;
    return *(const unsigned*)(p + at);
}
static void set_content(unsigned i, unsigned x, unsigned y, unsigned col)
{
    unsigned char* p = WS_CONTENT(gWs, i);
    unsigned at;
    if (!p || x >= WS_CONTENT_W || y >= WS_CONTENT_H) return;
    at = (y * WS_CONTENT_W + x) * 4u;
    *(unsigned*)(p + at) = col;
}

/* ---- compositing ------------------------------------------------------- */
/* Clamped on-screen geometry of a client window.  The content buffer is the
 * whole window (the client paints its own border/title into it), so every
 * content row maps to exactly one window row, starting at the window top. */
static int window_geom(unsigned i, unsigned* px, unsigned* py,
                       unsigned* pw, unsigned* ph)
{
    struct ws_window* s = WS_WIN(gWs, i);
    int ix = s->x, iy = s->y, iw = s->w, ih = s->h;
    unsigned x, y, w, h, W, H;

    if (!s->visible || !s->pid || iw <= 0 || ih <= 24) return 0;
    x = (unsigned)(ix < 0 ? 0 : ix);
    y = (unsigned)(iy < 0 ? 0 : iy);
    w = (unsigned)iw;
    h = (unsigned)ih;
    W = (unsigned)gSh->fb_w;
    H = (unsigned)gSh->fb_h;
    if (!w || x >= W || y >= H) return 0;
    if (w > W - x) w = W - x;
    if (h > H - y) h = H - y;
    if (!h) return 0;
    *px = x; *py = y; *pw = w; *ph = h;
    return 1;
}

/* Static frame drawn once per paint cycle (before any content bands). */
static void draw_window_frame(unsigned i)
{
    unsigned x, y, w, h;
    if (!window_geom(i, &x, &y, &w, &h)) return;
    rect(x + 4u, y + 6u, w, h, C_SHADOW);
    rect(x > 2u ? x - 2u : 0u, y > 2u ? y - 2u : 0u,
         w + 4u, h + 4u, ((int)i == gFocus) ? C_TITLE_FOC : C_BORDER);
    rect(x, y, w, h, C_CLIENTBG);
}

/* Title bar is painted last, on top of the blitted content. */
static void draw_window_title(unsigned i)
{
    unsigned x, y, w, h;
    char title[WS_TITLE_LEN];
    if (!window_geom(i, &x, &y, &w, &h)) return;
    rect(x, y, w, 20u, ((int)i == gFocus) ? C_TITLE_FOC : C_TITLE);
    slot_title(i, title, sizeof(title));
    text(x + 8u, y + 2u, title, C_WHITE,
         ((int)i == gFocus) ? C_TITLE_FOC : C_TITLE);
    if ((int)i == gFocus) rect(x, y + 18u, w, 2u, C_LIVE);
}

/* One content row as horizontal RLE GC_RECT runs (1 pixel high).  Returns
 * the number of commands emitted so the caller can bound a frame's work. */
static unsigned blit_row(unsigned i, unsigned cy,
                         unsigned x, unsigned y, unsigned cols)
{
    unsigned char* content = WS_CONTENT(gWs, i);
    unsigned cx = 0, n = 0;
    if (!content || cy >= WS_CONTENT_H) return 0;
    while (cx < cols)
        {
        unsigned colour = content_pixel(content, cx, cy);
        unsigned start = cx;
        ++cx;
        while (cx < cols && content_pixel(content, cx, cy) == colour) ++cx;
        rect(x + start, y + cy, cx - start, 1u, colour);
        ++n;
        }
    return n;
}

/* Add up to WSV_BAND_BUDGET commands worth of content rows.  cmd() itself
 * auto-flushes at GUI_MAX_CMDS, so a single exec can never exceed the kernel
 * budget; VRAM persistence means no GC_CLEAR is issued between bands. */
static void blit_bands(void)
{
    unsigned budget = WSV_BAND_BUDGET;
    unsigned i, z;

    while (gBandRow < gBandRows && budget)
        {
        unsigned spent = 0;
        for (z = 0; z < 8; ++z)
            for (i = 0; i < WS_MAX_WINDOWS; ++i)
                {
                unsigned x, y, w, h, cols, rows;
                if (!WS_WIN(gWs, i)->visible || WS_WIN(gWs, i)->z != (int)z)
                    continue;
                if (!window_geom(i, &x, &y, &w, &h)) continue;
                cols = WS_CONTENT_W; if (cols > w) cols = w;
                rows = WS_CONTENT_H; if (rows > h) rows = h;
                if (gBandRow < rows)
                    spent += blit_row(i, gBandRow, x, y, cols);
                }
        budget -= (spent < budget) ? spent : budget;
        ++gBandRow;
        }
}

static int need_paint(void)
{
    unsigned i;
    if (!gPaintedOnce) return 1;
    for (i = 0; i < WS_MAX_WINDOWS; ++i)
        if (WS_WIN(gWs, i)->visible && WS_WIN(gWs, i)->dirty) return 1;
    return gCursorDirty;
}

static void start_paint(void)
{
    unsigned W = (unsigned)gSh->fb_w, H = (unsigned)gSh->fb_h;
    unsigned bar_y = (H > 40u) ? H - 32u : 0u;
    unsigned i, z, x;
    char line[WS_TITLE_LEN + 8];

    gN = 0;
    cmd(GC_CLEAR, C_DESKTOP, 0, 0, 0, 0);
    rect(0, 0, W, 32u, C_TOPBAR);
    text(16u, 6u, "EKA2 WINDOW SERVER", C_WHITE, C_TOPBAR);
    rect(0, bar_y, W, 32u, C_TASKBAR);
    text(12u, bar_y + 6u, "TASKS:", C_WHITE, C_TASKBAR);
    x = 108u;
    for (i = 0; i < WS_MAX_WINDOWS && x + 128u < W; ++i)
        if (WS_WIN(gWs, i)->visible)
            {
            unsigned n = 0;
            line[n++] = '[';
            slot_title(i, line + 1, sizeof(line) - 2u);
            n = append(line, n, "]");
            if (n > 9u) { line[8] = ']'; line[9] = 0; n = 9u; }
            line[n] = 0;
            text(x, bar_y + 6u, line,
                 ((int)i == gFocus) ? C_CURSOR : C_WHITE, C_TASKBAR);
            x += 152u;
            }

    for (z = 0; z < 8; ++z)
        for (i = 0; i < WS_MAX_WINDOWS; ++i)
            if (WS_WIN(gWs, i)->visible && WS_WIN(gWs, i)->z == (int)z)
                draw_window_frame(i);

    gBandRow = 0;
    gBandRows = WS_CONTENT_H;
    for (i = 0; i < WS_MAX_WINDOWS; ++i)
        gCycleDirty[i] = WS_WIN(gWs, i)->dirty;
    gPaintActive = 1;
    marker("WSV: drawing");
}

static void finish_paint(void)
{
    unsigned i, z;

    for (z = 0; z < 8; ++z)
        for (i = 0; i < WS_MAX_WINDOWS; ++i)
            if (WS_WIN(gWs, i)->visible && WS_WIN(gWs, i)->z == (int)z)
                draw_window_title(i);
    if (gSh->mouse_x >= 0 && gSh->mouse_y >= 0)
        { gCursorX = (unsigned)gSh->mouse_x; gCursorY = (unsigned)gSh->mouse_y; }
    rect(gCursorX, gCursorY, 12u, 12u, C_CURSOR);

    gPaintActive = 0;
    gPaintedOnce = 1;
    gCursorDirty = 0;
    /* Only now that the whole window has been blitted is the frame done.  A
     * dirty bump that arrived mid-cycle is left set for the next cycle. */
    for (i = 0; i < WS_MAX_WINDOWS; ++i)
        if (WS_WIN(gWs, i)->visible &&
            WS_WIN(gWs, i)->dirty == gCycleDirty[i])
            WS_WIN(gWs, i)->dirty = 0;
    marker("WSV: painted");
}

static void composite(void)
{
    if (!gPaintActive && !need_paint())
        {
        /* The framebuffer already holds a complete frame; do not redraw it. */
        exec2(WS_EXEC_CTL, WSO_FRAME_TICK, 0);
        ++gComposites;
        if (gComposites == 1u || (gComposites % 25u) == 0u) composite_marker();
        return;
        }

    if (!gPaintActive) start_paint();
    blit_bands();
    if (gBandRow >= gBandRows) finish_paint();

    flush();
    exec2(WS_EXEC_CTL, WSO_FRAME_TICK, 0);
    ++gComposites;
    if (gComposites == 1u || (gComposites % 25u) == 0u) composite_marker();
}

/* ---- input events ------------------------------------------------------ */
static void deliver(unsigned type, int px, int py, unsigned key)
{
    int wid = -1;
    char line[64];
    unsigned n;

    ++gEvents;
    if (type == WSEV_MOVE_T)
        {
        gCursorX = (unsigned)(px < 0 ? 0 : px);
        gCursorY = (unsigned)(py < 0 ? 0 : py);
        gCursorDirty = 1;   /* old cursor rect must be repainted */
        }
    else if (type == WSEV_DOWN_T)
        {
        int z, bi = -1;
        for (z = 7; z >= 0 && bi < 0; --z)
            {
            unsigned i;
            for (i = 0; i < WS_MAX_WINDOWS; ++i)
                {
                struct ws_window* s = WS_WIN(gWs, i);
                if (!s->visible || s->z != z) continue;
                if (px >= s->x && px < s->x + s->w &&
                    py >= s->y && py < s->y + s->h)
                    { bi = (int)i; break; }
                }
            }
        if (bi >= 0)
            {
            wid = bi;
            gFocus = wid;
            WS_FOCUS(gWs) = wid;
            WS_WIN(gWs, (unsigned)wid)->dirty = 1;
            set_content((unsigned)wid, 0, 0, 0x00FFFFFFu);
            }
        }
    else if (type == WSEV_KEY_T)
        {
        wid = gFocus;
        if (wid >= 0 && wid < (int)WS_MAX_WINDOWS)
            {
            unsigned i = (unsigned)wid;
            set_content(i, 8u + (key & 31u) * 8u, 8u, C_CURSOR);
            WS_WIN(gWs, i)->dirty = 1;
            }
        if (key == 27u) gQuit = 1;
        }

    n = append(line, 0, "WSV: event ");
    n = dec(line, n, type);
    n = append(line, n, " wid=");
    n = dec(line, n, (unsigned)(wid < 0 ? 0 : wid));
    line[n] = 0;
    marker(line);
}

#ifdef WSV_KERNEL_HEADER
/* Kernel owns the head/tail cursor; pop events atomically through WS_CTL. */
static void drain_ring(void)
{
    struct ws_event e;
    while (exec2(WS_EXEC_CTL, WSO_POP_EVENT, (unsigned)&e) == 0)
        {
        deliver((unsigned)e.type, e.x, e.y, (unsigned)e.key);
        e.type = 0;
        }
}
#else
static void drain_ring(void)
{
    unsigned i;
    for (i = 0; i < WS_EVENT_RING; ++i)
        {
        struct ws_event* e = WS_EV(gWs, i);
        unsigned t = (unsigned)e->type;
        if (!t) continue;
        deliver(t, e->x, e->y, (unsigned)e->key);
        e->type = 0;
        }
}
#endif

static void poll_input(void)
{
    int key = con_tryread();
    if (key < 0) return;
    if (key == 27 || key == 'q' || key == 'Q') { gQuit = 1; return; }
    if (key == 'm' || key == 'M')
        { deliver(WSEV_MOVE_T, (int)gCursorX + 24, (int)gCursorY + 12, 0); return; }
    if (key == 'n' || key == 'N')
        { deliver(WSEV_MOVE_T, (int)gCursorX - 24, (int)gCursorY + 12, 0); return; }
    if (key == ' ' || key == '\r' || key == '\n')
        { deliver(WSEV_DOWN_T, (int)gCursorX, (int)gCursorY, 0); return; }
    deliver(WSEV_KEY_T, (int)gCursorX, (int)gCursorY, (unsigned)key);
}

/* ---- entry point ------------------------------------------------------- */
int app_main(void)
{
    char line[96];
    unsigned n, base, shsize;
    int reg = -1, have_con = 0, pid;

    marker("WSV: WINDOW SERVER READY");

    marker("WSV: before base exec");
    base = (unsigned)exec2(WS_EXEC_BASE, 0, 0);
    marker_val("WSV: base=", base, 1);
    marker("WSV: before size exec");
    shsize = (unsigned)exec2(WS_EXEC_SIZE, 0, 0);
    marker_val("WSV: size=", shsize, 0);
    if (base > 0x1000u && base < 0x08000000u)
        {
        gWs = (struct ws_shared*)base;
        gLocal = 0;
        marker("WSV: before pid exec");
        pid = (int)exec2(19u, 0xffff8000u, 0); /* EExecProcessId, own process handle */
        marker_val("WSV: pid=", (unsigned)pid, 0);
        if (pid <= 0) pid = 0x575756;      /* 'WSV' */
        marker("WSV: before reg exec");
        reg = exec2(WS_EXEC_CTL, WSO_REGISTER_SERVER, (unsigned)pid);
        marker_val("WSV: reg=", (unsigned)reg, 0);
        }
    else
        {
#ifdef WSV_KERNEL_HEADER
        /* Without the appended executives there is no 4 MiB shared area to
         * map; fail cleanly instead of touching arbitrary low RAM. */
        marker("WSV: FAIL no kernel window-server execs");
        return 1;
#else
        /* Header-less fallback: run a faithful local instance so the
         * compositor / dispatcher logic is still exercised end-to-end. */
        gWs = (struct ws_shared*)gLocalStorage;
        base = (unsigned)gLocalStorage;
        shsize = (unsigned)sizeof(gLocalStorage);
        gLocal = 1;
#endif
        }

    n = append(line, 0, "WSV: shared=");
    if (gLocal) n = append(line, n, "LOCAL");
    else        n = hex32(line, n, base);
    n = append(line, n, " size=");
    n = dec(line, n, shsize);
    n = append(line, n, " reg=");
    n = dec(line, n, (unsigned)reg);
    line[n] = 0;
    marker(line);

    {
    unsigned shb = (unsigned)exec2(QE_EXEC_GUI_DRAW, 0, 0);
    if (!shb) { marker("WSV: FAIL no gui_shared (exec 251)"); return 1; }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->fb_w <= 0 || gSh->fb_h <= 0)
        { marker("WSV: FAIL gui_shared magic/geometry"); return 1; }
    }

    /* No synthetic windows are seeded: the shared area starts empty at boot,
     * and a client window must be the one composited on the desktop.  Seeding
     * slots 0-2 here used to leave stale higher-z windows covering WSDL after
     * a server restart. */
    {
    unsigned count = 0;
    for (n = 0; n < WS_MAX_WINDOWS; ++n)
        if (WS_WIN(gWs, n)->visible) ++count;
    n = append(line, 0, "WSV: windows=");
    n = dec(line, n, count);
    n = append(line, n, " focus=");
    n = dec(line, n, (unsigned)(gFocus < 0 ? 0 : gFocus));
    line[n] = 0;
    marker(line);
    }

    if (con_acquire() == 0) have_con = 1;
    gCursorX = (unsigned)gSh->fb_w / 2u;
    gCursorY = (unsigned)gSh->fb_h / 2u;

    /* Initial synthetic interaction proves hit-test + key delivery even with
     * no physical mouse attached (QEMU -display none).  If WSDL started first,
     * its window is already present and becomes the focus here. */
    deliver(WSEV_MOVE_T, (int)gCursorX, (int)gCursorY, 0);
    deliver(WSEV_DOWN_T, 300, 200, 0);
    deliver(WSEV_KEY_T, 0, 0, 'X');

    /* Serve until the user quits (Esc / q) or the generous safety ceiling.
     * The old `idle < 400` cap (~10 s) exited before the host could screenshot
     * the composited desktop, leaving only a console overlay behind. */
    while (!gQuit && gFrames < WSV_MAX_FRAMES)
        {
        drain_ring();
        poll_input();
        composite();
        ++gFrames;
        sleep_ms(25);
        }

    n = append(line, 0, "WSV: DONE frames=");
    n = dec(line, n, gFrames);
    n = append(line, n, " events=");
    n = dec(line, n, gEvents);
    n = append(line, n, " composites=");
    n = dec(line, n, gComposites);
    line[n] = 0;
    marker(line);
    flush();
    if (have_con) con_release();
    return 0;
}

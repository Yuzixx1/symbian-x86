/*
 * guilib.c -- reusable PE2 GUI toolkit for the R1 QEMU port.
 *
 * Self-contained display-list transport (layout mirrors user/guishost.h):
 * exec 249 = LFB base, 250 = LFB size, 251(0) = address of struct gl_shared,
 * 251(n>0) = execute n commands and refresh the PS/2 mouse crosshair.  All
 * rendering is done in kernel context by variant/qemu_gui_exec.cpp; this file
 * never touches VRAM directly.  The PS/2 ISR publishes buttons/button_seq
 * straight into that shared struct, so gui_mouse() reports real click state.
 *
 * No hosted libc is used.  Serial markers are the caller's responsibility
 * (see guihello.c); this library is intentionally silent.
 */
#include "guilib.h"
#include "runtime.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

/* ---- transport definitions, kept private so guishost.h needs no changes -- */
#define QE_GL_FBBASE        249u
#define QE_GL_FBSIZE        250u
#define QE_GL_DRAW          251u

#define GL_MAGIC            0x47554931u   /* "GUI1" */
#define GL_VERSION          1u
#define GL_MAX_CMDS         640u   /* must mirror GUI_MAX_CMDS (shared ABI layout) */

#define GC_CLEAR            0u
#define GC_RECT             1u
#define GC_PUTC             2u

struct gl_cmd {
    volatile unsigned type;
    volatile unsigned a, b, c, d, e;
};

struct gl_shared {
    volatile unsigned magic;
    volatile unsigned version;
    volatile unsigned count;
    volatile unsigned draw_calls;
    volatile int      fb_w, fb_h;
    volatile int      mouse_x, mouse_y;
    volatile unsigned buttons;        /* kernel: PS/2 mask 1=L 2=R 4=M       */
    struct gl_cmd     cmds[GL_MAX_CMDS];
    volatile unsigned button_seq;     /* kernel: increments on button change */
};

/* ---- state -------------------------------------------------------------- */
static struct gl_shared* sSh;
static struct gl_cmd     sBack[GL_MAX_CMDS];   /* local (back) command list */
static unsigned          sN;
static int               sReady;
static int               sLeased;
static unsigned          sFbBase, sFbSize;
static int               sEscState;            /* ANSI arrow decoder state  */

#define GL_MAX_WINDOWS      16
static struct { int x, y, w, h; } sWins[GL_MAX_WINDOWS];
static int sWinCount;

static unsigned gl_strlen(const char* s)
{
    unsigned n = 0;
    while (s && s[n]) ++n;
    return n;
}

static void gl_cmd(unsigned type, unsigned a, unsigned b,
                   unsigned c, unsigned d, unsigned e)
{
    if (!sReady || sN >= GL_MAX_CMDS) return;
    struct gl_cmd* q = &sBack[sN++];
    q->type = type; q->a = a; q->b = b; q->c = c; q->d = d; q->e = e;
}

/* ---- setup / teardown --------------------------------------------------- */
int gui_init(void)
{
    int shb;

    sSh = 0; sN = 0; sReady = 0; sLeased = 0;
    sEscState = 0; sWinCount = 0;
    sFbBase = (unsigned)exec2(QE_GL_FBBASE, 0, 0);
    sFbSize = (unsigned)exec2(QE_GL_FBSIZE, 0, 0);
    shb = exec2(QE_GL_DRAW, 0, 0);
    if (shb <= 0) return -1;
    sSh = (struct gl_shared*)shb;
    if (sSh->magic != GL_MAGIC || sSh->version != GL_VERSION) return -2;
    sReady = 1;
    /* Input lease: the kernel shell stops polling while we hold it. */
    sLeased = (con_acquire() == 0);
    return 0;
}

void gui_shutdown(void)
{
    if (sLeased) { con_release(); sLeased = 0; }
    sReady = 0;
}

int gui_fb_width(void)  { return sSh ? (int)sSh->fb_w : 0; }
int gui_fb_height(void) { return sSh ? (int)sSh->fb_h : 0; }

/* ---- input -------------------------------------------------------------- */
int gui_key(void)
{
    for (;;) {
        int c = con_tryread();
        int printable;

        if (c < 0) {
            /* ESC with no ANSI follower is a standalone Escape.  State 2
               (ESC [ seen) survives until the final byte arrives. */
            if (sEscState == 1) { sEscState = 0; return GUI_KEY_ESC; }
            return GUI_KEY_NONE;
        }
        printable = (c >= 32 && c <= 126);

        if (sEscState == 0) {
            if (c != 27) {
                if (c == '\r' || c == '\n') return GUI_KEY_ENTER;
                if (c == 8 || c == 127) return GUI_KEY_BACKSPACE;
                return printable ? c : GUI_KEY_NONE;
            }
            /* ESC: give the two following ANSI bytes a moment to arrive on
               the serial line before deciding this is a standalone Escape. */
            sEscState = 1;
            sleep_us(1500);
            continue;
        }
        if (sEscState == 1) {
            if (c == '[' || c == 'O') { sEscState = 2; continue; }
            sEscState = 0;
            if (c == 27) return GUI_KEY_ESC;
            return printable ? c : GUI_KEY_NONE;
        }
        /* sEscState == 2: final byte of ESC [ X. */
        sEscState = 0;
        switch (c) {
        case 'A': return GUI_KEY_UP;
        case 'B': return GUI_KEY_DOWN;
        case 'C': return GUI_KEY_RIGHT;
        case 'D': return GUI_KEY_LEFT;
        default:  return GUI_KEY_NONE;
        }
    }
}

void gui_mouse(int* x, int* y, int* buttons)
{
    if (x) *x = sSh ? (int)sSh->mouse_x : 0;
    if (y) *y = sSh ? (int)sSh->mouse_y : 0;
    /* Published by the IRQ12 PS/2 ISR directly into the shared struct. */
    if (buttons) *buttons = sSh ? (int)sSh->buttons : 0;
}

/* ---- primitives --------------------------------------------------------- */
void gui_clear(uint32_t color)
{
    if (!sReady) return;
    sN = 0;                       /* start a new back-buffer frame */
    gl_cmd(GC_CLEAR, color, 0, 0, 0, 0);
}

void gui_rect(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0) return;
    gl_cmd(GC_RECT, (unsigned)x, (unsigned)y,
           (unsigned)w, (unsigned)h, color);
}

void gui_hline(int x, int y, int w, uint32_t color)
{
    gui_rect(x, y, w, 1, color);
}

void gui_vline(int x, int y, int h, uint32_t color)
{
    gui_rect(x, y, 1, h, color);
}

void gui_text(int x, int y, const char* s, uint32_t fg, uint32_t bg)
{
    unsigned i, n;
    if (!s) return;
    n = gl_strlen(s);
    for (i = 0; i < n; ++i)
        gl_cmd(GC_PUTC, (unsigned)(x + (int)i * GUI_CELL_W), (unsigned)y,
               (unsigned char)s[i], fg, bg);
}

/* ---- widgets ------------------------------------------------------------ */
int gui_window(int x, int y, int w, int h, const char* title,
               int border, int fill)
{
    int id;

    if (!sReady || w < 24 || h < 24) return -1;
    if (sWinCount >= GL_MAX_WINDOWS) return -1;
    if (fill)  gui_rect(x, y, w, h, GUI_BLUE);
    if (border) {
        gui_rect(x, y, w, 2, GUI_LIGHTGRAY);
        gui_rect(x, y + h - 2, w, 2, GUI_LIGHTGRAY);
        gui_rect(x, y, 2, h, GUI_LIGHTGRAY);
        gui_rect(x + w - 2, y, 2, h, GUI_LIGHTGRAY);
    }
    gui_rect(x + 2, y + 2, w - 4, GUI_ROW_PITCH, GUI_TITLEBAR);
    gui_text(x + 5, y + 4, title ? title : "", GUI_WHITE, GUI_TITLEBAR);
    id = sWinCount++;
    sWins[id].x = x; sWins[id].y = y; sWins[id].w = w; sWins[id].h = h;
    return id;
}

void gui_button(int win, int x, int y, int w, int h,
                const char* label, int focused)
{
    unsigned n;
    int tx, ty, bw;

    (void)win;                    /* geometry is explicit; id is validity only */
    if (w < 16 || h < 16) return;
    gui_rect(x, y, w, h, focused ? GUI_TITLEBAR : GUI_GRAY);
    gui_rect(x, y, w, 2, focused ? GUI_YELLOW : GUI_LIGHTGRAY);
    gui_rect(x, y + h - 2, w, 2, GUI_LIGHTGRAY);
    gui_rect(x, y, 2, h, GUI_LIGHTGRAY);
    gui_rect(x + w - 2, y, 2, h, GUI_LIGHTGRAY);
    n = gl_strlen(label);
    bw = (int)n * GUI_CELL_W;
    tx = x + (w - bw) / 2;
    if (tx < x + 3) tx = x + 3;
    ty = y + (h - GUI_CELL_H) / 2;
    if (ty < y + 3) ty = y + 3;
    gui_text(tx, ty, label, focused ? GUI_YELLOW : GUI_WHITE,
             focused ? GUI_TITLEBAR : GUI_GRAY);
}

/* ---- publish ------------------------------------------------------------ */
void gui_present(void)
{
    unsigned i;
    if (!sReady || sN == 0) return;
    for (i = 0; i < sN; ++i) {
        const struct gl_cmd* src = &sBack[i];
        struct gl_cmd* dst = &sSh->cmds[i];
        dst->type = src->type; dst->a = src->a; dst->b = src->b;
        dst->c = src->c;       dst->d = src->d; dst->e = src->e;
    }
    (void)exec2(QE_GL_DRAW, sN, 0);   /* arg1>0: run list + refresh mouse */
}

int gui_pending_commands(void)
{
    return (int)sN;
}

/* ---- helpers ------------------------------------------------------------ */
uint32_t gui_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

int gui_msgbox(const char* title, const char* text)
{
    int W = gui_fb_width(), H = gui_fb_height();
    int w = 480, h = 132, x, y, key;
    unsigned line = 0, start = 0, i;
    const char* p;

    if (!sReady) return GUI_KEY_NONE;
    if (W > w) x = (W - w) / 2; else x = 8;
    if (H > h) y = (H - h) / 2; else y = 8;
    gui_clear(GUI_NAVY);
    gui_window(x, y, w, h, title ? title : "MESSAGE", 1, 1);
    p = text ? text : "";
    for (i = 0; p[i] && line < 4; ++i) {
        if (p[i] == '\n') {
            if (i > start)
                gui_text(x + 12, y + 30 + (int)line * GUI_ROW_PITCH,
                         p + start, GUI_WHITE, GUI_BLUE);
            start = i + 1;
            ++line;
        }
    }
    if (start && p[start])
        gui_text(x + 12, y + 30 + (int)line * GUI_ROW_PITCH,
                 p + start, GUI_WHITE, GUI_BLUE);
    gui_text(x + 12, y + h - 26, "PRESS ANY KEY", GUI_YELLOW, GUI_BLUE);
    gui_present();
    do {
        key = gui_key();
        if (!key) sleep_ms(30);
    } while (!key);
    return key;
}

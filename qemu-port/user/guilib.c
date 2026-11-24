/*
 * guilib.c -- reusable PE2 GUI toolkit for the R1 QEMU port.
 *
 * WINSRV_MIGRATION_PLAN.md chunk B: the backend is now the GENUINE EWSRV
 * client path from user/ws_e32_client.c (chunk A), #included below so this
 * library can issue the draw opcodes over the very same session and window.
 * The winsrv slow execs 249/250/251 are gone from this file and all its
 * consumers (guihello.c, dtedit.c, dtcalc.c):
 *
 *   session/window : ws_open() / ws_window_create() / ws_set_title() /
 *                    ws_screen_size() / ws_close()      -- chunk A code.
 *   draw (minimal) : EConsoleClearScreen + EConsoleSetCursorPosAbs +
 *                    EConsoleWrite (CWsSession ops 2/6/10 of
 *                    kernelhwsrv/kernel/eka/include/ws_std.h), sent through
 *                    the chunk-A session via ws_e32_send().  Implemented
 *                    locally here because ws_e32_client.c's ws_fill/
 *                    ws_text/ws_present are still stubs; chunk 2 lands the
 *                    RWindow/GC pixel path (GC_RECT chrome) on top.
 *   keyboard       : PortCon lease (con_acquire/con_tryread), unchanged.
 *   mouse          : gui_shared no longer exists without exec 251; PS/2
 *                    input into genuine EWSRV is the open blocker (plan
 *                    section 5), so gui_mouse* report zeros until the
 *                    EWSRV input-injection bridge lands.
 *
 * Rendering model: the back display list is kept (same command budget), so
 * gui_pending_commands() and the apps' command-budget guards still work.
 * gui_present() interprets the list against the console text grid: pixel
 * (x,y) maps to cell (x/GUI_CELL_W, y/GUI_ROW_PITCH) on a 64x42 grid;
 * GC_CLEAR -> EConsoleClearScreen (with bg colour), GC_RECT -> chunk 2's
 * ws_draw_fill_rect (filled cells), GC_PUTC runs coalesce into one
 * EConsoleSetTextColors + EConsoleSetCursorPosAbs + EConsoleWrite per line.
 *
 * No hosted libc is used.  Serial markers are the caller's responsibility
 * (see guihello.c); this library is intentionally silent.
 */
#include "guilib.h"
#include "runtime.h"

/* Chunk A client, included so the draw helpers below can use its session
 * (sSession) and ws_e32_send() directly; gui_init()/gui_shutdown() call the
 * public ws_* API from the same translation unit. */
#include "ws_e32_client.c"

/* One more genuine CWsSession opcode (kernelhwsrv .../ws_std.h) not yet in
 * the included client's enum: EConsoleClearScreen.  The draw ops 6/10/21,
 * the EPtrC/EPtr descriptor-type macros (WS_E32_EPTRC/WS_E32_EPTR) and the
 * palette helpers ws_e32_rgb_to_index()/ws_draw_fill_rect() all come from the
 * included chunk-2 ws_e32_client.c. */
#define GL_OP_CLEAR_SCREEN    2    /* EConsoleClearScreen                   */

/* Display-list opcodes (same numbering as the old shim ABI, so the
 * recorded back list keeps its meaning for gui_pending_commands()). */
#define GC_CLEAR            0u
#define GC_RECT             1u
#define GC_PUTC             2u

#define GL_MAX_CMDS         640u   /* same per-frame budget as before */

struct gl_cmd {
    volatile unsigned type;
    volatile unsigned a, b, c, d, e;
};

/* ---- state -------------------------------------------------------------- */
static struct gl_cmd     sBack[GL_MAX_CMDS];   /* local (back) command list */
static unsigned          sN;
static int               sReady;
static int               sLeased;
static int               sScrW, sScrH;         /* EConsoleScreenSize, px    */
static int               sEscState;            /* ANSI arrow decoder state  */

#define GL_MAX_WINDOWS      16
static struct { int x, y, w, h; } sWins[GL_MAX_WINDOWS];
static int sWinCount;

/* ws_e32_client.c's marker() is unused here (this library stays silent);
 * reference it once so -Wunused-function does not fire. */
static void (*volatile gl_keep_marker)(const char*) = marker;

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

/* ---- minimal draw over the chunk-A session -------------------------------
 * Each helper mirrors the proven TIpcArgs shapes of ws_e32_client.c:
 * TPckgC<TPoint> as EDesC8 for SetCursorPosAbs, TPtrC16 as EDesC16 for
 * Write, two plain TInts for SetTextColors (exactly what the genuine
 * RConsole in ewsrv/co_cli.cpp sends). */
static int gl_send_clear(void)
{
    struct ws_e32_ipcargs args;
    ws_e32_args0(&args);
    return ws_e32_send(GL_OP_CLEAR_SCREEN, &args);
}

static int gl_set_colors(uint32_t aFg, uint32_t aBg)
{
    struct ws_e32_ipcargs args;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)ws_e32_rgb_to_index(aFg);   /* fg, TInt slot */
    args.iArgs[1] = (int)ws_e32_rgb_to_index(aBg);   /* bg, TInt slot */
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_TEXT_COLORS, &args);
}

static int gl_send_cursor_abs(int aCol, int aRow)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 pckg;            /* TPckgC<TPoint>                  */
    struct ws_e32_point pt;
    pt.iX = aCol; pt.iY = aRow;
    pckg.iLength = WS_E32_EPTRC((unsigned)sizeof(pt));
    pckg.iPtr = (const unsigned char*)&pt;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&pckg;
    args.iFlags = (WS_E32_EDESC_C8 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_CURSOR_POS_ABS, &args);
}

static int gl_send_write(const unsigned short* aText, unsigned aLen)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC16 desc;           /* TPtrC16                         */
    desc.iLength = WS_E32_EPTRC(aLen);
    desc.iPtr = aText;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&desc;
    args.iFlags = (WS_E32_EDESC_C16 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_WRITE, &args);
}

/* Flush one coalesced text run: set its colours, position the cursor, then
 * write the line. */
static void gl_flush_run(unsigned short* run, unsigned* len,
                         int* row, int* col, uint32_t* fg, uint32_t* bg)
{
    if (*len) {
        (void)gl_set_colors(*fg, *bg);
        (void)gl_send_cursor_abs(*col, *row);
        (void)gl_send_write(run, *len);
        *len = 0;
    }
}

/* ---- session connect ------------------------------------------------------
 * Same RSessionBase::CreateSession import (ord 211) as ws_e32_client.c's
 * ws_open(), driven from this translation unit.  The full genuine name
 * "!E32WindowServer" is tried first; the boot-started EWSRV.E32 image
 * currently registers its name truncated to "!" (the _LIT 4-byte-char quirk
 * under g++ -m32 -- EWCLIENT's _L collapses the same way, see the exec128
 * trace), so on KErrNotFound we retry with "!". */
static int gl_connect_one(const void* aNameDesc)
{
    int r;
    sSession.iHandle = 0;               /* KNullHandle */
    r = e32_connect(&sSession, aNameDesc, &sWsVersion, WS_E32_MESSAGE_SLOTS);
    if (r != 0) { sSession.iHandle = 0; return r; }
    if (sSession.iHandle == 0) return -1;
    return 0;
}

static int gl_connect(void)
{
    static const struct ws_e32_desC16 fullName =
        { WS_E32_EPTRC(WS_E32_SERVER_NAME_LEN), sServerName };
    static const struct { unsigned iTypeLength; unsigned short iBuf[1]; } bang =
        { 1u, { '!' } };
    int r;
    if (sOpen) return 0;
    r = gl_connect_one(&fullName);
    if (r == -1)                        /* KErrNotFound: name truncated */
        r = gl_connect_one(&bang);
    if (r != 0) return r;
    sOpen = 1;
    sWin = -1;
    return 0;
}

/* ---- setup / teardown --------------------------------------------------- */
int gui_init(void)
{
    int r, w = 0, h = 0;

    sN = 0; sReady = 0; sLeased = 0;
    sEscState = 0; sWinCount = 0;
    sScrW = 1024; sScrH = 768;

    /* Genuine EWSRV session via gl_connect() below (the same CreateSession
     * EUSER import ws_e32_client.c's ws_open uses, plus the name fallback
     * that matches the boot-started server).  Return the raw error. */
    r = gl_connect();
    if (r != 0) return r;


    /* Full-grid console window: 64x42 cells cover the 1024x768 screen
     * (one cell = GUI_CELL_W x GUI_ROW_PITCH px of the toolkit grid). */
    r = ws_window_create(0, 0, 64, 42, "EKA2 GUI");
    if (r != 0) { ws_close(); return r ? r : -2; }

    /* EConsoleScreenSize: answers in console cells (128x76 here); used as a
     * session-op proof.  The toolkit grid stays the design 1024x768 (64x42
     * cells of GUI_CELL_W x GUI_ROW_PITCH), so gui_fb_* keep app layouts. */
    r = ws_screen_size(&w, &h);
    if (r != 0) { ws_close(); return r; }

    /* Input lease: the kernel shell stops polling while we hold it. */
    sLeased = (con_acquire() == 0);
    sReady = 1;
    return 0;
}

void gui_shutdown(void)
{
    if (sLeased) { con_release(); sLeased = 0; }
    if (sReady) ws_close();        /* EConsoleDestroy + RHandleBase::Close */
    sReady = 0;
}

int gui_fb_width(void)  { return sReady ? sScrW : 0; }
int gui_fb_height(void) { return sReady ? sScrH : 0; }

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
    /* No kernel gui_shared on the EWSRV path; the input-injection bridge
     * (plan section 5) is the open blocker, so report "no mouse" for now. */
    if (x) *x = 0;
    if (y) *y = 0;
    if (buttons) *buttons = 0;
}

void gui_mouse_full(int* x, int* y, unsigned* buttons, unsigned* seq)
{
    if (x) *x = 0;
    if (y) *y = 0;
    if (buttons) *buttons = 0;
    if (seq) *seq = 0;
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
    /* Recorded; painted as pixels by chunk 2 (RWindow/GC), no-op on the
     * console text path for now. */
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
    unsigned short run[128];       /* one coalesced text line, UTF-16 */
    unsigned runLen = 0;
    int runRow = -1, runCol = -1;
    uint32_t runFg = 0, runBg = 0;

    if (!sReady || sN == 0) return;
    for (i = 0; i < sN; ++i) {
        const struct gl_cmd* q = &sBack[i];
        if (q->type == GC_CLEAR) {
            gl_flush_run(run, &runLen, &runRow, &runCol, &runFg, &runBg);
            (void)gl_set_colors(GUI_WHITE, q->a);
            (void)gl_send_clear();     /* EConsoleClearScreen, bg = q->a */
            continue;
        }
        if (q->type == GC_RECT) {
            /* Filled rect in console cells via chunk 2's genuine
             * ws_draw_fill_rect (SetTextColors + cursor + space cells). */
            int col = (int)(q->a / (unsigned)GUI_CELL_W);
            int row = (int)(q->b / (unsigned)GUI_ROW_PITCH);
            int cw  = (int)(q->c / (unsigned)GUI_CELL_W);
            int ch  = (int)(q->d / (unsigned)GUI_ROW_PITCH);
            if (cw < 1) cw = 1;
            if (ch < 1) ch = 1;
            gl_flush_run(run, &runLen, &runRow, &runCol, &runFg, &runBg);
            (void)ws_draw_fill_rect(0, col, row, cw, ch, q->e);
            continue;
        }
        if (q->type == GC_PUTC) {
            int col = (int)(q->a / (unsigned)GUI_CELL_W);
            int row = (int)(q->b / (unsigned)GUI_ROW_PITCH);
            if (runLen && (row != runRow || col != runCol + (int)runLen ||
                           q->d != runFg || q->e != runBg))
                gl_flush_run(run, &runLen, &runRow, &runCol, &runFg, &runBg);
            if (!runLen) { runRow = row; runCol = col; runFg = q->d; runBg = q->e; }
            if (runLen < sizeof(run) / sizeof(run[0]))
                run[runLen++] = (unsigned short)(q->c & 0xffffu);
        }
    }
    gl_flush_run(run, &runLen, &runRow, &runCol, &runFg, &runBg);
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

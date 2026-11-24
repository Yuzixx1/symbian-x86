/*
 * dtedit.c -- a WINDOWED text editor for the R1 QEMU port (PE2 E32).
 *
 * This is a second real GUI desktop application, built on the reusable
 * guilib.c toolkit: a decorated "EKA2 Notepad" window on the VBE LFB, a
 * clickable toolbar (SAVE / CLEAR / QUIT), a live text area with a blinking
 * cursor, and genuine keyboard editing (type / Backspace / Enter / arrows).
 * Mouse clicks are detected from the pointer button word + change-sequence
 * (rising-edge/seq-jump logic, fed by guilib's gui_mouse_full) and hit-tested
 * against the toolbar, the title-bar close box and the text cell grid.
 *
 * File I/O uses PortFile IPC (file_save) exactly like textedit.c.  Markers go
 * through the genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf);
 * IAT slot dtedit_printf_slot lives at 0x408010 (the ws_e32_client slots
 * occupy 0x408000-0x40800c; textSize stays 0x8000).
 */
/* WINSRV_MIGRATION_PLAN chunk B: no winsrv display-list exec here
 * anymore -- pointer state comes from guilib's gui_mouse_full() on the
 * genuine EWSRV client path (zeros until the EWSRV input bridge lands,
 * plan section 5). */
#include "runtime.h"
#include "guilib.h"

extern unsigned int dtedit_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&dtedit_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);
    else print(text);
}

/* ---- tiny formatters (no hosted libc, no printf varargs) ---- */
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

/* ---- geometry ---------------------------------------------------------- */
#define WIN_X      12
#define WIN_Y      40
#define WIN_W      1000
#define WIN_H      520
#define TEXT_X     (WIN_X + 12)        /* 24  */
#define TEXT_Y     (WIN_Y + 84)        /* 124 */
#define TEXT_COLS  60
#define TEXT_LINES 20
#define TEXT_VIS   20
#define BTN_Y      (WIN_Y + 28)        /* 68  */
#define BTN_W      160
#define BTN_H      40
#define BTN_SAVE_X (WIN_X + 16)        /* 28  */
#define BTN_CLR_X  (WIN_X + 188)       /* 200 */
#define BTN_QUIT_X (WIN_X + 360)       /* 372 */
#define CLOSE_X    (WIN_X + WIN_W - 48)
#define CLOSE_Y    (WIN_Y + 4)
#define CLOSE_W    40
#define CLOSE_H    16

/* Extra key codes from our con_tryread decoder. */
#define DT_CTRL_S  140
#define DT_CTRL_L  141

static char  gLines[TEXT_LINES][TEXT_COLS + 1];
static int   gLineCount = 1;
static int   gRow = 0, gCol = 0;
static int   gChars = 0;
static int   gDirty = 1, gDone = 0;
static int   gBlink = 0, gLastBlink = -1;
static unsigned gPrevButtons = 0, gPrevSeq = 0;
static int   gEscState = 0;
static int   gLastMx = -1, gLastMy = -1;

static void marker_mouse(int x, int y, unsigned buttons)
{
    char line[80];
    unsigned n = append(line, 0, "DTE: mouse x=");
    n = dec(line, n, (unsigned)(x < 0 ? 0 : x));
    n = append(line, n, " y=");
    n = dec(line, n, (unsigned)(y < 0 ? 0 : y));
    n = append(line, n, " buttons=");
    n = dec(line, n, buttons);
    line[n] = 0;
    marker(line);
}

static void marker_chars(void)
{
    char line[64];
    unsigned n = append(line, 0, "DTE: chars=");
    n = dec(line, n, (unsigned)gChars);
    line[n] = 0;
    marker(line);
}

/* ---- keyboard (con_tryread + ANSI arrow decoder + editor shortcuts) ----- */
static int dt_key(void)
{
    for (;;) {
        int c = con_tryread();
        if (c < 0) {
            if (gEscState == 1) { gEscState = 0; return GUI_KEY_ESC; }
            return GUI_KEY_NONE;
        }
        if (c == 0x13) return DT_CTRL_S;       /* Ctrl-S: save */
        if (c == 0x0c) return DT_CTRL_L;       /* Ctrl-L: clear */
        if (gEscState == 0) {
            if (c == 27) { gEscState = 1; sleep_us(1500); continue; }
            if (c == '\r' || c == '\n') return GUI_KEY_ENTER;
            if (c == 8 || c == 127) return GUI_KEY_BACKSPACE;
            return (c >= 32 && c <= 126) ? c : GUI_KEY_NONE;
        }
        if (gEscState == 1) {
            if (c == '[' || c == 'O') { gEscState = 2; continue; }
            gEscState = 0;
            if (c == 27) return GUI_KEY_ESC;
            return (c >= 32 && c <= 126) ? c : GUI_KEY_NONE;
        }
        gEscState = 0;
        switch (c) {
        case 'A': return GUI_KEY_UP;
        case 'B': return GUI_KEY_DOWN;
        case 'C': return GUI_KEY_RIGHT;
        case 'D': return GUI_KEY_LEFT;
        default:  return GUI_KEY_NONE;
        }
    }
}

/* ---- editing primitives ------------------------------------------------ */
static void insert_char(int ch)
{
    char* p = gLines[gRow];
    int col = gCol, n = 0;
    while (p[n] && n < TEXT_COLS) ++n;
    if (n >= TEXT_COLS) {
        /* Auto-wrap at the right margin when another line is available. */
        if (gRow + 1 >= gLineCount && gLineCount < TEXT_LINES) {
            ++gLineCount;
            gLines[gLineCount - 1][0] = 0;
        }
        if (gRow + 1 >= gLineCount) return;
        ++gRow; gCol = 0; p = gLines[gRow];
        n = 0;
    }
    for (int i = n; i > col; --i) p[i] = p[i - 1];
    p[col] = (char)ch;
    p[n + 1] = 0;
    ++gCol; ++gChars;
    gDirty = 1;
    marker_chars();
}

/* helper for backspace's line join and Enter's line split */
static void copy_line(char* dst, const char* src)
{
    int i = 0;
    while (src[i] && i < TEXT_COLS) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
}

static void backspace(void)
{
    char* p = gLines[gRow];
    int n = 0;
    while (p[n]) ++n;
    if (gCol > 0) {
        for (int i = gCol - 1; i + 1 < n; ++i) p[i] = p[i + 1];
        p[n - 1] = 0;
        --gCol; --gChars;
        gDirty = 1;
        marker_chars();
        return;
    }
    if (gRow > 0) {
        char* prev = gLines[gRow - 1];
        int pn = 0, i;
        while (prev[pn]) ++pn;
        if (pn + n > TEXT_COLS) return;
        for (i = 0; i < n; ++i) prev[pn + i] = p[i];
        prev[pn + n] = 0;
        for (i = gRow + 1; i < gLineCount; ++i)
            copy_line(gLines[i - 1], gLines[i]);
        --gLineCount; --gRow;
        gCol = pn;
        gLines[gLineCount][0] = 0;
        gDirty = 1;
        marker_chars();
    }
}

static void newline(void)
{
    char* p = gLines[gRow];
    char rest[TEXT_COLS + 1];
    int n = 0, i, r = 0;
    while (p[n]) ++n;
    if (gLineCount >= TEXT_LINES) return;
    while (gCol + r < n) { rest[r] = p[gCol + r]; ++r; }
    rest[r] = 0;
    for (i = gLineCount; i > gRow + 1; --i)
        copy_line(gLines[i], gLines[i - 1]);
    ++gLineCount;
    p[gCol] = 0;
    copy_line(gLines[gRow + 1], rest);
    ++gRow; gCol = 0;
    gDirty = 1;
    marker("DTE: newline");
}

static void cursor_left(void)
{
    if (gCol > 0) --gCol;
    else if (gRow > 0) {
        --gRow;
        while (gLines[gRow][gCol] && gCol < TEXT_COLS) ++gCol;
    }
    gDirty = 1;
}

static void cursor_right(void)
{
    int n = 0;
    while (gLines[gRow][n]) ++n;
    if (gCol < n) ++gCol;
    else if (gRow + 1 < gLineCount) { ++gRow; gCol = 0; }
    gDirty = 1;
}

static void save_file(void)
{
    static char flat[TEXT_LINES * (TEXT_COLS + 1)];
    unsigned len = 0;
    int i, j, r;
    for (i = 0; i < gLineCount; ++i) {
        for (j = 0; gLines[i][j]; ++j) flat[len++] = gLines[i][j];
        if (i + 1 < gLineCount) flat[len++] = '\n';
    }
    r = file_save("NOTE.DAT", flat, len);
    if (r < 0) { marker("DTE: save failed"); return; }
    char line[80];
    unsigned n = append(line, 0, "DTE: saved NOTE.DAT (");
    n = dec(line, n, len);
    n = append(line, n, " bytes)");
    line[n] = 0;
    marker(line);
}

static void clear_buffer(void)
{
    for (int i = 0; i < TEXT_LINES; ++i) gLines[i][0] = 0;
    gLineCount = 1; gRow = 0; gCol = 0; gChars = 0;
    gDirty = 1;
    marker("DTE: cleared");
    marker_chars();
}

static void quit_app(void)
{
    marker("DTE: DONE");
    gDone = 1;
}

/* ---- mouse hit testing ------------------------------------------------- */
static int in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void handle_click(int mx, int my)
{
    if (in_rect(mx, my, CLOSE_X, CLOSE_Y, CLOSE_W, CLOSE_H)) {
        marker("DTE: click CLOSE");
        quit_app();
        return;
    }
    if (in_rect(mx, my, BTN_SAVE_X, BTN_Y, BTN_W, BTN_H)) {
        marker("DTE: click SAVE");
        save_file();
        return;
    }
    if (in_rect(mx, my, BTN_CLR_X, BTN_Y, BTN_W, BTN_H)) {
        marker("DTE: click CLEAR");
        clear_buffer();
        return;
    }
    if (in_rect(mx, my, BTN_QUIT_X, BTN_Y, BTN_W, BTN_H)) {
        marker("DTE: click QUIT");
        quit_app();
        return;
    }
    if (in_rect(mx, my, TEXT_X, TEXT_Y, TEXT_COLS * GUI_CELL_W,
                TEXT_VIS * GUI_ROW_PITCH)) {
        int row = (my - TEXT_Y) / GUI_ROW_PITCH;
        int col = (mx - TEXT_X) / GUI_CELL_W;
        int n = 0;
        if (row < 0 || row >= TEXT_VIS || row >= gLineCount) return;
        while (gLines[row][n]) ++n;
        if (col < 0) col = 0;
        if (col > n) col = n;
        gRow = row; gCol = col;
        gDirty = 1;
        marker("DTE: click TEXT");
    }
}

/* ---- drawing ----------------------------------------------------------- */
static void draw_frame(void)
{
    char line[128];
    unsigned k;
    int i;

    gui_clear(GUI_NAVY);
    gui_rect(WIN_X, WIN_Y, WIN_W, WIN_H, GUI_BLUE);
    gui_window(WIN_X, WIN_Y, WIN_W, WIN_H, "EKA2 Notepad", 1, 0);
    gui_button(0, CLOSE_X, CLOSE_Y, CLOSE_W, CLOSE_H, "X", 0);

    gui_button(1, BTN_SAVE_X, BTN_Y, BTN_W, BTN_H, "SAVE", 0);
    gui_button(2, BTN_CLR_X, BTN_Y, BTN_W, BTN_H, "CLEAR", 0);
    gui_button(3, BTN_QUIT_X, BTN_Y, BTN_W, BTN_H, "QUIT", 0);

    /* Text area background and a thin recessed frame. */
    gui_rect(TEXT_X - 4, TEXT_Y - 4, TEXT_COLS * GUI_CELL_W + 8,
             TEXT_VIS * GUI_ROW_PITCH + 8, GUI_LIGHTGRAY);
    gui_rect(TEXT_X, TEXT_Y, TEXT_COLS * GUI_CELL_W,
             TEXT_VIS * GUI_ROW_PITCH, GUI_BLACK);

    /* Status strip (drawn before the text so it survives the budget guard). */
    k = append(line, 0, "CHARS ");
    k = dec(line, k, (unsigned)gChars);
    k = append(line, k, "  LINES ");
    k = dec(line, k, (unsigned)gLineCount);
    k = append(line, k, "  CUR ");
    k = dec(line, k, (unsigned)(gRow + 1));
    k = append(line, k, ",");
    k = dec(line, k, (unsigned)(gCol + 1));
    line[k] = 0;
    gui_text(TEXT_X, TEXT_Y - 26, line, GUI_YELLOW, GUI_BLUE);
    gui_text(TEXT_X, WIN_Y + WIN_H - 30,
             "TYPE  ENTER=newline  BS=delete  ARROWS=move  ^S=save  ESC=quit",
             GUI_LIGHTGRAY, GUI_BLUE);

    /* Render non-empty lines only, and stop before the 640-command ABI cap so
     * a completely full buffer can never produce a half-painted frame. */
    for (i = 0; i < TEXT_LINES && i < gLineCount; ++i) {
        if (gui_pending_commands() > 540) break;
        if (gLines[i][0])
            gui_text(TEXT_X, TEXT_Y + i * GUI_ROW_PITCH, gLines[i],
                     GUI_WHITE, GUI_BLACK);
    }
    if (gBlink && gui_pending_commands() <= 560)
        gui_rect(TEXT_X + gCol * GUI_CELL_W, TEXT_Y + gRow * GUI_ROW_PITCH,
                 GUI_CELL_W - 2, GUI_CELL_H - 2, GUI_GREEN);

    gui_present();
}

/* ---- entry point ------------------------------------------------------- */
int app_main(void)
{
    char line[96];
    unsigned n;
    int tick, key;
    int m0x = 0, m0y = 0;
    unsigned m0b = 0, m0s = 0;

    marker("DTE: WINDOWED EDITOR READY");
    n = append(line, 0, "DTE: window ");
    n = dec(line, n, WIN_W);
    n = append(line, n, "x");
    n = dec(line, n, WIN_H);
    line[n] = 0;
    marker(line);

    if (gui_init() != 0) {
        marker("DTE: FAIL gui_init");
        return 1;
    }
    /* Pointer state via guilib on the genuine EWSRV client path (no exec 251
     * gui_shared anymore). */
    gui_mouse_full(&m0x, &m0y, &m0b, &m0s);
    gPrevButtons = m0b;
    gPrevSeq = m0s;

    gLines[0][0] = 0;
    marker_chars();
    draw_frame();
    n = append(line, 0, "DTE: frame drawn cmds=");
    n = dec(line, n, (unsigned)gui_pending_commands());
    line[n] = 0;
    marker(line);

    marker_mouse(m0x, m0y, m0b);
    gLastMx = m0x;
    gLastMy = m0y;

    for (tick = 0; tick < 3000 && !gDone; ++tick) {
        unsigned buttons, seq;
        int mx, my;
        gui_mouse_full(&mx, &my, &buttons, &seq);
        if (mx != gLastMx || my != gLastMy) {
            gLastMx = mx; gLastMy = my;
            marker_mouse(mx, my, buttons);
        }
        if ((buttons != 0u && gPrevButtons == 0u) ||
            ((unsigned)(seq - gPrevSeq) >= 2u && buttons == 0u))
            handle_click(mx, my);
        gPrevButtons = buttons;
        gPrevSeq = seq;

        key = dt_key();
        if (key == GUI_KEY_UP) {
            if (gRow > 0) --gRow;
            gDirty = 1;
        } else if (key == GUI_KEY_DOWN) {
            if (gRow + 1 < gLineCount) ++gRow;
            gDirty = 1;
        } else if (key == GUI_KEY_LEFT) {
            cursor_left();
        } else if (key == GUI_KEY_RIGHT) {
            cursor_right();
        } else if (key == GUI_KEY_ENTER) {
            newline();
        } else if (key == GUI_KEY_BACKSPACE) {
            backspace();
        } else if (key == GUI_KEY_ESC) {
            quit_app();
        } else if (key == DT_CTRL_S) {
            marker("DTE: key ^S");
            save_file();
        } else if (key == DT_CTRL_L) {
            clear_buffer();
        } else if (key >= 32 && key <= 126) {
            insert_char(key);
        }

        gBlink = (tick % 16) < 8;
        if (gBlink != gLastBlink) { gLastBlink = gBlink; gDirty = 1; }
        if (gDirty) { draw_frame(); gDirty = 0; }
        sleep_ms(30);
    }

    /* Leave a clean final GUI image on the LFB after the serial markers. */
    if (!gDone) marker("DTE: timeout");
    gBlink = 0;
    draw_frame();
    gui_present();
    gui_shutdown();
    return 0;
}

/*
 * dtcalc.c -- a real WINDOWED desktop calculator for the R1 EKA2 port.
 *
 * This is not a text-mode calculator and not a canned demo: it paints a
 * decorated, shadowed window with a live display and a 20-key keypad onto the
 * real VBE linear framebuffer, using the reusable guilib.c toolkit (display
 * list transport via GUI slow execs 249/250/251).  Every key is clickable with
 * the real PS/2 mouse (press edges detected from gui_shared.buttons /
 * button_seq) and typeable on the serial keyboard (gui_key / con_tryread).
 * Expressions are evaluated by a recursive-descent parser adapted from
 * user/calc.c (+ - * /, parentheses, wrapping 32-bit arithmetic).
 *
 * Serial markers ride the genuine PE2 import of EUSER.DLL ordinal 915
 * (RDebug::Printf); IAT slot dtcalc_printf_slot lives at 0x408000.
 */
#include "runtime.h"
#include "guilib.h"
#include "guishost.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*dtcalc_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&dtcalc_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ---- tiny freestanding formatters (no hosted libc) ---------------------- */
static unsigned app(char* p, unsigned k, const char* s)
{
    while (*s) p[k++] = *s++;
    return k;
}

static unsigned dec_u(char* p, unsigned k, unsigned v)
{
    char t[12];
    int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10u); v /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}

static unsigned dec_s(char* p, unsigned k, int v)
{
    unsigned mag = (unsigned)v;
    if (v < 0) { p[k++] = '-'; mag = 0u - mag; }
    return dec_u(p, k, mag);
}

static unsigned slen(const char* s)
{
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}

/* ---- expression evaluator (adapted from user/calc.c) -------------------- */
enum { DC_OK = 0, DC_DIV0 = 1, DC_SYNTAX = 2 };

struct DCParser { const char* p; int err; };

static void dc_space(struct DCParser* s)
{
    while (*s->p == ' ' || *s->p == '\t') ++s->p;
}

static unsigned dc_expr(struct DCParser* s);

static unsigned dc_number(struct DCParser* s)
{
    unsigned v = 0;
    while (*s->p >= '0' && *s->p <= '9')
        { v = v * 10u + (unsigned)(*s->p - '0'); ++s->p; }
    return v;
}

static unsigned dc_factor(struct DCParser* s)
{
    unsigned v;
    dc_space(s);
    if (*s->p == '+') { ++s->p; return dc_factor(s); }
    if (*s->p == '-') { ++s->p; v = dc_factor(s); return 0u - v; }
    if (*s->p == '(')
        {
        ++s->p;
        v = dc_expr(s);
        dc_space(s);
        if (*s->p == ')') ++s->p; else s->err = DC_SYNTAX;
        return v;
        }
    if (*s->p >= '0' && *s->p <= '9') return dc_number(s);
    s->err = DC_SYNTAX;
    return 0;
}

static unsigned dc_term(struct DCParser* s)
{
    unsigned v = dc_factor(s);
    for (;;)
        {
        char op;
        unsigned r;
        dc_space(s);
        op = *s->p;
        if (op != '*' && op != '/') break;
        ++s->p;
        r = dc_factor(s);
        if (op == '*') v = v * r;
        else
            {
            int a = (int)v, b = (int)r;
            if (b == 0) { s->err = DC_DIV0; v = 0; }
            else if (a == (int)0x80000000u && b == -1) v = (unsigned)a;
            else v = (unsigned)(a / b);
            }
        }
    return v;
}

static unsigned dc_expr(struct DCParser* s)
{
    unsigned v = dc_term(s);
    for (;;)
        {
        char op;
        unsigned r;
        dc_space(s);
        op = *s->p;
        if (op != '+' && op != '-') break;
        ++s->p;
        r = dc_term(s);
        v = (op == '+') ? (v + r) : (v - r);
        }
    return v;
}

static int dc_eval(const char* e, int* value)
{
    struct DCParser s = {e, DC_OK};
    unsigned v = dc_expr(&s);
    dc_space(&s);
    if (*s.p) s.err = DC_SYNTAX;
    *value = (int)v;
    return s.err;
}

/* ---- keypad definition -------------------------------------------------- */
#define NB        20          /* keypad buttons */
#define CLOSE_IDX 20          /* title-bar close button (virtual button) */

static const char* const kLabel[NB] = {
    "7", "8", "9", "/",
    "4", "5", "6", "*",
    "1", "2", "3", "-",
    "0", "(", ")", "+",
    "C", "DEL", "=", "QUIT"
};

/* Window / keypad geometry (pixels on the 1024x768 LFB). */
#define WIN_W   576
#define WIN_H   560
#define KX_OFF  16
#define KY_OFF  112
#define KEY_W   128
#define KEY_H   56
#define GAP_X   8
#define GAP_Y   8

static void key_rect(int wx, int wy, int idx,
                     int* x, int* y, int* w, int* h)
{
    int col = idx % 4, row = idx / 4;
    *x = wx + KX_OFF + col * (KEY_W + GAP_X);
    *y = wy + KY_OFF + row * (KEY_H + GAP_Y);
    *w = KEY_W;
    *h = KEY_H;
}

static void close_rect(int wx, int wy, int* x, int* y, int* w, int* h)
{
    *x = wx + WIN_W - 46; *y = wy + 1; *w = 40; *h = 16;
}

static int hit_test(int wx, int wy, int mx, int my)
{
    int x, y, w, h, i;
    close_rect(wx, wy, &x, &y, &w, &h);
    if (mx >= x && mx < x + w && my >= y && my < y + h) return CLOSE_IDX;
    for (i = 0; i < NB; ++i)
        {
        key_rect(wx, wy, i, &x, &y, &w, &h);
        if (mx >= x && mx < x + w && my >= y && my < y + h) return i;
        }
    return -1;
}

/* ---- painting ----------------------------------------------------------- */
static void draw_frame(int wx, int wy, const char* expr,
                       const char* result, int hover)
{
    int i, x, y, w, h;
    int W = gui_fb_width();

    gui_clear(GUI_NAVY);
    /* shell-style banner so the frame reads as a desktop application */
    gui_rect(0, 0, W, 32, GUI_TITLEBAR);
    gui_text(12, 8, "EKA2 DESKTOP APPLICATION", GUI_WHITE, GUI_TITLEBAR);

    /* drop shadow, then a fully decorated window (border + title strip) */
    gui_rect(wx + 8, wy + 8, WIN_W, WIN_H, 0x000C1424u);
    gui_rect(wx + 4, wy + 4, WIN_W, WIN_H, 0x00081018u);
    gui_window(wx, wy, WIN_W, WIN_H, "EKA2 Calculator", 1, 1);

    /* close button in the title strip */
    close_rect(wx, wy, &x, &y, &w, &h);
    gui_button(0, x, y, w, h, "X", hover == CLOSE_IDX);

    /* live display field: expression on top, result / hint below */
    gui_rect(wx + 16, wy + 30, WIN_W - 32, 72, GUI_BLACK);
    gui_rect(wx + 16, wy + 30, WIN_W - 32, 2, GUI_LIGHTGRAY);
    gui_rect(wx + 16, wy + 100, WIN_W - 32, 2, GUI_LIGHTGRAY);
    gui_rect(wx + 16, wy + 30, 2, 72, GUI_LIGHTGRAY);
    gui_rect(wx + WIN_W - 18, wy + 30, 2, 72, GUI_LIGHTGRAY);
    gui_text(wx + 26, wy + 36, expr, GUI_YELLOW, GUI_BLACK);
    gui_text(wx + 26, wy + 58, result, GUI_GREEN, GUI_BLACK);

    /* keypad: every button has a border and a centred label */
    for (i = 0; i < NB; ++i)
        {
        key_rect(wx, wy, i, &x, &y, &w, &h);
        gui_button(i, x, y, w, h, kLabel[i], hover == i);
        }

    gui_text(wx + 18, wy + 448, "CLICK a key or TYPE: digits + - * / ( )",
             GUI_LIGHTGRAY, GUI_BLUE);
    gui_text(wx + 18, wy + 470, "ENTER = evaluate   BACKSPACE = delete   C = clear",
             GUI_LIGHTGRAY, GUI_BLUE);
    gui_text(wx + 18, wy + 492, "ESC or X = close calculator", GUI_LIGHTGRAY, GUI_BLUE);
    gui_present();
}

/* ---- actions ------------------------------------------------------------ */
static void press_key(int idx, char* expr, char* result,
                      int* just_eval, int* quit)
{
    unsigned n;
    char tmp[24];
    unsigned k;

    if (idx == CLOSE_IDX || idx == 19) { *quit = 1; return; }
    if (idx == 16)   /* C: clear everything */
        { expr[0] = 0; result[0] = 0; *just_eval = 0; return; }
    if (idx == 17)   /* DEL: drop the last character */
        { n = slen(expr); if (n) expr[n - 1] = 0; return; }
    if (idx == 18)   /* =: evaluate through the real parser */
        {
        int value, err;
        char line[160];
        err = dc_eval(expr, &value);
        if (err == DC_OK)
            { k = dec_s(tmp, 0, value); tmp[k] = 0; }
        else if (err == DC_DIV0)
            { k = app(tmp, 0, "ERR div0"); (void)k; }
        else
            { k = app(tmp, 0, "ERR syntax"); (void)k; }
        for (k = 0; tmp[k]; ++k) result[k] = tmp[k];
        result[k] = 0;
        k = app(line, 0, "DCALC: expr ");
        k = app(line, k, expr);
        k = app(line, k, " = ");
        k = app(line, k, result);
        line[k] = 0;
        marker(line);
        *just_eval = 1;
        return;
        }
    /* a literal key: digit, operator or parenthesis */
    if (*just_eval) { expr[0] = 0; result[0] = 0; *just_eval = 0; }
    n = slen(expr);
    if (n + 1 < 64) { expr[n] = kLabel[idx][0]; expr[n + 1] = 0; }
}

/* ---- entry point -------------------------------------------------------- */
int app_main(void)
{
    static struct gui_shared* sh;
    char line[160];
    char expr[64], result[24];
    unsigned prev_buttons, prev_seq;
    int rc, key, mx, my, mb, i, idx, hover = -1, prev_hover = -1;
    int wx, wy, dirty = 1, quit = 0, just_eval = 0, polls = 0;
    int prev_mx = -1, prev_my = -1;
    unsigned prev_bstate = 0xFFFFFFFFu;
    unsigned k;

    expr[0] = 0;
    result[0] = 0;

    rc = gui_init();
    if (rc != 0)
        {
        k = app(line, 0, "DCALC: FAIL gui_init rc=");
        k = dec_u(line, k, (unsigned)(rc < 0 ? -rc : rc));
        line[k] = 0;
        marker(line);
        return 1;
        }

    sh = (struct gui_shared*)exec2(QE_EXEC_GUI_DRAW, 0, 0);
    if (!sh || sh->magic != GUI_MAGIC || sh->version != GUI_VERSION)
        { marker("DCALC: FAIL shared buffer"); gui_shutdown(); return 1; }

    wx = (gui_fb_width() - WIN_W) / 2;
    wy = (gui_fb_height() - WIN_H) / 2;
    if (wx < 0) wx = 0;
    if (wy < 0) wy = 0;
    if (wx + WIN_W > gui_fb_width()) wx = gui_fb_width() - WIN_W;
    if (wy + WIN_H > gui_fb_height()) wy = gui_fb_height() - WIN_H;

    prev_buttons = sh->buttons;
    prev_seq = sh->button_seq;

    /* first complete frame, then announce readiness */
    draw_frame(wx, wy, expr, result, -1);
    marker("DCALC: WINDOWED CALCULATOR READY");

    k = app(line, 0, "DCALC: window ");
    k = dec_s(line, k, wx);
    k = app(line, k, ",");
    k = dec_s(line, k, wy);
    k = app(line, k, " ");
    k = dec_s(line, k, WIN_W);
    k = app(line, k, "x");
    k = dec_s(line, k, WIN_H);
    line[k] = 0;
    marker(line);

    k = app(line, 0, "DCALC: buttons=");
    k = dec_u(line, k, NB);
    line[k] = 0;
    marker(line);

    /* 4000 polls * 30 ms = 120 s safety ceiling; Esc/close exits earlier. */
    for (polls = 0; polls < 4000 && !quit; ++polls)
        {
        int clicked;
        unsigned buttons, seq, dseq;

        key = gui_key();
        idx = -1;
        if (key == GUI_KEY_ESC)
            {
            marker("DCALC: key ESC");
            quit = 1;
            break;
            }
        if (key == GUI_KEY_ENTER) idx = 18;
        else if (key == GUI_KEY_BACKSPACE) idx = 17;
        else if (key >= 32 && key <= 126)
            {
            /* map printable characters to keypad indices by label */
            if (key == 'c' || key == 'C') idx = 16;
            else if (key == 'q' || key == 'Q') idx = 19;
            else
                {
                for (i = 0; i < NB; ++i)
                    if (kLabel[i][0] == key && kLabel[i][1] == 0)
                        { idx = i; break; }
                }
            }

        if (idx >= 0)
            {
            char line2[96];
            press_key(idx, expr, result, &just_eval, &quit);
            k = app(line2, 0, "DCALC: key ");
            k = app(line2, k, kLabel[idx]);
            line2[k] = 0;
            marker(line2);
            dirty = 1;
            }

        /* ---- real PS/2 mouse: press edge + hit test -------------------- */
        buttons = sh->buttons;
        seq = sh->button_seq;
        dseq = seq - prev_seq;
        clicked = (buttons != 0u && prev_buttons == 0u) ||
                  (dseq >= 2u && buttons == 0u);
        mx = sh->mouse_x;
        my = sh->mouse_y;
        mb = (int)buttons;
        prev_buttons = buttons;
        prev_seq = seq;

        /* publish button changes: lets a host test verify PS/2 injection */
        if (buttons != prev_bstate)
            {
            char bl[64];
            unsigned bn = app(bl, 0, "DCALC: buttons ");
            bn = dec_u(bl, bn, buttons);
            bl[bn] = 0;
            marker(bl);
            prev_bstate = buttons;
            }

        /* publish cursor moves: lets a host test calibrate PS/2 injection */
        if (mx != prev_mx || my != prev_my)
            {
            char ml[64];
            unsigned mn = app(ml, 0, "DCALC: mouse ");
            mn = dec_s(ml, mn, mx);
            mn = app(ml, mn, ",");
            mn = dec_s(ml, mn, my);
            ml[mn] = 0;
            marker(ml);
            prev_mx = mx;
            prev_my = my;
            }

        hover = hit_test(wx, wy, mx, my);
        if (hover != prev_hover) { prev_hover = hover; dirty = 1; }

        if (clicked && hover >= 0)
            {
            char line3[96];
            k = app(line3, 0, "DCALC: click ");
            k = app(line3, k, hover == CLOSE_IDX ? "X" : kLabel[hover]);
            line3[k] = 0;
            marker(line3);
            press_key(hover, expr, result, &just_eval, &quit);
            dirty = 1;
            }
        (void)mb;

        if (dirty)
            {
            draw_frame(wx, wy, expr, result, hover);
            dirty = 0;
            }
        sleep_ms(30);
        }

    /* leave a clean final GUI on the framebuffer even after exit */
    draw_frame(wx, wy, expr, result, -1);
    marker("DCALC: DONE");
    gui_shutdown();
    return 0;
}

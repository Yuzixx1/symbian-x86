/*
 * wsdl.c -- WINDOWED CALCULATOR client for the R1 window server.
 *
 * This is a real client E32, not a screen-taking full-frame app: it opens ONE
 * window with ws_window_create(), paints its display + 20-key keypad into that
 * window's private content buffer, and services mouse/key events from
 * ws_get_event() until the user closes it.  The long-lived WINSRV compositor
 * keeps the desktop (and this window) painted while we run.
 *
 * Serial markers ride the genuine PE2 import of EUSER.DLL ordinal 915
 * (RDebug::Printf); IAT slot wsdl_printf_slot lives at 0x408000.
 */
#include "runtime.h"
#include "wsclient.h"

extern void (*wsdl_printf_slot)(const char*);

static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&wsdl_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback   */
}

/* ---- tiny freestanding formatters --------------------------------------- */
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

/* ---- expression evaluator (adapted from user/calc.c / dtcalc.c) ---------- */
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

/* ---- keypad / geometry (window is WS_CONTENT_W x WS_CONTENT_H) ---------- */
#define NB        20          /* keypad buttons */
#define CLOSE_IDX 20          /* title-bar close gadget (virtual button) */
#define WIN_W     ((int)WS_CONTENT_W)
#define WIN_H     ((int)WS_CONTENT_H)

#define KX_OFF    7
#define KY_OFF    64
#define KEY_W     72
#define KEY_H     24
#define GAP_X     6
#define GAP_Y     3

static const char* const kLabel[NB] = {
    "7", "8", "9", "/",
    "4", "5", "6", "*",
    "1", "2", "3", "-",
    "0", "(", ")", "+",
    "C", "DEL", "=", "QUIT"
};

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
    *x = wx + WIN_W - 22; *y = wy + 4; *w = 18; *h = 16;
}

static int hit_test(int wid, int mx, int my)
{
    int wx, wy, ww, wh, x, y, w, h, i;
    if (ws_window_geom(wid, &wx, &wy, &ww, &wh) != 0) return -1;
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
static void draw_frame(int wid, const char* expr,
                       const char* result, int hover)
{
    int i, x, y, w, h;

    /* window chrome (redrawn every frame so hover changes stay consistent) */
    ws_fill(wid, 0, 0, WIN_W, WIN_H, WSC_BLUE);
    ws_fill(wid, 0, 0, WIN_W, 2, WSC_LIGHTGRAY);
    ws_fill(wid, 0, WIN_H - 2, WIN_W, 2, WSC_LIGHTGRAY);
    ws_fill(wid, 0, 0, 2, WIN_H, WSC_LIGHTGRAY);
    ws_fill(wid, WIN_W - 2, 0, 2, WIN_H, WSC_LIGHTGRAY);
    ws_fill(wid, 2, 2, WIN_W - 4, 20, WSC_TITLEBAR);
    ws_text(wid, 5, 3, "WSDL Calculator", WSC_WHITE);
    close_rect(0, 0, &x, &y, &w, &h);
    ws_fill(wid, x, y, w, h, WSC_GRAY);
    ws_text(wid, x + 1, y, "X", WSC_YELLOW);

    /* live display: expression on top, result / hint below */
    ws_fill(wid, 8, 26, WIN_W - 16, 34, WSC_BLACK);
    ws_fill(wid, 8, 26, WIN_W - 16, 2, WSC_LIGHTGRAY);
    ws_fill(wid, 8, 58, WIN_W - 16, 2, WSC_LIGHTGRAY);
    ws_fill(wid, 8, 26, 2, 34, WSC_LIGHTGRAY);
    ws_fill(wid, WIN_W - 10, 26, 2, 34, WSC_LIGHTGRAY);
    ws_text(wid, 12, 27, expr, WSC_YELLOW);
    ws_text(wid, 12, 45, result, WSC_GREEN);

    /* keypad */
    for (i = 0; i < NB; ++i)
        {
        key_rect(0, 0, i, &x, &y, &w, &h);
        ws_button(wid, x, y, w, h, kLabel[i], hover == i);
        }
    /* The server now blits the whole 320x200 content buffer through RLE
     * GC_RECT display lists, so no block-origin stamping is needed -- every
     * button, glyph and display pixel reaches the composited desktop. */
    ws_present(wid);
}

/* ---- actions ------------------------------------------------------------ */
static void press_key(int idx, char* expr, char* result,
                      int* just_eval, int* quit)
{
    unsigned n, k;
    char tmp[24];

    if (idx == CLOSE_IDX || idx == 19) { *quit = 1; return; }
    if (idx == 16)   /* C: clear */
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
        k = app(line, 0, "WDL: expr ");
        k = app(line, k, expr);
        k = app(line, k, " = ");
        k = app(line, k, result);
        line[k] = 0;
        marker(line);
        *just_eval = 1;
        return;
        }
    /* literal key: digit, operator or parenthesis */
    if (*just_eval) { expr[0] = 0; result[0] = 0; *just_eval = 0; }
    n = slen(expr);
    if (n + 1 < 32) { expr[n] = kLabel[idx][0]; expr[n + 1] = 0; }
}

static void event_marker(unsigned type, int x, int y)
{
    char line[96];
    unsigned n = app(line, 0, "WDL: event ");
    n = dec_u(line, n, type);
    n = app(line, n, " ");
    n = dec_s(line, n, x);
    n = app(line, n, ",");
    n = dec_s(line, n, y);
    line[n] = 0;
    marker(line);
}

/* ---- event targeting ----------------------------------------------------- */
/* Every ring event carries the id of the window it was aimed at (-1/0 =
 * broadcast).  WSDL owns exactly one window: acting on an event aimed at
 * WINSRV or another client is what made our composited window vanish when the
 * harness sent ESC to the server.  The wsclient console fallback also stamps
 * our own window id on bytes it steals from PortCon, so a targeting filter
 * alone cannot catch that path -- see the ESC long-press guard in app_main. */
static int event_for_me(int wid, const struct ws_event* e)
{
    return e->target_wid == wid || e->target_wid == -1 || e->target_wid == 0;
}

/* Consecutive targeted ESC keypresses required to close the calculator.
 * A single ESC is the shared-console quit used to stop WINSRV; if WSDL happens
 * to snag it from the serial console it must not destroy its own window. */
#define ESC_CLOSE_RUN 3

/* ---- deterministic startup self-drive (same handler as real input) ------ */
struct DemoEvent { unsigned type; int x, y; unsigned key; };
static const struct DemoEvent sDemo[] = {
    {WSE_MOVE, 100, 100, 0},        /* hover */
    {WSE_KEY,  -1,  -1,  (unsigned)'7'},
    {WSE_KEY,  -1,  -1,  (unsigned)'+'},
    {WSE_KEY,  -1,  -1,  (unsigned)'8'},
    {WSE_KEY,  -1,  -1,  13u},      /* Enter -> evaluate 7+8 */
    {WSE_DOWN, 91, 90, 1u},         /* click a keypad button */
};
#define NDEMO ((int)(sizeof(sDemo) / sizeof(sDemo[0])))

/* ---- entry point -------------------------------------------------------- */
int app_main(void)
{
    char line[160];
    char expr[32], result[24];
    int wid, rc, i, idx, key, hover = -1, dirty = 1;
    int quit = 0, just_eval = 0, polls, demo = 0, esc_run = 0;

    expr[0] = 0;
    result[0] = 0;
    marker("WDL: CLIENT READY");

    rc = ws_open();
    if (rc != 0)
        {
        unsigned n = app(line, 0, "WDL: FAIL ws_open rc=");
        n = dec_s(line, n, rc);
        line[n] = 0;
        marker(line);
        return 1;
        }

    wid = ws_window_create(80, 60, WIN_W, WIN_H, "WSDL Calculator");
    if (wid < 0)
        {
        marker("WDL: FAIL window create");
        ws_close();
        return 1;
        }
    { unsigned n = app(line, 0, "WDL: window ");
      n = dec_s(line, n, wid);
      line[n] = 0;
      marker(line); }

    /* first complete frame, then announce readiness to the host */
    draw_frame(wid, expr, result, -1);
    marker("WDL: FRAME 1");

    /* Stay alive until closed.  2500 polls * 20 ms = 50 s safety ceiling. */
    for (polls = 0; polls < 2500 && !quit; ++polls)
        {
        struct ws_event e;
        int have;
        idx = -1;

        if (demo < NDEMO)
            {
            e.type = sDemo[demo].type;
            e.x = sDemo[demo].x; e.y = sDemo[demo].y;
            e.key = sDemo[demo].key;
            e.target_wid = (unsigned)wid;
            ++demo;
            have = 1;
            }
        else
            have = ws_get_event(&e);

        /* Input aimed at another window (typically the ESC that quits WINSRV)
         * must never reach this client, or we would destroy our own window. */
        if (have && !event_for_me(wid, &e))
            have = 0;

        if (have)
            {
            event_marker(e.type, e.x, e.y);

            if (e.type == WSE_KEY)
                {
                key = (int)(e.key & 0xFFu);
                if (key == 27)
                    {
                    /* A lone ESC is the shared-console quit aimed at WINSRV.
                     * If we snag it from PortCon it is NOT a close request:
                     * only a deliberate long-press closes the calculator. */
                    if (++esc_run >= ESC_CLOSE_RUN)
                        {
                        marker("WDL: close (ESC long-press)");
                        quit = 1;
                        }
                    }
                else
                    {
                    esc_run = 0;
                    if (key == 13 || key == 10) idx = 18;
                    else if (key == 8 || key == 127) idx = 17;
                    else if (key == 'c' || key == 'C') idx = 16;
                    else
                        {
                        idx = -1;
                        for (i = 0; i < NB; ++i)
                            if (kLabel[i][0] == key && kLabel[i][1] == 0)
                                { idx = i; break; }
                        }
                    if (!quit && idx >= 0)
                        {
                        press_key(idx, expr, result, &just_eval, &quit);
                        dirty = 1;
                        }
                    }
                }
            else if (e.type == WSE_DOWN || e.type == WSE_UP)
                {
                esc_run = 0;
                idx = hit_test(wid, e.x, e.y);
                if (e.type == WSE_DOWN && idx >= 0)
                    {
                    press_key(idx, expr, result, &just_eval, &quit);
                    dirty = 1;
                    }
                }
            else if (e.type == WSE_MOVE)
                {
                int nh;
                esc_run = 0;
                nh = hit_test(wid, e.x, e.y);
                if (nh != hover) { hover = nh; dirty = 1; }
                }
            }

        if (dirty)
            { draw_frame(wid, expr, result, hover); dirty = 0; }
        sleep_ms(20);
        }

    draw_frame(wid, expr, result, -1);
    marker("WDL: DONE");
    ws_close();
    return 0;
}

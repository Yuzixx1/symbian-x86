/*
 * ws_e32_draw_test.c -- WINSRV_MIGRATION_PLAN chunk 2 + GUISHELL chunk D0
 * proof E32.
 *
 * Opens a window on the GENUINE EWSRV (!E32WindowServer session, the chunk-1
 * ws_e32_client.c transport) and draws two filled rectangles with
 * ws_draw_fill_rect() (genuine CWsSession ops EConsoleSetTextColors +
 * EConsoleSetCursorPosAbs + EConsoleWrite of space cells; the server blits
 * every cell into the LFB), then prints two strings with ws_text() (same
 * ops, real UTF-16 glyphs instead of spaces) plus one ws_button().  Chunk D1:
 * it then blocks in ws_get_event() (genuine RConsole::Read / EConsoleRead)
 * until a KBINJ-injected key arrives and echoes it as "WSDRAW: KEY ...".
 * It then spins long enough for a QEMU screendump to capture the rendered
 * window, prints DONE and exits.
 *
 * Built as build/user/WSE32DRAW.E32 (user.mk): same four EUSER PE2 IAT slots
 * as WSE32TEST (ords 211/1547/120/915 at 0x408000..0x40800c) -- the draw ops
 * ride the one DoSendReceive import, so no new ordinals are needed.
 */
#include "runtime.h"

/* Self-contained decls (the retired winsrv shim client header is gone):
 * fixed-width types, the event record + WSE_* codes, and the ws_* API
 * prototypes implemented by the linked ws_e32_client.c translation unit. */
#if !defined(__cplusplus)
typedef unsigned int       uint32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;
#endif
#define WSE_MOVE  1u
#define WSE_DOWN  2u
#define WSE_UP    3u
#define WSE_KEY   4u
struct ws_event
    {
    volatile unsigned int type;
    volatile int x, y;
    volatile unsigned int key;
    volatile int target_wid;
    };
int  ws_open(void);
int  ws_window_create(int x, int y, int w, int h, const char* title);
int  ws_screen_size(int* w, int* h);
void ws_close(void);
uint32_t ws_rgb(uint8_t r, uint8_t g, uint8_t b);
void ws_text(int wid, int x, int y, const char* s, uint32_t fg);
void ws_button(int wid, int x, int y, int w, int h,
                const char* label, int hover);
int  ws_get_event(struct ws_event* e);

/* From ws_e32_client.c (linked together); PE2 IAT slot for ord 915. */
extern unsigned ws_e32_iat_printf;
extern int ws_draw_fill_rect(int wid, int x, int y, int w, int h, uint32_t color);

/* Serial marker through genuine EUSER.DLL ord 915 (RDebug::Printf). */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)&ws_e32_iat_printf;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback   */
}

static unsigned t_app(char* p, unsigned k, const char* s)
{
    while (*s) p[k++] = *s++;
    return k;
}

static unsigned t_dec(char* p, unsigned k, int v)
{
    char t[12];
    int n = 0;
    unsigned mag = (unsigned)v;
    if (v < 0) { p[k++] = '-'; mag = 0u - mag; }
    if (!mag) t[n++] = '0';
    while (mag) { t[n++] = (char)('0' + mag % 10u); mag /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}

static unsigned t_hex(char* p, unsigned k, unsigned v)
{
    static const char d[] = "0123456789abcdef";
    char t[8];
    int n = 0, i;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = d[v & 0xfu]; v >>= 4; }
    for (i = n - 1; i >= 0; --i) p[k++] = t[i];
    return k;
}

int app_main(void)
{
    char line[96];
    unsigned n;
    int r, w = 0, h = 0, wid;

    marker("WSDRAW: app_main started");

    r = ws_open();
    n = t_app(line, 0, "WSDRAW: Connect r="); n = t_dec(line, n, r); line[n] = 0;
    marker(line);
    if (r != 0) return 1;

    r = ws_screen_size(&w, &h);
    n = t_app(line, 0, "WSDRAW: ScreenSize r="); n = t_dec(line, n, r);
    n = t_app(line, n, " "); n = t_dec(line, n, w);
    n = t_app(line, n, "x"); n = t_dec(line, n, h);
    line[n] = 0;
    marker(line);

    wid = ws_window_create(8, 6, 60, 22, "WSDRAW RECT TEST");
    n = t_app(line, 0, "WSDRAW: Create wid="); n = t_dec(line, n, wid); line[n] = 0;
    marker(line);
    if (wid < 0)
        { ws_close(); return 1; }

    /* One filled red rectangle, 30x8 cells inside the window. */
    r = ws_draw_fill_rect(wid, 4, 3, 30, 8, ws_rgb(0xff, 0x20, 0x20));
    n = t_app(line, 0, "WSDRAW: FillRect red r="); n = t_dec(line, n, r); line[n] = 0;
    marker(line);

    /* A second, green rectangle below it. */
    r = ws_draw_fill_rect(wid, 4, 13, 20, 5, ws_rgb(0x20, 0xff, 0x20));
    n = t_app(line, 0, "WSDRAW: FillRect green r="); n = t_dec(line, n, r); line[n] = 0;
    marker(line);

    marker("WSDRAW: rect drawn");

    /* Chunk D0 proof: real glyphs (not space-fill) via ws_text -- the same
     * ops 21/6/10, no new IAT slots.  Line 1 yellow, line 2 cyan; both on
     * the black window background (ws_text bg = palette index 0). */
    ws_text(wid, 4, 19, "WSDRAW TEXT OK 0123456789", ws_rgb(0xff, 0xff, 0x00));
    ws_text(wid, 4, 20, "abcdefghijklmnopqrstuvwxyz", ws_rgb(0x00, 0xff, 0xff));

    /* ws_button composition (fill_rect + centred label) at the right. */
    ws_button(wid, 34, 19, 6, 2, "OK", 0);

    marker("WSDRAW: text drawn");

    /* Chunk D1 proof: genuine input read.  ws_get_event() issues the genuine
     * RConsole::Read (CWsSession::EConsoleRead, op 11) and BLOCKS until the
     * KBINJ-injected key (HMP/QMP sendkey) reaches EWSRV's key queue and the
     * server completes the read with an SConsoleKey.  Echo the received key
     * with the WSDRAW: KEY marker for the harness to assert on. */
    {
    struct ws_event ev;
    int er = ws_get_event(&ev);
    n = t_app(line, 0, "WSDRAW: KEY r="); n = t_dec(line, n, er);
    n = t_app(line, n, " type="); n = t_dec(line, n, (int)ev.type);
    n = t_app(line, n, " x="); n = t_dec(line, n, ev.x);
    n = t_app(line, n, " y="); n = t_dec(line, n, ev.y);
    n = t_app(line, n, " code=0x"); n = t_hex(line, n, ev.key);
    line[n] = 0;
    marker(line);
    }

    /* Stay alive so the harness can screendump the rendered window; the
     * server blits synchronously on EConsoleWrite, so the rects and the
     * text are already on the LFB when the marker above prints. */
    for (volatile unsigned long i = 0; i < 600000000ul; ++i) { }

    marker("WSDRAW: DONE");
    ws_close();
    return 0;
}

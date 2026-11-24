/*
 * wsclient.c -- CLIENT library for the R1 window server.
 *
 * Discovers struct ws_shared through slow execs WS_BASE / WS_SIZE / WS_OP and
 * paints into each window's private content buffer (inside the shared region
 * at WS_CONTENT_OFFSET(wid)).  Low RAM is identity-mapped and user-accessible
 * so the buffer is written directly; the LFB is never touched by a client.
 * Drawing uses the kernel's 7-row 8x8 stroke glyph set (scaled 2x into 16x16
 * cells on an 18px row pitch), matching guilib's on-screen text geometry.
 */
#include "wsclient.h"
#include "runtime.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

/* ---- state -------------------------------------------------------------- */
static struct ws_shared* sSh;
static unsigned          sPid = 0x574C494Eu;  /* "WLIN" client cookie */
static int               sOpen;
static int               sOwned[WS_MAX_WINDOWS];
static int               sOwnCount;

static unsigned ws_strlen(const char* s)
{
    unsigned n = 0;
    while (s && s[n]) ++n;
    return n;
}

/* -- the kernel display font (variant/qemu_display.cpp), 7 active rows ---- */
struct ws_glyph { uint8_t ch; uint8_t rows[7]; };
static const struct ws_glyph sGlyphs[] = {
    {'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C',{0x0F,0x10,0x10,0x10,0x10,0x10,0x0F}},
    {'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
    {'G',{0x0F,0x10,0x10,0x17,0x11,0x11,0x0F}},
    {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
    {'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
    {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
    {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},
    {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
    {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
    {'W',{0x11,0x11,0x11,0x15,0x15,0x1B,0x11}},
    {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
    {'Y',{0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
    {'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
    {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
    {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
    {'2',{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
    {'3',{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
    {'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
    {'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
    {'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
    {'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    {'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02}},
    {')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
    {'+',{0x00,0x04,0x04,0x1F,0x04,0x04,0x00}},
    {'-',{0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
    {'*',{0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00}},
    {'/',{0x01,0x02,0x02,0x04,0x08,0x08,0x10}},
    {'=',{0x00,0x1F,0x00,0x1F,0x00,0x00,0x00}},
    {'.',{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}},
    {':',{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}},
    {'>',{0x10,0x08,0x04,0x02,0x04,0x08,0x10}},
    {'<',{0x02,0x04,0x08,0x10,0x08,0x04,0x02}},
    {'_',{0x00,0x00,0x00,0x00,0x00,0x00,0x1F}},
    {'!',{0x04,0x04,0x04,0x04,0x04,0x00,0x04}},
};

static const uint8_t* ws_glyph(uint8_t ch)
{
    unsigned i;
    if (ch >= 'a' && ch <= 'z') ch = (uint8_t)(ch - 32);
    for (i = 0; i < sizeof(sGlyphs) / sizeof(sGlyphs[0]); ++i)
        if (sGlyphs[i].ch == ch) return sGlyphs[i].rows;
    return 0;   /* space / unsupported -> blank */
}

/* ---- setup / teardown --------------------------------------------------- */
int ws_open(void)
{
    int base, size;

    if (sOpen) return 0;
    base = exec2(QE_EXEC_WS_BASE, 0, 0);
    size = exec2(QE_EXEC_WS_SIZE, 0, 0);
    if (base <= 0) return -1;
    sSh = (struct ws_shared*)base;
    if (sSh->hdr.magic != WS_MAGIC || sSh->hdr.version != WS_VERSION)
        { sSh = 0; return -2; }
    if (size > 0 && (unsigned)size < sizeof(struct ws_shared)) { sSh = 0; return -3; }
    sOwnCount = 0;
    { unsigned k; for (k = 0; k < WS_MAX_WINDOWS; ++k) sOwned[k] = -1; }
    sOpen = 1;
    return 0;
}

int ws_window_count(void) { return (int)WS_MAX_WINDOWS; }

int ws_server_ready(void)
{
    return (sSh && sSh->hdr.server_pid != 0u);
}

unsigned char* ws_content(int wid)
{
    if (!sSh || wid < 0 || (unsigned)wid >= WS_MAX_WINDOWS) return 0;
    return (unsigned char*)sSh + sSh->hdr.wins[wid].content_offset;
}

int ws_window_geom(int wid, int* x, int* y, int* w, int* h)
{
    const struct ws_window* win;
    if (!sSh || wid < 0 || (unsigned)wid >= WS_MAX_WINDOWS) return -1;
    win = &sSh->hdr.wins[wid];
    if (!win->pid) return -1;
    if (x) *x = win->x;
    if (y) *y = win->y;
    if (w) *w = win->w;
    if (h) *h = win->h;
    return 0;
}

int ws_window_create(int x, int y, int w, int h, const char* title)
{
    struct ws_create_params p;
    unsigned n;
    int wid;

    if (!sOpen || !sSh) return -1;
    if (w <= 0 || h <= 0) return -1;
    if ((unsigned)w > WS_CONTENT_W) w = (int)WS_CONTENT_W;
    if ((unsigned)h > WS_CONTENT_H) h = (int)WS_CONTENT_H;

    n = 0;
    while (n < sizeof(p)) ((unsigned char*)&p)[n++] = 0;
    p.pid = sPid;
    p.x = x; p.y = y; p.w = w; p.h = h;
    p.z = sOwnCount & 7u;   /* server composites z=0..7 */
    n = 0;
    if (title) while (n + 1 < WS_TITLE_LEN && title[n]) { p.title[n] = title[n]; ++n; }
    p.title[n] = 0;

    wid = exec2(QE_EXEC_WS_CTL, WSO_CREATE_WINDOW, (unsigned)&p);
    if (wid < 0 || (unsigned)wid >= WS_MAX_WINDOWS) return -1;
    sOwned[sOwnCount++] = wid;

    /* Paint a standard frame into the private buffer so the window reads as a
     * window before the app draws its own content. */
    ws_fill(wid, 0, 0, w, h, WSC_BLUE);
    ws_fill(wid, 0, 0, w, 2, WSC_LIGHTGRAY);
    ws_fill(wid, 0, h - 2, w, 2, WSC_LIGHTGRAY);
    ws_fill(wid, 0, 0, 2, h, WSC_LIGHTGRAY);
    ws_fill(wid, w - 2, 0, 2, h, WSC_LIGHTGRAY);
    ws_fill(wid, 2, 2, w - 4, 20, WSC_TITLEBAR);
    ws_text(wid, 5, 3, title ? title : "WINDOW", WSC_WHITE);
    ws_fill(wid, w - 22, 4, 18, 16, WSC_GRAY);
    ws_text(wid, w - 21, 4, "X", WSC_YELLOW);
    return wid;
}

void ws_close(void)
{
    int i;
    if (!sOpen) return;
    for (i = 0; i < sOwnCount; ++i)
        (void)exec2(QE_EXEC_WS_CTL, WSO_DESTROY_WINDOW, (unsigned)sOwned[i]);
    sOpen = 0;
    sSh = 0;
    sOwnCount = 0;
}

/* ---- software rasteriser (private content buffer only) ------------------ */
static void ws_pixel(unsigned char* buf, int bw, int bh,
                     int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= bw || y >= bh) return;
    *(uint32_t*)(buf + (unsigned)y * (unsigned)bw * 4u + (unsigned)x * 4u) = color;
}

static unsigned char* ws_buf_for(int wid, int* bw, int* bh)
{
    int i;
    if (!sSh || wid < 0 || (unsigned)wid >= WS_MAX_WINDOWS) return 0;
    for (i = 0; i < sOwnCount; ++i)
        if (sOwned[i] == wid)
            {
            if (bw) *bw = (int)sSh->hdr.wins[wid].w;
            if (bh) *bh = (int)sSh->hdr.wins[wid].h;
            return ws_content(wid);
            }
    return 0;
}

void ws_fill(int wid, int x, int y, int w, int h, uint32_t color)
{
    int bw, bh, px, py;
    unsigned char* buf = ws_buf_for(wid, &bw, &bh);
    if (!buf || w <= 0 || h <= 0) return;
    for (py = y; py < y + h; ++py)
        for (px = x; px < x + w; ++px)
            ws_pixel(buf, WS_CONTENT_W, bh, px, py, color);
    if (sSh) ++sSh->hdr.wins[wid].dirty;
}

void ws_text(int wid, int x, int y, const char* s, uint32_t fg)
{
    int bw, bh, i, row, col;
    unsigned char* buf = ws_buf_for(wid, &bw, &bh);
    if (!buf || !s) return;
    for (i = 0; s[i]; ++i)
        {
        const uint8_t* g = ws_glyph((uint8_t)s[i]);
        int gx = x + i * WS_CELL_W;
        if (!g) continue;
        for (row = 0; row < 7; ++row)
            {
            unsigned bits = (unsigned)g[row] << 2;   /* 8 px wide */
            for (col = 0; col < 8; ++col)
                if (bits & (1u << (7 - col)))
                    {
                    ws_pixel(buf, WS_CONTENT_W, bh, gx + col * 2, y + row * 2, fg);
                    ws_pixel(buf, WS_CONTENT_W, bh, gx + col * 2 + 1, y + row * 2, fg);
                    ws_pixel(buf, WS_CONTENT_W, bh, gx + col * 2, y + row * 2 + 1, fg);
                    ws_pixel(buf, WS_CONTENT_W, bh, gx + col * 2 + 1, y + row * 2 + 1, fg);
                    }
            }
        }
    if (sSh) ++sSh->hdr.wins[wid].dirty;
}

void ws_button(int wid, int x, int y, int w, int h,
               const char* label, int hover)
{
    unsigned n;
    int tx, ty, tw;
    uint32_t face = hover ? WSC_TITLEBAR : WSC_GRAY;

    if (w < 16 || h < 16) return;
    ws_fill(wid, x, y, w, h, face);
    ws_fill(wid, x, y, w, 2, hover ? WSC_YELLOW : WSC_LIGHTGRAY);
    ws_fill(wid, x, y, 2, h, hover ? WSC_YELLOW : WSC_LIGHTGRAY);
    ws_fill(wid, x, y + h - 2, w, 2, 0x00404040u);
    ws_fill(wid, x + w - 2, y, 2, h, 0x00404040u);
    n = ws_strlen(label);
    tw = (int)n * WS_CELL_W;
    tx = x + (w - tw) / 2;
    if (tx < x + 3) tx = x + 3;
    ty = y + (h - WS_CELL_H) / 2;
    if (ty < y + 3) ty = y + 3;
    ws_text(wid, tx, ty, label, hover ? WSC_YELLOW : WSC_WHITE);
}

void ws_present(int wid)
{
    if (!sSh || wid < 0 || (unsigned)wid >= WS_MAX_WINDOWS) return;
    (void)exec2(QE_EXEC_WS_CTL, WSO_SET_DIRTY, (unsigned)wid);
}

/* ---- input -------------------------------------------------------------- */
int ws_get_event(struct ws_event* e)
{
    struct ws_event local;
    int rc;

    if (!sSh) return 0;
    rc = exec2(QE_EXEC_WS_CTL, WSO_POP_EVENT, (unsigned)&local);
    if (rc == 0)
        {
        if (e) *e = local;
        return 1;
        }

    /* Cooperative fallback: read the serial keyboard ourselves so a client
     * stays interactive even when it holds the console lease. */
    {
    int c = con_tryread();
    if (c >= 0 && sOwnCount > 0)
        {
        if (!e) e = &local;
        e->type = WSE_KEY;
        e->x = -1; e->y = -1;
        e->key = (unsigned)(unsigned char)c;
        e->target_wid = sOwned[0];
        return 1;
        }
    }
    return 0;
}

/* ---- helpers ------------------------------------------------------------ */
uint32_t ws_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* ---- E32 migration stubs (docs/WINSRV_MIGRATION_PLAN.md) ------------------
 * The R1 shim window server has no set-title / set-size / screen-size op, so
 * the old exec backend cannot answer these; the genuine EWSRV path
 * (ws_e32_client.c) implements them over the CWsSession opcodes. */
int ws_set_title(int wid, const char* title)
{
    (void)wid; (void)title;
    return -1;
}

int ws_set_size(int wid, int w, int h)
{
    (void)wid; (void)w; (void)h;
    return -1;
}

int ws_screen_size(int* w, int* h)
{
    if (w) *w = 0;
    if (h) *h = 0;
    return -1;
}

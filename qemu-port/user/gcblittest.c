/*
 * gcblittest.c -- PE2 E32 bulk-frame proof, WINSRV_MIGRATION_PLAN chunk C.
 *
 * Old (winsrv exec-251) path: allocate a 640x400 RGBA8888 frame in the
 * chunk-backed user heap, publish its physical base/size in the old shared
 * buffer's tail fields, submit ONE BLIT display-list command; the kernel
 * blitted all 1,024,000 bytes to the VBE LFB in ring 0.
 *
 * New (genuine EWSRV) path: the same 640x400 RGBA frame is still allocated,
 * filled with the same deterministic gradient and verified in user mode
 * (heap + content proof unchanged), then published to the genuine EWSRV
 * (!E32WindowServer) console window as 80x25 cells: each cell covers an
 * 8x16 pixel block of the frame, averaged to the nearest genuine palette
 * colour and drawn with EConsoleSetTextColors(21) + EConsoleSetCursorPosAbs
 * (6) + EConsoleWrite(10) -- the chunk-2 fill path.  80*25 = 2000 cells
 * through genuine IPC per frame replace the single kernel-side BLIT.
 * Zero winsrv execs (249/250/251) remain in this file.
 *
 * Transport = the user/ws_e32_client.c (chunk A) pattern: RSessionBase::
 * CreateSession(_L("!E32WindowServer"), TVersion(1,0,4016), 3) +
 * DoSendReceive with the genuine CWsSession opcodes, importing EUSER.DLL
 * ordinals 211/1547/120/915 through four PE2 IAT slots at codeBase+0x8000
 * (user.mk --defsym seeds, genuine/pack_multi.py --ordinals writes).  The
 * per-cell colour write rides the same session; ws_text/ws_present are
 * still stubs in ws_e32_client.c (chunk 3), so the cell writer is
 * implemented locally here.
 *
 * Serial markers keep the old "GB: BLIT ..." prefixes.  The old
 * "GB: BLIT submitted cmds=1" / kernel-side "GCB:" markers become
 * "GB: BLIT submitted cells=2000 err=0" (tools re-point at gate time).
 *
 * The frame is submitted a second time after the DONE marker so the
 * console's text mirror cannot smear the render before the QEMU screendump.
 * The program then sleeps forever; the test harness owns the VM lifetime.
 */
#include "runtime.h"

/* ---- PE2 EUSER import slots (absolute symbols, seeded by user.mk) --------
 * slot 0 @ 0x408000  ord 211  RSessionBase::CreateSession(TDesC16,TVersion,int)
 * slot 1 @ 0x408004  ord 1547 RSessionBase::DoSendReceive(int,TIpcArgs const*)
 * slot 2 @ 0x408008  ord 120  RHandleBase::Close()
 * slot 3 @ 0x40800c  ord 915  RDebug::Printf(char const*,...)              */
extern unsigned gcblittest_iat_connect;
extern unsigned gcblittest_iat_send;
extern unsigned gcblittest_iat_close;
extern unsigned gcblittest_iat_printf;

typedef int  (*gb_t_connect)(void* aThis, const void* aName, const void* aVer, int aSlots);
typedef int  (*gb_t_send)(const void* aThis, int aOp, const void* aArgs);
typedef void (*gb_t_close)(void* aThis);

static int gb_connect(void* aThis, const void* aName, const void* aVer, int aSlots)
{
    return ((gb_t_connect)(*(unsigned*)&gcblittest_iat_connect))(aThis, aName, aVer, aSlots);
}
static int gb_send_raw(const void* aThis, int aOp, const void* aArgs)
{
    return ((gb_t_send)(*(unsigned*)&gcblittest_iat_send))(aThis, aOp, aArgs);
}
static void gb_close_raw(void* aThis)
{
    ((gb_t_close)(*(unsigned*)&gcblittest_iat_close))(aThis);
}

/* Serial marker through genuine EUSER.DLL ord 915 (RDebug::Printf). */
static void gb_marker(const char* text)
{
    unsigned fn = *(unsigned*)&gcblittest_iat_printf;
    if (fn != 915u && fn != 0u) ((void (*)(const char*))fn)(text);  /* ord 915 */
    else print(text);                                                 /* fallback */
}

/* ---- tiny formatters (no hosted libc) ---- */
static unsigned append(char* p, unsigned k, const char* s)
{
    while (*s) p[k++] = *s++;
    return k;
}
static unsigned dec(char* p, unsigned k, int v)
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
static unsigned hex32(char* p, unsigned k, unsigned v)
{
    const char* h = "0123456789abcdef";
    k = append(p, k, "0x");
    for (int i = 28; i >= 0; i -= 4) p[k++] = h[(v >> i) & 0xfu];
    return k;
}

/* ---- genuine EKA2 ABI shapes (kernelhwsrv/, read-only; as ws_e32_client.c) */
struct gb_desC16 { unsigned iLength; const unsigned short* iPtr; };
struct gb_desC8  { unsigned iLength; const unsigned char*  iPtr; };
struct gb_version{ signed char iMajor; signed char iMinor; short iBuild; };
struct gb_ipcargs{ int iArgs[4]; int iFlags; };
struct gb_point  { int iX, iY; };
struct gb_size   { int iWidth, iHeight; };
struct gb_session{ int iHandle; };

/* CWsSession opcodes (kernelhwsrv/kernel/eka/include/ws_std.h, enum from 0). */
enum
    {
    GB_OP_CONSOLE_CREATE          = 0,   /* EConsoleCreate                 */
    GB_OP_CONSOLE_SET             = 1,   /* EConsoleSet(title, size)       */
    GB_OP_CONSOLE_SET_POS_ABS     = 4,   /* EConsoleSetWindowPosAbs        */
    GB_OP_CONSOLE_SET_CURSOR_ABS  = 6,   /* EConsoleSetCursorPosAbs        */
    GB_OP_CONSOLE_WRITE           = 10,  /* EConsoleWrite                  */
    GB_OP_CONSOLE_DESTROY         = 13,  /* EConsoleDestroy                */
    GB_OP_CONSOLE_SET_TEXT_COLORS = 21   /* EConsoleSetTextColors(fg,bg)   */
    };

#define GB_EDESC_C16 7
#define GB_EDESC_C8  6

static const unsigned short sServerName[] =
    { '!','E','3','2','W','i','n','d','o','w','S','e','r','v','e','r' };
#define GB_SERVER_NAME_LEN 16u
static const struct gb_desC16 sServerNameDesc =
    { GB_SERVER_NAME_LEN, sServerName };
static const struct gb_version sWsVersion = { 1, 0, 4016 };
#define GB_MESSAGE_SLOTS 3

/* ---- state -------------------------------------------------------------- */
static struct gb_session sSession;
static int sOpen;
static int gErr;

static void gb_args0(struct gb_ipcargs* a)
{
    a->iArgs[0] = a->iArgs[1] = a->iArgs[2] = a->iArgs[3] = 0;
    a->iFlags = 0;
}

static int gb_send(int aOp, struct gb_ipcargs* aArgs)
{
    if (!sOpen) return -1;
    return gb_send_raw(&sSession, aOp, aArgs);
}

#define GB_TEXT_MAX 16u
static unsigned short sTextBuf[GB_TEXT_MAX];
static struct gb_desC16 sTextDesc;

static unsigned gb_utf16(const char* s, unsigned short* buf, unsigned max)
{
    unsigned n = 0;
    if (s)
        while (n < max && s[n])
            { buf[n] = (unsigned short)(unsigned char)s[n]; ++n; }
    buf[n] = 0;
    return n;
}

/* ---- 0x00RRGGBB -> nearest genuine Palette256 index (as ws_e32_client.c) */
static unsigned gb_nearest_level(unsigned c)
{
    static const unsigned char L[6] = { 0x00, 0x33, 0x66, 0x99, 0xcc, 0xff };
    unsigned best = 0, bd = 0xffffffffu, i;
    for (i = 0; i < 6u; ++i)
        {
        unsigned d = c > L[i] ? c - L[i] : L[i] - c;
        if (d < bd) { bd = d; best = i; }
        }
    return best;
}
static unsigned gb_rgb_to_index(unsigned rgb)
{
    unsigned r = (rgb >> 16) & 0xffu, g = (rgb >> 8) & 0xffu, b = rgb & 0xffu;
    unsigned rl = gb_nearest_level(r), gl = gb_nearest_level(g);
    unsigned bl = gb_nearest_level(b);
    return (bl < 3u ? bl * 36u : 40u + bl * 36u) + gl * 6u + rl;
}

static unsigned gb_rgb(unsigned r, unsigned g, unsigned b)
    { return ((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu); }

/* ---- session / window / cell writer over the genuine EWSRV --------------- */
static int gb_open(void)
{
    int r;
    if (sOpen) return 0;
    sSession.iHandle = 0;
    r = gb_connect(&sSession, &sServerNameDesc, &sWsVersion, GB_MESSAGE_SLOTS);
    if (r != 0) { sSession.iHandle = 0; return r; }
    if (sSession.iHandle == 0) return -1;
    sOpen = 1;
    return 0;
}

static int gb_window(int x, int y, int w, int h, const char* title)
{
    struct gb_ipcargs args;
    struct gb_desC8 pckg;
    struct gb_size size;
    struct gb_point pos;
    int r;

    if (!sOpen || w <= 0 || h <= 0) return -1;

    gb_args0(&args);
    r = gb_send(GB_OP_CONSOLE_CREATE, &args);
    if (r != 0) return r;

    sTextDesc.iLength = gb_utf16(title ? title : "WINDOW",
                                 sTextBuf, GB_TEXT_MAX - 1u);
    sTextDesc.iPtr = sTextBuf;
    size.iWidth = w; size.iHeight = h;
    pckg.iLength = sizeof(size);
    pckg.iPtr = (const unsigned char*)&size;
    args.iArgs[0] = (int)&sTextDesc;
    args.iArgs[1] = (int)&pckg;
    args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (GB_EDESC_C16 << 0) | (GB_EDESC_C8 << 3);
    r = gb_send(GB_OP_CONSOLE_SET, &args);
    if (r != 0) return r;

    pos.iX = x; pos.iY = y;
    pckg.iLength = sizeof(pos);
    pckg.iPtr = (const unsigned char*)&pos;
    args.iArgs[0] = (int)&pckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (GB_EDESC_C8 << 0);
    return gb_send(GB_OP_CONSOLE_SET_POS_ABS, &args);
}

static void gb_close(void)
{
    struct gb_ipcargs args;
    if (!sOpen) return;
    gb_args0(&args);
    (void)gb_send(GB_OP_CONSOLE_DESTROY, &args);
    gb_close_raw(&sSession);
    sSession.iHandle = 0;
    sOpen = 0;
}

static void gb_goto(int x, int y)
{
    struct gb_ipcargs args;
    struct gb_desC8 pckg;
    struct gb_point pos;
    pos.iX = x; pos.iY = y;
    pckg.iLength = sizeof(pos);
    pckg.iPtr = (const unsigned char*)&pos;
    args.iArgs[0] = (int)&pckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (GB_EDESC_C8 << 0);
    if (gb_send(GB_OP_CONSOLE_SET_CURSOR_ABS, &args)) ++gErr;
}

static void gb_colors_idx(unsigned idx)
{
    struct gb_ipcargs args;
    gb_args0(&args);
    args.iArgs[0] = (int)idx;             /* fg (irrelevant for spaces) */
    args.iArgs[1] = (int)idx;             /* bg -> solid cell fill      */
    if (gb_send(GB_OP_CONSOLE_SET_TEXT_COLORS, &args)) ++gErr;
}

static void gb_write(const char* s)
{
    struct gb_ipcargs args;
    sTextDesc.iLength = gb_utf16(s, sTextBuf, GB_TEXT_MAX - 1u);
    sTextDesc.iPtr = sTextBuf;
    args.iArgs[0] = (int)&sTextDesc;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (GB_EDESC_C16 << 0);
    if (gb_send(GB_OP_CONSOLE_WRITE, &args)) ++gErr;
}

/* ---- frame geometry: one DOOM-class 640x400 RGBA frame ---- */
#define BLIT_W        640u
#define BLIT_H        400u
#define BLIT_SIZE     (BLIT_W * BLIT_H * 4u)   /* 1,024,000 bytes */

/* Console render geometry: 80x25 cells, each cell = an 8x16 pixel block. */
#define CELL_W 80u
#define CELL_H 25u
#define BLK_W  (BLIT_W / CELL_W)   /* 8  */
#define BLK_H  (BLIT_H / CELL_H)   /* 16 */

static void put_pixel(volatile unsigned char* p, unsigned x, unsigned y,
                      unsigned r, unsigned g, unsigned b)
{
    unsigned off = (y * BLIT_W + x) * 4u;
    p[off]      = (unsigned char)r;
    p[off + 1u] = (unsigned char)g;
    p[off + 2u] = (unsigned char)b;
    p[off + 3u] = 0xffu;
}

/* Deterministic gradient: R=x&0xff, G=y&0xff, B=(x+y)&0xff, plus a white
 * 4px border.  Centre pixel (320,200) is exactly (64,200,8). */
static void fill_frame(volatile unsigned char* p)
{
    unsigned x, y, t;
    for (y = 0; y < BLIT_H; ++y)
        for (x = 0; x < BLIT_W; ++x)
            put_pixel(p, x, y, x & 0xffu, y & 0xffu, (x + y) & 0xffu);
    for (x = 0; x < BLIT_W; ++x)
        for (t = 0; t < 4u; ++t)
            { put_pixel(p, x, t, 255u,255u,255u);
              put_pixel(p, x, BLIT_H - 1u - t, 255u,255u,255u); }
    for (y = 0; y < BLIT_H; ++y)
        for (t = 0; t < 4u; ++t)
            { put_pixel(p, t, y, 255u,255u,255u);
              put_pixel(p, BLIT_W - 1u - t, y, 255u,255u,255u); }
}

/* Publish the whole 640x400 frame to the genuine EWSRV window as 80x25
 * solid cells (the bulk-display replacement for the one BLIT exec-251
 * command).  Returns the number of failed IPC ops. */
static int render_frame(const volatile unsigned char* buf)
{
    unsigned cx, cy, x, y;
    int err0 = gErr;

    for (cy = 0; cy < CELL_H; ++cy)
        {
        gb_goto(0, (int)cy);
        for (cx = 0; cx < CELL_W; ++cx)
            {
            /* average the BLK_W x BLK_H pixel block (cx*BLK_W, cy*BLK_H) */
            unsigned sr = 0, sg = 0, sb = 0;
            for (y = 0; y < BLK_H; ++y)
                for (x = 0; x < BLK_W; ++x)
                    {
                    unsigned off =
                        ((cy * BLK_H + y) * BLIT_W + cx * BLK_W + x) * 4u;
                    sr += buf[off];
                    sg += buf[off + 1u];
                    sb += buf[off + 2u];
                    }
            sr /= BLK_W * BLK_H;
            sg /= BLK_W * BLK_H;
            sb /= BLK_W * BLK_H;
            gb_colors_idx(gb_rgb_to_index(gb_rgb(sr, sg, sb)));
            gb_write(" ");             /* one solid cell; cursor advances */
            }
        }
    return gErr - err0;
}

int app_main(void)
{
    char line[160];
    unsigned k;
    int r, e;
    volatile unsigned char* buf;

    gb_marker("GB: BLIT TEST READY");

    buf = (volatile unsigned char*)malloc(BLIT_SIZE);
    if (!buf)
        {
        gb_marker("GB: BLIT FAIL malloc");
        return 1;
        }

    k = append(line, 0, "GB: BLIT buf=");
    k = hex32(line, k, (unsigned)buf);
    k = append(line, k, " size=");
    k = dec(line, k, (int)BLIT_SIZE);
    line[k] = 0;
    gb_marker(line);

    fill_frame(buf);

    /* Centre pixel (320,200) must be exactly (64,200,8,255). */
    {
    unsigned off = (200u * BLIT_W + 320u) * 4u;
    if (buf[off] == 64u && buf[off + 1u] == 200u &&
        buf[off + 2u] == 8u && buf[off + 3u] == 0xffu)
        gb_marker("GB: BLIT centre px ok");
    else
        gb_marker("GB: BLIT centre px FAIL");
    }

    r = gb_open();
    k = append(line, 0, "GB: EWSRV Connect r="); k = dec(line, k, r);
    line[k] = 0;
    gb_marker(line);
    if (r != 0) return 1;

    r = gb_window(4, 4, (int)CELL_W, (int)CELL_H, "GB: BLITTEST");
    k = append(line, 0, "GB: Window r="); k = dec(line, k, r);
    line[k] = 0;
    gb_marker(line);
    if (r != 0) { gb_close(); return 1; }

    /* Low RAM is identity-mapped U/S=1 in user PDs, so the virtual pointer
     * is the physical address (0x1000..0x80000000) -- reported as src. */
    k = append(line, 0, "GB: BLIT src=");
    k = hex32(line, k, (unsigned)buf);
    k = append(line, k, " size=");
    k = dec(line, k, (int)BLIT_SIZE);
    k = append(line, k, " w=640 h=400");
    line[k] = 0;
    gb_marker(line);

    e = render_frame(buf);
    k = append(line, 0, "GB: BLIT submitted cells=2000 err=");
    k = dec(line, k, e);
    line[k] = 0;
    gb_marker(line);
    gb_marker("GB: BLIT TEST DONE");

    /* Re-submit as the final action: repaints every cell so any console
     * text mirrored by the markers above cannot smear the frame before the
     * QEMU screendump. */
    (void)render_frame(buf);

    for (;;) sleep_ms(100);
    return 0;
}

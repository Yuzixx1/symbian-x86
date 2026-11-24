/*
 * saver.c -- PE2 E32 SCREENSAVER / ANIMATION for the R1 QEMU port.
 *
 * WINSRV_MIGRATION_PLAN chunk C: migrated off the old winsrv display-list
 * execs (249/250/251) onto the GENUINE EWSRV via the
 * user/ws_e32_client.c transport pattern (chunk A, landed): a plain EUSER
 * RSessionBase to !E32WindowServer + DoSendReceive with the genuine
 * CWsSession console opcodes of kernelhwsrv/kernel/eka/include/ws_std.h.
 * Zero winsrv execs remain in this file.
 *
 * Old -> new primitive map:
 *   exec 249/250 (LFB phys base / byte size) -> EConsoleScreenSize (op 17),
 *                                               reported in char cells
 *   exec 251 arg1==0 (old shared buffer)     -> the EWSRV session itself;
 *                                               no shared buffer exists
 *   exec 251 arg1>0, CLEAR                   -> EConsoleClearScreen (op 2)
 *   exec 251 RECT                            -> EConsoleSetTextColors (21)
 *                                               + EConsoleSetCursorPosAbs (6)
 *                                               + EConsoleWrite (10) of space
 *                                               cells (chunk-2 fill path,
 *                                               ws_draw_fill_rect pattern)
 *   exec 251 PUTC                            -> SetTextColors (21) +
 *                                               SetCursorPosAbs (6) +
 *                                               EConsoleWrite (10) of the
 *                                               glyph run.  ws_text is still
 *                                               a stub in ws_e32_client.c
 *                                               (chunk 3), so the text run
 *                                               is implemented locally here.
 *
 * The animation is expressed in character cells on the genuine console
 * window (the only window type this tree's genuine EWSRV serves): the same
 * three styles across six short frames -- a bouncing coloured box with an
 * inner highlight, a shifting starfield, and a colour-gradient sweep.
 *
 * Transport imports: the four EUSER exports are imported from the prebuilt
 * genuine EUSER.DLL through PE2 IAT slots at codeBase+0x8000, seeded by
 * user.mk --defsym and packed by genuine/pack_multi.py (ordinals
 * 211,1547,120,915).  Serial markers use ord 915 (RDebug::Printf) with the
 * port's DebugPrint executive as fallback.
 */
#include "runtime.h"

/* ---- PE2 EUSER import slots (absolute symbols, seeded by user.mk) --------
 * slot 0 @ 0x408000  ord 211  RSessionBase::CreateSession(TDesC16,TVersion,int)
 * slot 1 @ 0x408004  ord 1547 RSessionBase::DoSendReceive(int,TIpcArgs const*)
 * slot 2 @ 0x408008  ord 120  RHandleBase::Close()
 * slot 3 @ 0x40800c  ord 915  RDebug::Printf(char const*,...)
 * The kernel loader overwrites each slot with the genuine export address at
 * load time, so reading the slot at run time yields a callable pointer. */
extern unsigned saver_iat_connect;
extern unsigned saver_iat_send;
extern unsigned saver_iat_close;
extern unsigned saver_iat_printf;

typedef int  (*sv_t_connect)(void* aThis, const void* aName, const void* aVer, int aSlots);
typedef int  (*sv_t_send)(const void* aThis, int aOp, const void* aArgs);
typedef void (*sv_t_close)(void* aThis);

static int sv_connect(void* aThis, const void* aName, const void* aVer, int aSlots)
{
    return ((sv_t_connect)(*(unsigned*)&saver_iat_connect))(aThis, aName, aVer, aSlots);
}
static int sv_send_raw(const void* aThis, int aOp, const void* aArgs)
{
    return ((sv_t_send)(*(unsigned*)&saver_iat_send))(aThis, aOp, aArgs);
}
static void sv_close_raw(void* aThis)
{
    ((sv_t_close)(*(unsigned*)&saver_iat_close))(aThis);
}

/* Serial marker through genuine EUSER.DLL ord 915 (RDebug::Printf). */
static void sv_marker(const char* text)
{
    unsigned fn = *(unsigned*)&saver_iat_printf;
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

/* ---- genuine EKA2 ABI shapes (kernelhwsrv/, read-only; as ws_e32_client.c)
 * TDesC16/TPtrC16, TPckgC (TDesC8), TPckg (TDes8), TVersion, TIpcArgs,
 * TPoint, TSize and RHandleBase layouts from e32cmn.h + e32des16.h +
 * e32std.h. */
struct sv_desC16 { unsigned iLength; const unsigned short* iPtr; };
struct sv_desC8  { unsigned iLength; const unsigned char*  iPtr; };
struct sv_des8   { unsigned iLength; unsigned char*        iPtr; };
struct sv_version{ signed char iMajor; signed char iMinor; short iBuild; };
struct sv_ipcargs{ int iArgs[4]; int iFlags; };   /* KMaxMessageArguments=4 */
struct sv_point  { int iX, iY; };
struct sv_size   { int iWidth, iHeight; };
struct sv_session{ int iHandle; };               /* RHandleBase/RSessionBase */

/* CWsSession opcodes (kernelhwsrv/kernel/eka/include/ws_std.h, enum from 0). */
enum
    {
    SV_OP_CONSOLE_CREATE          = 0,   /* EConsoleCreate                 */
    SV_OP_CONSOLE_SET             = 1,   /* EConsoleSet(title, size)       */
    SV_OP_CONSOLE_CLEAR           = 2,   /* EConsoleClearScreen            */
    SV_OP_CONSOLE_SET_POS_ABS     = 4,   /* EConsoleSetWindowPosAbs        */
    SV_OP_CONSOLE_SET_CURSOR_ABS  = 6,   /* EConsoleSetCursorPosAbs        */
    SV_OP_CONSOLE_WRITE           = 10,  /* EConsoleWrite                  */
    SV_OP_CONSOLE_DESTROY         = 13,  /* EConsoleDestroy                */
    SV_OP_CONSOLE_SET_TITLE       = 14,  /* EConsoleSetTitle               */
    SV_OP_CONSOLE_SCREEN_SIZE     = 17,  /* EConsoleScreenSize             */
    SV_OP_CONSOLE_SET_TEXT_COLORS = 21   /* EConsoleSetTextColors(fg,bg)   */
    };

/* TIpcArgs::TArgType values (e32cmn.h): 3 bits per slot, slot k at 3*k. */
#define SV_EDESC_C16 7   /* EDesC16 = EFlagDes|EFlagConst|EFlag16Bit */
#define SV_EDESC_C8  6   /* EDesC8  = EFlagDes|EFlagConst            */
#define SV_EDES8     4   /* EDes8   = EFlagDes                       */

/* KE32WindowServer (e32twin.h) as a 2-byte TPtrC16; KW32 version 1.0 build
 * 4016 (KE32BuildVersionNumber); KMessageSlots=3 -- as in ws_e32_client.c. */
static const unsigned short sServerName[] =
    { '!','E','3','2','W','i','n','d','o','w','S','e','r','v','e','r' };
#define SV_SERVER_NAME_LEN 16u
static const struct sv_desC16 sServerNameDesc =
    { SV_SERVER_NAME_LEN, sServerName };
static const struct sv_version sWsVersion = { 1, 0, 4016 };
#define SV_MESSAGE_SLOTS 3

/* ---- state -------------------------------------------------------------- */
static struct sv_session sSession;   /* iHandle; 0 = KNullHandle */
static int sOpen;
static int gErr;                     /* non-zero IPC returns during drawing */

static void sv_args0(struct sv_ipcargs* a)
{
    a->iArgs[0] = a->iArgs[1] = a->iArgs[2] = a->iArgs[3] = 0;
    a->iFlags = 0;
}

static int sv_send(int aOp, struct sv_ipcargs* aArgs)
{
    if (!sOpen) return -1;
    return sv_send_raw(&sSession, aOp, aArgs);
}

/* ---- ASCII -> UTF-16 run (what _L() produces under -m32) ---------------- */
#define SV_TEXT_MAX 80u
static unsigned short sTextBuf[SV_TEXT_MAX];
static struct sv_desC16 sTextDesc;

static unsigned sv_utf16(const char* s, unsigned short* buf, unsigned max)
{
    unsigned n = 0;
    if (s)
        while (n < max && s[n])
            { buf[n] = (unsigned short)(unsigned char)s[n]; ++n; }
    buf[n] = 0;
    return n;
}

/* ---- 0x00RRGGBB -> nearest genuine Palette256 index -----------------------
 * Palette256 (kernelhwsrv/.../edisp/epoc/generic/wd_generic.cpp) is four
 * 36-entry cubes over levels {0,33,66,99,cc,ff} (value 0x00BBGGRR, red
 * fastest) with 40 gray/special entries at 108..147, so a cube cell is
 * bl*36+gl*6+rl for bl<3 and 40+bl*36+gl*6+rl for bl>=3. */
static unsigned sv_nearest_level(unsigned c)
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
static unsigned sv_rgb_to_index(unsigned rgb)
{
    unsigned r = (rgb >> 16) & 0xffu, g = (rgb >> 8) & 0xffu, b = rgb & 0xffu;
    unsigned rl = sv_nearest_level(r), gl = sv_nearest_level(g);
    unsigned bl = sv_nearest_level(b);
    return (bl < 3u ? bl * 36u : 40u + bl * 36u) + gl * 6u + rl;
}

/* ---- session / window ops over the genuine EWSRV ------------------------ */
static int sv_open(void)
{
    int r;
    if (sOpen) return 0;
    sSession.iHandle = 0;                 /* KNullHandle */
    r = sv_connect(&sSession, &sServerNameDesc, &sWsVersion, SV_MESSAGE_SLOTS);
    if (r != 0)                           /* KErrNone = 0 */
        { sSession.iHandle = 0; return r; }
    if (sSession.iHandle == 0) return -1;
    sOpen = 1;
    return 0;
}

/* EConsoleCreate + EConsoleSet(title, size-in-cells) + EConsoleSetWindowPosAbs
 * -- the chunk-A ws_window_create sequence. */
static int sv_window(int x, int y, int w, int h, const char* title)
{
    struct sv_ipcargs args;
    struct sv_desC8 pckg;
    struct sv_size size;
    struct sv_point pos;
    int r;

    if (!sOpen || w <= 0 || h <= 0) return -1;

    sv_args0(&args);
    r = sv_send(SV_OP_CONSOLE_CREATE, &args);
    if (r != 0) return r;

    sTextDesc.iLength = sv_utf16(title ? title : "WINDOW",
                                 sTextBuf, SV_TEXT_MAX - 1u);
    sTextDesc.iPtr = sTextBuf;
    size.iWidth = w; size.iHeight = h;
    pckg.iLength = sizeof(size);
    pckg.iPtr = (const unsigned char*)&size;
    args.iArgs[0] = (int)&sTextDesc;
    args.iArgs[1] = (int)&pckg;
    args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (SV_EDESC_C16 << 0) | (SV_EDESC_C8 << 3);
    r = sv_send(SV_OP_CONSOLE_SET, &args);
    if (r != 0) return r;

    pos.iX = x; pos.iY = y;
    pckg.iLength = sizeof(pos);
    pckg.iPtr = (const unsigned char*)&pos;
    args.iArgs[0] = (int)&pckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (SV_EDESC_C8 << 0);
    return sv_send(SV_OP_CONSOLE_SET_POS_ABS, &args);
}

static int sv_set_title(const char* title)
{
    struct sv_ipcargs args;
    if (!sOpen) return -1;
    sTextDesc.iLength = sv_utf16(title ? title : "", sTextBuf, SV_TEXT_MAX - 1u);
    sTextDesc.iPtr = sTextBuf;
    args.iArgs[0] = (int)&sTextDesc;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (SV_EDESC_C16 << 0);
    return sv_send(SV_OP_CONSOLE_SET_TITLE, &args);
}

/* EConsoleScreenSize: the screen geometry in character cells -- the
 * replacement for the old exec 249/250 LFB base/size probes. */
static int sv_screen_size(int* w, int* h)
{
    struct sv_ipcargs args;
    struct sv_des8 pckg;                  /* TPckg<TSize> as TDes8* */
    struct sv_size size;
    int r;
    if (!sOpen) return -1;
    size.iWidth = 0; size.iHeight = 0;
    pckg.iLength = sizeof(size);
    pckg.iPtr = (unsigned char*)&size;
    args.iArgs[0] = (int)&pckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (SV_EDES8 << 0);
    r = sv_send(SV_OP_CONSOLE_SCREEN_SIZE, &args);
    if (r == 0)
        {
        if (w) *w = size.iWidth;
        if (h) *h = size.iHeight;
        }
    return r;
}

static void sv_close(void)
{
    struct sv_ipcargs args;
    if (!sOpen) return;
    sv_args0(&args);
    (void)sv_send(SV_OP_CONSOLE_DESTROY, &args);
    sv_close_raw(&sSession);              /* RHandleBase::Close */
    sSession.iHandle = 0;
    sOpen = 0;
}

/* ---- draw primitives over the console ops --------------------------------
 * Each primitive is the genuine CWsSession sequence the server executes
 * synchronously; failures are counted in gErr. */
static void sv_clear(void)
{
    struct sv_ipcargs args;
    sv_args0(&args);
    if (sv_send(SV_OP_CONSOLE_CLEAR, &args)) ++gErr;
}

static void sv_colors_idx(unsigned fg, unsigned bg)
{
    struct sv_ipcargs args;
    sv_args0(&args);
    args.iArgs[0] = (int)fg;              /* plain EInt slots, iFlags=0 */
    args.iArgs[1] = (int)bg;
    if (sv_send(SV_OP_CONSOLE_SET_TEXT_COLORS, &args)) ++gErr;
}

static void sv_goto(int x, int y)
{
    struct sv_ipcargs args;
    struct sv_desC8 pckg;
    struct sv_point pos;
    pos.iX = x; pos.iY = y;
    pckg.iLength = sizeof(pos);
    pckg.iPtr = (const unsigned char*)&pos;
    args.iArgs[0] = (int)&pckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (SV_EDESC_C8 << 0);
    if (sv_send(SV_OP_CONSOLE_SET_CURSOR_ABS, &args)) ++gErr;
}

/* EConsoleWrite of a NUL-terminated ASCII string as one UTF-16 TPtrC16. */
static void sv_write(const char* s)
{
    struct sv_ipcargs args;
    sTextDesc.iLength = sv_utf16(s, sTextBuf, SV_TEXT_MAX - 1u);
    sTextDesc.iPtr = sTextBuf;
    args.iArgs[0] = (int)&sTextDesc;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (SV_EDESC_C16 << 0);
    if (sv_send(SV_OP_CONSOLE_WRITE, &args)) ++gErr;
}

/* Filled rectangle of w x h character cells (chunk-2 ws_draw_fill_rect
 * pattern): SetTextColors(idx,idx) then per row SetCursorPosAbs + Write of
 * w space cells; a space glyph fills the whole 8x10 cell with the bg
 * colour.  This replaces the old RECT display-list command. */
#define SV_ROW_MAX 128u
static unsigned short sRowBuf[SV_ROW_MAX];

static void sv_fill_rect(int x, int y, int w, int h, unsigned rgb)
{
    struct sv_ipcargs args;
    struct sv_desC8 posPckg;
    struct sv_point pos;
    unsigned idx = sv_rgb_to_index(rgb);
    int row, k, r;

    if (w <= 0 || h <= 0) return;
    if ((unsigned)w > SV_ROW_MAX) w = (int)SV_ROW_MAX;

    sv_colors_idx(idx, idx);
    for (k = 0; k < w; ++k) sRowBuf[k] = 0x20;   /* space: solid fill */
    sTextDesc.iLength = (unsigned)w;
    sTextDesc.iPtr = sRowBuf;

    for (row = 0; row < h; ++row)
        {
        pos.iX = x; pos.iY = y + row;
        posPckg.iLength = sizeof(pos);
        posPckg.iPtr = (const unsigned char*)&pos;
        args.iArgs[0] = (int)&posPckg;
        args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
        args.iFlags = (SV_EDESC_C8 << 0);
        r = sv_send(SV_OP_CONSOLE_SET_CURSOR_ABS, &args);
        if (r) { ++gErr; return; }

        args.iArgs[0] = (int)&sTextDesc;
        args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
        args.iFlags = (SV_EDESC_C16 << 0);
        r = sv_send(SV_OP_CONSOLE_WRITE, &args);
        if (r) { ++gErr; return; }
        }
}

/* Text run -- replaces the old PUTC command.  Implemented locally because
 * ws_text is still a stub in ws_e32_client.c (chunk 3): SetTextColors(fg,bg)
 * + SetCursorPosAbs(x,y) + EConsoleWrite(s). */
static void sv_text(int x, int y, const char* s, unsigned fg, unsigned bg)
{
    sv_colors_idx(sv_rgb_to_index(fg), sv_rgb_to_index(bg));
    sv_goto(x, y);
    sv_write(s);
}

/* ---- palette (0x00RRGGBB) ---- */
#define C_BG      0x00000080u   /* navy desktop */
#define C_TITLE   0x000080C0u
#define C_WHITE   0x00FFFFFFu
#define C_RED     0x00FF3020u
#define C_GREEN   0x0020FF40u
#define C_BLUE    0x003060FFu
#define C_YELLOW  0x00FFE040u

static unsigned sv_rgb(unsigned r, unsigned g, unsigned b)
    { return ((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu); }

/* Console window geometry in character cells. */
#define WIN_W 64
#define WIN_H 20

/* Every frame clears the window to the desktop colour and repaints the
 * title strip. */
static void begin_frame(void)
{
    sv_colors_idx(sv_rgb_to_index(C_WHITE), sv_rgb_to_index(C_BG));
    sv_clear();                            /* whole window -> C_BG      */
    sv_fill_rect(0, 0, WIN_W, 1, C_TITLE); /* title strip               */
    sv_text(1, 0, "SV: SCREENSAVER", C_WHITE, C_TITLE);
}

/* --- animation 1: a bouncing coloured box ------------------------------- */
static void frame_bounce(unsigned step)
{
    unsigned bw = 20, bh = 7;
    unsigned x = 2u + (step * (WIN_W - 4u - bw)) / 3u;
    unsigned y = 3u + (step * (WIN_H - 6u - bh)) / 3u;
    unsigned col = step & 1u ? C_GREEN : C_RED;

    begin_frame();
    sv_fill_rect((int)x, (int)y, (int)bw, (int)bh, col);
    sv_fill_rect((int)x + 3, (int)y + 2, (int)bw - 6, (int)bh - 4, C_YELLOW);
    sv_fill_rect((int)x + 6, (int)y + 4, (int)bw - 12, (int)bh - 6, col);
    sv_text((int)x, (int)(y + bh + 1u),
            step & 1u ? "BOUNCE >>" : "<< BOUNCE", C_WHITE, C_BG);
}

/* --- animation 2: a shifting starfield ---------------------------------- */
static unsigned star_seed;
static unsigned star_rand(void)
{
    star_seed = star_seed * 1664525u + 1013904223u;
    return star_seed >> 8;
}
static void frame_stars(unsigned shift)
{
    unsigned i;

    begin_frame();
    star_seed = 0x51A5u + shift * 7919u;
    for (i = 0; i < 44u; ++i)
        {
        unsigned x = 2u + star_rand() % (WIN_W - 4u);
        unsigned y = 2u + star_rand() % (WIN_H - 5u);
        unsigned col = (i & 3u) == 0u ? C_YELLOW : C_WHITE;
        sv_text((int)x, (int)y, "*", col, C_BG);
        }
    sv_text(2, WIN_H - 2, "STARFIELD", C_WHITE, C_BG);
}

/* --- animation 3: a colour-gradient sweep -------------------------------- */
static void frame_gradient(unsigned shift)
{
    unsigned i, bars = 32u, bw = WIN_W / bars;

    begin_frame();
    for (i = 0; i < bars; ++i)
        {
        unsigned phase = i * 8u + shift * 40u;
        unsigned col = sv_rgb(phase & 0xffu, (phase >> 1) & 0xffu,
                              0x40u + ((phase >> 2) & 0xbfu));
        sv_fill_rect((int)(i * bw), 2, (int)bw, WIN_H - 4, col);
        }
    sv_text(2, WIN_H - 2, "GRADIENT SWEEP", C_WHITE, C_BG);
}

/* ---- render one frame through the genuine EWSRV -------------------------- */
static void draw_frame(unsigned frame)
{
    switch (frame)
        {
        case 0: frame_bounce(0); break;
        case 1: frame_bounce(1); break;
        case 2: frame_stars(0); break;
        case 3: frame_stars(1); break;
        case 4: frame_gradient(0); break;
        default: frame_gradient(1); break;
        }
}

int app_main(void)
{
    char line[128];
    unsigned k, frame;
    int r, sw = 0, sh = 0;

    sv_marker("SV: SCREENSAVER START");

    r = sv_open();
    k = append(line, 0, "SV: EWSRV Connect r="); k = dec(line, k, r);
    line[k] = 0;
    sv_marker(line);
    if (r != 0) return 1;

    r = sv_window(8, 8, WIN_W, WIN_H, "SV: SCREENSAVER");
    k = append(line, 0, "SV: Window r="); k = dec(line, k, r);
    line[k] = 0;
    sv_marker(line);
    if (r != 0) { sv_close(); return 1; }

    r = sv_set_title("SV: SCREENSAVER");
    k = append(line, 0, "SV: SetTitle r="); k = dec(line, k, r);
    line[k] = 0;
    sv_marker(line);

    r = sv_screen_size(&sw, &sh);
    k = append(line, 0, "SV: screen="); k = dec(line, k, sw);
    k = append(line, k, "x"); k = dec(line, k, sh);
    k = append(line, k, " r="); k = dec(line, k, r);
    line[k] = 0;
    sv_marker(line);

    for (frame = 0; frame < 6u; ++frame)
        {
        char fline[48];
        unsigned n = append(fline, 0, "SV: frame ");
        n = dec(fline, n, (int)frame);
        n = append(fline, n, " drawn");
        fline[n] = 0;

        draw_frame(frame);
        sv_marker(fline);
        if (frame != 5u) sleep_ms(120);   /* short delay before next frame */
        }

    k = append(line, 0, "SV: 6 frames err="); k = dec(line, k, gErr);
    line[k] = 0;
    sv_marker(line);
    sv_marker("SV: DONE");

    /* Stay alive: the console window lives as long as this session, so the
     * last frame stays on the LFB for the QEMU screendump (the old exec
     * path left the kernel-painted frame behind on process exit). */
    for (;;) sleep_ms(100);
    return 0;
}

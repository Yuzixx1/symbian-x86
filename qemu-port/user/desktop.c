/*
 * desktop.c -- DESKTOP / APPLICATION SHELL for the R1 QEMU port, on the
 * GENUINE EWSRV text-window path (winsrv migration chunk D4,
 * docs/GUISHELL_MIGRATION_MAP.md).
 *
 * This program no longer touches the old winsrv GUI transport: no kernel
 * display-list execs, no shared kernel GUI buffer, no PortCon input lease.
 * user/ws_e32_client.c
 * (chunks A/B/D0/D1 landed) is #included below exactly like user/guimenu.c
 * and user/breakout.c do, so this translation unit owns the one
 * RSessionBase session to "!E32WindowServer" and can issue the genuine
 * CWsSession opcodes itself:
 *
 *   session/window : type-corrected twins of ws_e32_client.c's ws_open() /
 *                    ws_window_create() / ws_screen_size() (dt_connect /
 *                    dt_window_create / dt_screen_size below -- ws_e32_client.c
 *                    sends descriptors without the EPtrC/EPtr type bits, so
 *                    its ws_open() fails KErrBadName=-28 at runtime; the file
 *                    is read-only for this chunk, guilib.c pattern), plus
 *                    ws_close() from chunk A
 *   paint          : local dt_rect() (filled cell rects: space cells with
 *                    bg=colour) and dt_text() (real glyphs), both over the
 *                    genuine ops EConsoleSetTextColors(21) +
 *                    EConsoleSetCursorPosAbs(6) + EConsoleWrite(10), plus a
 *                    full clear via EConsoleClearScreen(2).  All coordinates
 *                    are CONSOLE CELLS (8x10 px each; EConsoleScreenSize
 *                    reports cells -- 128x76 on the 1024x768 VBE mode).
 *   input          : genuine EWSRV console read, CWsSession op
 *                    EConsoleRead (11), issued ASYNCHRONOUSLY through
 *                    RSessionBase::DoSendReceive(int,TIpcArgs,TRequestStatus&)
 *                    (EUSER.DLL ordinal 1548, PE2 IAT slot
 *                    desktop_iat_async @ 0x408010).  The KBINJ bridge
 *                    (variant/qemu_variant_int.cpp, \KBINJ.TXT gate)
 *                    delivers PS/2 input into genuine EWSRV, which completes
 *                    the armed read with an SConsoleKey (iType 0 = key with
 *                    TKeyCode iCode, 1 = mouse click with TPoint
 *                    iMousePos in pixels -> /8,/10 = cells).  The poll is
 *                    non-blocking, so the live taskbar clock and the 90 s
 *                    idle budget keep working; EConsoleReadCancel (12)
 *                    runs on exit.
 *
 * MULTI-WINDOW DECISION (map section 2 item 5): this tree's genuine EWSRV
 * is the e32 text window server -- ONE console window per session
 * (ws_e32_client's sWin == 0, EConsoleCreate makes the default console).
 * The old 8-window WM stack (gWins[8], drag/raise/close) therefore cannot
 * be hosted as real windows yet: the desktop is rendered as a SINGLE
 * full-screen console window containing the 2-column application list, a
 * launch log (where the old "windows" used to appear) and the taskbar.
 * Real multi-window needs one EWSRV session per window or a later
 * RWindowGroup path -- explicitly out of scope for chunk D4.
 *
 * Launch still uses trusted slow exec 268 (SvcMgrLaunch, two TDesC8) --
 * svcmgr-owned, NOT a winsrv exec; it becomes a genuine RProcess create at
 * the separate svcmgr retirement step.
 *
 * Markers go through the genuine EUSER.DLL ordinal 915 (RDebug::Printf)
 * imported by ws_e32_client itself (its static marker(), IAT slot
 * ws_e32_iat_printf), with DebugPrint fallback.
 *
 * PE2 IAT map (genuine/pack_multi.py --iat-start 0x8000):
 *   0x408000 ord 211  RSessionBase::CreateSession(TDesC16,TVersion,int)
 *   0x408004 ord 1547 RSessionBase::DoSendReceive(int,TIpcArgs const*) const
 *   0x408008 ord 120  RHandleBase::Close()
 *   0x40800c ord 915  RDebug::Printf(char const*,...)   [ws_e32_client marker]
 *   0x408010 ord 1548 RSessionBase::DoSendReceive(int,TIpcArgs const*,
 *                     TRequestStatus&) const            [desktop event read]
 */
#include "runtime.h"

/* Chunk A/B/D0/D1 genuine-EWSRV client, included so the helpers below share
 * its session (sSession), ws_e32_send(), palette and marker() -- the
 * user/guimenu.c / user/breakout.c pattern. */
#include "ws_e32_client.c"

/* Only trusted slow exec 268 (SvcMgrLaunch, svcmgr) remains -- NOT a winsrv
 * GUI exec; it retires with svcmgr in a separate chunk. */
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int desktop_iat_async;   /* ord 1548 slot @ 0x408010 */

static void dt_read_cancel(void);        /* fwd: used by launch() below */

/* EConsoleClearScreen is not in ws_e32_client.c's enum yet (kernelhwsrv
 * .../ws_std.h); EConsoleRead(11)/EConsoleReadCancel(12) already are. */
#define DT_OP_CLEAR_SCREEN 2

/* Genuine descriptor TYPE bits: u32std.h enum TDesType {EBufC,EPtrC,EPtr,...}
 * lives in iLength>>28 (TPtrC/TPckgC -> EPtrC, TPckg/TDes8 -> EPtr).  The
 * hand-rolled descriptors inside the included ws_e32_client.c omit them, so
 * its ws_open() fails with KErrBadName (-28) at runtime (User::ValidateName
 * reads Ptr() through the type bits -- see guilib.c:50-56).  ws_e32_client.c
 * is read-only for this chunk, so desktop carries its OWN type-corrected
 * session/draw ops below, driving the same sSession/sOpen/sWin statics of
 * the included chunk-A code (the guilib.c gl_connect pattern); ws_close()
 * and the arg0-only ops of ws_e32_client.c keep working unchanged. */
#define DT_DESTYPE_EPTRC  0x10000000u  /* EPtrC<<28: TPtrC16, TPckgC<T>  */
#define DT_DESTYPE_EPTR   0x20000000u  /* EPtr<<28:  TPckg<T> (TDes8)    */

struct dt_des8 { unsigned iLength; unsigned char* iPtr; int iMaxLength; };

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
static unsigned two_digits(char* p, unsigned k, unsigned v)
{
    p[k++] = (char)('0' + (v / 10u) % 10u);
    p[k++] = (char)('0' + v % 10u);
    return k;
}


/* ---- session/window/screen-size with CORRECT descriptor type bits ---------
 * Mirror ws_e32_client.c's ws_open/ws_window_create/ws_screen_size but set
 * the EPtrC/EPtr type bits in each descriptor's iLength (see the comment at
 * the DT_DESTYPE_* defines).  They drive the same sSession/sOpen/sWin
 * statics of the included chunk-A code, so ws_e32_send() and ws_close() work
 * unchanged afterwards. */
static const unsigned short dtServerName[] =
    { '!','E','3','2','W','i','n','d','o','w','S','e','r','v','e','r' };

static int dt_connect(void)
{
    struct ws_e32_desC16 nameDesc;      /* TPtrC16: EPtrC<<28 | length     */
    struct ws_e32_version ver;          /* KW32 1.0, KE32Build 4016        */
    int r;
    if (sOpen) return 0;
    ver.iMajor = 1; ver.iMinor = 0; ver.iBuild = 4016;
    sSession.iHandle = 0;               /* KNullHandle                     */
    /* The full genuine name "!E32WindowServer" (16 chars). */
    nameDesc.iLength = DT_DESTYPE_EPTRC | 16u;
    nameDesc.iPtr = dtServerName;
    r = e32_connect(&sSession, &nameDesc, &ver, 3 /* KMessageSlots */);
    if (r == -1)   /* KErrNotFound: port-built servers may have registered
                    * the truncated "!" form of the name (see the exec128
                    * fallback in variant/e32load.cpp) -- retry with that. */
        {
        nameDesc.iLength = DT_DESTYPE_EPTRC | 1u;
        r = e32_connect(&sSession, &nameDesc, &ver, 3);
        }
    if (r != 0) { sSession.iHandle = 0; return r; }
    /* NOTE: no iHandle!=0 check -- in this port MakeHandle(index,instance)
     * = (instance<<16)|index, so the FIRST handle of a process is 0 and is
     * a perfectly valid session handle (ws_e32_client.c's ws_server_ready()
     * misreads it as KNullHandle; use sOpen instead). */
    sOpen = 1;
    sWin = -1;
    return 0;
}

#define DT_TITLE_MAX 48u
static unsigned short dtTitleBuf[DT_TITLE_MAX + 1u];

static void dt_title_desc(struct ws_e32_desC16* d, const char* t)
{
    unsigned n = 0;
    if (t)
        while (n < DT_TITLE_MAX && t[n])
            { dtTitleBuf[n] = (unsigned short)(unsigned char)t[n]; ++n; }
    dtTitleBuf[n] = 0;
    d->iLength = DT_DESTYPE_EPTRC | n;
    d->iPtr = dtTitleBuf;
}

/* EConsoleCreate + EConsoleSet(title,size) + EConsoleSetWindowPosAbs: make
 * the default console window at pixel (x,y) with a w x h CELL grid -- the
 * same three ops ws_e32_client.c's ws_window_create() sends, with the
 * descriptor type bits set. */
static int dt_window_create(int x, int y, int w, int h, const char* title)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 sizePckg;       /* TPckgC<TSize>                   */
    struct ws_e32_size size;
    struct ws_e32_desC8 posPckg;        /* TPckgC<TPoint>                  */
    struct ws_e32_point pos;
    struct ws_e32_desC16 titleDesc;
    int r;

    if (!sOpen || w <= 0 || h <= 0) return -1;

    /* EConsoleCreate: create the default console window (no args). */
    ws_e32_args0(&args);
    r = ws_e32_send(WS_E32_OP_CONSOLE_CREATE, &args);
    if (r != 0) return r;

    /* EConsoleSet(title, size): w/h are character cells. */
    dt_title_desc(&titleDesc, title ? title : "WINDOW");
    size.iWidth = w; size.iHeight = h;
    sizePckg.iLength = DT_DESTYPE_EPTRC | (unsigned)sizeof(size);
    sizePckg.iPtr = (const unsigned char*)&size;
    args.iArgs[0] = (int)&titleDesc;
    args.iArgs[1] = (int)&sizePckg;
    args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDESC_C16 << 0) | (WS_E32_EDESC_C8 << 3);
    r = ws_e32_send(WS_E32_OP_CONSOLE_SET, &args);
    if (r != 0) return r;

    /* EConsoleSetWindowPosAbs: pixel position on the screen. */
    pos.iX = x; pos.iY = y;
    posPckg.iLength = DT_DESTYPE_EPTRC | (unsigned)sizeof(pos);
    posPckg.iPtr = (const unsigned char*)&pos;
    args.iArgs[0] = (int)&posPckg;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDESC_C8 << 0);
    r = ws_e32_send(WS_E32_OP_CONSOLE_SET_POS_ABS, &args);
    if (r != 0) return r;

    sWin = 0;                           /* single console window id 0      */
    return 0;
}

static int dt_screen_size(int* w, int* h)
{
    struct ws_e32_ipcargs args;
    struct dt_des8 sizePckg;            /* TPckg<TSize>: EPtr + iMaxLength */
    struct ws_e32_size size;
    int r;
    if (!sOpen) return -1;
    size.iWidth = 0; size.iHeight = 0;
    sizePckg.iLength = DT_DESTYPE_EPTR | (unsigned)sizeof(size);
    sizePckg.iPtr = (unsigned char*)&size;
    sizePckg.iMaxLength = (int)sizeof(size);
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&sizePckg;
    args.iFlags = (WS_E32_EDES8 << 0);
    r = ws_e32_send(WS_E32_OP_CONSOLE_SCREEN_SIZE, &args);
    if (r == 0)
        {
        if (w) *w = size.iWidth;
        if (h) *h = size.iHeight;
        }
    return r;
}

/* ---- paint over the genuine session (all coordinates in CELLS) ---------- */
static int dt_set_colors(uint32_t aFg, uint32_t aBg)
{
    struct ws_e32_ipcargs args;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)ws_e32_rgb_to_index(aFg);   /* fg, plain TInt slot */
    args.iArgs[1] = (int)ws_e32_rgb_to_index(aBg);   /* bg, plain TInt slot */
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_TEXT_COLORS, &args);
}

static int dt_cursor_abs(int aCol, int aRow)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 pckg;            /* TPckgC<TPoint>: EPtrC type bits */
    struct ws_e32_point pt;
    pt.iX = aCol; pt.iY = aRow;
    pckg.iLength = DT_DESTYPE_EPTRC | (unsigned)sizeof(pt);
    pckg.iPtr = (const unsigned char*)&pt;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&pckg;
    args.iFlags = (WS_E32_EDESC_C8 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_CURSOR_POS_ABS, &args);
}

static int dt_write16(const unsigned short* aText, unsigned aLen)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC16 desc;           /* TPtrC16: EPtrC type bits        */
    desc.iLength = DT_DESTYPE_EPTRC | aLen;
    desc.iPtr = aText;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&desc;
    args.iFlags = (WS_E32_EDESC_C16 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_WRITE, &args);
}

static void dt_clear(uint32_t aBg)
{
    struct ws_e32_ipcargs args;
    (void)dt_set_colors(0x00FFFFFFu, aBg);   /* clear fills with bg colour */
    ws_e32_args0(&args);
    (void)ws_e32_send(DT_OP_CLEAR_SCREEN, &args);
}

/* Filled cell rectangle: space (0x20, all-zero glyph) cells with bg=col, one
 * cursor+write per row -- ws_draw_fill_rect's algorithm with the type bits
 * set on every descriptor (ws_draw_fill_rect itself sends untyped descs). */
static unsigned short dt_row[120];

static void dt_rect(int x, int y, int w, int h, uint32_t col)
{
    int r, k;
    if (w <= 0 || h <= 0) return;
    if (w > (int)(sizeof(dt_row) / sizeof(dt_row[0]))) w = (int)(sizeof(dt_row) / sizeof(dt_row[0]));
    (void)dt_set_colors(col, col);       /* fg irrelevant for spaces      */
    for (k = 0; k < w; ++k) dt_row[k] = 0x20;
    for (r = 0; r < h; ++r)
        {
        (void)dt_cursor_abs(x, y + r);
        (void)dt_write16(dt_row, (unsigned)w);
        }
}

static void dt_text(int x, int y, const char* s, uint32_t fg, uint32_t bg)
{
    unsigned short buf[120];
    unsigned n = 0;
    if (!s) return;
    while (s[n] && n < 120u)
        {
        unsigned char c = (unsigned char)s[n++];
        buf[n - 1u] = (c < 0x20u || c == 0x7fu) ? 0x20u : (unsigned short)c;
        }
    if (!n) return;
    (void)dt_set_colors(fg, bg);
    (void)dt_cursor_abs(x, y);
    (void)dt_write16(buf, n);
}

/* ---- palette (0x00RRGGBB, mapped to the genuine 256-colour palette) ----- */
#define C_BG      0x00000080u   /* desktop blue                          */
#define C_TITLE   0x000080C0u   /* header bar                            */
#define C_WHITE   0x00FFFFFFu
#define C_PANEL   0x00001030u   /* normal app tile                       */
#define C_BORDER  0x004090D0u
#define C_HILITE  0x00C06020u   /* selected tile (amber)                 */
#define C_FOOT    0x00000040u   /* taskbar                               */
#define C_LIVE    0x0000FF00u
#define C_HINT    0x00FFE040u

/* ---- application table: 10 desktop entries ------------------------------ */
struct App { const char* label; const char* image; };
static const struct App kApps[] = {
    { "CALC",     "CALC.E32"     },
    { "FILEMGR",  "FILEMGR.E32"  },
    { "TEXTEDIT", "TEXTEDIT.E32" },
    { "PING",     "PING.E32"     },
    { "WBROWSER", "WBROWSER.E32" },
    { "BREAKOUT", "BREAKOUT.E32" },
    { "GUIMENU",  "GUIMENU.E32"  },
    { "SYSINFO",  "SYSINFO.E32"  },
    { "DTCALC",   "DTCALC.E32"   },
    { "DTEDIT",   "DTEDIT.E32"   },
};
#define NAPPS   ((int)(sizeof(kApps) / sizeof(kApps[0])))
#define COLROWS ((NAPPS + 1) / 2)

/* ---- geometry / state (console cells; window is W x H) ------------------ */
static int W, H;                 /* window size in cells (screen minus the
                                  * genuine 2-cell window border)          */
static unsigned gColW;           /* one app-tile column width              */
static int gSel = 0;             /* selected app                           */
static int gLaunchN = 0;         /* launch attempts                        */
static int gRepaints = 0;
static int gLastSec = -1;

/* Launch log -- the single-window replacement for the old WM window stack
 * (see the MULTI-WINDOW DECISION above). */
#define LOG_MAX 24
static char gLog[LOG_MAX][72];
static int  gLogN = 0;

static void log_add(const char* s)
{
    int i;
    if (gLogN >= LOG_MAX)
        {
        for (i = 0; i < LOG_MAX - 1; ++i) { char* d = gLog[i]; const char* p = gLog[i + 1]; unsigned k = 0; while ((d[k] = p[k]) != 0) ++k; }
        gLogN = LOG_MAX - 1;
        }
    {
    unsigned k = 0;
    while (s[k] && k < sizeof(gLog[0]) - 1u) { gLog[gLogN][k] = s[k]; ++k; }
    gLog[gLogN][k] = 0;
    ++gLogN;
    }
}

static void layout(void)
{
    gColW = (unsigned)(W - 4) / 2u;
}

/* ---- app tile geometry -------------------------------------------------- */
static void entry_geom(int i, int* x, int* y)
{
    *x = 1 + (i / COLROWS) * ((int)gColW + 1);
    *y = 3 + (i % COLROWS) * 2;      /* each tile is 2 cell rows tall */
}
static int in_rect(int px, int py, int rx, int ry, int rw, int rh)
{
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

/* ---- drawing ------------------------------------------------------------ */
static void draw_entry(int idx, int selected)
{
    int x, y;
    unsigned face = selected ? C_HILITE : C_PANEL;
    char l0[40], l1[40];
    unsigned n = 0;
    entry_geom(idx, &x, &y);

    dt_rect(x, y, (int)gColW, 2, face);
    n = dec(l0, 0, (unsigned)(idx + 1));
    l0[n++] = '.'; l0[n++] = ' ';
    n = append(l0, n, kApps[idx].label);
    l0[n] = 0;
    dt_text(x + 1, y, l0, C_WHITE, face);
    n = append(l1, 0, kApps[idx].image);
    l1[n] = 0;
    dt_text(x + 1, y + 1, l1, selected ? C_HINT : C_BORDER, face);
    if (selected)
        dt_rect(x, y, 1, 2, C_LIVE);  /* selection stripe */
}

/* Partial taskbar-clock update: overwrite just the eight HH:MM:SS cells
 * (each written cell paints its own bg), no clear, no full repaint. */
static void draw_clock(void)
{
    unsigned sec = (unsigned)time_now() % 86400u;
    char clk[16];
    unsigned n = 0;
    n = two_digits(clk, 0, sec / 3600u); clk[n++] = ':';
    n = two_digits(clk, n, (sec % 3600u) / 60u); clk[n++] = ':';
    n = two_digits(clk, n, sec % 60u); clk[n] = 0;
    dt_text(W - 9, H - 1, clk, C_LIVE, C_FOOT);
}

static void draw_full(void)
{
    char st[96];
    unsigned n = 0;
    int i, ylog, cap, first;

    dt_clear(C_BG);
    dt_rect(0, 0, W, 2, C_TITLE);                       /* header bar */
    dt_text(1, 0, "EKA2 PC OS - DESKTOP (genuine EWSRV console)", C_WHITE, C_TITLE);
    dt_text(1, 1, "W/S row  A/D col  ENTER launch  C clear log  ESC exit",
            C_HINT, C_TITLE);

    for (i = 0; i < NAPPS; ++i)
        draw_entry(i, i == gSel);

    /* launch log between the app grid and the taskbar */
    ylog = 3 + COLROWS * 2 + 1;
    dt_text(1, ylog, "LAUNCH LOG:", C_BORDER, C_BG);
    cap = (H - 3) - (ylog + 1);
    if (cap < 0) cap = 0;
    if (cap > LOG_MAX) cap = LOG_MAX;
    first = (gLogN > cap) ? gLogN - cap : 0;
    for (i = first; i < gLogN; ++i)
        dt_text(1, ylog + 1 + (i - first), gLog[i], C_WHITE, C_BG);

    dt_rect(0, H - 2, W, 2, C_FOOT);                    /* taskbar */
    n = append(st, 0, "EKA2 PC OS | Sel: ");
    n = append(st, n, kApps[gSel].label);
    n = append(st, n, " | Launches: ");
    n = dec(st, n, (unsigned)gLaunchN);
    st[n] = 0;
    dt_text(1, H - 2, st, C_WHITE, C_FOOT);
    dt_text(1, H - 1, "E32 window server path", C_BORDER, C_FOOT);
    draw_clock();

    ++gRepaints;
    if (gRepaints <= 3 || (gRepaints % 10) == 0)
        {
        char line[64];
        unsigned k = append(line, 0, "DT: repaint ");
        k = dec(line, k, (unsigned)gRepaints);
        line[k] = 0;
        marker(line);
        }
}

/* ---- trusted slow exec 268 (SvcMgrLaunch) --------------------------------
 * svcmgr-owned (NOT a winsrv exec): two TDesC8 args, "\NAME.E32" + empty
 * command line.  Becomes a genuine RProcess create at the svcmgr step. */
typedef struct Desc { unsigned type_length; const char* ptr; } Desc;
static char s_path[32];
static char s_empty[] = "";

static int svc_launch(const struct App* app)
{
    unsigned n = 0;
    const char* img = app->image;
    s_path[n++] = '\\';
    while (*img && n < sizeof(s_path) - 1u) { s_path[n++] = *img++; }
    s_path[n] = 0;
    Desc p = { 0x10000000u | n, s_path };   /* TDesC8: "\NAME.E32" */
    Desc c = { 0x10000000u, s_empty };      /* empty command line */
    return exec2(268u, (unsigned)&p, (unsigned)&c);
}

static void emit_select(void)
{
    char line[64];
    unsigned n = append(line, 0, "DT: select ");
    n = append(line, n, kApps[gSel].label);
    line[n] = 0;
    marker(line);
}

static void launch(int idx)
{
    const struct App* app = &kApps[idx];
    char line[128], lg[72];
    unsigned n = 0, m = 0;
    int ret;

    ++gLaunchN;
    ret = svc_launch(app);

    n = append(line, 0, "DT: launched ");
    n = append(line, n, app->label);
    n = append(line, n, " r=");
    n = dec(line, n, (unsigned)ret);
    line[n] = 0;
    marker(line);

    m = append(lg, 0, app->image);
    m = append(lg, m, " r=");
    m = dec(lg, m, (unsigned)ret);
    lg[m] = 0;
    log_add(lg);

    if (ret > 0)
        {
        /* The child is now the foreground program: leave the desktop so we
         * do not repaint over the child's console window.  The shell
         * detects our exit and repaints the prompt when the child ends. */
        marker("DT: handing off");
        dt_read_cancel();
        marker("DT: DONE");            /* same end marker as ESC exit (tests) */
        ws_close();
        process_exit(0);
        }

    /* exec 268 failed: stay on the desktop, the log line above records it */
    n = append(line, 0, "DT: fallback ");
    n = append(line, n, app->label);
    n = append(line, n, " (use shell)");
    line[n] = 0;
    marker(line);
}

/* ---- input: genuine EConsoleRead (op 11) via async DoSendReceive ---------
 * SConsoleKey (kernelhwsrv .../e32twin.h) as written by CWsWindow::
 * DrainReadRequest(); struct ws_e32_console_key comes from the included
 * ws_e32_client.c.  iType: 0 = EKeyPress (iCode = TKeyCode), 1 = EMouseClick
 * (iMousePos = pointer in PIXELS). */
static volatile int sReadStatus;
static struct ws_e32_console_key sKey;
static struct ws_e32_console_key sEv;   /* snapshot taken before re-arm */
static int sReadArmed;

typedef void (*dt_async_send_t)(const void* aThis, int aOp,
                                const void* aArgs, volatile int* aStatus);
static void dt_send_async(int aOp, struct ws_e32_ipcargs* aArgs,
                          volatile int* aStatus)
{
    ((dt_async_send_t)(*(volatile unsigned int*)&desktop_iat_async))
        (&sSession, aOp, aArgs, aStatus);
}

/* Arm one pending console read (EConsoleRead).  No-op if already armed.
 * The payload is TPckgBuf<SConsoleKey> seen as a TDes8: EPtr type bits plus
 * the iMaxLength third word (the 2-word des8 of ws_e32_client.c is not a
 * genuine TDes8). */
static void dt_read_arm(void)
{
    struct ws_e32_ipcargs args;
    struct dt_des8 kd;
    if (!sOpen || sWin < 0 || sReadArmed) return;
    kd.iLength = DT_DESTYPE_EPTR | (unsigned)sizeof(sKey);
    kd.iPtr = (unsigned char*)&sKey;
    kd.iMaxLength = (int)sizeof(sKey);
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&kd;
    args.iFlags = (WS_E32_EDES8 << 0);          /* TDes8 slot: keystroke out */
    sReadStatus = QE_KREQUEST_PENDING;
    dt_send_async(WS_E32_OP_CONSOLE_READ, &args, &sReadStatus);
    sReadArmed = 1;
}

/* Returns 1 when an event completed (snapshot in sEv), 0 while pending or
 * on a completed-with-error read (which is dropped). */
static int dt_read_poll(void)
{
    int st;
    if (!sReadArmed) return 0;
    st = sReadStatus;
    if (st == (int)QE_KREQUEST_PENDING) return 0;
    sReadArmed = 0;
    sEv = sKey;                                 /* snapshot before re-arm  */
    dt_read_arm();                              /* keep one read armed     */
    if (st != 0)                                /* KErrCancel etc: drop    */
        {
        char db[64];
        unsigned k = append(db, 0, "DT: read err=");
        k = dec(db, k, (unsigned)st);
        db[k] = 0;
        marker(db);
        return 0;
        }
    {   /* Input marker: log every completed EConsoleRead event */
        char db[96];
        unsigned k = append(db, 0, "DT: ev t=");
        k = dec(db, k, (unsigned)sEv.iType);
        k = append(db, k, " code=");
        k = dec(db, k, sEv.iCode);
        k = append(db, k, " mod=");
        k = dec(db, k, sEv.iModifiers);
        k = append(db, k, " ptr=");
        k = dec(db, k, (unsigned)sEv.iPointerNumber);
        db[k] = 0;
        marker(db);
    }
    return 1;                                   /* key(0) click(1) raw(3/4) */
}

static void dt_read_cancel(void)
{
    struct ws_e32_ipcargs args;
    if (!sReadArmed) return;
    ws_e32_args0(&args);
    (void)ws_e32_send(WS_E32_OP_CONSOLE_READ_CANCEL, &args);
    sReadArmed = 0;
}

/* ---- key mapping (genuine TKeyCode values, e32keys.h) ------------------- */
#define DT_KEY_ENTER 0x000du   /* EKeyEnter      */
#define DT_KEY_ESC   0x001bu   /* EKeyEscape     */
#define DT_KEY_SPACE 0x0020u   /* EKeySpace      */
#define DT_KEY_LEFT  0xf807u   /* EKeyLeftArrow  */
#define DT_KEY_RIGHT 0xf808u   /* EKeyRightArrow */
#define DT_KEY_UP    0xf809u   /* EKeyUpArrow    */
#define DT_KEY_DOWN  0xf80au   /* EKeyDownArrow  */

/* Map a key code to a new selection index; -1 means "not a navigation key".
 * Letter keys arrive as their ASCII TKeyCode (identity keymap). */
static int navigate(int key)
{
    if (key == (int)DT_KEY_UP    || key == 'w' || key == 'W') return (gSel + NAPPS - 1) % NAPPS;
    if (key == (int)DT_KEY_DOWN  || key == 's' || key == 'S') return (gSel + 1) % NAPPS;
    if (key == (int)DT_KEY_LEFT  || key == 'a' || key == 'A') return (gSel + NAPPS - COLROWS) % NAPPS;
    if (key == (int)DT_KEY_RIGHT || key == 'd' || key == 'D') return (gSel + COLROWS) % NAPPS;
    return -1;
}

/* Returns 1 when the screen changed, -1 on ESC (quit), 0 otherwise. */
static int handle_key(int c)
{
    int next;
    if (c == (int)DT_KEY_ESC) return -1;
    if (c == (int)DT_KEY_ENTER || c == '\n' || c == '\r' || c == (int)DT_KEY_SPACE)
        { launch(gSel); return 1; }
    if (c == 'c' || c == 'C')
        { gLogN = 0; marker("DT: log cleared"); return 1; }
    if (c == 'r' || c == 'R') return 1;         /* silent full repaint */
    next = navigate(c);
    if (next >= 0 && next != gSel)
        { gSel = next; emit_select(); return 1; }
    return 0;
}

/* Mouse click: SConsoleKey.iMousePos is in screen pixels; the window sits at
 * (0,0), so cell = pixel / (8x10).  A click on an app tile selects and
 * launches it, exactly like the old desktop icon click. */
static int handle_click(int mx, int my)
{
    int cx = mx / 8, cy = my / 10;
    int i, x, y;
    if (cx < 0 || cy < 0 || cx >= W || cy >= H) return 0;
    for (i = 0; i < NAPPS; ++i)
        {
        entry_geom(i, &x, &y);
        if (in_rect(cx, cy, x, y, (int)gColW, 2))
            {
            gSel = i;
            emit_select();
            launch(i);
            return 1;
            }
        }
    return 0;
}

/* ---- entry point -------------------------------------------------------- */
int app_main(void)
{
    char line[96];
    unsigned n;
    int r, sw = 0, sh = 0, idle, changed, quit = 0, now_sec;

    marker("DT: D4 desktop on genuine EWSRV text-window path");

    /* Genuine EWSRV session + full-grid console window.  dt_connect() is the
     * type-corrected twin of ws_e32_client.c's ws_open() (which sends untyped
     * descriptors and fails KErrBadName=-28 -- see the DT_DESTYPE_* comment);
     * both drive the same session statics of the included chunk-A code. */
    r = dt_connect();
    n = append(line, 0, "DT: Connect r=");
    n = dec(line, n, (unsigned)r);
    n = append(line, n, " h=");
    n = dec(line, n, (unsigned)sSession.iHandle);
    line[n] = 0;
    marker(line);
    if (r != 0) return 1;
    if (!sOpen)                       /* ws_server_ready() misreads h=0 */
        {
        marker("DT: FAIL server not ready");
        ws_close();
        return 1;
        }

    r = dt_screen_size(&sw, &sh);        /* cells: 128x76 on 1024x768 */
    n = append(line, 0, "DT: ScreenSize r=");
    n = dec(line, n, (unsigned)r);
    n = append(line, n, " ");
    n = dec(line, n, (unsigned)sw);
    n = append(line, n, "x");
    n = dec(line, n, (unsigned)sh);
    line[n] = 0;
    marker(line);
    if (r != 0 || sw < 42 || sh < 22) { sw = 80; sh = 25; }

    /* The genuine window adds a 2-cell border, so the usable grid is
     * screen cells minus 2 in each dimension (breakout.c convention). */
    W = sw - 2;
    H = sh - 2;
    r = dt_window_create(0, 0, W, H, "EKA2 DESKTOP");
    n = append(line, 0, "DT: Create r=");
    n = dec(line, n, (unsigned)r);
    n = append(line, n, " ");
    n = dec(line, n, (unsigned)W);
    n = append(line, n, "x");
    n = dec(line, n, (unsigned)H);
    line[n] = 0;
    marker(line);
    if (r != 0) { ws_close(); return 1; }

    layout();
    gSel = 0;
    gLogN = 0;
    gLaunchN = 0;
    gRepaints = 0;
    gLastSec = -1;

    draw_full();
    marker("DT: DESKTOP READY");
    n = append(line, 0, "DT: apps=");
    n = dec(line, n, (unsigned)NAPPS);
    line[n] = 0;
    marker(line);
    emit_select();

    /* 90 s idle budget at 20 ms/poll: a forgotten desktop cannot wedge input. */
    for (idle = 0; idle < 4500 && !quit; ++idle)
        {
        changed = 0;
        dt_read_arm();
        if (dt_read_poll())
            {
            if (sEv.iType == 0 || sEv.iType == 3 || sEv.iType == 4)
                {                               /* EKeyPress / raw down/up */
                int hr = handle_key((int)sEv.iCode);
                if (hr < 0) { quit = 1; break; }
                changed |= hr;
                }
            else if (sEv.iType == 1)            /* EMouseClick */
                changed |= handle_click(sEv.iMousePos.iX, sEv.iMousePos.iY);
            idle = 0;
            }

        /* 1 Hz clock tick: a small partial update of the eight clock cells,
         * no clear and no full repaint. */
        now_sec = (int)time_now();
        if (now_sec != gLastSec)
            {
            gLastSec = now_sec;
            draw_clock();
            }
        if (changed) draw_full();
        sleep_ms(20);
        }

    dt_read_cancel();
    marker(quit ? "DT: DONE" : "DT: TIMEOUT");
    ws_close();
    return 0;
}

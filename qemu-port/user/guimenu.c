/*
 * guimenu.c -- full-screen APPLICATION LAUNCHER for the R1 QEMU port.
 *
 * WINSRV MIGRATION CHUNK D2 (docs/GUISHELL_MIGRATION_MAP.md): this program no
 * longer uses the old winsrv GUI slow execs or the kernel display-list
 * header.  It runs on the GENUINE EWSRV text-window path:
 *
 *   session/window : ws_open() / ws_window_create() / ws_screen_size() /
 *                    ws_server_ready() / ws_close() from user/ws_e32_client.c
 *                    (chunks A/B), #included below exactly like guilib.c so
 *                    this translation unit can issue the CWsSession opcodes
 *                    over the very same !E32WindowServer session.
 *   paint          : filled rects via ws_draw_fill_rect() and text via a local
 *                    gm_text() over the genuine ops EConsoleSetTextColors(21)
 *                    + EConsoleSetCursorPosAbs(6) + EConsoleWrite(10), plus
 *                    EConsoleClearScreen(2) for the frame clear.  All
 *                    coordinates are CONSOLE CELLS (the window is a 126x74
 *                    cell grid on the 128x76-cell 1024x768 screen).
 *   input          : the genuine console read -- CWsSession op EConsoleRead
 *                    (11), sent ASYNCHRONOUSLY through RSessionBase::
 *                    DoSendReceive(int,TIpcArgs,TRequestStatus&) (EUSER.DLL
 *                    ordinal 1548, PE2 IAT slot guimenu_iat_send_async).  The
 *                    KBINJ bridge (variant/qemu_variant_int.cpp, \KBINJ.TXT
 *                    gate) delivers PS/2 EKeyDown into genuine EWSRV, which
 *                    completes the pending read with an SConsoleKey
 *                    (kernelhwsrv .../e32twin.h: iCode/iModifiers/...).
 *                    gm_read_poll() is non-blocking so the 90 s idle budget
 *                    is kept; EConsoleReadCancel (op 12) runs on exit.
 *                    No PortCon lease on the E32 path (the session implies it).
 *
 * Launch still uses trusted slow exec 268 (SvcMgrLaunch, two TDesC8) --
 * svcmgr-owned, NOT part of the winsrv exec set; it becomes a genuine
 * RProcess create at the separate svcmgr retirement step (map section 1).
 *
 * Markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf), imported
 * through the PE2 IAT slot guimenu_printf_slot, with the port's DebugPrint
 * executive as a fallback (same pattern as guishell.c/saver.c).
 *
 * PE2 IAT map (genuine/pack_multi.py --iat-start 0x8000):
 *   0x408000 ord 211  RSessionBase::CreateSession(TDesC16,TVersion,int)
 *   0x408004 ord 1547 RSessionBase::DoSendReceive(int,TIpcArgs const*) const
 *   0x408008 ord 120  RHandleBase::Close()
 *   0x40800c ord 915  RDebug::Printf(char const*,...)   [ws_e32_client]
 *   0x408010 ord 1548 RSessionBase::DoSendReceive(int,TIpcArgs const*,
 *                     TRequestStatus&) const            [guimenu event read]
 *   0x408014 ord 915  RDebug::Printf(char const*,...)   [guimenu markers]
 */
#include "runtime.h"

/* Chunk A/B genuine-EWSRV client, included so the draw/read helpers below
 * share its session (sSession) and ws_e32_send(); same pattern as guilib.c. */
#include "ws_e32_client.c"

/* Only trusted slow exec 268 (SvcMgrLaunch, svcmgr) remains -- NOT a winsrv
 * GUI exec; it retires with svcmgr in a separate chunk. */
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int guimenu_iat_send_async;   /* ord 1548 slot @ 0x408010 */
extern unsigned int guimenu_printf_slot;      /* ord 915  slot @ 0x408014 */
typedef void (*NbPrintf)(const char*, ...);

static void gm_marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&guimenu_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);  /* EUSER ord 915 */
    else print(text);                                  /* DebugPrint fallback */
}

/* ws_e32_client.c's own static marker() is unused in this TU (no test main);
 * reference it once so -Wunused-function does not fire. */
static void (*volatile gm_keep_marker)(const char*) = marker;

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

/* ---- genuine CWsSession opcodes not yet in ws_e32_client.c's enum --------
 * (kernelhwsrv/kernel/eka/include/ws_std.h): EConsoleClearScreen=2,
 * EConsoleRead=11, EConsoleReadCancel=12. */
#define GM_OP_CLEAR_SCREEN        2
#define GM_OP_CONSOLE_READ        11
#define GM_OP_CONSOLE_READ_CANCEL 12

/* ---- paint over the genuine session (all coordinates in CELLS) ---------- */
static int gm_set_colors(uint32_t aFg, uint32_t aBg)
{
    struct ws_e32_ipcargs args;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)ws_e32_rgb_to_index(aFg);   /* fg, plain TInt slot */
    args.iArgs[1] = (int)ws_e32_rgb_to_index(aBg);   /* bg, plain TInt slot */
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_TEXT_COLORS, &args);
}

static int gm_cursor_abs(int aCol, int aRow)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 pckg;
    struct ws_e32_point pt;
    pt.iX = aCol; pt.iY = aRow;
    pckg.iLength = sizeof(pt);
    pckg.iPtr = (const unsigned char*)&pt;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&pckg;
    args.iFlags = (WS_E32_EDESC_C8 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_CURSOR_POS_ABS, &args);
}

static int gm_write16(const unsigned short* aText, unsigned aLen)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC16 desc;
    desc.iLength = aLen;
    desc.iPtr = aText;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&desc;
    args.iFlags = (WS_E32_EDESC_C16 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_WRITE, &args);
}

static void gm_clear(uint32_t aBg)
{
    struct ws_e32_ipcargs args;
    (void)gm_set_colors(0x00FFFFFFu, aBg);   /* clear fills with bg colour */
    ws_e32_args0(&args);
    (void)ws_e32_send(GM_OP_CLEAR_SCREEN, &args);
}

static void gm_rect(int x, int y, int w, int h, uint32_t col)
{
    if (w <= 0 || h <= 0) return;
    /* chunk-2 genuine fill: space cells with bg=col, one write per row */
    (void)ws_draw_fill_rect(0, x, y, w, h, col);
}

static void gm_text(int x, int y, const char* s, uint32_t fg, uint32_t bg)
{
    unsigned short buf[128];
    unsigned n = 0;
    if (!s) return;
    while (s[n] && n < sizeof(buf) / sizeof(buf[0]))
        { buf[n] = (unsigned short)(unsigned char)s[n]; ++n; }
    if (!n) return;
    (void)gm_set_colors(fg, bg);
    (void)gm_cursor_abs(x, y);
    (void)gm_write16(buf, n);
}

/* ---- palette (0x00RRGGBB, mapped to the genuine 256-colour palette) ----- */
#define C_BG       0x00000080u   /* desktop blue                          */
#define C_TITLE    0x000080C0u   /* title bar                             */
#define C_WHITE    0x00FFFFFFu
#define C_PANEL    0x00001030u   /* normal row / panel face               */
#define C_BORDER   0x004090D0u
#define C_HILITE   0x00C06020u   /* selection bar (amber/orange)          */
#define C_FOOT     0x00000040u
#define C_LIVE     0x0000FF00u
#define C_HINT     0x00FFE040u

/* ---- application table: menu label <-> 8.3 FAT image name -------------- */
struct App { const char* label; const char* image; const char* desc; };
static const struct App kApps[] = {
    { "CALC",      "CALC.E32",      "Calculator: integer arithmetic, bases and expressions." },
    { "FILEMGR",   "FILEMGR.E32",   "File manager: browse, copy and delete FAT files." },
    { "TEXTEDIT",  "TEXTEDIT.E32",  "Text editor: create and modify text files." },
    { "PING",      "PING.E32",      "Real ICMP echo over the e1000 netbridge." },
    { "WBROWSER",  "WBROWSER.E32",  "Web browser: fetch pages over the TCP/IP stack." },
    { "WGET",      "WGET.E32",      "Command-line HTTP download utility." },
    { "SNAKE",     "SNAKE.E32",     "Classic interactive snake game." },
    { "BREAKOUT",  "BREAKOUT.E32",  "Breakout: paddle, ball and a wall of bricks." },
    { "MINES",     "MINES.E32",     "Minesweeper: find every safe square." },
    { "CHESS",     "CHESS.E32",     "Chess board with move entry and display." },
    { "SUDOKU",    "SUDOKU.E32",    "Number puzzle: complete the 9x9 grid." },
    { "GUIHELLO",  "GUIHELLO.E32",  "Minimal graphical hello-world on the VBE LFB." },
    { "GUISHELL",  "GUISHELL.E32",  "GUI desktop shell demonstration and mouse test." },
    { "SAVER",     "SAVER.E32",     "Animated screensaver: bounce, stars, gradient." },
    { "SYSINFO",   "SYSINFO.E32",   "System information: CPU, memory and drives." },
    { "SELFTEST",  "SELFTEST.E32",  "End-user OS self-test with a PASS/FAIL verdict." },
};
#define NAPPS ((int)(sizeof(kApps) / sizeof(kApps[0])))
#define COLROWS ((NAPPS + 1) / 2)

/* ---- geometry (console cells; window is GM_COLS x GM_ROWS) -------------- */
#define GM_COLS 126
#define GM_ROWS 74
static unsigned gColX[2], gColW, gRowY, gRowH, gDescY, gDescH;
static int gSel = 0;

static void layout(void)
{
    gColW = (GM_COLS - 4u) / 2u;
    gColX[0] = 1u;
    gColX[1] = 1u + gColW + 1u;
    gRowY = 3u;
    gRowH = 2u;                                  /* one row = 2 cell lines */
    gDescY = gRowY + (unsigned)COLROWS * gRowH + 1u;
    gDescH = 7u;
}

/* ---- static frame ------------------------------------------------------ */
static void draw_row(unsigned idx, int selected)
{
    unsigned col = idx / (unsigned)COLROWS;
    unsigned row = idx % (unsigned)COLROWS;
    unsigned x = gColX[col], y = gRowY + row * gRowH;
    unsigned face = selected ? C_HILITE : C_PANEL;
    char label[40];
    unsigned n = 0;

    gm_rect((int)x, (int)y, (int)gColW, (int)gRowH, face);
    n = dec(label, 0, idx + 1u);
    label[n++] = '.';
    label[n++] = ' ';
    n = append(label, n, kApps[idx].label);
    label[n] = 0;
    gm_text((int)x + 1, (int)y, label, C_WHITE, face);
    if (selected)
        gm_rect((int)x, (int)y, 1, (int)gRowH, C_LIVE);  /* selection stripe */
}

/* Word-wrap a description into the panel; at most 3 lines are emitted. */
static void draw_description(unsigned idx)
{
    unsigned x = 1u, y = gDescY;
    unsigned width = GM_COLS - 2u;
    unsigned maxchars = width - 2u;
    unsigned border = 1u;
    const char* s = kApps[idx].desc;
    char line[128];
    unsigned n = 0, lines = 0;

    gm_rect((int)x, (int)y, (int)width, (int)gDescH, C_PANEL);
    gm_rect((int)x, (int)y, (int)width, (int)border, C_BORDER);
    gm_rect((int)x, (int)(y + gDescH - border), (int)width, (int)border, C_BORDER);
    gm_rect((int)x, (int)y, (int)border, (int)gDescH, C_BORDER);
    gm_rect((int)(x + width - border), (int)y, (int)border, (int)gDescH, C_BORDER);

    n = append(line, 0, "SELECTED: ");
    n = append(line, n, kApps[idx].label);
    line[n] = 0;
    gm_text((int)x + 1, (int)y + 1, line, C_LIVE, C_PANEL);

    while (*s && lines < 3u)
        {
        unsigned room = (maxchars > 20u) ? maxchars - 4u : 20u;
        unsigned k = 0;
        while (*s == ' ') ++s;
        while (*s && k < room)
            {
            line[k++] = *s++;
            if (*s == ' ' && k > room - 12u) break;
            }
        line[k] = 0;
        gm_text((int)x + 1, (int)y + 3u + lines, line, C_WHITE, C_PANEL);
        ++lines;
        }
}

static void draw_full(void)
{
    unsigned i;

    gm_clear(C_BG);
    gm_rect(0, 0, GM_COLS, 2, C_TITLE);                       /* header */
    gm_text(1, 0, "EKA2 PC OS - APPLICATION LAUNCHER", C_WHITE, C_TITLE);
    gm_text(1, 1, "UP/DOWN or W/S select    A/D column    ENTER launch    ESC exit",
            C_HINT, C_TITLE);
    for (i = 0; i < (unsigned)NAPPS; ++i)
        draw_row(i, (int)i == gSel);
    draw_description((unsigned)gSel);
    gm_rect(0, GM_ROWS - 2, GM_COLS, 2, C_FOOT);              /* footer */
    gm_text(1, GM_ROWS - 2,
            "W/S move   A/D column   ENTER launch   ESC exit   R redraw",
            C_WHITE, C_FOOT);
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
    char line[80];
    unsigned n = append(line, 0, "GM: select ");
    n = append(line, n, kApps[gSel].label);
    line[n] = 0;
    gm_marker(line);
}

static void launch_selected(void)
{
    const struct App* app = &kApps[gSel];
    char line[160];
    unsigned n;
    int ret = svc_launch(app);

    n = append(line, 0, "GM: launching ");
    n = append(line, n, app->label);
    n = append(line, n, " r=");
    n = dec(line, n, (unsigned)ret);
    line[n] = 0;
    gm_marker(line);

    if (ret <= 0)
        {
        /* Exec 268 unavailable / file not found: report a graceful shell
         * fallback instead of failing the menu interaction. */
        n = append(line, 0, "GM: fallback ");
        n = append(line, n, app->label);
        n = append(line, n, " (use shell: ");
        n = append(line, n, app->label);
        n = append(line, n, ")");
        line[n] = 0;
        gm_marker(line);
        }
    /* EWSRV redraws on every write, so just repaint to keep the menu
     * legible after a launch. */
    draw_full();
}

/* ---- input: genuine EConsoleRead (op 11) via async DoSendReceive ---------
 * SConsoleKey (kernelhwsrv .../e32twin.h) as written by CWsWindow::
 * DrainReadRequest(): iCode, iModifiers, iMousePos, iType, iPointerNumber. */
struct gm_cons_key {
    int iCode;
    unsigned iModifiers;
    int iMouseX, iMouseY;
    int iType;
    unsigned char iPointerNumber;
    unsigned char iPad[3];
};

static volatile int sReadStatus;
static struct gm_cons_key sKey;
static int sReadArmed;

typedef int (*gm_t_send_async)(const void* aThis, int aOp,
                               const void* aArgs, volatile int* aStatus);
static int gm_send_async(int aOp, struct ws_e32_ipcargs* aArgs,
                         volatile int* aStatus)
{
    return ((gm_t_send_async)(*(volatile unsigned int*)&guimenu_iat_send_async))
           (&sSession, aOp, aArgs, aStatus);
}

/* Arm one pending console read (EConsoleRead).  No-op if already armed. */
static void gm_read_arm(void)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_des8 kd;
    if (!sOpen || sWin < 0 || sReadArmed) return;
    kd.iLength = sizeof(sKey);
    kd.iPtr = (unsigned char*)&sKey;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&kd;
    args.iFlags = (WS_E32_EDES8 << 0);          /* TDes8 slot: keystroke out */
    sReadStatus = QE_KREQUEST_PENDING;
    (void)gm_send_async(GM_OP_CONSOLE_READ, &args, &sReadStatus);
    sReadArmed = 1;
}

/* Returns the TKeyCode when a key arrived, 0 while still pending,
 * negative on a completed-with-error read. */
static int gm_read_poll(void)
{
    if (!sReadArmed) return 0;
    if (sReadStatus == QE_KREQUEST_PENDING) return 0;
    sReadArmed = 0;
    if (sReadStatus != 0) return -1;
    return sKey.iCode;
}

static void gm_read_cancel(void)
{
    struct ws_e32_ipcargs args;
    if (!sReadArmed) return;
    ws_e32_args0(&args);
    (void)ws_e32_send(GM_OP_CONSOLE_READ_CANCEL, &args);
    sReadArmed = 0;
}

/* ---- key mapping (genuine TKeyCode values, e32keys.h) ------------------- */
#define GM_KEY_ENTER 0x000du   /* EKeyEnter       */
#define GM_KEY_ESC   0x001bu   /* EKeyEscape      */
#define GM_KEY_SPACE 0x0020u   /* EKeySpace       */
#define GM_KEY_LEFT  0xf807u   /* EKeyLeftArrow   */
#define GM_KEY_RIGHT 0xf808u   /* EKeyRightArrow  */
#define GM_KEY_UP    0xf809u   /* EKeyUpArrow     */
#define GM_KEY_DOWN  0xf80au   /* EKeyDownArrow   */

/* Map a key code to a new selection index; -1 means "not a navigation key".
 * Letter keys arrive as their uppercase ASCII TKeyCode (identity keymap). */
static int navigate(int key)
{
    if (key == (int)GM_KEY_UP   || key == 'w' || key == 'W' || key == 'k' || key == 'K')
        return (gSel + NAPPS - 1) % NAPPS;
    if (key == (int)GM_KEY_DOWN || key == 's' || key == 'S' || key == 'j' || key == 'J')
        return (gSel + 1) % NAPPS;
    if (key == (int)GM_KEY_LEFT || key == 'a' || key == 'A' || key == 'h' || key == 'H')
        return (gSel + NAPPS - COLROWS) % NAPPS;
    if (key == (int)GM_KEY_RIGHT|| key == 'd' || key == 'D' || key == 'l' || key == 'L')
        return (gSel + COLROWS) % NAPPS;
    return -1;
}

/* ---- entry point ------------------------------------------------------- */
int app_main(void)
{
    char line[96];
    unsigned k;
    int r, w = 0, h = 0, key, next, idle;

    gm_marker("GM: D2 e32 launcher start");

    /* Genuine EWSRV session + full-grid console window (chunk A/B client). */
    r = ws_open();
    k = append(line, 0, "GM: Connect r=");
    k = dec(line, k, (unsigned)r);
    line[k] = 0;
    gm_marker(line);
    if (r != 0) return 1;

    r = ws_screen_size(&w, &h);            /* cells: 128x76 on 1024x768 */
    k = append(line, 0, "GM: ScreenSize r=");
    k = dec(line, k, (unsigned)r);
    k = append(line, k, " ");
    k = dec(line, k, (unsigned)w);
    k = append(line, k, "x");
    k = dec(line, k, (unsigned)h);
    line[k] = 0;
    gm_marker(line);

    r = ws_window_create(0, 0, GM_COLS, GM_ROWS, "EKA2 LAUNCHER");
    k = append(line, 0, "GM: Create r=");
    k = dec(line, k, (unsigned)r);
    line[k] = 0;
    gm_marker(line);
    if (r != 0) { ws_close(); return 1; }
    if (!ws_server_ready())
        {
        gm_marker("GM: FAIL server not ready");
        ws_close();
        return 1;
        }

    layout();
    gSel = 0;
    draw_full();
    gm_marker("GM: LAUNCHER READY");
    k = append(line, 0, "GM: apps=");
    k = dec(line, k, (unsigned)NAPPS);
    line[k] = 0;
    gm_marker(line);
    emit_select();

    /* 90 s idle budget at 20 ms/poll: a forgotten menu cannot wedge input. */
    for (idle = 0; idle < 4500; ++idle)
        {
        gm_read_arm();
        key = gm_read_poll();
        if (key <= 0) { sleep_ms(20); continue; }   /* pending / error */
        idle = 0;

        if (key == (int)GM_KEY_ESC) break;          /* Esc: back to shell */
        if (key == 'r' || key == 'R')
            { draw_full(); continue; }              /* silent full repaint */
        if (key == (int)GM_KEY_ENTER || key == '\n' || key == '\r' ||
            key == (int)GM_KEY_SPACE)
            { launch_selected(); continue; }        /* Enter: launch app  */

        next = navigate(key);
        if (next < 0 || next == gSel) continue;

        draw_row((unsigned)gSel, 0);                /* repaint changed rows */
        gSel = next;
        draw_row((unsigned)gSel, 1);
        draw_description((unsigned)gSel);
        emit_select();
        }

    gm_read_cancel();
    gm_marker(idle >= 4500 ? "GM: TIMEOUT" : "GM: DONE");
    ws_close();
    return 0;
}

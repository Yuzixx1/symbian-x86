/*
 * guimenu.c -- full-screen GRAPHICAL APPLICATION LAUNCHER for the R1 QEMU port.
 *
 * A genuine PE2 E32 program.  It discovers the VBE linear framebuffer through
 * slow execs 249 (base) / 250 (size), obtains the low-RAM gui_shared display
 * list from exec 251 (arg1==0) and paints the real LFB by submitting display
 * lists through exec 251 (arg1>0).  Unlike the shell's type-a-name interface,
 * this is a point-and-shoot desktop menu: two columns of bundled applications,
 * a highlighted selection bar, a live description panel and key hints.
 *
 * Input comes from the PortCon lease (con_acquire/con_tryread).  Up/Down (or
 * W/S) move the selection, A/D jump between columns, Enter launches via the
 * trusted slow exec 268 (SvcMgrLaunch), Esc returns to the shell.  The kernel
 * currently drops PS/2 E0 (arrow) scancodes, so W/S and A/D are the primary
 * bindings; a short escape-sequence decoder is included for future kernels.
 *
 * Markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf), imported
 * through the PE2 IAT slot guimenu_printf_slot at 0x408000, with the port's
 * DebugPrint executive as a fallback (same pattern as guishell.c/saver.c).
 */
#include "runtime.h"
#include "guishost.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int guimenu_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&guimenu_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);  /* EUSER ord 915 */
    else print(text);                                  /* DebugPrint fallback */
}

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

/* ---- display-list builder (guishost.h layout) -------------------------- */
static struct gui_shared* gSh;
static unsigned gN;

/* A full frame needs far more than GUI_MAX_CMDS putc commands.  cmd() auto-
 * flushes each complete 160-command list; lists execute in submission order
 * with no implicit clear, so several of them compose one atomic-looking frame. */
static void flush(void)
{
    if (gN)
        {
        exec2(QE_EXEC_GUI_DRAW, gN, 0);
        gN = 0;
        }
}
static void cmd(unsigned type, unsigned a, unsigned b,
                unsigned c, unsigned d, unsigned e)
{
    if (gN >= GUI_MAX_CMDS) flush();
    struct gui_cmd* q = &gSh->cmds[gN++];
    q->type = type; q->a = a; q->b = b; q->c = c; q->d = d; q->e = e;
}
static void rect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned col)
    { cmd(GC_RECT, x, y, w, h, col); }
/* The kernel font is an 8x8 glyph scaled 2x, so one character cell is 16 px. */
#define CELL_W 16u

static void text(unsigned x, unsigned y, const char* s,
                 unsigned fg, unsigned bg)
{
    while (*s)
        {
        cmd(GC_PUTC, x, y, (unsigned char)*s, fg, bg);
        x += CELL_W; ++s;
        }
}

/* ---- palette (0x00RRGGBB) ---------------------------------------------- */
#define C_BG       0x00000080u   /* desktop blue (matches kernel readback)   */
#define C_TITLE    0x000080C0u   /* title bar (also satisfies pixel readback)*/
#define C_WHITE    0x00FFFFFFu
#define C_PANEL    0x00001030u   /* normal row / panel face                  */
#define C_BORDER   0x004090D0u
#define C_HILITE   0x00C06020u   /* selection bar (amber/orange)             */
#define C_FOOT     0x00000040u
#define C_LIVE     0x0000FF00u

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

/* ---- geometry ---------------------------------------------------------- */
static unsigned gColX[2], gColW, gRowY, gRowH, gDescY, gDescH;
static int gSel = 0;

static void layout(void)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;

    gColW = (W - 64u) / 2u;
    gColX[0] = 24u;
    gColX[1] = 24u + gColW + 16u;
    gRowY = 104u;
    gRowH = 36u;
    gDescY = gRowY + (unsigned)COLROWS * gRowH + 16u;
    gDescH = (H > gDescY + 96u) ? 112u : (H - gDescY - 8u);
    if (gDescH > 112u) gDescH = 112u;
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

    rect(x, y, gColW, gRowH - 4u, face);
    n = dec(label, 0, idx + 1u);
    label[n++] = '.';
    label[n++] = ' ';
    n = append(label, n, kApps[idx].label);
    label[n] = 0;
    text(x + 10u, y + 7u, label, C_WHITE, face);
    if (selected)
        rect(x, y, 4u, gRowH - 4u, C_LIVE);   /* selection marker stripe */
}

/* Word-wrap a description into the panel; at most 3 lines are emitted. */
static void draw_description(unsigned idx)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned x = 24u, y = gDescY;
    unsigned width = W - 48u;
    unsigned maxchars = width / CELL_W;
    unsigned border = 2u;
    const char* s = kApps[idx].desc;
    char line[80];
    unsigned n = 0, lines = 0;

    rect(x, y, width, gDescH, C_PANEL);
    rect(x, y, width, border, C_BORDER);
    rect(x, y + gDescH - border, width, border, C_BORDER);
    rect(x, y, border, gDescH, C_BORDER);
    rect(x + width - border, y, border, gDescH, C_BORDER);

    n = append(line, 0, "SELECTED: ");
    n = append(line, n, kApps[idx].label);
    line[n] = 0;
    text(x + 12u, y + 10u, line, C_LIVE, C_PANEL);

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
        text(x + 12u, y + 36u + lines * 22u, line, C_WHITE, C_PANEL);
        ++lines;
        }
}

static void draw_full(void)
{
    unsigned W = (unsigned)gSh->fb_w;
    unsigned H = (unsigned)gSh->fb_h;
    unsigned i;

    gN = 0;
    cmd(GC_CLEAR, C_BG, 0, 0, 0, 0);
    rect(8u, 8u, W - 16u, 40u, C_TITLE);          /* header */
    text(24u, 18u, "EKA2 PC OS - APPLICATION LAUNCHER", C_WHITE, C_TITLE);
    text(24u, 52u, "Select a program with UP/DOWN or W/S, A/D to change column.",
         C_WHITE, C_BG);
    text(24u, 74u, "ENTER launches     ESC returns to the shell",
         0x00FFE040u, C_BG);
    for (i = 0; i < (unsigned)NAPPS; ++i)
        draw_row(i, (int)i == gSel);
    draw_description((unsigned)gSel);
    rect(8u, H - 40u, W - 16u, 32u, C_FOOT);      /* key-hint footer */
    text(20u, H - 32u,
         "W/S move   A/D column   ENTER launch   ESC exit   R redraw",
         C_WHITE, C_FOOT);
    flush();
}

/* ---- trusted slow exec 268 (SvcMgrLaunch) ------------------------------ */
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
    marker(line);
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
    marker(line);

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
        marker(line);
        }
    /* Marker prints are mirrored into the LFB by the kernel teletype, so
     * repaint silently to keep the menu legible after a launch. */
    draw_full();
}

/* ---- input ------------------------------------------------------------- */
/* Map a typed key to a new selection index; -1 means "not a navigation key". */
static int navigate(int key)
{
    if (key == 'w' || key == 'W' || key == 'k' || key == 'K')
        return (gSel + NAPPS - 1) % NAPPS;
    if (key == 's' || key == 'S' || key == 'j' || key == 'J')
        return (gSel + 1) % NAPPS;
    if (key == 'a' || key == 'A' || key == 'h' || key == 'H')
        return (gSel + COLROWS) % NAPPS;
    if (key == 'd' || key == 'D' || key == 'l' || key == 'L')
        return (gSel + COLROWS) % NAPPS;
    return -1;
}

/* Escape sequences are not produced by the current PS/2 decoder (E0 is
 * dropped), but accept them cheaply if a future kernel decodes arrow keys. */
static int read_key(void)
{
    int c = con_tryread();
    int i, j, d;

    if (c != 27) return c;
    for (i = 0; i < 8; ++i)
        {
        sleep_ms(10);
        d = con_tryread();
        if (d < 0) continue;
        if (d != '[') return 27;
        for (j = 0; j < 8; ++j)
            {
            sleep_ms(10);
            d = con_tryread();
            if (d < 0) continue;
            if      (d == 'A') return 'w';       /* up   */
            else if (d == 'B') return 's';       /* down */
            else if (d == 'C') return 'd';       /* right*/
            else if (d == 'D') return 'a';       /* left */
            else return 27;
            }
        return 27;
        }
    return 27;
}

/* ---- entry point ------------------------------------------------------- */
int app_main(void)
{
    char line[96];
    unsigned k;
    int shb, key, next, idle;

    k = append(line, 0, "GM: fb=");
    k = append(line, k, "0x");
    /* hex32 by hand: exec 249 returns the physical LFB base. */
    {
    unsigned v = (unsigned)exec2(QE_EXEC_GUI_FBBASE, 0, 0);
    const char* h = "0123456789abcdef";
    int i;
    for (i = 28; i >= 0; i -= 4) line[k++] = h[(v >> i) & 0xfu];
    }
    k = append(line, k, " bytes=");
    k = dec(line, k, (unsigned)exec2(QE_EXEC_GUI_FBSIZE, 0, 0));
    line[k] = 0;
    marker(line);

    shb = exec2(QE_EXEC_GUI_DRAW, 0, 0);   /* arg1==0 -> shared buffer base */
    if (shb <= 0)
        {
        marker("GM: FAIL no shared buffer (exec 251)");
        return 1;
        }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->version != GUI_VERSION)
        {
        marker("GM: FAIL shared magic");
        return 1;
        }
    if (gSh->fb_w <= 0 || gSh->fb_h <= 0)
        {
        marker("GM: FAIL framebuffer geometry");
        return 1;
        }

    if (con_acquire() < 0)
        {
        marker("GM: console busy");
        return 1;
        }

    layout();
    gSel = 0;
    draw_full();
    marker("GM: LAUNCHER READY");
    line[0] = 0;
    k = append(line, 0, "GM: apps=");
    k = dec(line, k, (unsigned)NAPPS);
    line[k] = 0;
    marker(line);
    emit_select();

    /* 90 s idle budget at 20 ms/poll: a forgotten menu cannot wedge input. */
    for (idle = 0; idle < 4500; ++idle)
        {
        key = read_key();
        if (key < 0) { sleep_ms(20); continue; }
        idle = 0;

        if (key == 27) break;                       /* Esc: back to shell */
        if (key == 'r' || key == 'R')
            { draw_full(); continue; }              /* silent full repaint */
        if (key == '\n' || key == '\r' || key == ' ')
            { launch_selected(); continue; }        /* Enter: launch app  */

        next = navigate(key);
        if (next < 0 || next == gSel) continue;

        draw_row((unsigned)gSel, 0);                /* repaint changed rows  */
        gSel = next;
        draw_row((unsigned)gSel, 1);
        draw_description((unsigned)gSel);
        flush();
        emit_select();
        }

    marker(idle >= 4500 ? "GM: TIMEOUT" : "GM: DONE");
    con_release();
    return 0;
}

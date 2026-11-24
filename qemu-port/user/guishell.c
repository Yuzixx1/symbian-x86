/*
 * guishell.c -- PE2 E32 GUI desktop shell for the R1 QEMU port.
 *
 * WINSRV MIGRATION CHUNK D5 (docs/GUISHELL_MIGRATION_MAP.md, last interactive
 * app): this program no longer uses the old kernel GUI slow-exec display
 * list or the low-RAM shared-buffer host header.  It runs entirely on the
 * GENUINE EWSRV text-window path:
 *
 *   session/window : ws_open() / ws_window_create() / ws_screen_size() /
 *                    ws_server_ready() / ws_close() from user/ws_e32_client.c
 *                    (chunks A/B), #included below exactly like guilib.c /
 *                    guimenu.c so this translation unit can issue the
 *                    CWsSession opcodes over the very same !E32WindowServer
 *                    session.
 *   paint          : filled rects via ws_draw_fill_rect() (chunk B) and text
 *                    via ws_e32_text_bg() / ws_text() (chunk D0 landed) over
 *                    the genuine ops EConsoleSetTextColors(21) +
 *                    EConsoleSetCursorPosAbs(6) + EConsoleWrite(10).  All
 *                    coordinates are CONSOLE CELLS (the window is a
 *                    WxH cell grid; EConsoleScreenSize reports 128x76 cells
 *                    on the 1024x768 VBE mode).
 *   input          : the genuine console read -- CWsSession op EConsoleRead
 *                    (11).  The one-shot mouse-position readback is issued
 *                    ASYNCHRONOUSLY through RSessionBase::DoSendReceive
 *                    (int,TIpcArgs,TRequestStatus&) (EUSER.DLL ordinal 1548,
 *                    PE2 IAT slot guishell_iat_async) and polled, so the
 *                    self-exiting shell cannot wedge on the synchronous
 *                    ws_get_event() form of the same op.  EConsoleReadCancel
 *                    (op 12) runs on exit.
 *
 * App launching stays with the svcmgr-owned trusted launch executive in the
 * launcher/desktop programs; this file has no launch call site.
 *
 * Markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf) through the
 * ws_e32_client IAT slot ws_e32_iat_printf (0x40800c), with the port's
 * DebugPrint executive as fallback.
 *
 * PE2 multi-import pack (genuine/pack_multi.py --iat-start 0x8000):
 *   0x408000 ord 211  RSessionBase::CreateSession(TDesC16,TVersion,int)
 *   0x408004 ord 1547 RSessionBase::DoSendReceive(int,TIpcArgs const*) const
 *   0x408008 ord 120  RHandleBase::Close()
 *   0x40800c ord 915  RDebug::Printf(char const*,...)   [markers]
 *   0x408010 ord 1548 RSessionBase::DoSendReceive(int,TIpcArgs const*,
 *                     TRequestStatus&) const            [event read]
 */
#include "runtime.h"

/* Chunk A/B/D0/D1 genuine-EWSRV client, included so the paint + event helpers
  * below share its session (sSession) and ws_e32_send(); same pattern as
  * guilib.c / guimenu.c. */
#include "ws_e32_client.c"

/* Fifth PE2 IAT slot: EUSER.DLL ordinal 1548 (async DoSendReceive), seeded by
  * --defsym=guishell_iat_async=0x408010 and written by pack_multi.py. */
extern unsigned guishell_iat_async;
typedef int (*gs_t_send_async)(const void* aThis, int aOp,
                               const void* aArgs, volatile int* aStatus);

static int gs_send_async(int aOp, struct ws_e32_ipcargs* aArgs,
                         volatile int* aStatus)
{
    return ((gs_t_send_async)(*(volatile unsigned*)&guishell_iat_async))
           (&sSession, aOp, aArgs, aStatus);
}

/* Serial marker through genuine EUSER.DLL ord 915 (the ws_e32_client printf
  * slot); DebugPrint fallback when the slot is not bound. */
typedef void (*NbPrintf)(const char*, ...);
static void gs_marker(const char* text)
{
    unsigned fn = *(volatile unsigned*)&ws_e32_iat_printf;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);
    else print(text);
}

/* ws_e32_client.c's own static marker() is unused in this TU (no test main);
  * reference it once so -Wunused-function does not fire. */
static void (*volatile gs_keep_marker)(const char*) = marker;

/* ---- tiny formatters (no libc) ---- */
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

/* ---- paint over the genuine session (all coordinates in CELLS) ---------- */
static int gW, gH;                     /* window size in cells */

static void rect(int x, int y, int w, int h, uint32_t col)
{
    if (w <= 0 || h <= 0) return;
    (void)ws_draw_fill_rect(0, x, y, w, h, col);   /* wid 0 = our window */
}

static void text(int x, int y, const char* s, uint32_t fg, uint32_t bg)
{
    (void)ws_e32_text_bg(0, x, y, s, fg, bg);      /* chunk D0 real glyphs */
}

/* ---- desktop palette (0x00RRGGBB) ---- */
#define C_DESKTOP   0x00000080u   /* dark blue                        */
#define C_TITLEBAR  0x000080C0u
#define C_TITLEFG   0x00FFFFFFu
#define C_WINBODY   0x000060A0u
#define C_WINBORDER 0x00C0C0C0u
#define C_WINHEAD   0x00004080u
#define C_TASKBAR   0x00000040u
#define C_LIVE      0x0000FF00u

static void window(int x, int y, int w, int h, const char* label)
{
    rect(x, y, w, h, C_WINBODY);              /* body */
    rect(x, y, w, 1, C_WINBORDER);           /* top border */
    rect(x, y + h - 1, w, 1, C_WINBORDER);   /* bottom border */
    rect(x, y, 1, h, C_WINBORDER);           /* left border */
    rect(x + w - 1, y, 1, h, C_WINBORDER);   /* right border */
    rect(x + 1, y + 1, w - 2, 2, C_WINHEAD);  /* title strip */
    text(x + 3, y + 1, label, C_TITLEFG, C_WINHEAD);
}

static void paint_desktop(void)
{
    int colw, x;

    rect(0, 0, gW, gH, C_DESKTOP);            /* full-screen clear */

    /* title bar across the top */
    rect(1, 1, gW - 2, 4, C_TITLEBAR);
    rect(1, 5, gW - 2, 1, C_WINBORDER);
    text(3, 2, "EKA2 PC OS DESKTOP", C_TITLEFG, C_TITLEBAR);

    /* three sample windows, evenly spaced */
    colw = (gW - 8) / 3;
    x = 2;
    window(x, 8, colw, gH / 2 - 8, "FILES");
    x += colw + 2;
    window(x, 8, colw, gH / 2 - 8, "SYSTEM");
    x += colw + 2;
    window(x, 8, colw, gH / 2 - 8, "ABOUT");

    /* taskbar with a status label */
    rect(1, gH - 3, gW - 2, 2, C_TASKBAR);
    text(2, gH - 2, "EKA2 DESKTOP READY", C_TITLEFG, C_TASKBAR);

    ws_present(0);                            /* no-op on the E32 path */
}

/* A second frame after a short delay: a green "live" indicator proves the
   shell can animate the real framebuffer, not just paint once at boot. */
static void animate_frame(void)
{
    rect(gW - 16, gH - 2, 2, 1, C_LIVE);
    text(gW - 13, gH - 2, "LIVE", C_LIVE, C_TASKBAR);
    ws_present(0);
}

/* ---- genuine EWSRV event read (op 11) via async DoSendReceive -------------
 * SConsoleKey (kernelhwsrv .../e32twin.h) as written by CWsWindow::
 * DrainReadRequest(): iCode, iModifiers, iMousePos, iType, iPointerNumber.
 * The KBINJ bridge (variant/qemu_variant_int.cpp) delivers PS/2 events into
 * genuine EWSRV, which completes the pending read. */
static volatile int sEvStatus;
static struct ws_e32_console_key sEvKey;
static int sEvArmed;

static void gs_event_arm(void)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_des8 kd;
    if (!sOpen || sWin < 0 || sEvArmed) return;
    kd.iLength = sizeof(sEvKey);
    kd.iPtr = (unsigned char*)&sEvKey;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&kd;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDES8 << 0);         /* TDes8 slot: keystroke out */
    sEvStatus = QE_KREQUEST_PENDING;
    (void)gs_send_async(WS_E32_OP_CONSOLE_READ, &args, &sEvStatus);
    sEvArmed = 1;
}

/* Returns 1 when an event arrived (sEvKey filled), 0 while pending,
 * negative on a completed-with-error read. */
static int gs_event_poll(void)
{
    if (!sEvArmed) return 0;
    if (sEvStatus == QE_KREQUEST_PENDING) return 0;
    sEvArmed = 0;
    return sEvStatus == 0 ? 1 : -1;
}

static void gs_event_cancel(void)
{
    struct ws_e32_ipcargs args;
    if (!sEvArmed) return;
    ws_e32_args0(&args);
    (void)ws_e32_send(WS_E32_OP_CONSOLE_READ_CANCEL, &args);
    sEvArmed = 0;
}

/* ---- entry point --------------------------------------------------------- */
int app_main(void)
{
    char line[128];
    unsigned k;
    int r, w = 0, h = 0;

    gs_marker("GUI: D5 e32 desktop shell start");

    /* Session handle replaces the old shared-buffer pointer. */
    r = ws_open();
    k = append(line, 0, "GUI: ws_open r=");
    k = dec(line, k, (unsigned)r);
    line[k] = 0;
    gs_marker(line);
    if (r != 0) return 1;

    /* Geometry comes from the genuine EConsoleScreenSize (op 17), reported
     * in console cells (128x76 on the 1024x768 VBE mode). */
    r = ws_screen_size(&w, &h);
    k = append(line, 0, "GUI: screen r=");
    k = dec(line, k, (unsigned)r);
    k = append(line, k, " ");
    k = dec(line, k, (unsigned)w);
    k = append(line, k, "x");
    k = dec(line, k, (unsigned)h);
    k = append(line, k, " cells");
    line[k] = 0;
    gs_marker(line);
    if (r != 0 || w <= 0 || h <= 0) { w = 126; h = 74; }
    gW = w; gH = h;
    if (gW > 128) gW = 128;
    if (gH > 76) gH = 76;

    r = ws_window_create(0, 0, gW, gH, "EKA2 DESKTOP");
    k = append(line, 0, "GUI: window_create r=");
    k = dec(line, k, (unsigned)r);
    line[k] = 0;
    gs_marker(line);
    if (r != 0)
        { ws_close(); return 1; }
    if (!ws_server_ready())                   /* replaces the magic check */
        {
        gs_marker("GUI: FAIL server not ready");
        ws_close();
        return 1;
        }

    paint_desktop();
    gs_marker("GUI: desktop drawn");
    gs_marker("GUI: 3 windows");

    /* Mouse-position readback: the old shared-buffer pointer fields become
     * a genuine EConsoleRead event, polled async (ord 1548) so this one-shot
     * shell never blocks the way the sync ws_get_event() would. */
    gs_event_arm();
    sleep_ms(200);
    r = gs_event_poll();
    k = append(line, 0, "GUI: mouse x=");
    k = dec(line, k, r == 1 ? (unsigned)sEvKey.iMousePos.iX : 0u);
    k = append(line, k, " y=");
    k = dec(line, k, r == 1 ? (unsigned)sEvKey.iMousePos.iY : 0u);
    if (r != 1) k = append(line, k, " (no event)");
    line[k] = 0;
    gs_marker(line);
    gs_event_cancel();

    sleep_ms(300);
    animate_frame();
    gs_marker("GUI: animation frame drawn");

    gs_marker("GUI: DONE");
    ws_close();
    return 0;
}

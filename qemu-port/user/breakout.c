/*
 * breakout.c -- PE2 E32 BREAKOUT arcade game for the R1 QEMU port.
 *
 * WINSRV MIGRATION chunk D3 (docs/GUISHELL_MIGRATION_MAP.md): the game now
 * runs entirely on the GENUINE EWSRV text-window path.  user/ws_e32_client.c
 * (chunks A/B) is #included below, exactly like user/guilib.c does, so the
 * same translation unit owns the RSessionBase session to "!E32WindowServer":
 *
 *   session/window : ws_open() / ws_window_create() / ws_screen_size() /
 *                    ws_close()                         -- chunk A code.
 *   pixels         : ws_draw_fill_rect() over the genuine CWsSession ops
 *                    EConsoleSetTextColors(21) + EConsoleSetCursorPosAbs(6)
 *                    + EConsoleWrite(10)                -- chunk B code.
 *   full clear     : EConsoleSetTextColors + EConsoleClearScreen(2), one IPC.
 *   text           : local brk_text() over the same three ops (ws_e32_client's
 *                    ws_text is still a stub; this is the guilib.c pattern).
 *   keyboard       : genuine EWSRV event read -- CWsSession op 11
 *                    (EConsoleRead) issued ASYNC through EUSER.DLL ordinal
 *                    1548 (RSessionBase::DoSendReceive(op,args,TRequestStatus&))
 *                    into a TPckgBuf<SConsoleKey>; the kernel-side KBINJ bridge
 *                    (variant/qemu_variant_int.cpp, \KBINJ.TXT gate) feeds
 *                    EWSRV's key queue, the pending read completes and the
 *                    frame loop polls the TRequestStatus (KRequestPending =
 *                    no key yet).  One read is kept armed at all times.
 *
 * The old kernel display-list execs, the shared-buffer host header and the
 * PortCon input lease are all GONE from this file.
 * Layout is in character cells (the genuine console window is a cell grid,
 * 8x10 px per cell; EConsoleScreenSize reports cells -- 128x76 on the 1024x768
 * VBE mode).  Serial markers use the genuine EUSER.DLL ordinal 915
 * (RDebug::Printf) through ws_e32_client's ws_e32_iat_printf slot.
 *
 * PE2 multi-import pack (genuine/pack_multi.py, IAT at codeBase+0x8000):
 *   0x408000 ord 211  RSessionBase::CreateSession    (ws_e32_client)
 *   0x408004 ord 1547 RSessionBase::DoSendReceive    (ws_e32_client, sync)
 *   0x408008 ord 120  RHandleBase::Close             (ws_e32_client)
 *   0x40800c ord 915  RDebug::Printf                 (ws_e32_client markers)
 *   0x408010 ord 1548 RSessionBase::DoSendReceive async (breakout_iat_async,
 *                     the EConsoleRead event op transport)
 */
#include "runtime.h"

/* Chunk A/B client, included so the draw + event helpers below can use its
 * session (sSession) and ws_e32_send() directly -- the user/guilib.c pattern. */
#include "ws_e32_client.c"

/* ---- extra genuine CWsSession opcodes (kernelhwsrv .../ws_std.h) --------- */
#define BRK_OP_CLEAR_SCREEN      2    /* EConsoleClearScreen                 */
#define BRK_OP_CONSOLE_READ     11    /* EConsoleRead  (async event read)    */
#define BRK_OP_CONSOLE_READ_CANCEL 12 /* EConsoleReadCancel                 */

/* EUSER.DLL ordinal 1548: RSessionBase::DoSendReceive(int, const TIpcArgs*,
 * TRequestStatus&) -- the async form.  PE2 IAT slot breakout_iat_async. */
extern unsigned breakout_iat_async;
typedef void (*brk_async_send_t)(const void* aThis, int aOp,
                                 const void* aArgs, volatile int* aStatus);

/* SConsoleKey (kernelhwsrv .../e32twin.h) as the EConsoleRead payload:
 * TPckgBuf<SConsoleKey> is passed as an EDes8 in IPC slot 0. */
struct brk_console_key {
    unsigned iCode;                 /* TKeyCode                            */
    unsigned iModifiers;            /* TUint                               */
    int      iMouseX, iMouseY;      /* TPoint iMousePos                    */
    int      iType;                 /* EKeyPress=0 / EMouseClick=1         */
    unsigned char iPointerNumber;   /* TUint8                              */
};

static volatile int sKeyStatus;     /* TRequestStatus of the armed read    */
static struct brk_console_key sKey; /* SConsoleKey buffer                  */
static int sReadArmed;

/* ---- tiny formatters (no hosted libc) ---- */
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
static unsigned sdec(char* p, unsigned k, int v)
{
    unsigned mag = (unsigned)v;
    if (v < 0) { p[k++] = '-'; mag = 0u - mag; }
    return dec(p, k, mag);
}

/* ---- palette (0x00RRGGBB; mapped to the genuine 256-colour palette by
 *      ws_e32_client's ws_e32_rgb_to_index) ---- */
#define C_BG      0x00000080u   /* dark blue  */
#define C_TITLE   0x000080C0u   /* title bar  */
#define C_WHITE   0x00FFFFFFu
#define C_YELLOW  0x00FFE040u
#define C_RED     0x00FF3020u
#define C_CYAN    0x0020E0E0u
#define C_GREY    0x00A0A0A0u

static const unsigned BRICK_COL[5] = {
    0x00FF3020u,  /* red    */
    0x00FF9020u,  /* orange */
    0x00FFE040u,  /* yellow */
    0x0020FF40u,  /* green  */
    0x003060FFu   /* blue   */
};

/* ---- game geometry / state (ALL IN CHARACTER CELLS) ---- */
#define COLS 10u
#define ROWS 5u
#define BALL 2          /* 2x2 cells = 16x20 px                    */
#define PADDLE_W 14     /* cells                                   */
#define PADDLE_H 1

static int W, H;                        /* window size in cells        */
static int pf_left, pf_right, pf_top, pf_bottom;
static int bx, by, bvx, bvy;
static int px, py;
static int score, lives, alive;
static int bricks[ROWS][COLS];
static int mode;              /* 0 = title, 1 = playing, 2 = gameover, 3 = win */
static unsigned frame;
static int running;

enum { M_TITLE = 0, M_PLAY = 1, M_OVER = 2, M_WIN = 3 };

/* ---- low-level console ops over the included ws_e32_client session ------ */
static int brk_colors(uint32_t fg, uint32_t bg)
{
    struct ws_e32_ipcargs args;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)ws_e32_rgb_to_index(fg);   /* fg, TInt slot */
    args.iArgs[1] = (int)ws_e32_rgb_to_index(bg);   /* bg, TInt slot */
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_TEXT_COLORS, &args);
}

static int brk_cursor(int col, int row)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_desC8 pckg;
    struct ws_e32_point pt;
    pt.iX = col; pt.iY = row;
    pckg.iLength = sizeof(pt);
    pckg.iPtr = (const unsigned char*)&pt;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&pckg;
    args.iFlags = (WS_E32_EDESC_C8 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_SET_CURSOR_POS_ABS, &args);
}

static int brk_write(const char* s, unsigned n)
{
    unsigned short buf[64];         /* one text run, UTF-16 like _L()     */
    unsigned i;
    struct ws_e32_ipcargs args;
    struct ws_e32_desC16 desc;
    if (n > 64u) n = 64u;
    for (i = 0; i < n; ++i) buf[i] = (unsigned short)(unsigned char)s[i];
    desc.iLength = n;
    desc.iPtr = buf;
    ws_e32_args0(&args);
    args.iArgs[0] = (int)&desc;
    args.iFlags = (WS_E32_EDESC_C16 << 0);
    return ws_e32_send(WS_E32_OP_CONSOLE_WRITE, &args);
}

static void brk_text(int col, int row, const char* s, uint32_t fg, uint32_t bg)
{
    unsigned n = 0;
    while (s && s[n] && n < 64u) ++n;
    if (!n) return;
    (void)brk_colors(fg, bg);
    (void)brk_cursor(col, row);
    (void)brk_write(s, n);
}

static void brk_clear(uint32_t bg)
{
    struct ws_e32_ipcargs args;
    (void)brk_colors(C_WHITE, bg);      /* Clear() fills with current bg   */
    ws_e32_args0(&args);
    (void)ws_e32_send(BRK_OP_CLEAR_SCREEN, &args);
}

static void rect(int x, int y, int w, int h, uint32_t col)
{
    (void)ws_draw_fill_rect(0, x, y, w, h, col);   /* wid 0 = our window */
}

static void emit_dec(int col, int row, unsigned v, uint32_t fg,
                     uint32_t bg, unsigned digits)
{
    char b[16];
    unsigned i;
    for (i = 0; i < digits && i < 15u; ++i)
        {
        unsigned div = 1;
        unsigned d = i;
        while (d) { div *= 10u; --d; }
        b[i] = (char)('0' + (v / div) % 10u);
        }
    b[i < 15u ? i : 15u] = 0;
    brk_text(col, row, b, fg, bg);
}

/* ---- genuine EWSRV keyboard: async EConsoleRead, one armed at a time ---- */
static void read_arm(void)
{
    struct ws_e32_ipcargs args;
    struct ws_e32_des8 desc;
    ws_e32_args0(&args);
    desc.iLength = sizeof(sKey);        /* TPckgBuf<SConsoleKey> as EDes8  */
    desc.iPtr = (unsigned char*)&sKey;
    args.iArgs[0] = (int)&desc;
    args.iArgs[1] = args.iArgs[2] = args.iArgs[3] = 0;
    args.iFlags = (WS_E32_EDES8 << 0);
    sKeyStatus = QE_KREQUEST_PENDING;   /* TRequestStatus before the call  */
    ((brk_async_send_t)(*(volatile unsigned*)&breakout_iat_async))
        (&sSession, BRK_OP_CONSOLE_READ, &args, &sKeyStatus);
    sReadArmed = 1;
}

/* Returns one translated TKeyCode (>=0) or -1 if no key event is pending. */
static int key_poll(void)
{
    int st, c;
    if (!sReadArmed) return -1;
    st = sKeyStatus;
    if (st == QE_KREQUEST_PENDING) return -1;   /* still waiting          */
    sReadArmed = 0;
    if (st != 0) return -1;                      /* KErrCancel etc: drop   */
    c = (int)sKey.iCode;                         /* SConsoleKey.iCode      */
    read_arm();                                  /* re-arm for the next key */
    return c;
}

/* ---- game state ---- */
static void reset_ball(void)
{
    bx = W / 2 - BALL / 2;
    by = pf_top + (int)ROWS * 3 + 2;
    bvx = 1;
    bvy = -2;
}

static void reset_game(void)
{
    unsigned r, c;
    score = 0;
    lives = 3;
    alive = (int)(COLS * ROWS);
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c < COLS; ++c) bricks[r][c] = 1;
    px = W / 2 - PADDLE_W / 2;
    py = pf_bottom - 1;
    reset_ball();
    mode = M_PLAY;
}

/* ---- input: keys from the genuine EWSRV event read (op 11) ----
 * A/D move the paddle, ENTER (re)starts, ESC quits.  The port keymap maps
 * letter keys to their ASCII codes, so both cases are accepted. */
static int move_dir;
static int move_hold;

static void handle_key(int c)
{
    if (c == 'a' || c == 'A')
        { move_dir = -1; move_hold = 4; }
    else if (c == 'd' || c == 'D')
        { move_dir = 1; move_hold = 4; }
    else if (c == 0x0d)                 /* EKeyEnter */
        {
        if (mode == M_TITLE || mode == M_OVER || mode == M_WIN)
            {
            reset_game();
            marker("BRK: restart");
            }
        }
    else if (c == 0x1b)                 /* EKeyEscape */
        running = 0;
}

/* ---- physics (cell units) ---- */
static void update(void)
{
    unsigned r, c;
    int hit = 0;

    if (move_hold > 0)
        {
        px += move_dir * 2;
        --move_hold;
        if (px < pf_left) px = pf_left;
        if (px > pf_right - PADDLE_W) px = pf_right - PADDLE_W;
        }

    bx += bvx;
    by += bvy;

    if (bx < pf_left) { bx = pf_left; bvx = -bvx; }
    if (bx + BALL > pf_right) { bx = pf_right - BALL; bvx = -bvx; }
    if (by < pf_top) { by = pf_top; bvy = -bvy; }

    /* paddle bounce (angle depends on where the ball struck) */
    if (bvy > 0 && by + BALL >= py && by <= py + PADDLE_H &&
        bx + BALL >= px && bx <= px + PADDLE_W)
        {
        int centre = bx + BALL / 2;
        int offset = centre - (px + PADDLE_W / 2);
        bvy = -bvy;
        bvx = offset / 4;
        if (bvx == 0) bvx = (offset >= 0) ? 1 : -1;
        if (bvx > 3) bvx = 3;
        if (bvx < -3) bvx = -3;
        by = py - BALL;
        }

    /* brick collision: first overlapping brick only */
    for (r = 0; r < ROWS && !hit; ++r)
        {
        for (c = 0; c < COLS; ++c)
            {
            int rx = pf_left + 1 + (int)c * (((pf_right - pf_left) - 2) / (int)COLS);
            int rw = ((pf_right - pf_left) - 2) / (int)COLS - 1;
            int ry = pf_top + (int)r * 3;
            int rh = 2;
            int ox, oy, br, bb, rr, tb;
            if (!bricks[r][c]) continue;
            if (bx + BALL <= rx || bx >= rx + rw ||
                by + BALL <= ry || by >= ry + rh) continue;

            br = rx + rw;  bb = bx + BALL;
            rr = ry + rh;  tb = by + BALL;
            ox = (bb < br ? bb : br) - (bx > rx ? bx : rx);
            oy = (tb < rr ? tb : rr) - (by > ry ? by : ry);
            if (ox < oy) bvx = -bvx; else bvy = -bvy;
            bricks[r][c] = 0;
            --alive;
            score += 100;
            hit = 1;
            break;
            }
        }
    if (hit && alive <= 0)
        {
        mode = M_WIN;
        marker("BRK: WIN all bricks cleared");
        }

    /* lost ball */
    if (by > pf_bottom)
        {
        --lives;
        if (lives <= 0)
            {
            char line[48];
            unsigned k = append(line, 0, "BRK: GAME OVER score=");
            k = dec(line, k, (unsigned)score);
            line[k] = 0;
            marker(line);
            mode = M_OVER;
            }
        else
            reset_ball();
        }
}

/* ---- render one full frame (cell grid, EWSRV blits on every write) ---- */
static void draw_frame(void)
{
    unsigned r, c;

    brk_clear(C_BG);
    rect(0, 0, W, 1, C_TITLE);
    brk_text(W / 2 - 4, 0, "BREAKOUT", C_WHITE, C_TITLE);

    /* HUD */
    brk_text(1, 2, "SCORE:", C_YELLOW, C_BG);
    emit_dec(8, 2, (unsigned)score, C_WHITE, C_BG, 6);
    brk_text(W - 15, 2, "LIVES:", C_YELLOW, C_BG);
    emit_dec(W - 8, 2, (unsigned)lives, C_WHITE, C_BG, 1);

    /* bricks */
    for (r = 0; r < ROWS; ++r)
        {
        int ry = pf_top + (int)r * 3;
        for (c = 0; c < COLS; ++c)
            {
            int rx, rw;
            if (!bricks[r][c]) continue;
            rw = ((pf_right - pf_left) - 2) / (int)COLS - 1;
            rx = pf_left + 1 + (int)c * (((pf_right - pf_left) - 2) / (int)COLS);
            rect(rx, ry, rw, 2, BRICK_COL[r]);
            }
        }

    /* paddle + ball */
    rect(px, py, PADDLE_W, PADDLE_H, C_CYAN);
    rect(bx, by, BALL, BALL, C_WHITE);

    /* status / help line */
    if (mode == M_PLAY)
        brk_text(1, H - 1, "A/D MOVE  ENTER RESTART  ESC QUIT", C_GREY, C_BG);
    else if (mode == M_TITLE)
        brk_text(1, H - 1, "PRESS ENTER TO START", C_YELLOW, C_BG);
    else if (mode == M_WIN)
        brk_text(1, H - 1, "YOU WIN!  ENTER TO PLAY AGAIN", C_YELLOW, C_BG);
    else
        {
        brk_text(W / 2 - 9, H / 2 - 1, "GAME OVER", C_RED, C_BG);
        brk_text(W / 2 - 12, H / 2 + 1, "PRESS ENTER TO RESTART", C_YELLOW, C_BG);
        }
}

int app_main(void)
{
    char line[96];
    unsigned n;
    int r, sw = 0, sh = 0;
    unsigned last_score, last_lives;

    marker("BRK: app_main (genuine EWSRV text-window path)");

    /* Chunk A session to the genuine "!E32WindowServer". */
    r = ws_open();
    n = append(line, 0, "BRK: ws_open r="); n = sdec(line, n, r); line[n] = 0;
    marker(line);
    if (r != 0) return 1;
    if (!ws_server_ready()) { marker("BRK: FAIL server not ready"); return 1; }

    /* EConsoleScreenSize reports CHARACTER CELLS (128x76 on 1024x768). */
    r = ws_screen_size(&sw, &sh);
    n = append(line, 0, "BRK: screen r="); n = sdec(line, n, r);
    n = append(line, n, " "); n = dec(line, n, (unsigned)sw);
    n = append(line, n, "x"); n = dec(line, n, (unsigned)sh); line[n] = 0;
    marker(line);
    if (r != 0 || sw < 42 || sh < 22) { sw = 80; sh = 25; }

    /* The genuine window adds a 2-cell border, so the usable grid is
     * screen cells minus 2 in each dimension. */
    W = sw - 2;
    H = sh - 2;
    r = ws_window_create(0, 0, W, H, "BREAKOUT");
    n = append(line, 0, "BRK: window r="); n = sdec(line, n, r);
    n = append(line, n, " "); n = dec(line, n, (unsigned)W);
    n = append(line, n, "x"); n = dec(line, n, (unsigned)H); line[n] = 0;
    marker(line);
    if (r != 0) { ws_close(); marker("BRK: FAIL window_create"); return 1; }

    pf_left = 1;
    pf_right = W - 1;
    pf_top = 4;
    pf_bottom = H - 2;

    marker("BRK: BREAKOUT READY");

    /* Arm the genuine EWSRV event read before entering the loop. */
    read_arm();

    /* Auto-start playing so the ball is genuinely animated from launch;
       ENTER still restarts at any time. */
    reset_game();
    frame = 0;
    running = 1;
    last_score = (unsigned)score;
    last_lives = (unsigned)lives;
    marker("BRK: playing");

    while (running)
        {
        int c;
        while ((c = key_poll()) >= 0)
            handle_key(c);
        if (mode == M_PLAY) update();

        draw_frame();
        ++frame;

        if ((frame % 30u) == 0u)
            {
            n = append(line, 0, "BRK: frame ");
            n = dec(line, n, frame);
            line[n] = 0;
            marker(line);
            }
        if ((unsigned)score != last_score || (unsigned)lives != last_lives)
            {
            n = append(line, 0, "BRK: score ");
            n = dec(line, n, (unsigned)score);
            n = append(line, n, " lives ");
            n = dec(line, n, (unsigned)lives);
            line[n] = 0;
            marker(line);
            last_score = (unsigned)score;
            last_lives = (unsigned)lives;
            }

        sleep_ms(20);   /* ~30-60 fps including EWSRV blit time */
        }

    /* Cancel the armed read, then tear the session down. */
    {
    struct ws_e32_ipcargs args;
    ws_e32_args0(&args);
    (void)ws_e32_send(BRK_OP_CONSOLE_READ_CANCEL, &args);
    }
    ws_close();
    marker("BRK: DONE");
    return 0;
}

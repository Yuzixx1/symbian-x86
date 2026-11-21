/*
 * breakout.c -- PE2 E32 BREAKOUT arcade game for the R1 QEMU port.
 *
 * A genuinely animated, real-time graphical game rendered to the real VBE
 * linear framebuffer.  A ring-3 PE2 builds a display list in the low-RAM
 * gui_shared buffer and asks the kernel (slow exec 251, arg1>0) to paint it in
 * ring-0 context -- VRAM lives in the supervisor-only half of the flat page
 * map, so the game can never touch the LFB directly.  Execs 249/250 discover
 * the framebuffer base/size; exec 251 (arg1==0) returns the shared buffer.
 *
 * Game: colourful brick rows, a player paddle, a ball with wall/paddle/brick
 * collision physics, score, 3 lives, WIN and GAME OVER states.  Input is real
 * keyboard input through the PortCon lease: A / D move the paddle, ENTER
 * (re)starts, ESC quits.  NOTE: the kernel's PS/2 driver deliberately drops
 * extended (0xE0) scancodes, so arrow keys never reach userland; A/D are the
 * guaranteed controls (documented in the on-screen help).
 *
 * Serial markers use the genuine EUSER.DLL ordinal 915 (RDebug::Printf)
 * imported through the PE2 IAT slot breakout_printf_slot at 0x408000, with the
 * port's DebugPrint executive as a fallback (same pattern as saver.c).
 */
#include "runtime.h"
#include "guishost.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int breakout_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&breakout_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);  /* EUSER ord 915 */
    else print(text);                                  /* no EUSER bound */
}

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

/* ---- display-list builder (guishost.h layout, max 160 cmds/frame) ---- */
static struct gui_shared* gSh;
static unsigned gN;

static void cmd(unsigned type, unsigned a, unsigned b,
                unsigned c, unsigned d, unsigned e)
{
    if (gN >= GUI_MAX_CMDS) return;
    struct gui_cmd* q = &gSh->cmds[gN++];
    q->type = type; q->a = a; q->b = b; q->c = c; q->d = d; q->e = e;
}
static void rect(unsigned x, unsigned y, unsigned w, unsigned h, unsigned col)
    { cmd(GC_RECT, x, y, w, h, col); }
static void text(unsigned x, unsigned y, const char* s,
                 unsigned fg, unsigned bg)
{
    while (*s)
        {
        cmd(GC_PUTC, x, y, (unsigned char)*s, fg, bg);
        x += 16u; ++s;   /* QEMU_DISPLAY_CELL_W (8px glyph * 2 scale) */
        }
}

/* ---- palette (0x00RRGGBB) ---- */
#define C_BG      0x00000080u   /* dark blue: matches kernel pixel readback */
#define C_TITLE   0x000080C0u   /* title bar: matches kernel pixel readback  */
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

/* ---- game geometry / state ---- */
#define COLS 10u
#define ROWS 5u
#define BALL 12
#define PADDLE_W 110
#define PADDLE_H 14

static unsigned W, H;
static unsigned pf_left, pf_right, pf_top, pf_bottom;
static int bx, by, bvx, bvy;
static int px, py;
static int score, lives, alive;
static int bricks[ROWS][COLS];
static int mode;              /* 0 = title, 1 = playing, 2 = gameover, 3 = win */
static unsigned frame;
static int running;

enum { M_TITLE = 0, M_PLAY = 1, M_OVER = 2, M_WIN = 3 };

static void emit_dec(unsigned x, unsigned y, unsigned v, unsigned fg,
                     unsigned bg, unsigned digits)
{
    char b[16];
    unsigned k = 0, i;
    /* right-align v into `digits` places */
    for (i = 0; i < digits; ++i)
        {
        unsigned div = 1;
        unsigned d = i;
        while (d) { div *= 10u; --d; }
        b[i] = (char)('0' + (v / div) % 10u);
        }
    b[digits] = 0;
    /* strip nothing: keep leading zeros for a stable HUD width */
    (void)k;
    text(x, y, b, fg, bg);
}

static void reset_ball(void)
{
    /* Start just below the brick field, moving up: a real brick hit (and the
       resulting score marker) happens within a few frames of launch. */
    bx = (int)(W / 2u) - BALL / 2;
    by = (int)pf_top + 16 + (int)ROWS * 34 + 28;
    bvx = 4;
    bvy = -6;
}

static void reset_game(void)
{
    unsigned r, c;
    score = 0;
    lives = 3;
    alive = (int)(COLS * ROWS);
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c < COLS; ++c) bricks[r][c] = 1;
    px = (int)(W / 2u) - PADDLE_W / 2;
    py = (int)pf_bottom - 24;
    reset_ball();
    mode = M_PLAY;
}

/* ---- input: real keyboard via the PortCon console lease ---- */
static int move_dir;
static int move_hold;

static void handle_input(void)
{
    int c;
    while ((c = con_tryread()) >= 0)
        {
        if (c == 'a' || c == 'A')
            { move_dir = -1; move_hold = 8; }
        else if (c == 'd' || c == 'D')
            { move_dir = 1; move_hold = 8; }
        else if (c == '\r' || c == '\n')
            {
            if (mode == M_TITLE || mode == M_OVER || mode == M_WIN)
                {
                reset_game();
                marker("BRK: restart");
                }
            }
        else if (c == 27)   /* ESC */
            running = 0;
        }
}

/* ---- physics ---- */
static void update(void)
{
    unsigned r, c;
    int hit = 0;

    /* paddle (held direction persists a few frames so a tap is responsive
       even without PS/2 auto-repeat) */
    if (move_hold > 0)
        {
        px += move_dir * 9;
        --move_hold;
        if (px < (int)pf_left) px = (int)pf_left;
        if (px > (int)pf_right - PADDLE_W) px = (int)pf_right - PADDLE_W;
        }

    /* ball integration */
    bx += bvx;
    by += bvy;

    /* walls */
    if (bx < (int)pf_left) { bx = (int)pf_left; bvx = -bvx; }
    if (bx + BALL > (int)pf_right) { bx = (int)pf_right - BALL; bvx = -bvx; }
    if (by < (int)pf_top) { by = (int)pf_top; bvy = -bvy; }

    /* paddle bounce (angle depends on where the ball struck) */
    if (bvy > 0 && by + BALL >= py && by <= py + PADDLE_H &&
        bx + BALL >= px && bx <= px + PADDLE_W)
        {
        int centre = bx + BALL / 2;
        int offset = centre - (px + PADDLE_W / 2);
        bvy = -bvy;
        bvx = offset / 13;
        if (bvx == 0) bvx = (offset >= 0) ? 2 : -2;
        if (bvx > 6) bvx = 6;
        if (bvx < -6) bvx = -6;
        by = py - BALL;
        }

    /* brick collision: first overlapping brick only */
    for (r = 0; r < ROWS && !hit; ++r)
        {
        for (c = 0; c < COLS; ++c)
            {
            unsigned rx = pf_left + 8u + c * (((pf_right - pf_left) - 16u) / COLS);
            unsigned rw = ((pf_right - pf_left) - 16u) / COLS - 4u;
            unsigned ry = pf_top + 16u + r * 34u;
            unsigned rh = 28u;
            int ox, oy, br, bb, rr, tb;
            if (!bricks[r][c]) continue;
            if (bx + BALL <= (int)rx || bx >= (int)(rx + rw) ||
                by + BALL <= (int)ry || by >= (int)(ry + rh)) continue;

            br = (int)(rx + rw);  bb = bx + BALL;
            rr = (int)(ry + rh);  tb = by + BALL;
            ox = (bb < br ? bb : br) - (bx > (int)rx ? bx : (int)rx);
            oy = (tb < rr ? tb : rr) - (by > (int)ry ? by : (int)ry);
            if (ox < oy) bvx = -bvx; else bvy = -bvy;
            bricks[r][c] = 0;
            --alive;
            score += 100;
            hit = 1;
            break;
            }
        }
    if (hit)
        {
        if (alive <= 0)
            {
            mode = M_WIN;
            marker("BRK: WIN all bricks cleared");
            }
        }

    /* lost ball */
    if (by > (int)pf_bottom)
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

/* ---- render one full frame into the display list ---- */
static void draw_frame(void)
{
    unsigned r, c;
    gN = 0;

    /* background + title bar (keeps the kernel readback at (0,0)/(100,20) OK) */
    cmd(GC_CLEAR, C_BG, 0, 0, 0, 0);
    rect(8, 8, W - 16, 34, C_TITLE);
    text(W / 2u - 64u, 12, "BREAKOUT", C_WHITE, C_TITLE);

    /* HUD */
    text(16, 50, "SCORE:", C_YELLOW, C_BG);
    emit_dec(16u + 6u * 16u, 50, (unsigned)score, C_WHITE, C_BG, 6);
    text(W - 152u, 50, "LIVES:", C_YELLOW, C_BG);
    emit_dec(W - 152u + 6u * 16u, 50, (unsigned)lives, C_WHITE, C_BG, 1);

    /* bricks */
    for (r = 0; r < ROWS; ++r)
        {
        unsigned ry = pf_top + 16u + r * 34u;
        for (c = 0; c < COLS; ++c)
            {
            unsigned rx, rw;
            if (!bricks[r][c]) continue;
            rw = ((pf_right - pf_left) - 16u) / COLS - 4u;
            rx = pf_left + 8u + c * (((pf_right - pf_left) - 16u) / COLS);
            rect(rx, ry, rw, 28u, BRICK_COL[r]);
            rect(rx, ry, rw, 4u, C_WHITE);   /* top highlight */
            }
        }

    /* paddle + ball */
    rect((unsigned)px, (unsigned)py, PADDLE_W, PADDLE_H, C_CYAN);
    rect((unsigned)px + 4u, (unsigned)py + 2u, PADDLE_W - 8u, 3u, C_WHITE);
    rect((unsigned)bx, (unsigned)by, BALL, BALL, C_WHITE);

    /* status / help line */
    if (mode == M_PLAY)
        text(16, H - 26u, "A/D MOVE  ENTER RESTART  ESC QUIT", C_GREY, C_BG);
    else if (mode == M_TITLE)
        text(16, H - 26u, "PRESS ENTER TO START", C_YELLOW, C_BG);
    else if (mode == M_WIN)
        text(16, H - 26u, "YOU WIN!  ENTER TO PLAY AGAIN", C_YELLOW, C_BG);
    else
        {
        text(W / 2u - 72u, H / 2u - 20u, "GAME OVER", C_RED, C_BG);
        text(W / 2u - 128u, H / 2u + 8u, "PRESS ENTER TO RESTART",
             C_YELLOW, C_BG);
        }

    exec2(QE_EXEC_GUI_DRAW, gN, 0);   /* arg1>0 -> run list + refresh mouse */
}

int app_main(void)
{
    char line[96];
    unsigned k;
    int shb;
    unsigned last_score, last_lives;

    /* discover the framebuffer through execs 249/250/251 */
    {
    unsigned base = (unsigned)exec2(QE_EXEC_GUI_FBBASE, 0, 0);
    const char* h = "0123456789abcdef";
    k = append(line, 0, "BRK: framebase=0x");
    for (int i = 28; i >= 0; i -= 4) line[k++] = h[(base >> i) & 0xfu];
    k = append(line, k, " size=");
    k = dec(line, k, (unsigned)exec2(QE_EXEC_GUI_FBSIZE, 0, 0));
    line[k] = 0;
    marker(line);
    }

    shb = exec2(QE_EXEC_GUI_DRAW, 0, 0);   /* arg1==0 -> shared buffer base */
    if (shb <= 0) { marker("BRK: FAIL no shared buffer"); return 1; }
    gSh = (struct gui_shared*)shb;
    if (gSh->magic != GUI_MAGIC || gSh->version != GUI_VERSION)
        { marker("BRK: FAIL shared magic"); return 1; }

    W = (unsigned)gSh->fb_w;
    H = (unsigned)gSh->fb_h;
    pf_left = 24u;
    pf_right = W - 24u;
    pf_top = 76u;
    pf_bottom = H - 40u;

    {
    char geo[16];
    unsigned g = dec(geo, 0, W);
    unsigned n;
    geo[g] = 'x'; ++g;
    g = dec(geo, g, H);
    geo[g] = 0;
    n = append(line, 0, "BRK: geometry ");
    n = append(line, n, geo);
    line[n] = 0;
    marker(line);
    }

    marker("BRK: BREAKOUT READY");

    if (con_acquire() < 0)
        { marker("BRK: console busy"); return 1; }

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
        handle_input();
        if (mode == M_PLAY) update();

        draw_frame();
        ++frame;

        if ((frame % 30u) == 0u)
            {
            unsigned n = append(line, 0, "BRK: frame ");
            n = dec(line, n, frame);
            line[n] = 0;
            marker(line);
            }
        if ((unsigned)score != last_score || (unsigned)lives != last_lives)
            {
            unsigned n = append(line, 0, "BRK: score ");
            n = dec(line, n, (unsigned)score);
            n = append(line, n, " lives ");
            n = dec(line, n, (unsigned)lives);
            line[n] = 0;
            marker(line);
            last_score = (unsigned)score;
            last_lives = (unsigned)lives;
            }

        sleep_ms(20);   /* ~30-60 fps including kernel paint time */
        }

    con_release();
    marker("BRK: DONE");
    return 0;
}

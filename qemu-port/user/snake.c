/*
 * snake.c -- SNAKE, a genuine interactive end-user game for the Sybian REAL
 * port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * A 16x8 board holds a snake that the player steers with w/a/s/d read from
 * PortCon through read_line().  Food spawns at positions taken from a fixed,
 * deterministic table (with a deterministic row-major fallback), the snake
 * grows when it eats and the score increments.  Running into a wall or into
 * your own body ends the game.  Commands: w/a/s/d, board, new, quit.  When no
 * input arrives before the read timeout a built-in auto-demo steers right,
 * eats the first food and drives into the wall so an automated test can assert
 * the full marker set (including GAMEOVER) without complex interaction.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*snake_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&snake_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

#define W 16
#define H 8
#define MAXLEN (W * H)

/* Snake body: index 0 is the head, index g_len-1 is the tail. */
static unsigned char g_x[MAXLEN], g_y[MAXLEN];
static int g_len;
static int g_dx, g_dy;
static int g_score;
static int g_food_index;
static int g_food_x, g_food_y;
static int g_dead;

/* Fixed, deterministic food layout.  food[0]=(6,4) sits two steps east of the
 * starting head so two 'd' commands eat it and raise the score to 1. */
static const unsigned char kFood[][2] = {
    {6, 4}, {9, 2}, {12, 6}, {2, 1},
    {8, 7}, {14, 3}, {5, 0}, {11, 5},
    {0, 6}, {7, 1}, {15, 7}, {3, 3}
};
#define NFOOD ((int)(sizeof(kFood) / sizeof(kFood[0])))

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker directly into a buffer, no hosted libc. */
static unsigned add_text(char* out, unsigned n, const char* s)
{
    while (*s) out[n++] = *s++;
    return n;
}

static unsigned add_uint(char* out, unsigned n, unsigned v)
{
    char d[12];
    int k = 0;
    if (!v) d[k++] = '0';
    while (v) { d[k++] = (char)('0' + v % 10); v /= 10; }
    while (k) out[n++] = d[--k];
    return n;
}

/* ------------------------------------------------------------ game engine */
static int cell_is_body(int x, int y, int ignore_tail)
{
    int i, last = g_len - (ignore_tail ? 1 : 0);
    for (i = 0; i < last; ++i)
        if (g_x[i] == x && g_y[i] == y) return 1;
    return 0;
}

static void spawn_food(void)
{
    int k, i;
    for (k = 0; k < NFOOD; ++k)
        {
        i = (g_food_index + k) % NFOOD;
        if (!cell_is_body(kFood[i][0], kFood[i][1], 0))
            {
            g_food_x = kFood[i][0];
            g_food_y = kFood[i][1];
            g_food_index = (i + 1) % NFOOD;
            return;
            }
        }
    /* Deterministic row-major fallback if every table slot is occupied. */
    for (i = 0; i < MAXLEN; ++i)
        {
        int x = i % W, y = i / W;
        if (!cell_is_body(x, y, 0))
            {
            g_food_x = x; g_food_y = y;
            return;
            }
        }
    g_food_x = -1; g_food_y = -1;   /* board full */
}

static void reset_game(void)
{
    g_len = 4;
    g_x[0] = 4; g_y[0] = 4;   /* head */
    g_x[1] = 3; g_y[1] = 4;
    g_x[2] = 2; g_y[2] = 4;
    g_x[3] = 1; g_y[3] = 4;   /* tail */
    g_dx = 1; g_dy = 0;       /* heading east */
    g_score = 0;
    g_dead = 0;
    g_food_index = 0;
    spawn_food();
}

static void emit_row(const char* row)
{
    char text[8 + W + 1];
    unsigned n = 0;
    n = add_text(text, n, "SNAKE: ");
    n = add_text(text, n, row);
    text[n] = 0;
    marker(text);
}

static void render_board(void)
{
    char row[W + 1];
    int y, x, i;
    for (y = 0; y < H; ++y)
        {
        for (x = 0; x < W; ++x)
            {
            char ch = '.';
            for (i = 0; i < g_len; ++i)
                if (g_x[i] == x && g_y[i] == y)
                    { ch = (i == 0) ? '@' : 'o'; break; }
            if (x == g_food_x && y == g_food_y) ch = '*';
            row[x] = ch;
            }
        row[W] = 0;
        emit_row(row);
        }
}

static void marker_move(char dir)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "SNAKE: move ");
    text[n++] = dir;
    n = add_text(text, n, " score=");
    n = add_uint(text, n, (unsigned)g_score);
    text[n] = 0;
    marker(text);
}

static void marker_gameover(void)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "SNAKE: GAMEOVER score=");
    n = add_uint(text, n, (unsigned)g_score);
    text[n] = 0;
    marker(text);
}

static void marker_dims(void)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "SNAKE: board ");
    n = add_uint(text, n, W);
    n = add_text(text, n, "x");
    n = add_uint(text, n, H);
    text[n] = 0;
    marker(text);
}

static void game_over(void)
{
    if (g_dead) return;
    g_dead = 1;
    render_board();
    marker_gameover();
}

/* Steer one step.  Returns nothing; a recognised move always emits a marker. */
static void move(char dir)
{
    int ndx, ndy, nx, ny, growing, i;
    if (g_dead) return;
    if      (dir == 'w' || dir == 'W') { ndx = 0;  ndy = -1; }
    else if (dir == 's' || dir == 'S') { ndx = 0;  ndy = 1;  }
    else if (dir == 'a' || dir == 'A') { ndx = -1; ndy = 0;  }
    else if (dir == 'd' || dir == 'D') { ndx = 1;  ndy = 0;  }
    else return;
    if (dir >= 'A' && dir <= 'Z') dir = (char)(dir - 'A' + 'a');

    /* No 180 degree turn into our own neck. */
    if (g_len > 1 && ndx == -g_dx && ndy == -g_dy) return;
    g_dx = ndx; g_dy = ndy;
    nx = g_x[0] + g_dx;
    ny = g_y[0] + g_dy;
    if (nx < 0 || nx >= W || ny < 0 || ny >= H) { game_over(); return; }

    growing = (nx == g_food_x && ny == g_food_y);
    if (cell_is_body(nx, ny, !growing)) { game_over(); return; }

    /* Grow by retaining the old tail, then shift every cell toward the head. */
    if (growing && g_len < MAXLEN)
        {
        g_x[g_len] = g_x[g_len - 1];
        g_y[g_len] = g_y[g_len - 1];
        ++g_len;
        }
    for (i = g_len - 1; i > 0; --i)
        { g_x[i] = g_x[i - 1]; g_y[i] = g_y[i - 1]; }
    g_x[0] = (unsigned char)nx;
    g_y[0] = (unsigned char)ny;
    if (growing)
        {
        ++g_score;
        spawn_food();
        }
    render_board();
    marker_move(dir);
}

/* ------------------------------------------------------------ command loop */
static int word_is(const char* word, unsigned length, const char* literal)
{
    unsigned i = 0;
    while (i < length && literal[i])
        {
        char a = word[i], b = literal[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == length && literal[i] == 0) ? 1 : 0;
}

/* Returns 1 when the user asked to quit or the game ended. */
static int run_command(const char* input)
{
    const char* word;
    unsigned wl;
    while (*input == ' ') ++input;
    word = input; wl = 0;
    while (word[wl] && word[wl] != ' ') ++wl;

    if (wl == 1)
        {
        char d = word[0];
        if (d >= 'A' && d <= 'Z') d = (char)(d - 'A' + 'a');
        if (d == 'w' || d == 'a' || d == 's' || d == 'd')
            { move(d); return g_dead ? 1 : 0; }
        }
    if (word_is(word, wl, "board")) { render_board(); return 0; }
    if (word_is(word, wl, "new"))
        { reset_game(); render_board(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q")) return 1;
    marker("SNAKE: commands: w a s d board new quit");
    return 0;
}

/* No input ever arrived: steer east to eat the first food and then into the
 * wall.  Uses the very same move engine, so the demo exercises growth, score
 * and collision detection. */
static void auto_demo(void)
{
    int i;
    for (i = 0; i < 40 && !g_dead; ++i) move('d');
}

static char line[128];

int app_main(void)
{
    int r, got_input = 0;

    if (con_acquire() < 0) { marker("SNAKE: console busy"); return 1; }

    reset_game();
    marker("SNAKE: READY");
    marker_dims();
    render_board();

    for (;;)
        {
        print("SNAKE> ");
        r = read_line(line, sizeof(line), 5000);
        if (r < 0)
            {
            if (!got_input) auto_demo();   /* no input: built-in demonstration */
            break;
            }
        got_input = 1;
        if (run_command(line)) break;      /* quit / game over */
        }

    marker("SNAKE: DONE");
    con_release();
    return 0;
}

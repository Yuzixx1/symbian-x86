/*
 * pacman.c -- PACMAN, a genuine interactive maze-muncher end-user game for
 * the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * An 11x9 maze holds dots ('.'), walls ('#') and the pac character ('@').
 * The player steers with w/a/s/d read from PortCon through read_line(); every
 * step onto a dot eats it (the cell becomes empty), adds 10 to the score and
 * decrements the dot count.  Commands: new, w/a/s/d, board, selftest, quit.
 * The built-in selftest runs three scripted east moves, eating three dots for
 * score=30, so an automated test can assert the full marker set.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*pacman_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&pacman_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

#define W 11
#define H 9

/* The maze is a closed outer wall with four internal blocks.  All 42 open
 * cells start as dots; the pac occupies (1,1) so 41 remain after reset. */
static const char* const kMaze[H] = {
    "###########",
    "#.........#",
    "#.###.###.#",
    "#.#.....#.#",
    "#.#.###.#.#",
    "#.#.....#.#",
    "#.#####.#.#",
    "#.........#",
    "###########"
};

static char g_board[H][W + 1];
static int g_px, g_py;
static int g_score;
static int g_dots_left;

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
static void reset_game(void)
{
    int x, y;
    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x)
            g_board[y][x] = kMaze[y][x];
    g_px = 1; g_py = 1;                 /* pac starts in the top-left corner */
    g_board[g_py][g_px] = '@';
    g_score = 0;
    g_dots_left = 0;
    for (y = 0; y < H; ++y)
        for (x = 0; x < W; ++x)
            if (g_board[y][x] == '.') ++g_dots_left;
}

static void emit_row(const char* row)
{
    char text[8 + W + 1];
    unsigned n = 0;
    n = add_text(text, n, "PC: ");
    n = add_text(text, n, row);
    text[n] = 0;
    marker(text);
}

static void marker_dots(void)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "PC: dots left=");
    n = add_uint(text, n, (unsigned)g_dots_left);
    text[n] = 0;
    marker(text);
}

static void render_board(void)
{
    int y;
    for (y = 0; y < H; ++y) emit_row(g_board[y]);
    marker_dots();
}

static void marker_ate(void)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "PC: ate dot score=");
    n = add_uint(text, n, (unsigned)g_score);
    text[n] = 0;
    marker(text);
}

/* Move one cell.  Walls block the move (the pac stays put); entering a dot
 * cell eats it.  Every accepted step re-renders the board. */
static void move(char dir)
{
    int ndx, ndy, nx, ny;
    if      (dir == 'w') { ndx = 0;  ndy = -1; }
    else if (dir == 's') { ndx = 0;  ndy = 1;  }
    else if (dir == 'a') { ndx = -1; ndy = 0;  }
    else if (dir == 'd') { ndx = 1;  ndy = 0;  }
    else return;
    nx = g_px + ndx;
    ny = g_py + ndy;
    if (nx < 0 || nx >= W || ny < 0 || ny >= H)
        { marker("PC: blocked"); return; }
    if (g_board[ny][nx] == '#')
        { marker("PC: blocked"); return; }

    g_board[g_py][g_px] = ' ';          /* leave an empty cell behind */
    if (g_board[ny][nx] == '.')
        {
        g_board[ny][nx] = ' ';
        g_score += 10;                  /* 10 points for each dot eaten */
        --g_dots_left;
        marker_ate();
        }
    g_px = nx; g_py = ny;
    g_board[ny][nx] = '@';
    render_board();
}

/* Scripted demonstration: reset, then three east moves that each eat a dot
 * (row 1 is open from column 1 to column 9), ending with score=30. */
static void selftest(void)
{
    marker("PC: SELFTEST");
    reset_game();
    render_board();
    move('d');
    move('d');
    move('d');
    marker((g_score == 30 && g_dots_left == 38)
           ? "PC: SELFTEST OK" : "PC: SELFTEST FAIL");
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

/* Returns 1 when the user asked to quit. */
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
            { move(d); return 0; }
        }
    if (word_is(word, wl, "new"))      { reset_game(); render_board(); return 0; }
    if (word_is(word, wl, "board"))    { render_board(); return 0; }
    if (word_is(word, wl, "selftest")) { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q")) return 1;
    marker("PC: commands: new w a s d board selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("PC: console busy"); return 1; }

    marker("PC: PACMAN 11x9");
    reset_game();
    render_board();

    for (;;)
        {
        print("PC> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;
        if (run_command(line)) break;      /* quit */
        }

    marker("PC: DONE");
    con_release();
    return 0;
}

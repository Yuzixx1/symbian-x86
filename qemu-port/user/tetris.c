/*
 * tetris.c -- TETRIS, a genuine interactive falling-block game for the
 * Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * A 10x12 board receives one deterministic 1x1 square piece at a time.  The
 * piece starts in the middle of the top row and is steered with "left" and
 * "right"; "drop" runs it straight down onto the stack (no rotation needed),
 * clears any full row and raises the score.  Commands are read from PortCon
 * through read_line(): new, left, right, drop, board, selftest, quit.  The
 * built-in selftest drops three predetermined pieces and reports score/lines.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*tetris_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&tetris_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

#define COLS 10
#define ROWS 12

/* '.' = empty cell, '#' = landed block. */
static char g_board[ROWS][COLS];
static int g_x;          /* active piece column */
static int g_y;          /* active piece row (set when it lands) */
static int g_score;
static int g_lines;
static int g_over;       /* 1 after a top-out */

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
    int r, c;
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c < COLS; ++c)
            g_board[r][c] = '.';
    g_x = 4;
    g_y = 0;
    g_score = 0;
    g_lines = 0;
    g_over = 0;
}

static void spawn_piece(void)
{
    g_x = COLS / 2;
    g_y = 0;
    if (g_board[0][g_x] != '.') g_over = 1;   /* no room at the top */
}

static void marker_row(const char* row)
{
    char line[8 + COLS + 1];
    unsigned n = 0;
    n = add_text(line, n, "TT2: ");
    n = add_text(line, n, row);
    line[n] = 0;
    marker(line);
}

static void marker_dims(void)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "TT2: TETRIS ");
    n = add_uint(text, n, COLS);
    n = add_text(text, n, "x");
    n = add_uint(text, n, ROWS);
    text[n] = 0;
    marker(text);
}

static void marker_landed(int x, int score)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "TT2: piece landed at ");
    n = add_uint(text, n, (unsigned)x);
    n = add_text(text, n, " score=");
    n = add_uint(text, n, (unsigned)score);
    text[n] = 0;
    marker(text);
}

static void marker_piece(int x)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "TT2: piece at ");
    n = add_uint(text, n, (unsigned)x);
    text[n] = 0;
    marker(text);
}

static void render_board(void)
{
    char row[COLS + 1];
    int r, c;
    for (r = 0; r < ROWS; ++r)
        {
        for (c = 0; c < COLS; ++c) row[c] = g_board[r][c];
        row[COLS] = 0;
        marker_row(row);
        }
}

/* Remove every full row; surviving rows drop to the bottom and new empty
 * rows appear on top.  Updates g_lines and returns the number cleared. */
static int clear_lines(void)
{
    char next[ROWS][COLS];
    int r, c, n = 0, dst = ROWS - 1;
    for (r = ROWS - 1; r >= 0; --r)
        {
        int full = 1;
        for (c = 0; c < COLS; ++c)
            if (g_board[r][c] == '.') { full = 0; break; }
        if (full) { ++n; continue; }
        for (c = 0; c < COLS; ++c) next[dst][c] = g_board[r][c];
        --dst;
        }
    for (; dst >= 0; --dst)
        for (c = 0; c < COLS; ++c) next[dst][c] = '.';
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c < COLS; ++c) g_board[r][c] = next[r][c];
    g_lines += n;
    return n;
}

/* Steer the active square one column.  It cannot leave the board or move
 * sideways into an occupied top cell. */
static void move_piece(int dx)
{
    int nx;
    if (g_over) return;
    nx = g_x + dx;
    if (nx < 0 || nx >= COLS) return;
    if (g_board[0][nx] != '.') return;
    g_x = nx;
    marker_piece(g_x);
}

/* Hard drop: run the square down its column until it rests on the stack. */
static void drop_piece(void)
{
    int y, cleared;
    if (g_over) { marker("TT2: TOPOUT"); return; }
    y = ROWS - 1;
    while (y >= 0 && g_board[y][g_x] != '.') --y;
    if (y < 0) { g_over = 1; marker("TT2: TOPOUT"); return; }
    g_y = y;
    g_board[g_y][g_x] = '#';
    cleared = clear_lines();
    g_score += 10 + cleared * 100;
    marker_landed(g_x, g_score);
    render_board();
    spawn_piece();
}

/* Drop three squares in fixed columns; report the final score and line count
 * using the very same drop engine as interactive play. */
static void selftest(void)
{
    marker("TT2: selftest");
    reset_game();
    g_x = 4; drop_piece();
    g_x = 3; drop_piece();
    g_x = 5; drop_piece();
    {
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "TT2: selftest score=");
    n = add_uint(text, n, (unsigned)g_score);
    n = add_text(text, n, " lines=");
    n = add_uint(text, n, (unsigned)g_lines);
    text[n] = 0;
    marker(text);
    }
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

    if (word_is(word, wl, "new"))
        { reset_game(); marker("TT2: TETRIS 10x12"); render_board(); return 0; }
    if (word_is(word, wl, "left"))
        { move_piece(-1); return 0; }
    if (word_is(word, wl, "right"))
        { move_piece(1); return 0; }
    if (word_is(word, wl, "drop"))
        { drop_piece(); return 0; }
    if (word_is(word, wl, "board"))
        { render_board(); return 0; }
    if (word_is(word, wl, "selftest"))
        { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q"))
        return 1;
    marker("TT2: commands: new left right drop board selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("TT2: console busy"); return 1; }

    reset_game();
    marker_dims();
    render_board();

    for (;;)
        {
        print("TT2> ");
        r = read_line(line, sizeof(line), 30000);
        if (r < 0) break;
        if (run_command(line)) break;          /* quit */
        }

    marker("TT2: DONE");
    con_release();
    return 0;
}

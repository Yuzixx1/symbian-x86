/*
 * connect4.c -- CONNECT-4, a genuine interactive end-user game for the
 * Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * A 6-row x 7-column board pits the human (X) against a simple CPU (O) that
 * always plays the first column with a free cell.  Commands are read from
 * PortCon through read_line(): new, drop <col> (0..6), board, selftest, quit.
 * The built-in selftest suspends the CPU and stacks four X's in column 0 for
 * a deterministic vertical WIN (a live CPU would occupy column 0 itself).
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*connect4_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&connect4_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

#define ROWS 6
#define COLS 7

/* Cell contents: '.' empty, 'X' human, 'O' CPU.  Row 0 is the top. */
static char g_board[ROWS][COLS];
static int g_over;          /* 1 once the game has ended */
static int g_result;        /* 1 = WIN, 2 = DRAW, 3 = LOSE */
static int g_no_cpu;        /* 1 during the scripted selftest */

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
    g_over = 0;
    g_result = 0;
    g_no_cpu = 0;
}

static void render_board(void)
{
    char line[16];
    int r, c;
    for (r = 0; r < ROWS; ++r)
        {
        unsigned n = 0;
        n = add_text(line, n, "C4: ");
        for (c = 0; c < COLS; ++c) line[n++] = g_board[r][c];
        line[n] = 0;
        marker(line);
        }
}

static void marker_drop(int col)
{
    char text[32];
    unsigned n = 0;
    n = add_text(text, n, "C4: drop ");
    n = add_uint(text, n, (unsigned)col);
    text[n] = 0;
    marker(text);
}

/* Return the lowest free row in a column, or -1 when the column is full. */
static int landing_row(int col)
{
    int r;
    if (col < 0 || col >= COLS) return -1;
    for (r = ROWS - 1; r >= 0; --r)
        if (g_board[r][col] == '.') return r;
    return -1;
}

static int has_line(char ch)
{
    int r, c, i, n;
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c <= COLS - 4; ++c)
            {
            n = 0;
            for (i = 0; i < 4; ++i)
                if (g_board[r][c + i] == ch) ++n;
            if (n == 4) return 1;
            }
    for (c = 0; c < COLS; ++c)
        for (r = 0; r <= ROWS - 4; ++r)
            {
            n = 0;
            for (i = 0; i < 4; ++i)
                if (g_board[r + i][c] == ch) ++n;
            if (n == 4) return 1;
            }
    /* down-right diagonals */
    for (r = 0; r <= ROWS - 4; ++r)
        for (c = 0; c <= COLS - 4; ++c)
            {
            n = 0;
            for (i = 0; i < 4; ++i)
                if (g_board[r + i][c + i] == ch) ++n;
            if (n == 4) return 1;
            }
    /* down-left diagonals */
    for (r = 0; r <= ROWS - 4; ++r)
        for (c = 3; c < COLS; ++c)
            {
            n = 0;
            for (i = 0; i < 4; ++i)
                if (g_board[r + i][c - i] == ch) ++n;
            if (n == 4) return 1;
            }
    return 0;
}

static int board_full(void)
{
    int r, c;
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c < COLS; ++c)
            if (g_board[r][c] == '.') return 0;
    return 1;
}

/* Emit the terminal marker at most once. */
static void check_end(void)
{
    if (g_over) return;
    if (has_line('X'))      { g_over = 1; g_result = 1; marker("C4: WIN"); }
    else if (has_line('O')) { g_over = 1; g_result = 3; marker("C4: LOSE"); }
    else if (board_full())  { g_over = 1; g_result = 2; marker("C4: DRAW"); }
}

/* CPU: first column (0..6) that still has a free cell. */
static void cpu_move(void)
{
    int col, row;
    for (col = 0; col < COLS; ++col)
        {
        row = landing_row(col);
        if (row < 0) continue;
        g_board[row][col] = 'O';
        marker_drop(col);
        render_board();
        check_end();
        return;
        }
}

static void human_drop(int col)
{
    int row;
    if (g_over) return;
    row = landing_row(col);
    if (row < 0)
        {
        marker(col < 0 || col >= COLS ? "C4: bad column" : "C4: column full");
        return;
        }
    g_board[row][col] = 'X';
    marker_drop(col);
    render_board();
    check_end();
    if (!g_over && !g_no_cpu)
        {
        cpu_move();
        if (!g_over) render_board();
        }
}

/* Scripted demonstration: suspend the CPU, stack four X's in column 0. */
static void selftest(void)
{
    marker("C4: SELFTEST");
    reset_game();
    g_no_cpu = 1;
    render_board();
    human_drop(0);
    human_drop(0);
    human_drop(0);
    human_drop(0);              /* vertical line -> WIN */
    g_no_cpu = 0;
    marker(g_result == 1 ? "C4: SELFTEST OK" : "C4: SELFTEST FAIL");
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

static int parse_int(const char* s, unsigned len, int* out)
{
    int value = 0, negative = 0, i;
    if (len && s[0] == '-') { negative = 1; ++s; --len; }
    if (!len) return 0;
    for (i = 0; i < (int)len; ++i)
        {
        if (s[i] < '0' || s[i] > '9') return 0;
        value = value * 10 + (s[i] - '0');
        }
    *out = negative ? -value : value;
    return 1;
}

/* Returns 1 when the user asked to quit or the game just ended. */
static int run_command(const char* input)
{
    const char *word, *arg;
    unsigned wl, al;
    int col;

    while (*input == ' ') ++input;
    word = input; wl = 0;
    while (word[wl] && word[wl] != ' ') ++wl;
    arg = word + wl;
    while (*arg == ' ') ++arg;
    al = 0;
    while (arg[al] && arg[al] != ' ') ++al;

    if (word_is(word, wl, "new"))
        { reset_game(); render_board(); return 0; }
    if (word_is(word, wl, "board"))
        { render_board(); return 0; }
    if (word_is(word, wl, "drop"))
        {
        if (!parse_int(arg, al, &col))
            { marker("C4: bad column"); return 0; }
        human_drop(col);
        return g_over;
        }
    if (word_is(word, wl, "selftest"))
        { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q"))
        return 1;
    marker("C4: commands: new drop col board selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("C4: console busy"); return 1; }

    reset_game();
    marker("C4: CONNECT-4 6x7");
    render_board();

    for (;;)
        {
        print("C4> ");
        r = read_line(line, sizeof(line), 30000);
        if (r < 0) break;
        if (run_command(line)) break;      /* quit / game over */
        }

    marker("C4: DONE");
    con_release();
    return 0;
}

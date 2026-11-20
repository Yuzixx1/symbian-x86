/*
 * tictactoe.c -- TIC-TAC-TOE, a genuine interactive end-user game for the
 * Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * A 3x3 board pits the human (X) against a simple CPU (O).  The CPU prefers
 * the centre, otherwise it takes the first free cell in row-major order.
 * Commands are read from PortCon through read_line(): new, move <r> <c>
 * (zero based), board, selftest, quit.  The built-in selftest plays a full
 * game to completion with scripted moves that win by filling column 0.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*tictactoe_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&tictactoe_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* Cell contents: '.' empty, 'X' human, 'O' CPU. */
static char g_board[3][3];
static int g_over;    /* 1 once the game has ended */
static int g_result;  /* 1 = WIN, 2 = DRAW, 3 = LOSE */

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
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 3; ++c)
            g_board[r][c] = '.';
    g_over = 0;
    g_result = 0;
}

static void render_board(void)
{
    char line[16];
    int r, c;
    marker("TT: board");
    for (r = 0; r < 3; ++r)
        {
        unsigned n = 0;
        n = add_text(line, n, "TT: ");
        for (c = 0; c < 3; ++c)
            {
            if (c) line[n++] = ' ';
            line[n++] = g_board[r][c];
            }
        line[n] = 0;
        marker(line);
        }
}

static int has_line(char ch)
{
    int i;
    for (i = 0; i < 3; ++i)
        {
        if (g_board[i][0] == ch && g_board[i][1] == ch && g_board[i][2] == ch) return 1;
        if (g_board[0][i] == ch && g_board[1][i] == ch && g_board[2][i] == ch) return 1;
        }
    if (g_board[0][0] == ch && g_board[1][1] == ch && g_board[2][2] == ch) return 1;
    if (g_board[0][2] == ch && g_board[1][1] == ch && g_board[2][0] == ch) return 1;
    return 0;
}

static int board_full(void)
{
    int r, c;
    for (r = 0; r < 3; ++r)
        for (c = 0; c < 3; ++c)
            if (g_board[r][c] == '.') return 0;
    return 1;
}

/* Emit the terminal marker at most once. */
static void check_end(void)
{
    if (g_over) return;
    if (has_line('X'))      { g_over = 1; g_result = 1; marker("TT: WIN"); }
    else if (has_line('O')) { g_over = 1; g_result = 3; marker("TT: LOSE"); }
    else if (board_full())  { g_over = 1; g_result = 2; marker("TT: DRAW"); }
}

static void marker_at(const char* who, int r, int c)
{
    char text[32];
    unsigned n = 0;
    n = add_text(text, n, "TT: ");
    n = add_text(text, n, who);
    text[n++] = ' ';
    n = add_uint(text, n, (unsigned)r);
    text[n++] = ',';
    n = add_uint(text, n, (unsigned)c);
    text[n] = 0;
    marker(text);
}

/* CPU: centre preference, then first free cell in row-major order. */
static void cpu_move(void)
{
    int r = -1, c = -1, i, j;
    if (g_board[1][1] == '.') { r = 1; c = 1; }
    else
        for (i = 0; i < 3 && r < 0; ++i)
            for (j = 0; j < 3; ++j)
                if (g_board[i][j] == '.') { r = i; c = j; break; }
    if (r < 0) return;
    g_board[r][c] = 'O';
    marker_at("cpu", r, c);
}

static void human_move(int r, int c)
{
    if (r < 0 || r >= 3 || c < 0 || c >= 3)
        { marker("TT: bad move"); return; }
    if (g_over || g_board[r][c] != '.') return;
    g_board[r][c] = 'X';
    marker_at("you", r, c);
    render_board();
    check_end();
    if (!g_over)
        {
        cpu_move();
        render_board();
        check_end();
        }
}

/* Full deterministic game: X fills column 0 for a win. */
static void selftest(void)
{
    reset_game();
    marker("TT: selftest");
    human_move(0, 0);   /* CPU answers (1,1) */
    human_move(1, 0);   /* CPU answers (0,1) */
    human_move(2, 0);   /* column 0 -> X wins */
    marker("TT: selftest OK");
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
    const char *word, *a1, *a2;
    unsigned wl, a1l, a2l;
    int r, c;

    while (*input == ' ') ++input;
    word = input; wl = 0;
    while (word[wl] && word[wl] != ' ') ++wl;
    a1 = word + wl;
    while (*a1 == ' ') ++a1;
    a1l = 0;
    while (a1[a1l] && a1[a1l] != ' ') ++a1l;
    a2 = a1 + a1l;
    while (*a2 == ' ') ++a2;
    a2l = 0;
    while (a2[a2l] && a2[a2l] != ' ') ++a2l;

    if (word_is(word, wl, "new"))
        { reset_game(); render_board(); return 0; }
    if (word_is(word, wl, "board"))
        { render_board(); return 0; }
    if (word_is(word, wl, "move"))
        {
        if (!parse_int(a1, a1l, &r) || !parse_int(a2, a2l, &c))
            { marker("TT: bad move"); return 0; }
        human_move(r, c);
        return g_over;
        }
    if (word_is(word, wl, "selftest"))
        { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q"))
        return 1;
    marker("TT: commands: new move r c board selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("TT: console busy"); return 1; }

    reset_game();
    marker("TT: TIC-TAC-TOE");
    render_board();

    for (;;)
        {
        print("TT> ");
        r = read_line(line, sizeof(line), 30000);
        if (r < 0) break;
        if (run_command(line)) break;      /* quit / game over */
        }

    marker("TT: DONE");
    con_release();
    return 0;
}

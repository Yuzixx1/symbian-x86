/*
 * sudoku.c -- SUDOKU, a genuine 9x9 sudoku game/validator for the Sybian REAL
 * port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader patches the slot before the process resumes.
 * All serial markers go through that ordinal, with a DebugPrint fallback so
 * the trace is always visible.
 *
 * A fixed puzzle (the classic 9x9 grid) is preloaded with 0 used for blank
 * cells.  Commands are read from PortCon through read_line(): board,
 * set <r> <c> <1-9>, check, solve, selftest, quit (1-based row/column).
 * Placements that are out of range, overwrite a prefilled cell, or duplicate
 * a digit in the row/column/3x3 box are rejected.  `check` scans rows, columns
 * and boxes for duplicates; `solve` displays a built-in completed grid.  The
 * built-in selftest validates the prefilled puzzle, places one missing digit,
 * re-checks and solves, so an automated test can assert every marker.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*sudoku_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&sudoku_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ grids
 * The classic published puzzle and its unique solution.  Zero = empty. */
static const unsigned char kPuzzle[9][9] = {
    {5,3,0,0,7,0,0,0,0},
    {6,0,0,1,9,5,0,0,0},
    {0,9,8,0,0,0,0,6,0},
    {8,0,0,0,6,0,0,0,3},
    {4,0,0,8,0,3,0,0,1},
    {7,0,0,0,2,0,0,0,6},
    {0,6,0,0,0,0,2,8,0},
    {0,0,0,4,1,9,0,0,5},
    {0,0,0,0,8,0,0,7,9}
};

static const unsigned char kSolution[9][9] = {
    {5,3,4,6,7,8,9,1,2},
    {6,7,2,1,9,5,3,4,8},
    {1,9,8,3,4,2,5,6,7},
    {8,5,9,7,6,1,4,2,3},
    {4,2,6,8,5,3,7,9,1},
    {7,1,3,9,2,4,8,5,6},
    {9,6,1,5,3,7,2,8,4},
    {2,8,7,4,1,9,6,3,5},
    {3,4,5,2,8,6,1,7,9}
};

static unsigned char g_board[9][9];

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

/* ------------------------------------------------------------ board output */
static void render_board(void)
{
    char text[16];
    unsigned r, c, n;
    for (r = 0; r < 9; ++r)
        {
        n = add_text(text, 0, "SU: ");
        for (c = 0; c < 9; ++c)
            {
            unsigned char d = g_board[r][c];
            text[n++] = d ? (char)('0' + d) : '.';
            }
        text[n] = 0;
        marker(text);
        }
}

static void emit_set(unsigned r, unsigned c, unsigned d)
{
    char text[40];
    unsigned n = add_text(text, 0, "SU: set ");
    n = add_uint(text, n, r);
    text[n++] = ',';
    n = add_uint(text, n, c);
    text[n++] = '=';
    n = add_uint(text, n, d);
    text[n] = 0;
    marker(text);
}

static void emit_reject(const char* why)
{
    char text[48];
    unsigned n = add_text(text, 0, "SU: set reject ");
    n = add_text(text, n, why);
    text[n] = 0;
    marker(text);
}

/* ------------------------------------------------------------ game engine */
static void reset_board(void)
{
    unsigned r, c;
    for (r = 0; r < 9; ++r)
        for (c = 0; c < 9; ++c)
            g_board[r][c] = kPuzzle[r][c];
}

/* Would digit d duplicate an already-filled cell in row r, column c or the
 * containing 3x3 box? */
static int conflicts(unsigned r, unsigned c, unsigned d)
{
    unsigned i, j, br = (r / 3) * 3, bc = (c / 3) * 3;
    for (i = 0; i < 9; ++i)
        if (i != c && g_board[r][i] == d) return 1;
    for (i = 0; i < 9; ++i)
        if (i != r && g_board[i][c] == d) return 1;
    for (i = br; i < br + 3; ++i)
        for (j = bc; j < bc + 3; ++j)
            if ((i != r || j != c) && g_board[i][j] == d) return 1;
    return 0;
}

/* 1 = placed, 0 = rejected (a reject marker explains why). */
static int do_set(unsigned r, unsigned c, unsigned d)
{
    if (r < 1 || r > 9 || c < 1 || c > 9 || d < 1 || d > 9)
        { emit_reject("range"); return 0; }
    --r; --c;
    if (kPuzzle[r][c] != 0)
        { emit_reject("fixed"); return 0; }
    if (g_board[r][c] != 0)
        { emit_reject("occupied"); return 0; }
    if (conflicts(r, c, d))
        { emit_reject("conflict"); return 0; }
    g_board[r][c] = (unsigned char)d;
    emit_set(r + 1, c + 1, d);
    return 1;
}

/* Validate rows, columns and 3x3 boxes for duplicate nonzero digits. */
static int check_board(void)
{
    unsigned i, j, br, bc, d;
    unsigned char seen[10];

    for (i = 0; i < 9; ++i)
        {
        for (d = 0; d < 10; ++d) seen[d] = 0;
        for (j = 0; j < 9; ++j)
            {
            d = g_board[i][j];
            if (!d) continue;
            if (seen[d]) return 0;
            seen[d] = 1;
            }
        }
    for (j = 0; j < 9; ++j)
        {
        for (d = 0; d < 10; ++d) seen[d] = 0;
        for (i = 0; i < 9; ++i)
            {
            d = g_board[i][j];
            if (!d) continue;
            if (seen[d]) return 0;
            seen[d] = 1;
            }
        }
    for (br = 0; br < 9; br += 3)
        for (bc = 0; bc < 9; bc += 3)
            {
            for (d = 0; d < 10; ++d) seen[d] = 0;
            for (i = br; i < br + 3; ++i)
                for (j = bc; j < bc + 3; ++j)
                    {
                    d = g_board[i][j];
                    if (!d) continue;
                    if (seen[d]) return 0;
                    seen[d] = 1;
                    }
            }
    return 1;
}

static void emit_check(int ok)
{
    marker(ok ? "SU: check OK" : "SU: check conflict");
}

static void do_check(void)
{
    emit_check(check_board());
}

static void do_solve(void)
{
    unsigned r, c;
    for (r = 0; r < 9; ++r)
        for (c = 0; c < 9; ++c)
            g_board[r][c] = kSolution[r][c];
    render_board();
    marker("SU: solved");
}

/* ------------------------------------------------------------ selftest */
static void selftest(void)
{
    int ok = 1;
    marker("SU: SELFTEST begin");
    reset_board();
    if (!check_board()) ok = 0;             /* prefilled puzzle: no conflicts */
    emit_check(check_board());
    if (!do_set(1, 3, 4)) ok = 0;           /* blank r1c3 is 4 in the solution */
    if (!check_board()) ok = 0;
    emit_check(check_board());
    do_solve();                             /* shows the completed grid */
    if (g_board[0][2] != 4) ok = 0;
    marker(ok ? "SU: SELFTEST OK" : "SU: SELFTEST FAIL");
}

/* ------------------------------------------------------------ command loop */
static const char* next_token(const char* p, unsigned* len)
{
    const char* s;
    while (*p == ' ' || *p == '\t' || *p == '\r') ++p;
    s = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r') ++p;
    *len = (unsigned)(p - s);
    return s;
}

static int token_is(const char* tok, unsigned length, const char* literal)
{
    unsigned i = 0;
    while (i < length && literal[i])
        {
        char a = tok[i], b = literal[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == length && literal[i] == 0) ? 1 : 0;
}

static int parse_uint(const char* tok, unsigned length, unsigned* out)
{
    unsigned v = 0, i;
    if (!length) return 0;
    for (i = 0; i < length; ++i)
        {
        if (tok[i] < '0' || tok[i] > '9') return 0;
        v = v * 10u + (unsigned)(tok[i] - '0');
        if (v > 100000u) return 0;
        }
    *out = v;
    return 1;
}

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    unsigned al, bl, cl, dl;
    unsigned r, c, d;
    const char* cmd = next_token(input, &al);
    const char* a;
    const char* b;
    const char* cc;

    if (token_is(cmd, al, "quit") || token_is(cmd, al, "q")) return 1;
    if (token_is(cmd, al, "board")) { render_board(); return 0; }
    if (token_is(cmd, al, "check")) { do_check(); return 0; }
    if (token_is(cmd, al, "solve")) { do_solve(); return 0; }
    if (token_is(cmd, al, "selftest")) { selftest(); return 0; }
    if (token_is(cmd, al, "set"))
        {
        a = next_token(cmd + al, &bl);
        b = next_token(a + bl, &cl);
        cc = next_token(b + cl, &dl);
        if (!parse_uint(a, bl, &r) || !parse_uint(b, cl, &c)
            || !parse_uint(cc, dl, &d))
            emit_reject("args");
        else
            do_set(r, c, d);
        return 0;
        }
    marker("SU: commands: board set <r> <c> <1-9> check solve selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("SU: console busy"); return 1; }

    marker("SU: SUDOKU");
    reset_board();
    render_board();

    for (;;)
        {
        print("SU> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("SU: DONE");
    con_release();
    return 0;
}

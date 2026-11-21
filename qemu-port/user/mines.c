/*
 * mines.c -- MINESWEEPER, a genuine interactive end-user game for the
 * Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * An 8x8 field holds 10 mines in a fixed, deterministic layout.  Cells are
 * hidden / revealed / flagged; revealed safe cells show their neighbour mine
 * count.  Opening a cell with no neighbouring mines flood-reveals its safe
 * connected region, exactly as in a real Minesweeper.  Commands are read from
 * PortCon through read_line(): new, open <row> <col>, flag <row> <col>, board,
 * quit.  Rows/columns are zero based.  If no input arrives before the read
 * timeout, a built-in auto-demo reveals two predetermined safe cells so an
 * automated test can assert the markers without complex interaction.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*mines_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&mines_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

#define ROWS 8
#define COLS 8
#define NMINES 10
#define NSAFE (ROWS * COLS - NMINES)

/* Cell state: 0 = hidden, 1 = revealed, 2 = flagged. */
static unsigned char g_mine[ROWS][COLS];
static unsigned char g_count[ROWS][COLS];
static unsigned char g_state[ROWS][COLS];
static int g_opened_safe;
static int g_dead;
static int g_won;

/* Fixed deterministic mine layout.  (0,0) and the auto-demo cells (7,7) and
 * (0,7) are deliberately safe so scripted openings never hit a mine. */
static const unsigned char kMines[NMINES][2] = {
    {1, 1}, {1, 6}, {2, 3}, {2, 7}, {3, 5},
    {4, 0}, {4, 7}, {5, 2}, {6, 4}, {7, 6}
};

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
static unsigned count_neighbors(int r, int c)
{
    unsigned n = 0;
    int dr, dc;
    for (dr = -1; dr <= 1; ++dr)
        for (dc = -1; dc <= 1; ++dc)
            {
            int rr, cc;
            if (!dr && !dc) continue;
            rr = r + dr; cc = c + dc;
            if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS) continue;
            if (g_mine[rr][cc]) ++n;
            }
    return n;
}

static void reset_game(void)
{
    int r, c, i;
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c < COLS; ++c)
            { g_mine[r][c] = 0; g_state[r][c] = 0; g_count[r][c] = 0; }
    for (i = 0; i < NMINES; ++i)
        g_mine[kMines[i][0]][kMines[i][1]] = 1;
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c < COLS; ++c)
            g_count[r][c] = (unsigned char)count_neighbors(r, c);
    g_opened_safe = 0;
    g_dead = 0;
    g_won = 0;
}

static void render_board(void)
{
    char line[16];
    int r, c;
    for (r = 0; r < ROWS; ++r)
        {
        unsigned n = 0;
        n = add_text(line, n, "MS: ");
        for (c = 0; c < COLS; ++c)
            {
            unsigned char st = g_state[r][c];
            char ch;
            if      (st == 2) ch = 'F';                     /* flagged */
            else if (st == 0) ch = '.';                     /* hidden */
            else if (g_mine[r][c]) ch = '*';                /* detonated */
            else ch = (char)('0' + g_count[r][c]);          /* safe, revealed */
            line[n++] = ch;
            }
        line[n] = 0;
        marker(line);
        }
}

static void marker_opened(int r, int c, unsigned neighbors)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "MS: opened r=");
    n = add_uint(text, n, (unsigned)r);
    n = add_text(text, n, " c=");
    n = add_uint(text, n, (unsigned)c);
    n = add_text(text, n, " neighbors=");
    n = add_uint(text, n, neighbors);
    text[n] = 0;
    marker(text);
}

static void marker_flag(int r, int c, int on)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "MS: flag r=");
    n = add_uint(text, n, (unsigned)r);
    n = add_text(text, n, " c=");
    n = add_uint(text, n, (unsigned)c);
    n = add_text(text, n, on ? " on" : " off");
    text[n] = 0;
    marker(text);
}

static void marker_dims(void)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "MS: board ");
    n = add_uint(text, n, ROWS);
    n = add_text(text, n, "x");
    n = add_uint(text, n, COLS);
    n = add_text(text, n, " mines=");
    n = add_uint(text, n, NMINES);
    text[n] = 0;
    marker(text);
}

/* Flood reveal: reveal a safe hidden cell; when it has no neighbouring mines,
 * expand into every hidden safe neighbour.  Mines are never auto-revealed. */
static void reveal(int r, int c)
{
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return;
    if (g_state[r][c] != 0) return;
    if (g_mine[r][c]) return;
    g_state[r][c] = 1;
    ++g_opened_safe;
    if (g_count[r][c] == 0)
        {
        int dr, dc;
        for (dr = -1; dr <= 1; ++dr)
            for (dc = -1; dc <= 1; ++dc)
                if (dr || dc) reveal(r + dr, c + dc);
        }
}

static void check_win(void)
{
    if (!g_dead && !g_won && g_opened_safe == NSAFE)
        { g_won = 1; marker("MS: WIN"); }
}

static void open_cell(int r, int c)
{
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS)
        { marker("MS: bad coords"); return; }
    if (g_dead || g_won) return;
    if (g_state[r][c] == 1 || g_state[r][c] == 2) return;
    if (g_mine[r][c])
        {
        g_state[r][c] = 1;
        g_dead = 1;
        render_board();
        marker("MS: BOOM");
        return;
        }
    {
    unsigned neighbors = g_count[r][c];
    reveal(r, c);
    render_board();
    marker_opened(r, c, neighbors);
    check_win();
    }
}

static void flag_cell(int r, int c)
{
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS)
        { marker("MS: bad coords"); return; }
    if (g_dead || g_won || g_state[r][c] == 1) return;
    if (g_state[r][c] == 2) { g_state[r][c] = 0; marker_flag(r, c, 0); }
    else                    { g_state[r][c] = 2; marker_flag(r, c, 1); }
    render_board();
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

/* Returns 1 when the user asked to quit or the game ended. */
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
        { reset_game(); marker("MS: NEW GAME"); render_board(); return 0; }
    if (word_is(word, wl, "board"))
        { render_board(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q"))
        return 1;
    if (word_is(word, wl, "open") || word_is(word, wl, "flag"))
        {
        if (!parse_int(a1, a1l, &r) || !parse_int(a2, a2l, &c))
            { marker("MS: bad coords"); return 0; }
        if (word_is(word, wl, "open")) open_cell(r, c);
        else flag_cell(r, c);
        return (g_dead || g_won) ? 1 : 0;
        }
    marker("MS: commands: new board open r c flag r c quit");
    return 0;
}

/* Reveal two predetermined safe cells when no user input ever arrived. */
static void auto_demo(void)
{
    if (!g_dead) open_cell(7, 7);
    if (!g_dead) open_cell(0, 7);
}

static char line[128];

int app_main(void)
{
    int r, got_input = 0;

    if (con_acquire() < 0) { marker("MS: console busy"); return 1; }

    reset_game();
    marker("MS: MINESWEEPER READY");
    marker_dims();
    render_board();

    for (;;)
        {
        con_puts_raw("MS> ");
        r = read_line(line, sizeof(line), 30000);
        if (r < 0)
            {
            if (!got_input) auto_demo();   /* no input: built-in demonstration */
            break;
            }
        got_input = 1;
        if (run_command(line)) break;      /* quit / game over */
        }

    marker("MS: DONE");
    con_release();
    return 0;
}

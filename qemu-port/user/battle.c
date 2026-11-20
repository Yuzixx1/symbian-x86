/*
 * battle.c -- BATTLESHIP, a genuine interactive end-user game for the
 * Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * An 8x8 grid holds three length-3 ships at fixed positions.  The player fires
 * at the enemy fleet with `fire <row> <col>` (zero based); the CPU returns fire
 * at a deterministic cell after every shot.  A ship sinks when all three of its
 * cells are hit; sink all three to WIN, or let the CPU sink yours to LOSE.
 * Commands are read from PortCon through read_line(): new, fire r c, board,
 * selftest, quit.  The built-in selftest fires at the nine cells of the three
 * known ships, sinking the whole fleet and printing WIN.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*battle_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&battle_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

#define ROWS 8
#define COLS 8
#define NSHIPS 3
#define SHIPLEN 3
#define NCELLS (ROWS * COLS)

/* Fixed fleet: two horizontal ships and one vertical, no overlapping cells. */
static const unsigned char kShips[NSHIPS][SHIPLEN][2] = {
    { {1, 1}, {1, 2}, {1, 3} },   /* ship 0, row 1 */
    { {4, 4}, {4, 5}, {4, 6} },   /* ship 1, row 4 */
    { {5, 2}, {6, 2}, {7, 2} }    /* ship 2, column 2 */
};

/* Player's view of the enemy grid: 0 = unfired, 1 = hit, 2 = miss. */
static unsigned char g_shots[ROWS][COLS];
static unsigned char g_ship_hits[NSHIPS];
/* CPU's fire at our (identically laid out) fleet. */
static unsigned char g_cpu_damage[ROWS][COLS];
static unsigned char g_cpu_ship_hits[NSHIPS];
/* Deterministic return-fire order: all 55 water cells row-major, then the
 * 9 ship cells row-major, so the CPU cannot win before the scripted selftest. */
static unsigned char g_cpu_order[NCELLS][2];
static int g_sunk;
static int g_cpu_sunk;
static int g_cpu_idx;
static int g_over;                 /* 0 = playing, 1 = WIN, 2 = LOSE */

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
static int ship_at(int r, int c)
{
    int s, k;
    for (s = 0; s < NSHIPS; ++s)
        for (k = 0; k < SHIPLEN; ++k)
            if (kShips[s][k][0] == r && kShips[s][k][1] == c) return s;
    return -1;
}

static void build_cpu_order(void)
{
    int r, c, pass, i = 0;
    for (pass = 0; pass < 2; ++pass)
        for (r = 0; r < ROWS; ++r)
            for (c = 0; c < COLS; ++c)
                if ((ship_at(r, c) >= 0) == (pass == 1))
                    {
                    g_cpu_order[i][0] = (unsigned char)r;
                    g_cpu_order[i][1] = (unsigned char)c;
                    ++i;
                    }
}

static void reset_game(void)
{
    int r, c, s;
    for (r = 0; r < ROWS; ++r)
        for (c = 0; c < COLS; ++c)
            { g_shots[r][c] = 0; g_cpu_damage[r][c] = 0; }
    for (s = 0; s < NSHIPS; ++s)
        { g_ship_hits[s] = 0; g_cpu_ship_hits[s] = 0; }
    g_sunk = 0;
    g_cpu_sunk = 0;
    g_cpu_idx = 0;
    g_over = 0;
    build_cpu_order();
}

/* Render the eight rows of the player's firing grid.
 * '.' = unseen, 'X' = hit, 'o' = miss. */
static void render_board(void)
{
    char line[ROWS + 8];
    int r, c;
    for (r = 0; r < ROWS; ++r)
        {
        unsigned n = 0;
        n = add_text(line, n, "BT: ");
        for (c = 0; c < COLS; ++c)
            {
            unsigned char st = g_shots[r][c];
            line[n++] = (st == 1) ? 'X' : (st == 2) ? 'o' : '.';
            }
        line[n] = 0;
        marker(line);
        }
}

static void marker_dims(void)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "BT: board ");
    n = add_uint(text, n, ROWS);
    n = add_text(text, n, "x");
    n = add_uint(text, n, COLS);
    n = add_text(text, n, " ships=");
    n = add_uint(text, n, NSHIPS);
    text[n] = 0;
    marker(text);
}

static void marker_at(const char* who, int r, int c, const char* result)
{
    char text[48];
    unsigned n = 0;
    n = add_text(text, n, "BT: ");
    n = add_text(text, n, who);
    n = add_text(text, n, " ");
    n = add_uint(text, n, (unsigned)r);
    n = add_text(text, n, ",");
    n = add_uint(text, n, (unsigned)c);
    n = add_text(text, n, " ");
    n = add_text(text, n, result);
    text[n] = 0;
    marker(text);
}

static void marker_sunk(const char* who, int count)
{
    char text[48];
    unsigned n = 0;
    n = add_text(text, n, "BT: ");
    if (who[0]) { n = add_text(text, n, who); n = add_text(text, n, " "); }
    n = add_text(text, n, "sunk ");
    n = add_uint(text, n, (unsigned)count);
    n = add_text(text, n, "/");
    n = add_uint(text, n, NSHIPS);
    text[n] = 0;
    marker(text);
}

/* The CPU answers at the next deterministic cell. */
static void cpu_fire(void)
{
    int i, r, c, s;
    if (g_over || g_cpu_idx >= NCELLS) return;
    i = g_cpu_idx++;
    r = g_cpu_order[i][0];
    c = g_cpu_order[i][1];
    s = ship_at(r, c);
    g_cpu_damage[r][c] = 1;
    marker_at("cpu", r, c, s >= 0 ? "hit" : "miss");
    if (s >= 0)
        {
        if (++g_cpu_ship_hits[s] == SHIPLEN)
            marker_sunk("cpu", ++g_cpu_sunk);
        if (g_cpu_sunk == NSHIPS)
            { g_over = 2; marker("BT: LOSE"); }
        }
}

static void fire_at(int r, int c)
{
    int s;
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS)
        { marker("BT: bad coords"); return; }
    if (g_over) return;
    if (g_shots[r][c])
        { marker_at("already", r, c, "fired"); return; }

    s = ship_at(r, c);
    g_shots[r][c] = (unsigned char)(s >= 0 ? 1 : 2);
    marker_at("fire", r, c, s >= 0 ? "hit" : "miss");
    if (s >= 0)
        {
        if (++g_ship_hits[s] == SHIPLEN)
            marker_sunk("", ++g_sunk);
        if (g_sunk == NSHIPS)
            { g_over = 1; marker("BT: WIN"); return; }
        }
    cpu_fire();
}

/* Fire at all nine known ship cells: every ship sinks and the game is won. */
static void selftest(void)
{
    int s, k;
    marker("BT: SELFTEST begin");
    reset_game();
    for (s = 0; s < NSHIPS; ++s)
        for (k = 0; k < SHIPLEN; ++k)
            fire_at(kShips[s][k][0], kShips[s][k][1]);
    marker(g_over == 1 ? "BT: SELFTEST OK" : "BT: SELFTEST FAIL");
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
        { reset_game(); marker("BT: NEW GAME"); render_board(); return 0; }
    if (word_is(word, wl, "board"))
        { render_board(); return 0; }
    if (word_is(word, wl, "fire"))
        {
        if (!parse_int(a1, a1l, &r) || !parse_int(a2, a2l, &c))
            { marker("BT: bad coords"); return 0; }
        fire_at(r, c);
        return g_over;
        }
    if (word_is(word, wl, "selftest"))
        { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q"))
        return 1;
    marker("BT: commands: new fire r c board selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("BT: console busy"); return 1; }

    reset_game();
    marker("BT: BATTLESHIP");
    marker_dims();
    render_board();

    for (;;)
        {
        print("BT> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;
        if (run_command(line)) break;      /* quit / game over */
        }

    marker("BT: DONE");
    con_release();
    return 0;
}

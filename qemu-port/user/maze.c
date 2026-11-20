/*
 * maze.c -- MAZE, a deterministic 9x9 maze generator/solver for the Sybian
 * REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * The maze is a 9x9 character grid: odd coordinates are cells, even
 * coordinates are walls, and the outer ring is permanent wall.  A fixed-seed
 * recursive-backtracker DFS carves corridors between cells, producing a
 * perfect (fully connected, loop-free) maze.  `gen` regenerates, `show`
 * renders the raw grid (# walls, . paths), `solve` runs a breadth-first
 * search from the top-left cell (1,1) to the bottom-right cell (7,7), renders
 * the path with '*', and prints its length.  Commands are read from PortCon
 * through read_line(): gen / show / solve / help / selftest / quit.  The
 * built-in selftest regenerates the deterministic maze, shows it, solves it
 * and reports the path length so an automated test can assert every marker.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*maze_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&maze_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ maze state */
#define N 9
#define WALL 1
#define PATH 0
#define BASE_SEED 0x12345678u

static unsigned char g_maze[N][N];    /* WALL or PATH */
static unsigned char g_mark[N][N];    /* 1 = on the solution path */
static unsigned g_state;              /* LCG state for the current generation */
static unsigned g_seed;               /* seed used by the next generate() */

/* 15-bit pseudo-random from a 32-bit LCG (identical on every run). */
static unsigned rnd(void)
{
    g_state = g_state * 1103515245u + 12345u;
    return (g_state >> 16) & 0x7fffu;
}

/* Cell-to-cell steps (2 grid units); cells live at odd coordinates. */
static const int kCY[4] = { -2, 0, 2, 0 };
static const int kCX[4] = {  0, 2, 0, -2 };

/* Recursive-backtracker carve.  At each cell a random direction chooses the
 * scan order; every unvisited neighbour is carved and recursed into. */
static void carve(int y, int x)
{
    unsigned r = rnd();
    int d0 = (int)(r % 4u);
    int k;
    for (k = 0; k < 4; ++k)
        {
        int d = (d0 + k) % 4;
        int ny = y + kCY[d], nx = x + kCX[d];
        if (ny < 1 || ny > N - 2 || nx < 1 || nx > N - 2) continue;
        if (g_maze[ny][nx] != WALL) continue;
        g_maze[(y + ny) / 2][(x + nx) / 2] = PATH; /* knocked-down wall */
        g_maze[ny][nx] = PATH;
        carve(ny, nx);
        }
}

/* Fill the grid with walls, carve from (1,1), advance the seed so a later
 * `gen` produces a different (but still deterministic) maze. */
static void generate(void)
{
    int y, x;
    for (y = 0; y < N; ++y)
        for (x = 0; x < N; ++x)
            g_maze[y][x] = WALL;
    g_state = g_seed;
    g_maze[1][1] = PATH;
    carve(1, 1);
    g_maze[N - 2][N - 2] = PATH; /* goal cell (already carved) */
    g_seed = g_seed + 0x9e3779b9u;
}

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

/* Render all nine rows.  With marked=1 the solved path is drawn as '*'. */
static void render(int marked)
{
    char text[16];
    int r, c, n;
    for (r = 0; r < N; ++r)
        {
        n = add_text(text, 0, "MZ: ");
        for (c = 0; c < N; ++c)
            {
            if (g_maze[r][c] == WALL) text[n++] = '#';
            else if (marked && g_mark[r][c]) text[n++] = '*';
            else text[n++] = '.';
            }
        text[n] = 0;
        marker(text);
        }
}

static void emit_length(unsigned len)
{
    char text[40];
    unsigned n = add_text(text, 0, "MZ: path length=");
    n = add_uint(text, n, len);
    text[n] = 0;
    marker(text);
}

/* ------------------------------------------------------------ solver
 * Breadth-first search from (1,1) to (7,7) over PATH cells; parents are
 * recorded so the unique perfect-maze route can be marked on unwinding.
 * Returns the number of cells on the path (0 when unreachable). */
static const int kNY[4] = { -1, 1, 0, 0 };
static const int kNX[4] = {  0, 0, 1, -1 };

static unsigned char g_visit[N][N];
static int g_py[N][N], g_px[N][N];

static unsigned solve(void)
{
    int q[N * N], head = 0, tail = 0;
    int y, x, cy, cx, d;
    unsigned len = 0;

    for (y = 0; y < N; ++y)
        for (x = 0; x < N; ++x)
            {
            g_visit[y][x] = 0;
            g_py[y][x] = -1;
            g_px[y][x] = -1;
            g_mark[y][x] = 0;
            }
    q[tail++] = 1 * N + 1;               /* start cell (1,1) */
    g_visit[1][1] = 1;

    while (head < tail)
        {
        int pos = q[head++];
        cy = pos / N;
        cx = pos % N;
        if (cy == N - 2 && cx == N - 2) break;
        for (d = 0; d < 4; ++d)
            {
            int ny = cy + kNY[d], nx = cx + kNX[d];
            if (ny < 0 || ny >= N || nx < 0 || nx >= N) continue;
            if (g_maze[ny][nx] != PATH || g_visit[ny][nx]) continue;
            g_visit[ny][nx] = 1;
            g_py[ny][nx] = cy;
            g_px[ny][nx] = cx;
            q[tail++] = ny * N + nx;
            }
        }

    if (!g_visit[N - 2][N - 2]) return 0;
    cy = N - 2;
    cx = N - 2;
    for (;;)
        {
        int py, px;
        g_mark[cy][cx] = 1;
        ++len;
        py = g_py[cy][cx];
        px = g_px[cy][cx];
        if (py < 0) break;
        cy = py;
        cx = px;
        }
    return len;
}

/* ------------------------------------------------------------ commands */
static void do_gen(void)    { generate(); render(0); }
static void do_show(void)   { render(0); }
static void do_solve(void)
{
    unsigned len = solve();
    render(1);
    emit_length(len);
}

/* ------------------------------------------------------------ selftest */
static void selftest(void)
{
    unsigned len;
    marker("MZ: SELFTEST begin");
    g_seed = BASE_SEED;                 /* deterministic maze for the test */
    generate();
    render(0);
    len = solve();
    render(1);
    emit_length(len);
    marker(len >= 13u ? "MZ: SELFTEST OK" : "MZ: SELFTEST FAIL");
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

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    unsigned al;
    const char* cmd = next_token(input, &al);

    if (token_is(cmd, al, "quit") || token_is(cmd, al, "q")) return 1;
    if (token_is(cmd, al, "gen"))      { do_gen(); return 0; }
    if (token_is(cmd, al, "show"))     { do_show(); return 0; }
    if (token_is(cmd, al, "solve"))    { do_solve(); return 0; }
    if (token_is(cmd, al, "selftest")) { selftest(); return 0; }
    if (token_is(cmd, al, "help") || token_is(cmd, al, "h"))
        {
        marker("MZ: commands: gen show solve help selftest quit");
        return 0;
        }
    marker("MZ: commands: gen show solve help selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("MZ: console busy"); return 1; }

    marker("MZ: MAZE GENERATOR 9x9");
    g_seed = BASE_SEED;
    generate();
    render(0);

    for (;;)
        {
        print("MZ> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("MZ: DONE");
    con_release();
    return 0;
}

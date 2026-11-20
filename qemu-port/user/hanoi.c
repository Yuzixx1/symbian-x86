/*
 * hanoi.c -- HANOI, a genuine Tower of Hanoi puzzle for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * The puzzle uses up to four disks (1 = smallest, 4 = largest) and three pegs
 * (A/B/C).  Every solve is produced by a real recursive algorithm; each move
 * is applied to an in-memory peg state (with the disk-number ordering rule
 * enforced) and the first few moves are printed as serial markers.  Commands
 * are read from PortCon through read_line(): solve [n], board, help, selftest,
 * quit.  The built-in selftest solves the 3-disk puzzle (7 moves) and renders
 * the final peg state (all disks on C).
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*hanoi_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&hanoi_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ peg state
 * Peg indices: A=0, B=1, C=2.  Each peg stores disks bottom..top. */
#define MAX_DISKS 4
#define NPEG 3
static unsigned char g_peg[NPEG][MAX_DISKS];
static unsigned g_count[NPEG];
static unsigned g_moves;          /* moves executed by the current solve */
static unsigned g_moves_total;    /* 2^n - 1 for the current solve */
#define MAX_PRINT_MOVES 8         /* print at most this many "move" markers */

static char peg_letter(unsigned p) { return (char)('A' + p); }

static void reset_board(unsigned n)
{
    unsigned p, i;
    for (p = 0; p < NPEG; ++p) g_count[p] = 0;
    /* Load peg A bottom=largest .. top=smallest. */
    for (i = n; i >= 1; --i)
        g_peg[0][g_count[0]++] = (unsigned char)i;
}

/* Apply one legal move: pop the top disk of "from", push onto "to".
 * Returns the disk number moved, or 0 on an illegal move. */
static unsigned apply_move(unsigned from, unsigned to)
{
    unsigned disk;
    if (from >= NPEG || to >= NPEG || from == to) return 0;
    if (g_count[from] == 0) return 0;
    disk = g_peg[from][g_count[from] - 1];
    if (g_count[to] >= MAX_DISKS) return 0;
    if (g_count[to] > 0 && g_peg[to][g_count[to] - 1] < disk) return 0;
    --g_count[from];
    g_peg[to][g_count[to]++] = (unsigned char)disk;
    return disk;
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

/* Append one peg's disks (bottom..top) or '-' when empty. */
static unsigned add_peg(char* out, unsigned n, unsigned p)
{
    unsigned i;
    if (g_count[p] == 0)
        {
        out[n++] = '-';
        return n;
        }
    for (i = 0; i < g_count[p]; ++i)
        out[n++] = (char)('0' + g_peg[p][i]);
    return n;
}

/* Emit "HN: peg A: <disks> B: <disks> C: <disks>". */
static void emit_board(void)
{
    char text[80];
    unsigned n = add_text(text, 0, "HN: peg A: ");
    n = add_peg(text, n, 0);
    n = add_text(text, n, " B: ");
    n = add_peg(text, n, 1);
    n = add_text(text, n, " C: ");
    n = add_peg(text, n, 2);
    text[n] = 0;
    marker(text);
}

/* Emit one move marker: "HN: move <disk> <from>-><to>". */
static void emit_move(unsigned disk, unsigned from, unsigned to)
{
    char text[48];
    unsigned n = add_text(text, 0, "HN: move ");
    n = add_uint(text, n, disk);
    text[n++] = ' ';
    text[n++] = peg_letter(from);
    text[n++] = '-';
    text[n++] = '>';
    text[n++] = peg_letter(to);
    text[n] = 0;
    marker(text);
}

/* ------------------------------------------------------------ solver
 * Classic recursion: move k-1 disks from -> via, move the kth disk from -> to,
 * then move k-1 disks via -> to.  Every move is simulated; only the first few
 * are printed as markers so large puzzles do not flood the serial console. */
static void hanoi(unsigned k, unsigned from, unsigned to, unsigned via)
{
    unsigned disk;
    if (k == 0) return;
    hanoi(k - 1, from, via, to);
    disk = apply_move(from, to);
    if (disk != 0)
        {
        ++g_moves;
        if (g_moves <= MAX_PRINT_MOVES)
            emit_move(disk, from, to);
        }
    hanoi(k - 1, via, to, from);
}

/* Solve n disks from peg A to peg C (peg B as auxiliary). */
static void do_solve(unsigned n)
{
    char text[80];
    unsigned nlen;

    if (n < 1 || n > MAX_DISKS)
        {
        marker("HN: solve: n must be 1..4");
        return;
        }
    reset_board(n);
    g_moves = 0;
    g_moves_total = (1u << n) - 1u;

    nlen = add_text(text, 0, "HN: solve ");
    nlen = add_uint(text, nlen, n);
    nlen = add_text(text, nlen, " disks -> ");
    nlen = add_uint(text, nlen, g_moves_total);
    nlen = add_text(text, nlen, " moves");
    text[nlen] = 0;
    marker(text);

    hanoi(n, 0, 2, 1);              /* A -> C, using B */

    emit_board();                   /* final peg state */
}

/* ------------------------------------------------------------ selftest */
static void selftest(void)
{
    int ok = 1;
    marker("HN: SELFTEST begin");
    do_solve(3);                    /* 7 moves, all disks end up on peg C */
    if (g_moves != 7u) ok = 0;
    if (g_count[0] != 0u || g_count[1] != 0u || g_count[2] != 3u) ok = 0;
    if (g_count[2] == 3u &&
        (g_peg[2][0] != 3 || g_peg[2][1] != 2 || g_peg[2][2] != 1)) ok = 0;
    marker(ok ? "HN: SELFTEST OK" : "HN: SELFTEST FAIL");
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
        if (v > 1000u) return 0;
        }
    *out = v;
    return 1;
}

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    unsigned al, bl;
    unsigned n;
    const char* cmd = next_token(input, &al);
    const char* arg;

    if (token_is(cmd, al, "quit") || token_is(cmd, al, "q")) return 1;
    if (token_is(cmd, al, "board"))    { emit_board(); return 0; }
    if (token_is(cmd, al, "selftest")) { selftest(); return 0; }
    if (token_is(cmd, al, "help") || token_is(cmd, al, "h"))
        {
        marker("HN: commands: solve [1-4] board help selftest quit");
        return 0;
        }
    if (token_is(cmd, al, "solve"))
        {
        arg = next_token(cmd + al, &bl);
        if (bl == 0) do_solve(MAX_DISKS);
        else if (!parse_uint(arg, bl, &n)) marker("HN: solve: n must be 1..4");
        else do_solve(n);
        return 0;
        }
    marker("HN: commands: solve [1-4] board help selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("HN: console busy"); return 1; }

    marker("HN: TOWER OF HANOI");
    reset_board(MAX_DISKS);        /* default puzzle: 4 disks on peg A */

    for (;;)
        {
        print("HN> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;          /* timeout / input error */
        if (run_command(line)) break;  /* quit */
        }

    marker("HN: DONE");
    con_release();
    return 0;
}

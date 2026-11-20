/*
 * chess.c -- CHESS, a genuine chess opening moves viewer for the Sybian REAL
 * port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader patches the slot before the process resumes.
 * All serial markers go through that ordinal, with a DebugPrint fallback so
 * the trace is always visible.
 *
 * A fixed 8x8 starting position is preloaded.  Commands are read from PortCon
 * through read_line(): board, moves <square> (e.g. e2), opening <name> (Italian
 * Game / Sicilian Defense / French Defense), help, selftest, quit.  `moves`
 * lists the pseudo-legal destination squares of the piece on an algebraic
 * square.  The built-in selftest renders the board, the e2 pawn moves and the
 * Italian Game line so an automated test can assert every marker.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*chess_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&chess_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ board
 * Index g_board[rank][file], rank 0 = rank 1 (White), file 0 = file a.
 * Uppercase = White, lowercase = Black, '.' = empty. */
static const char kWhiteBack[8] = { 'R','N','B','Q','K','B','N','R' };
static const char kBlackBack[8] = { 'r','n','b','q','k','b','n','r' };
static char g_board[8][8];

static void reset_board(void)
{
    unsigned f, r;
    for (r = 0; r < 8; ++r)
        for (f = 0; f < 8; ++f)
            {
            if (r == 0)      g_board[r][f] = kWhiteBack[f];
            else if (r == 1) g_board[r][f] = 'P';
            else if (r == 6) g_board[r][f] = 'p';
            else if (r == 7) g_board[r][f] = kBlackBack[f];
            else             g_board[r][f] = '.';
            }
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker directly into a buffer, no hosted libc. */
static unsigned add_text(char* out, unsigned n, const char* s)
{
    while (*s) out[n++] = *s++;
    return n;
}

static unsigned add_square(char* out, unsigned n, unsigned code)
{
    out[n++] = (char)('a' + (code & 7u));          /* file 0..7 -> a..h */
    out[n++] = (char)('1' + (code >> 3));          /* rank 0..7 -> 1..8 */
    return n;
}

/* ------------------------------------------------------------ board output
 * Print rank 8 down to rank 1, files a..h, one "CH: <8 chars>" marker/row. */
static void render_board(void)
{
    char text[16];
    unsigned f, r, n;
    int rr;
    for (rr = 7; rr >= 0; --rr)
        {
        r = (unsigned)rr;
        n = add_text(text, 0, "CH: ");
        for (f = 0; f < 8; ++f) text[n++] = g_board[r][f];
        text[n] = 0;
        marker(text);
        }
}

/* ------------------------------------------------------------ move engine */
static int is_white(char p) { return p >= 'A' && p <= 'Z'; }
static int is_black(char p) { return p >= 'a' && p <= 'z'; }

static int own_piece(char p, int white)
{
    return white ? is_white(p) : is_black(p);
}
static int enemy_piece(char p, int white)
{
    return white ? is_black(p) : is_white(p);
}
static int on_board(int f, int r)
{
    return f >= 0 && f < 8 && r >= 0 && r < 8;
}

/* Collect pseudo-legal destination square codes (0..63) for the piece at f,r.
 * Returns the count; moves[] is written with no duplicates. */
static unsigned gen_moves(unsigned f, unsigned r, unsigned char* moves)
{
    char p = g_board[r][f];
    int white = is_white(p);
    unsigned n = 0, i;
    int df, dr, nf, nr;

    if (p == '.') return 0;

    if (p == 'P' || p == 'p')
        {
        int dir = white ? 1 : -1;
        int start = white ? 1 : 6;
        nr = (int)r + dir;
        if (on_board((int)f, nr) && g_board[nr][f] == '.')
            {
            moves[n++] = (unsigned char)(nr * 8 + (int)f);
            nr = (int)r + 2 * dir;
            if ((int)r == start && on_board((int)f, nr) && g_board[nr][f] == '.')
                moves[n++] = (unsigned char)(nr * 8 + (int)f);
            }
        for (i = 0; i < 2; ++i)
            {
            nf = (int)f + (i ? 1 : -1);
            nr = (int)r + dir;
            if (on_board(nf, nr) && enemy_piece(g_board[nr][nf], white))
                moves[n++] = (unsigned char)(nr * 8 + nf);
            }
        return n;
        }

    if (p == 'N' || p == 'n')
        {
        static const int nd[8][2] = {
            { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 },
            { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 }
        };
        for (i = 0; i < 8; ++i)
            {
            nf = (int)f + nd[i][0];
            nr = (int)r + nd[i][1];
            if (on_board(nf, nr) && !own_piece(g_board[nr][nf], white))
                moves[n++] = (unsigned char)(nr * 8 + nf);
            }
        return n;
        }

    if (p == 'K' || p == 'k')
        {
        for (dr = -1; dr <= 1; ++dr)
            for (df = -1; df <= 1; ++df)
                {
                if (!df && !dr) continue;
                nf = (int)f + df;
                nr = (int)r + dr;
                if (on_board(nf, nr) && !own_piece(g_board[nr][nf], white))
                    moves[n++] = (unsigned char)(nr * 8 + nf);
                }
        return n;
        }

    /* Sliding pieces: rook (orthogonal), bishop (diagonal), queen (both). */
    {
    static const int rd[4][2] = {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }
    };
    static const int bd[4][2] = {
        { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 }
    };
    unsigned kind = (p == 'R' || p == 'r') ? 1 :
                    (p == 'B' || p == 'b') ? 2 : 3; /* 3 = queen */
    unsigned t;
    for (t = 0; t < 2; ++t)                /* t=0 orthogonals, t=1 diagonals */
        {
        const int (*table)[2];
        unsigned k;
        if (kind == 1 && t == 1) break;     /* rook: orthogonals only */
        if (kind == 2 && t == 0) continue;  /* bishop: diagonals only */
        table = (t == 0) ? rd : bd;
        for (k = 0; k < 4; ++k)
            {
            df = table[k][0];
            dr = table[k][1];
            nf = (int)f + df;
            nr = (int)r + dr;
            while (on_board(nf, nr))
                {
                char q = g_board[nr][nf];
                if (q == '.')
                    moves[n++] = (unsigned char)(nr * 8 + nf);
                else
                    {
                    if (enemy_piece(q, white))
                        moves[n++] = (unsigned char)(nr * 8 + nf);
                    break;                          /* blocked either way */
                    }
                nf += df;
                nr += dr;
                }
            }
        }
    }
    return n;
}

/* Emit "CH: <square> -> <list of squares>". */
static void emit_moves(unsigned f, unsigned r)
{
    unsigned char moves[32];
    unsigned n = gen_moves(f, r, moves);
    unsigned i, len;
    char text[160];

    len = add_text(text, 0, "CH: ");
    len = add_square(text, len, r * 8 + f);
    len = add_text(text, len, " ->");
    for (i = 0; i < n; ++i)
        {
        text[len++] = ' ';
        len = add_square(text, len, moves[i]);
        }
    text[len] = 0;
    marker(text);
}

/* ------------------------------------------------------------ openings */
struct opening { const char* key; const char* name; const char* line; };
static const struct opening kOpenings[] = {
    { "italian",  "Italian Game",     "1. e4 e5 2. Nf3 Nc6 3. Bc4 Bc5" },
    { "sicilian", "Sicilian Defense", "1. e4 c5" },
    { "french",   "French Defense",   "1. e4 e6" }
};

static void emit_opening(const struct opening* op)
{
    char text[128];
    unsigned n = add_text(text, 0, "CH: ");
    n = add_text(text, n, op->name);
    n = add_text(text, n, ": ");
    n = add_text(text, n, op->line);
    text[n] = 0;
    marker(text);
}

/* Case-insensitive substring match. */
static int contains_ci(const char* text, const char* needle)
{
    const char* p;
    for (p = text; *p; ++p)
        {
        const char* a = p, *b = needle;
        while (*a && *b)
            {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
            if (ca != cb) break;
            ++a; ++b;
            }
        if (!*b) return 1;
        }
    return 0;
}

/* ------------------------------------------------------------ selftest */
static void selftest(void)
{
    marker("CH: SELFTEST begin");
    reset_board();
    render_board();                        /* prints the 8 board rows */
    emit_moves(4, 1);                      /* e2 pawn -> e3 e4 */
    emit_opening(&kOpenings[0]);           /* Italian Game */
    marker("CH: SELFTEST OK");
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

/* Parse "e2" -> f=4, r=1.  Case-insensitive on the file letter. */
static int parse_square(const char* tok, unsigned length, unsigned* out_f,
                        unsigned* out_r)
{
    char f, r;
    if (length != 2) return 0;
    f = tok[0];
    r = tok[1];
    if (f >= 'A' && f <= 'H') f = (char)(f - 'A' + 'a');
    if (f < 'a' || f > 'h') return 0;
    if (r < '1' || r > '8') return 0;
    *out_f = (unsigned)(f - 'a');
    *out_r = (unsigned)(r - '1');
    return 1;
}

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    unsigned al, bl;
    unsigned f, r;
    unsigned i;
    const char* cmd = next_token(input, &al);
    const char* arg;

    if (token_is(cmd, al, "quit") || token_is(cmd, al, "q")) return 1;
    if (token_is(cmd, al, "board"))      { render_board(); return 0; }
    if (token_is(cmd, al, "selftest"))   { selftest(); return 0; }
    if (token_is(cmd, al, "help"))
        {
        marker("CH: commands: board moves <square> opening <name> help selftest quit");
        return 0;
        }
    if (token_is(cmd, al, "moves"))
        {
        arg = next_token(cmd + al, &bl);
        if (!parse_square(arg, bl, &f, &r))
            marker("CH: moves bad square");
        else
            emit_moves(f, r);
        return 0;
        }
    if (token_is(cmd, al, "opening") || token_is(cmd, al, "open"))
        {
        arg = cmd + al;
        while (*arg == ' ' || *arg == '\t') ++arg;
        for (i = 0; i < sizeof(kOpenings) / sizeof(kOpenings[0]); ++i)
            if (contains_ci(arg, kOpenings[i].key))
                { emit_opening(&kOpenings[i]); return 0; }
        marker("CH: unknown opening");
        return 0;
        }
    marker("CH: commands: board moves <square> opening <name> help selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("CH: console busy"); return 1; }

    marker("CH: CHESS");
    reset_board();
    render_board();

    for (;;)
        {
        print("CH> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("CH: DONE");
    con_release();
    return 0;
}

/*
 * codebreak.c -- CODEBREAKER (Mastermind), a genuine interactive game for the
 * Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * The computer picks a 4-color secret code from the palette "RGBYGP"
 * (red green blue yellow green purple); repeats are allowed.  The player has
 * eight guesses.  Each guess reports black = right colour in the right place,
 * white = right colour in the wrong place.  Commands are read from PortCon
 * through read_line(): new / guess <4-letter-code> / show / selftest / quit.
 * The secret stays hidden in normal play; the built-in selftest reveals it and
 * solves a known code in three guesses so an automated test can assert every
 * marker including the WIN.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*codebreak_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&codebreak_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ palette/state */
/* Red, Green, Blue, Yellow, Green, Purple (the literal RGBYGP palette).  The
 * second G simply makes green twice as likely when the computer picks a code;
 * it is still the same colour letter for input validation and scoring. */
static const char kColors[] = "RGBYGP";
#define NCOLORS ((unsigned)(sizeof(kColors) - 1))
#define CLEN 4u
#define MAX_GUESSES 8u

static char g_secret[CLEN + 1];
static char g_history[MAX_GUESSES][CLEN + 1];
static unsigned char g_bw[MAX_GUESSES][2];
static unsigned g_guesses;
static int g_over;
static int g_won;

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

/* Return the palette slot of a colour letter (case-insensitive), or -1. */
static int color_index(char c)
{
    unsigned i;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (i = 0; i < NCOLORS; ++i)
        if (kColors[i] == c) return (int)i;
    return -1;
}

/* ------------------------------------------------------------ game engine */
/* Begin a game on a specific secret.  Used by `new`, startup and selftest;
 * this routine never prints the secret (normal play keeps it hidden). */
static void start_secret(const char* code)
{
    unsigned i;
    for (i = 0; i < CLEN; ++i) g_secret[i] = code[i];
    g_secret[CLEN] = 0;
    for (i = 0; i < MAX_GUESSES; ++i)
        {
        g_history[i][0] = 0;
        g_bw[i][0] = 0;
        g_bw[i][1] = 0;
        }
    g_guesses = 0;
    g_over = 0;
    g_won = 0;
}

/* Pick a palette index from the genuine kernel MathRandom executive (exec 11)
 * with a system-clock fallback for its error sentinels. */
static unsigned pick_index(void)
{
    int r = (int)random32();
    if (r == 0 || r == -1 || r == -45) r = time_now();
    if (r < 0) r = -r;
    return (unsigned)r % NCOLORS;
}

static void new_game(void)
{
    char code[CLEN + 1];
    unsigned i;
    for (i = 0; i < CLEN; ++i) code[i] = kColors[pick_index()];
    code[CLEN] = 0;
    marker("CB: NEW GAME");
    start_secret(code);
}

/* Score with the standard two-pass Mastermind rule so duplicate colours are
 * counted correctly: first exact position+colour matches (black), then pair
 * up the remaining colours (white) by the minimum of the two counts. */
static void score(const char* guess, unsigned* black, unsigned* white)
{
    unsigned char gs[NCOLORS], ss[NCOLORS];
    unsigned b = 0, w = 0, i;
    int gi, si;
    for (i = 0; i < NCOLORS; ++i) { gs[i] = 0; ss[i] = 0; }
    for (i = 0; i < CLEN; ++i)
        if (guess[i] == g_secret[i]) ++b;
    for (i = 0; i < CLEN; ++i)
        {
        if (guess[i] == g_secret[i]) continue;
        gi = color_index(guess[i]);
        si = color_index(g_secret[i]);
        if (gi >= 0) ++gs[gi];
        if (si >= 0) ++ss[si];
        }
    for (i = 0; i < NCOLORS; ++i)
        {
        unsigned m = gs[i] < ss[i] ? gs[i] : ss[i];
        w += m;
        }
    *black = b;
    *white = w;
}

static void emit_guess(const char* code, unsigned black, unsigned white)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "CB: guess ");
    n = add_text(text, n, code);
    n = add_text(text, n, " black=");
    n = add_uint(text, n, black);
    n = add_text(text, n, " white=");
    n = add_uint(text, n, white);
    text[n] = 0;
    marker(text);
    if (g_guesses < MAX_GUESSES)
        {
        unsigned i;
        for (i = 0; i <= CLEN; ++i) g_history[g_guesses][i] = code[i];
        g_bw[g_guesses][0] = (unsigned char)black;
        g_bw[g_guesses][1] = (unsigned char)white;
        ++g_guesses;
        }
}

static void emit_win_lose(int win)
{
    char text[32];
    unsigned n = 0;
    n = add_text(text, n, win ? "CB: WIN code=" : "CB: LOSE code=");
    n = add_text(text, n, g_secret);
    text[n] = 0;
    marker(text);
    g_over = 1;
    g_won = win;
}

static void do_guess(const char* code)
{
    unsigned black, white, i;
    if (g_over) { marker("CB: game over - use new"); return; }
    for (i = 0; i < CLEN; ++i)
        {
        if (!code[i]) { marker("CB: bad guess: need 4 colors"); return; }
        if (color_index(code[i]) < 0) { marker("CB: bad guess: invalid color"); return; }
        }
    if (code[CLEN]) { marker("CB: bad guess: too long"); return; }
    score(code, &black, &white);
    emit_guess(code, black, white);
    if (black == CLEN) { emit_win_lose(1); return; }
    if (g_guesses >= MAX_GUESSES) emit_win_lose(0);
}

/* `show` prints the game status and replays every guess made so far. */
static void do_show(void)
{
    char text[64];
    unsigned n = 0, i;
    n = add_text(text, n, "CB: status guesses=");
    n = add_uint(text, n, g_guesses);
    n = add_text(text, n, "/8");
    if (g_over) n = add_text(text, n, g_won ? " WIN" : " LOSE");
    text[n] = 0;
    marker(text);
    for (i = 0; i < g_guesses; ++i)
        {
        char line[64];
        unsigned m = 0;
        m = add_text(line, m, "CB: guess ");
        m = add_text(line, m, g_history[i]);
        m = add_text(line, m, " black=");
        m = add_uint(line, m, g_bw[i][0]);
        m = add_text(line, m, " white=");
        m = add_uint(line, m, g_bw[i][1]);
        line[m] = 0;
        marker(line);
        }
}

/* ------------------------------------------------------------ selftest
 * Reveal a known secret, then solve it in three guesses that exercise both
 * black-only, white and the final WIN feedback. */
static void selftest(void)
{
    const char* secret = "RGBY";
    char text[16];
    unsigned n = 0;
    marker("CB: SELFTEST begin");
    start_secret(secret);
    n = add_text(text, n, "CB: secret=");
    n = add_text(text, n, secret);
    text[n] = 0;
    marker(text);                      /* secret is printed only in selftest */
    do_guess("RGBP");                  /* black=3 white=0 */
    do_guess("RBGY");                  /* black=2 white=2 */
    do_guess("RGBY");                  /* black=4 white=0 -> WIN */
    marker(g_won ? "CB: SELFTEST OK" : "CB: SELFTEST FAIL");
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
    unsigned cl, al;
    const char* cmd = next_token(input, &cl);
    const char* arg = next_token(cmd + cl, &al);

    if (token_is(cmd, cl, "quit") || token_is(cmd, cl, "q")) return 1;
    if (token_is(cmd, cl, "new")) { new_game(); return 0; }
    if (token_is(cmd, cl, "show")) { do_show(); return 0; }
    if (token_is(cmd, cl, "guess"))
        {
        if (!al) marker("CB: guess: need code");
        else do_guess(arg);
        return 0;
        }
    if (token_is(cmd, cl, "selftest")) { selftest(); return 0; }
    if (token_is(cmd, cl, "help") || token_is(cmd, cl, "h"))
        marker("CB: commands: new guess <code> show selftest quit");
    else
        marker("CB: commands: new guess <code> show selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("CB: console busy"); return 1; }

    marker("CB: CODEBREAKER");
    marker("CB: commands: new guess <code> show selftest quit");
    new_game();                        /* pick a hidden secret for normal play */

    for (;;)
        {
        print("CB> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("CB: DONE");
    con_release();
    return 0;
}

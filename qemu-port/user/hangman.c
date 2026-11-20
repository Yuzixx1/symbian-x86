/*
 * hangman.c -- HANGMAN, a genuine interactive word game for the Sybian REAL
 * port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * A word is picked from a small built-in list using the genuine kernel
 * MathRandom executive (slow exec 11 via random32()), with the system clock as
 * a fallback when that executive fails.  Six wrong guesses are allowed.
 * Commands are read from PortCon through read_line(): new, guess <letter>,
 * board, selftest, quit.  The built-in selftest plays a complete winning game
 * on the known word KERNEL so an automated test can assert every marker.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*hangman_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&hangman_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

#define MAX_WORD 16
#define MAX_WRONG 6

/* Built-in dictionary.  All entries are uppercase ASCII and short enough for
 * the masked board marker below. */
static const char* const kWords[] = {
    "SYMBIAN", "KERNEL", "EKA2", "QEMU",
    "DESKTOP", "NETWORK", "DRIVER"
};
#define NWORDS ((unsigned)(sizeof(kWords) / sizeof(kWords[0])))

static char g_word[MAX_WORD + 1];
static unsigned g_len;
static unsigned g_unique;      /* number of distinct letters in the word */
static unsigned g_hits;        /* distinct word letters guessed correctly */
static unsigned g_wrong;       /* incorrect guesses so far */
static unsigned char g_guessed[26];
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

static unsigned str_len(const char* s)
{
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}

/* ------------------------------------------------------------ game engine */
static unsigned distinct_letters(const char* word)
{
    unsigned char seen[26];
    unsigned n = 0, i;
    for (i = 0; i < 26; ++i) seen[i] = 0;
    for (i = 0; word[i]; ++i)
        {
        unsigned k = (unsigned)(word[i] - 'A');
        if (k < 26u && !seen[k]) { seen[k] = 1; ++n; }
        }
    return n;
}

static void emit_word_length(void)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, "HG: word ");
    n = add_uint(text, n, g_len);
    n = add_text(text, n, " letters");
    text[n] = 0;
    marker(text);
}

static void render_board(void)
{
    char text[MAX_WORD + 16];
    unsigned n = 0, i;
    n = add_text(text, n, "HG: board ");
    for (i = 0; i < g_len; ++i)
        {
        char ch = g_word[i];
        unsigned k = (unsigned)(ch - 'A');
        if (k >= 26u || !g_guessed[k]) ch = '_';
        text[n++] = ch;
        }
    text[n] = 0;
    marker(text);
}

/* Begin a game on a specific word.  Used both by `new` and the selftest. */
static void start_word(const char* word)
{
    unsigned i;
    g_len = str_len(word);
    if (g_len > MAX_WORD) g_len = MAX_WORD;
    for (i = 0; i < g_len; ++i) g_word[i] = word[i];
    g_word[g_len] = 0;
    for (i = 0; i < 26; ++i) g_guessed[i] = 0;
    g_unique = distinct_letters(g_word);
    g_hits = 0;
    g_wrong = 0;
    g_over = 0;
    g_won = 0;
    emit_word_length();
    render_board();
}

/* Pick a dictionary index: genuine exec 11 (MathRandom) with a system-clock
 * fallback for error sentinels (0, KErrGeneral=-1, KErrNotSupported=-45). */
static unsigned pick_index(void)
{
    int r = (int)random32();
    if (r == 0 || r == -1 || r == -45) r = time_now();
    if (r < 0) r = -r;
    return (unsigned)r % NWORDS;
}

static void new_game(void)
{
    marker("HG: NEW GAME");
    start_word(kWords[pick_index()]);
}

static void emit_win_lose(int win)
{
    char text[64];
    unsigned n = 0;
    n = add_text(text, n, win ? "HG: WIN word=" : "HG: LOSE word=");
    n = add_text(text, n, g_word);
    text[n] = 0;
    marker(text);
    g_over = 1;
    g_won = win;
}

static void emit_guess(char c, const char* result)
{
    char text[32];
    unsigned n = 0;
    n = add_text(text, n, "HG: guess ");
    text[n++] = c;
    n = add_text(text, n, " ");
    n = add_text(text, n, result);
    text[n] = 0;
    marker(text);
}

static void do_guess(char c)
{
    unsigned k, i, found = 0;
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c < 'A' || c > 'Z') { marker("HG: bad letter"); return; }
    if (g_over) return;
    k = (unsigned)(c - 'A');
    if (g_guessed[k]) { marker("HG: repeat letter"); return; }
    g_guessed[k] = 1;
    for (i = 0; i < g_len; ++i)
        if (g_word[i] == c) ++found;
    if (found)
        {
        emit_guess(c, "correct");
        ++g_hits;                 /* a letter is counted once, however often it occurs */
        render_board();
        if (g_hits >= g_unique) emit_win_lose(1);
        }
    else
        {
        char text[32];
        unsigned n = 0;
        ++g_wrong;
        emit_guess(c, "wrong");
        n = add_text(text, n, "HG: misses ");
        n = add_uint(text, n, g_wrong);
        n = add_text(text, n, "/");
        n = add_uint(text, n, MAX_WRONG);
        text[n] = 0;
        marker(text);
        render_board();
        if (g_wrong >= MAX_WRONG) emit_win_lose(0);
        }
}

/* ------------------------------------------------------------ selftest
 * Play a flawless game on a known word, exercising correct guesses, the
 * masked board and the WIN marker. */
static void selftest(void)
{
    const char* word = "KERNEL";
    unsigned i;
    marker("HG: SELFTEST begin word=KERNEL");
    start_word(word);
    for (i = 0; word[i]; ++i)
        {
        unsigned j, seen = 0;
        for (j = 0; j < i; ++j) if (word[j] == word[i]) { seen = 1; break; }
        if (!seen) do_guess(word[i]);
        }
    if (g_won) marker("HG: SELFTEST OK");
    else marker("HG: SELFTEST FAIL");
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
    const char* word, *arg;
    unsigned wl, al;
    while (*input == ' ') ++input;
    word = input; wl = 0;
    while (word[wl] && word[wl] != ' ') ++wl;
    arg = word + wl;
    while (*arg == ' ') ++arg;
    al = 0;
    while (arg[al] && arg[al] != ' ') ++al;

    if (word_is(word, wl, "new"))
        { new_game(); return 0; }
    if (word_is(word, wl, "board"))
        { render_board(); return 0; }
    if (word_is(word, wl, "guess"))
        {
        if (!al) marker("HG: guess: need letter");
        else do_guess(arg[0]);
        return 0;
        }
    if (word_is(word, wl, "selftest"))
        { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q"))
        return 1;
    marker("HG: commands: new guess <letter> board selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("HG: console busy"); return 1; }

    marker("HG: HANGMAN");
    start_word(kWords[pick_index()]);

    for (;;)
        {
        print("HG> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("HG: DONE");
    con_release();
    return 0;
}

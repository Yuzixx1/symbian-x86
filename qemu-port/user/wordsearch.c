/*
 * wordsearch.c -- WORD SEARCH, a genuine interactive word-find puzzle for the
 * Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * A fixed 10x10 letter grid hides five words, placed horizontally or
 * vertically.  The grid is generated from a deterministic LCG (seed 12345) so
 * the built-in selftest always finds the same five words at the same
 * coordinates.  Commands are read from PortCon through read_line():
 * board, find <word>, words, selftest, quit.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*wordsearch_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&wordsearch_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

#define GRID 10
#define NWORDS 5
#define MAX_WORD 16

static char g_grid[GRID][GRID];

/* The five hidden words and where they were planted (zero-based row/col). */
static const char* const kWords[NWORDS] = {
    "SYMBIAN", "KERNEL", "QEMU", "EKA2", "DRIVER"
};
static const unsigned char kRow[NWORDS] = { 0, 0, 4, 5, 9 };
static const unsigned char kCol[NWORDS] = { 0, 9, 1, 2, 3 };
static const char kDir[NWORDS] = { 'H', 'V', 'H', 'V', 'H' };

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

/* ------------------------------------------------------------ grid engine */
static void build_grid(void)
{
    unsigned x = 12345u;
    int r, c, i;

    /* Deterministic background of letters. */
    for (r = 0; r < GRID; ++r)
        for (c = 0; c < GRID; ++c)
            {
            x = x * 1103515245u + 12345u;
            g_grid[r][c] = (char)('A' + (x & 0x7fffffffu) % 26u);
            }
    /* Plant the five words over the background. */
    for (i = 0; i < NWORDS; ++i)
        {
        const char* w = kWords[i];
        for (r = 0; w[r]; ++r)
            {
            if (kDir[i] == 'H') g_grid[kRow[i]][kCol[i] + r] = w[r];
            else                g_grid[kRow[i] + r][kCol[i]] = w[r];
            }
        }
}

static void render_board(void)
{
    int r, c;
    for (r = 0; r < GRID; ++r)
        {
        char line[GRID + 8];
        unsigned n = add_text(line, 0, "WS: ");
        for (c = 0; c < GRID; ++c) line[n++] = g_grid[r][c];
        line[n] = 0;
        marker(line);
        }
}

static void emit_words(void)
{
    char text[96];
    unsigned n = add_text(text, 0, "WS: words: ");
    int i;
    for (i = 0; i < NWORDS; ++i)
        {
        if (i) text[n++] = ' ';
        n = add_text(text, n, kWords[i]);
        }
    text[n] = 0;
    marker(text);
}

static void emit_found(const char* word, unsigned r, unsigned c, char dir)
{
    char text[64];
    unsigned n = add_text(text, 0, "WS: ");
    n = add_text(text, n, word);
    n = add_text(text, n, " found at r=");
    n = add_uint(text, n, r);
    n = add_text(text, n, " c=");
    n = add_uint(text, n, c);
    n = add_text(text, n, " dir=");
    text[n++] = dir;
    text[n] = 0;
    marker(text);
}

/* Search left-to-right rows first, then top-to-bottom columns; return the
 * first occurrence and emit the marker. */
static int find_word(const char* input)
{
    char word[MAX_WORD + 1];
    unsigned len = str_len(input), i, r, c;

    if (!len) { marker("WS: find: need word"); return 0; }
    if (len > MAX_WORD) len = MAX_WORD;
    for (i = 0; i < len; ++i)
        {
        char ch = input[i];
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        word[i] = ch;
        }
    word[len] = 0;

    for (r = 0; r < GRID; ++r)
        for (c = 0; c + len <= GRID; ++c)
            {
            for (i = 0; i < len; ++i)
                if (g_grid[r][c + i] != word[i]) break;
            if (i == len) { emit_found(word, r, c, 'H'); return 1; }
            }
    for (c = 0; c < GRID; ++c)
        for (r = 0; r + len <= GRID; ++r)
            {
            for (i = 0; i < len; ++i)
                if (g_grid[r + i][c] != word[i]) break;
            if (i == len) { emit_found(word, r, c, 'V'); return 1; }
            }

    {
    char text[MAX_WORD + 16];
    unsigned n = add_text(text, 0, "WS: ");
    n = add_text(text, n, word);
    n = add_text(text, n, " not found");
    text[n] = 0;
    marker(text);
    }
    return 0;
}

/* Find every hidden word; the selftest passes only if all five are found. */
static void selftest(void)
{
    int i, found = 0;
    marker("WS: SELFTEST begin");
    render_board();
    emit_words();
    for (i = 0; i < NWORDS; ++i)
        if (find_word(kWords[i])) ++found;
    if (found == NWORDS) marker("WS: SELFTEST OK");
    else marker("WS: SELFTEST FAIL");
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
    const char *word, *arg;
    unsigned wl, al;

    while (*input == ' ') ++input;
    word = input; wl = 0;
    while (word[wl] && word[wl] != ' ') ++wl;
    arg = word + wl;
    while (*arg == ' ') ++arg;
    al = 0;
    while (arg[al] && arg[al] != ' ') ++al;

    if (word_is(word, wl, "board"))
        { render_board(); return 0; }
    if (word_is(word, wl, "find"))
        {
        if (!al) marker("WS: find: need word");
        else find_word(arg);
        return 0;
        }
    if (word_is(word, wl, "words"))
        { emit_words(); return 0; }
    if (word_is(word, wl, "selftest"))
        { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q"))
        return 1;
    marker("WS: commands: board find <word> words selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("WS: console busy"); return 1; }

    build_grid();
    marker("WS: WORD SEARCH 10x10");

    for (;;)
        {
        print("WS> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("WS: DONE");
    con_release();
    return 0;
}

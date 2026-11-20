/*
 * fortune.c -- a FORTUNE COOKIE E32 program for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot
 * `fortune_printf_slot` at codeBase+0x8000 (0x408000); elf2e32.py
 * --euser-import 915 emits the PE2 import section and the loader overwrites
 * the slot with the resolved export address before the process resumes.
 * All serial markers go through that ordinal, with the port's DebugPrint
 * executive as a fallback so the trace is always visible (same pattern as
 * user/calc.c, user/filemgr.c and user/clock.c).
 *
 * A fortune is picked from a table of 16 built-in one-line fortunes using the
 * genuine kernel MathRandom executive (slow exec 11, EExecMathRandom).  If
 * that executive reports failure (a zero / error sentinel) we fall back to a
 * tick-based index from EExecTickCount (slow exec 5).  Console commands
 * (next / selftest / help / quit) are read from PortCon through read_line();
 * the built-in selftest emits three consecutive fortunes.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*fortune_printf_slot)(const char*);
extern int exec0(unsigned number);

#define EEXEC_MATH_RANDOM 11u          /* genuine EExecMathRandom */
#define EEXEC_TICK_COUNT  5u           /* genuine EExecTickCount (fallback) */
#define FORTUNE_COUNT     16u

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&fortune_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ table of fortunes
 * Sixteen short, ASCII, one-line fortunes.  Kept well below 128 bytes each so
 * a "FORT: <text>" marker always fits the scratch buffer below. */
static const char* const kFortunes[FORTUNE_COUNT] = {
    "A journey of a thousand miles begins with a single step.",
    "You will find a valuable opportunity in unexpected places.",
    "Patience is the key to solving todays puzzle.",
    "Good things come to those who wait and better to those who code.",
    "The early bird gets the worm, the second mouse gets the cheese.",
    "A smile is a curve that sets everything straight.",
    "Your hard work will soon pay off in surprising ways.",
    "Beware of low-flying cookies.",
    "The best time to plant a tree was twenty years ago.",
    "Fortune favors the bold, but verifies the cautious.",
    "Today is a fine day to try something completely new.",
    "You will soon hear from a friend you have not seen in years.",
    "Keep your friends close and your backups closer.",
    "Every problem has a solution; look at it from another angle.",
    "Luck is what happens when preparation meets opportunity.",
    "The cookie crumbles, but the wisdom remains."
};

/* ------------------------------------------------------------ text helpers
 * Freestanding: no hosted libc, so build each marker directly into a buffer. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

static unsigned append_int(char* dst, unsigned n, unsigned value)
{
    char digits[12];
    int d = 0;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10); value /= 10; }
    while (d) dst[n++] = digits[--d];
    return n;
}

/* ------------------------------------------------------------ fortune picking
 * Pick a table index using exec 11 (MathRandom).  A zero or error return (0,
 * KErrGeneral=-1, KErrNotSupported=-45) is treated as a failed executive and
 * falls back to the genuine system tick count (exec 5). */
static unsigned fortune_index(void)
{
    int r = exec0(EEXEC_MATH_RANDOM);
    int failed = (r == 0) || (r == -1) || (r == -45); /* 0, KErrGeneral, KErrNotSupported */
    unsigned source;
    if (failed)
        source = (unsigned)exec0(EEXEC_TICK_COUNT);   /* tick-based fallback */
    else
        source = (unsigned)r;
    return source % FORTUNE_COUNT;
}

/* Emit one fortune: "FORT: <text>" followed by "FORT: cookie #<n> of 16". */
static void show_index(unsigned index)
{
    char line[160];
    unsigned n = 0;
    if (index >= FORTUNE_COUNT) index = 0;
    n = copy_text(line, n, "FORT: ");
    n = copy_text(line, n, kFortunes[index]);
    line[n] = 0;
    marker(line);
    n = 0;
    n = copy_text(line, n, "FORT: cookie #");
    n = append_int(line, n, index + 1u);          /* 1..16 */
    n = copy_text(line, n, " of 16");
    line[n] = 0;
    marker(line);
}

static void show_next(void) { show_index(fortune_index()); }

/* Built-in self-test: three consecutive fortunes from a random start point. */
static void selftest(void)
{
    unsigned base = fortune_index();
    unsigned i;
    marker("FORT: SELFTEST 3 CONSECUTIVE FORTUNES");
    for (i = 0; i < 3u; ++i)
        show_index((base + i) % FORTUNE_COUNT);
}

static void help(void)
{
    print("FORT: commands: next  selftest  help  quit");
}

/* Case-insensitive compare of a NUL-terminated command word to a literal. */
static int word_is(const char* word, unsigned length, const char* literal)
{
    unsigned i = 0;
    while (i < length && literal[i])
        {
        char a = word[i], b = literal[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == length && literal[i] == 0) ? 1 : 0;
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    while (*input == ' ') ++input;
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    if      (word_is(input, length, "next"))     show_next();
    else if (word_is(input, length, "selftest")) selftest();
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        help();
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("FORT: unknown command");
    return 0;
}

int app_main(void)
{
    static U16 arguments[64];
    char line[128];
    int length = command_line(arguments, 64);
    int r;

    marker("FORT: FORTUNE COOKIE");

    /* An optional `selftest` on the command line runs the built-in suite and
     * exits; with no arguments we are an interactive PortCon application. */
    if (length > 0)
        {
        char arg[64];
        int i;
        for (i = 0; i < length && i < 63; ++i)
            arg[i] = arguments[i] < 128 ? (char)arguments[i] : '?';
        arg[i] = 0;
        if (word_is(arg, (unsigned)i, "selftest")) selftest();
        marker("FORT: DONE");
        return 0;
        }

    r = con_acquire();
    if (r < 0) { marker("FORT: console busy"); marker("FORT: DONE"); return 1; }

    /* Show one fortune on startup so the markers appear even with no input. */
    show_next();

    for (;;)
        {
        print("FORT> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("FORT: DONE");
    con_release();
    return 0;
}

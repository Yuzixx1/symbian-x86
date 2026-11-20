/*
 * dice.c -- a DICE ROLLER / RANDOM tool E32 program for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot
 * `dice_printf_slot` at codeBase+0x8000 (0x408000); elf2e32.py
 * --euser-import 915 emits the PE2 import section and the loader overwrites
 * the slot with the resolved export address before the process resumes.
 * All serial markers go through that ordinal, with the port's DebugPrint
 * executive as a fallback so the trace is always visible (same pattern as
 * user/fortune.c, user/calc.c and user/filemgr.c).
 *
 * Randomness comes from the genuine kernel MathRandom executive (slow exec 11,
 * EExecMathRandom).  A zero / error return (0, KErrGeneral=-1,
 * KErrNotSupported=-45) is treated as a failed executive and falls back to the
 * genuine system tick count (slow exec 5, EExecTickCount).  Values are reduced
 * modulo the number of faces / choices for dice, coin flips and random picks.
 *
 * Console commands (roll <NdN> / flip / pick <a,b,c,...> / selftest / help /
 * quit) are read from PortCon through read_line().  `roll 2d6` rolls two
 * six-sided dice, `flip` tosses a coin and `pick` chooses from a comma list.
 */

#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*dice_printf_slot)(const char*);
extern int exec0(unsigned number);

#define EEXEC_MATH_RANDOM 11u          /* genuine EExecMathRandom */
#define EEXEC_TICK_COUNT  5u           /* genuine EExecTickCount (fallback) */

#define MAX_DICE    20u                 /* cap a single roll at 20 dice */
#define MAX_FACES   1000u               /* cap faces at d1000 */

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&dice_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: no hosted libc, so build each marker directly into a buffer. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

static unsigned append_uint(char* dst, unsigned n, unsigned value)
{
    char digits[12];
    int d = 0;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10); value /= 10; }
    while (d) dst[n++] = digits[--d];
    return n;
}

/* ------------------------------------------------------------ randomness
 * Pick an unsigned random source using exec 11 (MathRandom).  A zero or error
 * return (0, KErrGeneral=-1, KErrNotSupported=-45) is treated as a failed
 * executive and falls back to the genuine system tick count (exec 5). */
static unsigned random_source(void)
{
    int r = exec0(EEXEC_MATH_RANDOM);
    int failed = (r == 0) || (r == -1) || (r == -45);
    return failed ? (unsigned)exec0(EEXEC_TICK_COUNT) : (unsigned)r;
}

/* A uniformly-ish reduced value in [1..limit] using mod N. */
static unsigned roll_one(unsigned limit)
{
    return (random_source() % limit) + 1u;
}

/* ------------------------------------------------------------ N d N parsing
 * Parse a "NdN" spec such as "2d6" into count and faces.  Returns 1 on
 * success.  Faces must be >=2 (a one-sided die is meaningless). */
static int parse_roll(const char* spec, unsigned* count, unsigned* faces)
{
    unsigned c = 0, f = 0;
    while (*spec >= '0' && *spec <= '9')
        {
        c = c * 10u + (unsigned)(*spec - '0');
        ++spec;
        }
    if (!c || (*spec != 'd' && *spec != 'D')) return 0;
    ++spec;
    while (*spec >= '0' && *spec <= '9')
        {
        f = f * 10u + (unsigned)(*spec - '0');
        ++spec;
        }
    if (*spec || !f || f < 2u) return 0;
    if (c > MAX_DICE || f > MAX_FACES) return 0;
    *count = c;
    *faces = f;
    return 1;
}

/* Roll a validated NdN spec and emit exactly:
 *   DC: <spec> -> [v1,v2,...] sum=<total> */
static void do_roll(const char* spec)
{
    unsigned count, faces;
    char line[256];
    unsigned n = 0, i, sum = 0;
    if (!parse_roll(spec, &count, &faces))
        {
        marker("DC: roll: use <NdN> e.g. 2d6");
        return;
        }
    n = copy_text(line, n, "DC: ");
    n = copy_text(line, n, spec);
    n = copy_text(line, n, " -> [");
    for (i = 0; i < count; ++i)
        {
        unsigned v = roll_one(faces);
        if (i) line[n++] = ',';
        n = append_uint(line, n, v);
        sum += v;
        }
    n = copy_text(line, n, "] sum=");
    n = append_uint(line, n, sum);
    line[n] = 0;
    marker(line);
}

/* ------------------------------------------------------------ coin flip */
static void do_flip(void)
{
    char line[48];
    unsigned n = 0;
    n = copy_text(line, n, "DC: flip -> ");
    n = copy_text(line, n, roll_one(2u) == 1u ? "heads" : "tails");
    line[n] = 0;
    marker(line);
}

/* ------------------------------------------------------------ comma pick
 * Split the remainder on commas into up to 16 non-empty choices, then pick
 * one with mod N.  Surrounding spaces are trimmed.  Emits:
 *   DC: pick -> <item> */
static void do_pick(const char* list)
{
    const char* items[16];
    unsigned lengths[16];
    unsigned count = 0;
    const char* p = list;

    while (*p && count < 16u)
        {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        items[count] = p;
        while (*p && *p != ',') ++p;
        lengths[count] = (unsigned)(p - items[count]);
        /* Trim trailing whitespace from the item. */
        while (lengths[count] && (items[count][lengths[count] - 1] == ' '
                               || items[count][lengths[count] - 1] == '\t'))
            --lengths[count];
        ++count;
        if (*p == ',') ++p;
        }
    if (count == 0)
        {
        marker("DC: pick: use <a,b,c,...>");
        return;
        }
    {
    unsigned index = (random_source() % count);
    char line[160];
    unsigned n = 0, i;
    n = copy_text(line, n, "DC: pick -> ");
    for (i = 0; i < lengths[index]; ++i)
        line[n++] = items[index][i];
    line[n] = 0;
    marker(line);
    }
}

/* ------------------------------------------------------------ selftest
 * Exercises all three random paths through the same functions used for live
 * commands, then reports a single completion marker. */
static void selftest(void)
{
    do_roll("2d6");
    do_flip();
    do_pick("red,green,blue,yellow");
    marker("DC: selftest OK");
}

static void help(void)
{
    marker("DC: commands: roll <NdN>  flip  pick <a,b,c,...>  selftest  help  quit");
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

/* Return the first whitespace-delimited word and skip leading spaces. */
static const char* first_word(const char* input, unsigned* length)
{
    while (*input == ' ' || *input == '\t') ++input;
    const char* start = input;
    while (*input && *input != ' ' && *input != '\t') ++input;
    *length = (unsigned)(input - start);
    return start;
}

static const char* skip_spaces(const char* p)
{
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    unsigned length;
    const char* word = first_word(input, &length);
    const char* rest = skip_spaces(word + length);
    if      (word_is(word, length, "roll"))    do_roll(rest);
    else if (word_is(word, length, "flip"))    do_flip();
    else if (word_is(word, length, "pick"))    do_pick(rest);
    else if (word_is(word, length, "selftest")) selftest();
    else if (word_is(word, length, "help") || word_is(word, length, "?"))
        help();
    else if (word_is(word, length, "quit") || word_is(word, length, "q"))
        return 1;
    else marker("DC: unknown command");
    return 0;
}

int app_main(void)
{
    static U16 arguments[64];
    char line[128];
    int length = command_line(arguments, 64);
    int r;

    marker("DC: DICE ROLLER");

    /* An optional command on the command line (e.g. `dice 2d6` or
     * `dice selftest`) runs once and exits; with no arguments we are an
     * interactive PortCon application. */
    if (length > 0)
        {
        char arg[64];
        int i;
        for (i = 0; i < length && i < 63; ++i)
            arg[i] = arguments[i] < 128 ? (char)arguments[i] : '?';
        arg[i] = 0;
        run_command(arg);
        marker("DC: DONE");
        return 0;
        }

    r = con_acquire();
    if (r < 0) { marker("DC: console busy"); marker("DC: DONE"); return 1; }

    for (;;)
        {
        print("DC> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("DC: DONE");
    con_release();
    return 0;
}

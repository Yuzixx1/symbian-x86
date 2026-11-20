/*
 * zodiac.c -- a ZODIAC / HOROSCOPE E32 program for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot
 * `zodiac_printf_slot` at codeBase+0x8000 (0x408000); elf2e32.py
 * --euser-import 915 emits the PE2 import section and the loader overwrites
 * the slot with the resolved export address before the process resumes.
 * All serial markers go through that ordinal, with the port's DebugPrint
 * executive as a fallback so the trace is always visible (same pattern as
 * user/fortune.c, user/periodic.c, user/filemgr.c and user/solar.c).
 *
 * The viewer holds a built-in table of the 12 western zodiac signs (name,
 * date range, element) plus a deterministic daily horoscope: a fixed seed is
 * mixed with the sign name through a small LCG and reduced modulo a table of
 * eight one-line readings.  Being seed-driven (not time-driven) the reading is
 * stable across boots, which makes the built-in selftest reproducible.
 * Console commands (sign / list / horoscope / selftest / help / quit) are read
 * from PortCon through read_line(); the built-in selftest prints the Aries
 * fact row and its horoscope.  All markers start with "ZD: ".
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*zodiac_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&zodiac_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: no hosted libc, so build each marker directly into a buffer. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

/* Case-insensitive compare of a NUL-terminated word to a literal. */
static int word_is(const char* word, const char* literal)
{
    while (*word && *literal)
        {
        char a = *word++, b = *literal++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
        }
    return (*word == 0 && *literal == 0) ? 1 : 0;
}

/* Copy one whitespace-delimited word out of *pp, advancing past it. */
static void take_word(const char** pp, char* out, unsigned cap)
{
    const char* p = *pp;
    unsigned n = 0;
    while (*p == ' ') ++p;
    while (*p && *p != ' ' && n + 1 < cap)
        out[n++] = *p++;
    out[n] = 0;
    *pp = p;
}

/* ------------------------------------------------------------ zodiac data
 * name | dates | element.  twelve signs in traditional order. */
struct Sign {
    const char* name;
    const char* dates;
    const char* element;
};

static const struct Sign kSigns[] = {
    { "Aries",       "Mar 21 - Apr 19", "Fire"  },
    { "Taurus",      "Apr 20 - May 20", "Earth" },
    { "Gemini",      "May 21 - Jun 20", "Air"   },
    { "Cancer",      "Jun 21 - Jul 22", "Water" },
    { "Leo",         "Jul 23 - Aug 22", "Fire"  },
    { "Virgo",       "Aug 23 - Sep 22", "Earth" },
    { "Libra",       "Sep 23 - Oct 22", "Air"   },
    { "Scorpio",     "Oct 23 - Nov 21", "Water" },
    { "Sagittarius", "Nov 22 - Dec 21", "Fire"  },
    { "Capricorn",   "Dec 22 - Jan 19", "Earth" },
    { "Aquarius",    "Jan 20 - Feb 18", "Air"   },
    { "Pisces",      "Feb 19 - Mar 20", "Water" },
};
#define NSIGNS ((int)(sizeof(kSigns) / sizeof(kSigns[0])))

/* Eight short, deterministic one-line daily readings. */
static const char* const kHoroscopes[] = {
    "A bright idea arrives before noon; share it with a friend.",
    "Patience pays off as a stalled matter resumes this afternoon.",
    "Expect a pleasant surprise from an old acquaintance.",
    "Focus on one task at a time and the day will reward you.",
    "A short walk clears the mind and sparks fresh creativity.",
    "Financial caution today leads to a steadier week ahead.",
    "Your words carry extra weight today; choose them kindly.",
    "An unexpected message sets the evening in a warm direction."
};
#define NHOROSCOPES ((unsigned)(sizeof(kHoroscopes) / sizeof(kHoroscopes[0])))

static int find_sign(const char* name)
{
    int i;
    if (!name[0]) return -1;
    for (i = 0; i < NSIGNS; ++i)
        if (word_is(name, kSigns[i].name)) return i;
    return -1;
}

/* Emit: ZD: <name> <dates> element=<element> */
static void emit_sign(int index)
{
    char line[128];
    unsigned n = 0;
    const struct Sign* s = &kSigns[index];
    n = copy_text(line, n, "ZD: ");
    n = copy_text(line, n, s->name);
    n = copy_text(line, n, " ");
    n = copy_text(line, n, s->dates);
    n = copy_text(line, n, " element=");
    n = copy_text(line, n, s->element);
    line[n] = 0;
    marker(line);
}

/* Pick a reading from a FIXED seed mixed with the sign name (LCG), so the
 * daily fortune is deterministic and reproducible across boots. */
static unsigned horoscope_index(int sign_index)
{
    unsigned seed = 0x5EED1234u;
    const char* name = kSigns[sign_index].name;
    while (*name)
        {
        seed = seed * 33u + (unsigned char)*name;
        ++name;
        }
    seed ^= ((unsigned)sign_index + 1u) * 0x9E3779B9u;
    seed = seed * 1664525u + 1013904223u;   /* numerical-recipes LCG */
    return (seed >> 8) % NHOROSCOPES;
}

/* Emit: ZD: horoscope for <name>: <text> */
static void emit_horoscope(int sign_index)
{
    char line[160];
    unsigned n = 0;
    const struct Sign* s = &kSigns[sign_index];
    n = copy_text(line, n, "ZD: horoscope for ");
    n = copy_text(line, n, s->name);
    n = copy_text(line, n, ": ");
    n = copy_text(line, n, kHoroscopes[horoscope_index(sign_index)]);
    line[n] = 0;
    marker(line);
}

static void list_signs(void)
{
    int i;
    marker("ZD: 12 ZODIAC SIGNS");
    for (i = 0; i < NSIGNS; ++i)
        emit_sign(i);
}

static void show_sign(const char* arg)
{
    char name[24];
    int index;
    take_word(&arg, name, sizeof(name));
    index = find_sign(name);
    if (index < 0) { marker("ZD: sign: unknown sign"); return; }
    emit_sign(index);
}

static void show_horoscope(const char* arg)
{
    char name[24];
    int index;
    take_word(&arg, name, sizeof(name));
    index = find_sign(name);
    if (index < 0) { marker("ZD: horoscope: unknown sign"); return; }
    emit_horoscope(index);
}

/* Built-in selftest prints the Aries fact row and its horoscope. */
static void selftest(void)
{
    emit_sign(0);                 /* Aries */
    emit_horoscope(0);            /* Aries daily reading */
    marker("ZD: selftest OK");
}

static void help(void)
{
    marker("ZD: commands: sign <name>  list  horoscope <name>  selftest  help  quit");
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    char word[24];
    const char* p = input;
    take_word(&p, word, sizeof(word));
    if      (word_is(word, "sign"))      show_sign(p);
    else if (word_is(word, "list"))      list_signs();
    else if (word_is(word, "horoscope")) show_horoscope(p);
    else if (word_is(word, "selftest"))  selftest();
    else if (word_is(word, "help") || word_is(word, "?"))
        help();
    else if (word_is(word, "quit") || word_is(word, "q"))
        return 1;
    else marker("ZD: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("ZD: console busy"); return 1; }

    fn = *(unsigned*)(void*)&zodiac_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    /* Startup guarantees the ZD markers even with no console input. */
    marker("ZD: ZODIAC");
    marker(s_bound ? "ZD: EUSER ord 915 bound"
                   : "ZD: using DebugPrint marker path");
    emit_sign(0);
    emit_horoscope(0);

    for (;;)
        {
        print("ZD> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("ZD: DONE");
    con_release();
    return 0;
}

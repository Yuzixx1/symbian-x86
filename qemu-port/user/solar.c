/*
 * solar.c -- a SOLAR SYSTEM fact viewer for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/calc.c, user/filemgr.c and user/units.c).
 *
 * The viewer holds a built-in table of the eight planets with four facts each
 * (diameter in km, mean distance from the Sun in millions of km, length of a
 * day in hours, and number of known moons).  Console commands are read from
 * PortCon through read_line():
 *   planets          list the eight planet names
 *   info <name>      print one planet's facts
 *   selftest         print the Earth and Jupiter fact rows
 *   help / quit      help text / leave the viewer
 * Markers all start with "SO: " so the QEMU fixture can assert them.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*solar_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&solar_printf_slot;
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

static unsigned append_uint(char* dst, unsigned n, unsigned value)
{
    char digits[12];
    int d = 0;
    if (!value) digits[d++] = '0';
    while (value)
        {
        digits[d++] = (char)('0' + (value % 10));
        value /= 10;
        }
    while (d) dst[n++] = digits[--d];
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

/* ------------------------------------------------------------- planet data
 * diameter km | mean distance from Sun (million km) | day length h | moons.
 * Integer approximations from standard NASA planetary fact-sheet values. */
struct Planet {
    const char* name;
    unsigned diameter;
    unsigned dist;
    unsigned day;
    unsigned moons;
};

static const struct Planet kPlanets[] = {
    { "Mercury",  4879u,   58u, 1408u,   0u },
    { "Venus",   12104u,  108u, 5832u,   0u },
    { "Earth",   12742u,  150u,   24u,   1u },
    { "Mars",     6779u,  228u,   25u,   2u },
    { "Jupiter",139820u,  778u,   10u,  95u },
    { "Saturn", 116460u, 1434u,   11u, 146u },
    { "Uranus",  50724u, 2873u,   17u,  28u },
    { "Neptune", 49244u, 4495u,   16u,  16u },
};
#define NPLANETS ((int)(sizeof(kPlanets) / sizeof(kPlanets[0])))

/* Emit: SO: <name> diameter=<N>km dist=<N>Mkm day=<N>h moons=<N> */
static void emit_planet(const struct Planet* p)
{
    char line[192];
    unsigned n = 0;
    n = copy_text(line, n, "SO: ");
    n = copy_text(line, n, p->name);
    n = copy_text(line, n, " diameter=");
    n = append_uint(line, n, p->diameter);
    n = copy_text(line, n, "km dist=");
    n = append_uint(line, n, p->dist);
    n = copy_text(line, n, "Mkm day=");
    n = append_uint(line, n, p->day);
    n = copy_text(line, n, "h moons=");
    n = append_uint(line, n, p->moons);
    line[n] = 0;
    marker(line);
}

/* Emit: SO: planets: Mercury Venus Earth Mars Jupiter Saturn Uranus Neptune */
static void list_planets(void)
{
    char line[192];
    unsigned n = 0;
    int i;
    n = copy_text(line, n, "SO: planets:");
    for (i = 0; i < NPLANETS; ++i)
        {
        n = copy_text(line, n, " ");
        n = copy_text(line, n, kPlanets[i].name);
        }
    line[n] = 0;
    marker(line);
}

static void show_info(const char* arg)
{
    char name[32];
    int i;
    take_word(&arg, name, sizeof(name));
    if (!name[0]) { marker("SO: info: no planet"); return; }
    for (i = 0; i < NPLANETS; ++i)
        {
        if (word_is(name, kPlanets[i].name))
            {
            emit_planet(&kPlanets[i]);
            return;
            }
        }
    marker("SO: info: unknown planet");
}

/* Deterministic suite through the very same formatter used by `info`:
 * print the Earth and Jupiter fact rows. */
static void selftest(void)
{
    emit_planet(&kPlanets[2]);   /* Earth */
    emit_planet(&kPlanets[4]);   /* Jupiter */
    marker("SO: selftest OK");
}

static void help(void)
{
    marker("SO: commands: planets  info <name>  selftest  help  quit");
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    char word[32];
    const char* p = input;
    take_word(&p, word, sizeof(word));
    if      (word_is(word, "planets"))  list_planets();
    else if (word_is(word, "info"))     show_info(p);
    else if (word_is(word, "selftest")) selftest();
    else if (word_is(word, "help") || word_is(word, "?"))
        help();
    else if (word_is(word, "quit") || word_is(word, "q"))
        return 1;
    else marker("SO: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("SO: console busy"); return 1; }

    fn = *(unsigned*)(void*)&solar_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    /* Startup guarantees the SO markers even with no console input. */
    marker("SO: SOLAR SYSTEM");
    marker(s_bound ? "SO: EUSER ord 915 bound"
                   : "SO: using DebugPrint marker path");
    list_planets();

    for (;;)
        {
        print("SO> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("SO: DONE");
    con_release();
    return 0;
}

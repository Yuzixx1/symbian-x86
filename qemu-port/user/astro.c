/*
 * astro.c -- an ASTRONOMY / deep-sky fact viewer for the Sybian REAL port
 *            (PE2 E32, "astro").
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/solar.c, user/ocean.c, user/periodic.c and user/filemgr.c).
 *
 * The viewer holds a built-in table of six deep-sky objects (name, object
 * type, distance in light years, constellation).  Console commands are read
 * from PortCon through read_line():
 *   list             list the six object names
 *   info <name>      print one object's facts (case-insensitive; full name
 *                    or just its first word, e.g. "polaris")
 *   selftest         print the Andromeda Galaxy and Polaris fact rows
 *   help / quit      help text / leave the viewer
 * Markers all start with "AS: " so the QEMU fixture can assert them.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*astro_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&astro_printf_slot;
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

static char lower(char c)
{
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return c;
}

/* Case-insensitive compare of a NUL-terminated word to a literal. */
static int word_is(const char* word, const char* literal)
{
    while (*word && *literal)
        {
        if (lower(*word++) != lower(*literal++)) return 0;
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

/* First word of a name into a small buffer (used for short aliases). */
static void first_word(const char* s, char* out, unsigned cap)
{
    take_word(&s, out, cap);
}

/* Case-insensitive full-name match.  Leading/trailing spaces in the argument
 * are tolerated so "  north star polaris  " matches "North Star Polaris". */
static int name_is(const char* arg, const char* name)
{
    while (*arg == ' ') ++arg;
    while (*name && *arg)
        {
        if (lower(*arg++) != lower(*name++)) return 0;
        }
    while (*arg == ' ') ++arg;
    return (*arg == 0 && *name == 0) ? 1 : 0;
}

/* -------------------------------------------------------------- sky data
 * name | object type | distance (light years) | constellation.
 * Distances are rounded values from standard astronomical references. */
struct Object {
    const char* name;
    const char* type;
    unsigned dist;
    const char* constellation;
};

static const struct Object kObjects[] = {
    { "Andromeda Galaxy",   "Galaxy",      2530000u, "Andromeda"   },
    { "Orion Nebula",       "Nebula",         1344u, "Orion"       },
    { "Pleiades",           "Star cluster",    444u, "Taurus"      },
    { "Crab Nebula",        "Nebula",         6500u, "Taurus"      },
    { "Milky Way",          "Galaxy",        26000u, "Sagittarius" }, /* centre */
    { "North Star Polaris", "Star",            433u, "Ursa Minor"  },
};
#define NOBJECTS ((int)(sizeof(kObjects) / sizeof(kObjects[0])))

/* Emit: AS: <name> type=<...> dist=<N>ly constellation=<...> */
static void emit_object(const struct Object* o)
{
    char line[192];
    unsigned n = 0;
    n = copy_text(line, n, "AS: ");
    n = copy_text(line, n, o->name);
    n = copy_text(line, n, " type=");
    n = copy_text(line, n, o->type);
    n = copy_text(line, n, " dist=");
    n = append_uint(line, n, o->dist);
    n = copy_text(line, n, "ly constellation=");
    n = copy_text(line, n, o->constellation);
    line[n] = 0;
    marker(line);
}

/* Emit: AS: objects: Andromeda Galaxy, Orion Nebula, ... */
static void list_objects(void)
{
    char line[256];
    unsigned n = 0;
    int i;
    n = copy_text(line, n, "AS: objects:");
    for (i = 0; i < NOBJECTS; ++i)
        {
        n = copy_text(line, n, (i == 0) ? " " : ", ");
        n = copy_text(line, n, kObjects[i].name);
        }
    line[n] = 0;
    marker(line);
}

static void show_info(const char* arg)
{
    char alias[32];
    int i;
    first_word(arg, alias, sizeof(alias));
    if (!alias[0]) { marker("AS: info: no name"); return; }
    for (i = 0; i < NOBJECTS; ++i)
        {
        char first[32];
        first_word(kObjects[i].name, first, sizeof(first));
        if (name_is(arg, kObjects[i].name) || word_is(alias, first))
            {
            emit_object(&kObjects[i]);
            return;
            }
        }
    marker("AS: info: unknown object");
}

/* Deterministic suite through the very same formatter used by `info`:
 * print the Andromeda Galaxy and Polaris fact rows. */
static void selftest(void)
{
    emit_object(&kObjects[0]);   /* Andromeda Galaxy */
    emit_object(&kObjects[5]);   /* North Star Polaris */
    marker("AS: selftest OK");
}

static void help(void)
{
    marker("AS: commands: list  info <name>  selftest  help  quit");
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    char word[32];
    const char* p = input;
    take_word(&p, word, sizeof(word));
    if      (word_is(word, "list"))      list_objects();
    else if (word_is(word, "objects"))   list_objects();
    else if (word_is(word, "info"))      show_info(p);
    else if (word_is(word, "selftest"))  selftest();
    else if (word_is(word, "help") || word_is(word, "?"))
        help();
    else if (word_is(word, "quit") || word_is(word, "q"))
        return 1;
    else marker("AS: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("AS: console busy"); return 1; }

    fn = *(unsigned*)(void*)&astro_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    /* Startup guarantees the AS markers even with no console input. */
    marker("AS: ASTRONOMY");
    marker(s_bound ? "AS: EUSER ord 915 bound"
                   : "AS: using DebugPrint marker path");
    list_objects();

    for (;;)
        {
        print("AS> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("AS: DONE");
    con_release();
    return 0;
}

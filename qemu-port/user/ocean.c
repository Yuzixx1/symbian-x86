/*
 * ocean.c -- an OCEAN / GEOGRAPHY fact viewer for the Sybian REAL port (PE2
 *            E32, "ocean").
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/solar.c, user/periodic.c, user/calc.c and user/filemgr.c).
 *
 * The viewer holds a built-in table of the five oceans (name, area in millions
 * of km2, maximum depth in metres) and the five highest mountains (name, height
 * in metres, mountain range).  Console commands are read from PortCon through
 * read_line():
 *   oceans           list the five ocean names
 *   mountains        list the five mountain names
 *   info <name>      print one ocean's or mountain's facts (case-insensitive)
 *   selftest         print the Pacific ocean and Everest fact rows
 *   help / quit      help text / leave the viewer
 * Markers all start with "OC: " so the QEMU fixture can assert them.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*ocean_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&ocean_printf_slot;
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

/* -------------------------------------------------------------- ocean data
 * name | surface area (million km2) | maximum depth (metres).
 * Integer approximations from standard geographic reference values. */
struct Ocean {
    const char* name;
    unsigned area;
    unsigned depth;
};

static const struct Ocean kOceans[] = {
    { "Pacific",  165u, 10994u },  /* Challenger Deep, Mariana Trench */
    { "Atlantic", 106u,  8605u },  /* Puerto Rico Trench */
    { "Indian",    70u,  7290u },  /* Java (Sunda) Trench */
    { "Southern",  20u,  7235u },  /* South Sandwich Trench */
    { "Arctic",    15u,  5550u },  /* Molloy Deep */
};
#define NOCEANS ((int)(sizeof(kOceans) / sizeof(kOceans[0])))

/* ----------------------------------------------------------- mountain data
 * name | height above sea level (metres) | mountain range. */
struct Mountain {
    const char* name;
    unsigned height;
    const char* range;
};

static const struct Mountain kMountains[] = {
    { "Everest",      8849u, "Himalayas"  },
    { "K2",           8611u, "Karakoram"  },
    { "Kangchenjunga",8586u, "Himalayas"  },
    { "Lhotse",       8516u, "Himalayas"  },
    { "Makalu",       8485u, "Himalayas"  },
};
#define NMOUNTAINS ((int)(sizeof(kMountains) / sizeof(kMountains[0])))

/* Emit: OC: <name> area=<N>Mkm2 depth=<N>m */
static void emit_ocean(const struct Ocean* o)
{
    char line[128];
    unsigned n = 0;
    n = copy_text(line, n, "OC: ");
    n = copy_text(line, n, o->name);
    n = copy_text(line, n, " area=");
    n = append_uint(line, n, o->area);
    n = copy_text(line, n, "Mkm2 depth=");
    n = append_uint(line, n, o->depth);
    n = copy_text(line, n, "m");
    line[n] = 0;
    marker(line);
}

/* Emit: OC: <name> height=<N>m range=<...> */
static void emit_mountain(const struct Mountain* m)
{
    char line[128];
    unsigned n = 0;
    n = copy_text(line, n, "OC: ");
    n = copy_text(line, n, m->name);
    n = copy_text(line, n, " height=");
    n = append_uint(line, n, m->height);
    n = copy_text(line, n, "m range=");
    n = copy_text(line, n, m->range);
    line[n] = 0;
    marker(line);
}

/* Emit: OC: oceans: Pacific Atlantic Indian Southern Arctic */
static void list_oceans(void)
{
    char line[192];
    unsigned n = 0;
    int i;
    n = copy_text(line, n, "OC: oceans:");
    for (i = 0; i < NOCEANS; ++i)
        {
        n = copy_text(line, n, " ");
        n = copy_text(line, n, kOceans[i].name);
        }
    line[n] = 0;
    marker(line);
}

/* Emit: OC: mountains: Everest K2 Kangchenjunga Lhotse Makalu */
static void list_mountains(void)
{
    char line[192];
    unsigned n = 0;
    int i;
    n = copy_text(line, n, "OC: mountains:");
    for (i = 0; i < NMOUNTAINS; ++i)
        {
        n = copy_text(line, n, " ");
        n = copy_text(line, n, kMountains[i].name);
        }
    line[n] = 0;
    marker(line);
}

static void show_info(const char* arg)
{
    char name[32];
    int i;
    take_word(&arg, name, sizeof(name));
    if (!name[0]) { marker("OC: info: no name"); return; }
    for (i = 0; i < NOCEANS; ++i)
        {
        if (word_is(name, kOceans[i].name))
            {
            emit_ocean(&kOceans[i]);
            return;
            }
        }
    for (i = 0; i < NMOUNTAINS; ++i)
        {
        if (word_is(name, kMountains[i].name))
            {
            emit_mountain(&kMountains[i]);
            return;
            }
        }
    marker("OC: info: unknown name");
}

/* Deterministic suite through the very same formatters used by `info`:
 * print the Pacific ocean and Everest fact rows. */
static void selftest(void)
{
    emit_ocean(&kOceans[0]);       /* Pacific */
    emit_mountain(&kMountains[0]); /* Everest */
    marker("OC: selftest OK");
}

static void help(void)
{
    marker("OC: commands: oceans  mountains  info <name>  selftest  help  quit");
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    char word[32];
    const char* p = input;
    take_word(&p, word, sizeof(word));
    if      (word_is(word, "oceans"))    list_oceans();
    else if (word_is(word, "mountains")) list_mountains();
    else if (word_is(word, "info"))      show_info(p);
    else if (word_is(word, "selftest"))  selftest();
    else if (word_is(word, "help") || word_is(word, "?"))
        help();
    else if (word_is(word, "quit") || word_is(word, "q"))
        return 1;
    else marker("OC: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("OC: console busy"); return 1; }

    fn = *(unsigned*)(void*)&ocean_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    /* Startup guarantees the OC markers even with no console input. */
    marker("OC: OCEAN & GEOGRAPHY");
    marker(s_bound ? "OC: EUSER ord 915 bound"
                   : "OC: using DebugPrint marker path");
    list_oceans();
    list_mountains();

    for (;;)
        {
        print("OC> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("OC: DONE");
    con_release();
    return 0;
}

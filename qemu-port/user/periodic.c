/*
 * periodic.c -- a PERIODIC TABLE viewer for the Sybian REAL port (PE2 E32,
 *               "periodic").
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/solar.c, user/calc.c, user/filemgr.c and user/units.c).
 *
 * The viewer holds a built-in table of the first 20 elements (symbol, name,
 * atomic number, atomic mass) plus iron as one extra fact row so the built-in
 * selftest can print H, C, O and Fe.  Console commands are read from PortCon
 * through read_line():
 *   list             list the first 20 element symbols
 *   info <symbol>    print one element's facts (case-insensitive)
 *   selftest         print the H, C, O and Fe fact rows
 *   help / quit      help text / leave the viewer
 * Markers all start with "PT: " so the QEMU fixture can assert them.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*periodic_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&periodic_printf_slot;
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
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
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

/* ------------------------------------------------------------ element data
 * symbol | name | atomic number Z | standard atomic mass (string, no float).
 * The first 20 entries are Z=1..20; iron (Z=26) is an extra fact row. */
struct Element {
    const char* symbol;
    const char* name;
    unsigned z;
    const char* mass;
};

static const struct Element kElements[] = {
    { "H",  "Hydrogen",    1u,  "1.008"  },
    { "He", "Helium",      2u,  "4.0026" },
    { "Li", "Lithium",     3u,  "6.94"   },
    { "Be", "Beryllium",   4u,  "9.0122" },
    { "B",  "Boron",       5u,  "10.81"  },
    { "C",  "Carbon",      6u,  "12.011" },
    { "N",  "Nitrogen",    7u,  "14.007" },
    { "O",  "Oxygen",      8u,  "15.999" },
    { "F",  "Fluorine",    9u,  "18.998" },
    { "Ne", "Neon",       10u,  "20.180" },
    { "Na", "Sodium",     11u,  "22.990" },
    { "Mg", "Magnesium",  12u,  "24.305" },
    { "Al", "Aluminium",  13u,  "26.982" },
    { "Si", "Silicon",    14u,  "28.085" },
    { "P",  "Phosphorus", 15u,  "30.974" },
    { "S",  "Sulfur",     16u,  "32.06"  },
    { "Cl", "Chlorine",   17u,  "35.45"  },
    { "Ar", "Argon",      18u,  "39.948" },
    { "K",  "Potassium",  19u,  "39.098" },
    { "Ca", "Calcium",    20u,  "40.078" },
    { "Fe", "Iron",       26u,  "55.845" },  /* extra fact row for selftest */
};
#define NELEMENTS ((int)(sizeof(kElements) / sizeof(kElements[0])))
#define NFIRST20  20

/* Emit: PT: <sym> <name> Z=<n> mass=<m> */
static void emit_element(const struct Element* e)
{
    char line[128];
    unsigned n = 0;
    n = copy_text(line, n, "PT: ");
    n = copy_text(line, n, e->symbol);
    n = copy_text(line, n, " ");
    n = copy_text(line, n, e->name);
    n = copy_text(line, n, " Z=");
    n = append_uint(line, n, e->z);
    n = copy_text(line, n, " mass=");
    n = copy_text(line, n, e->mass);
    line[n] = 0;
    marker(line);
}

/* Emit: PT: elements: H He Li ... Ca */
static void list_elements(void)
{
    char line[192];
    unsigned n = 0;
    int i;
    n = copy_text(line, n, "PT: elements:");
    for (i = 0; i < NFIRST20; ++i)
        {
        n = copy_text(line, n, " ");
        n = copy_text(line, n, kElements[i].symbol);
        }
    line[n] = 0;
    marker(line);
}

static void show_info(const char* arg)
{
    char symbol[16];
    int i;
    take_word(&arg, symbol, sizeof(symbol));
    if (!symbol[0]) { marker("PT: info: no element"); return; }
    for (i = 0; i < NELEMENTS; ++i)
        {
        if (word_is(symbol, kElements[i].symbol))
            {
            emit_element(&kElements[i]);
            return;
            }
        }
    marker("PT: info: unknown element");
}

/* Deterministic suite through the very same formatter used by `info`:
 * print the H, C, O and Fe fact rows. */
static void selftest(void)
{
    emit_element(&kElements[0]);   /* H  */
    emit_element(&kElements[5]);   /* C  */
    emit_element(&kElements[7]);   /* O  */
    emit_element(&kElements[20]);  /* Fe */
    marker("PT: selftest OK");
}

static void help(void)
{
    marker("PT: commands: list  info <symbol>  selftest  help  quit");
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    char word[32];
    const char* p = input;
    take_word(&p, word, sizeof(word));
    if      (word_is(word, "list"))     list_elements();
    else if (word_is(word, "info"))     show_info(p);
    else if (word_is(word, "selftest")) selftest();
    else if (word_is(word, "help") || word_is(word, "?"))
        help();
    else if (word_is(word, "quit") || word_is(word, "q"))
        return 1;
    else marker("PT: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("PT: console busy"); return 1; }

    fn = *(unsigned*)(void*)&periodic_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    /* Startup guarantees the PT markers even with no console input. */
    marker("PT: PERIODIC TABLE");
    marker(s_bound ? "PT: EUSER ord 915 bound"
                   : "PT: using DebugPrint marker path");
    list_elements();

    for (;;)
        {
        print("PT> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("PT: DONE");
    con_release();
    return 0;
}

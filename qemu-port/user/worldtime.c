/*
 * worldtime.c -- a WORLD TIMEZONE clock for the Sybian REAL port
 *                (PE2 E32, "worldtime").
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/clock.c, user/units.c and user/astro.c).
 *
 * A built-in table holds eight cities and their fixed UTC offsets in minutes
 * (minutes, not whole hours, so Mumbai's half-hour +5:30 is exact).  Console
 * commands are read from PortCon through read_line():
 *   list             list the eight cities with their UTC offsets
 *   now <city>       show that city's local HH:MM (case-insensitive name)
 *   selftest         print the London, Tokyo and Mumbai local times
 *   help / quit      help text / leave the clock
 *
 * Local time is derived from the genuine EExecTimeNow executive (exec 68,
 * reached via time_now()).  time_now() returns UTC seconds since 2000-01-01;
 * adding the 1970->2000 epoch offset and reducing modulo 86400 gives the UTC
 * time of day (the same RTC source used by clock.c and ntpc.c).  The city's
 * offset in minutes is then added, with wraparound at midnight.
 *
 * All markers start with "WT: " so the QEMU fixture can assert them.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*worldtime_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&worldtime_printf_slot;
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

static unsigned append_2(char* dst, unsigned n, unsigned value)
{
    dst[n++] = (char)('0' + (value / 10u) % 10u);
    dst[n++] = (char)('0' + (value % 10u));
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

/* Full-name match, tolerating leading/trailing spaces in the user argument. */
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

/* Last whitespace-delimited word of a string (alias for two-word cities). */
static void last_word(const char* s, char* out, unsigned cap)
{
    const char* p = s;
    const char* last = s;
    while (*p)
        {
        while (*p == ' ') ++p;
        if (!*p) break;
        last = p;
        while (*p && *p != ' ') ++p;
        }
    take_word(&last, out, cap);
}

/* -------------------------------------------------------------- city table
 * Fixed UTC offsets in minutes (negative = west of Greenwich).  Minutes keep
 * the half-hour zones (Mumbai +5:30) exact without any floating point. */
struct City {
    const char* name;
    int offset_min;
};

static const struct City kCities[] = {
    { "New York", -300 },  /* UTC-5    */
    { "London",      0 },  /* UTC+0    */
    { "Paris",      60 },  /* UTC+1    */
    { "Moscow",    180 },  /* UTC+3    */
    { "Dubai",     240 },  /* UTC+4    */
    { "Mumbai",    330 },  /* UTC+5:30 */
    { "Tokyo",     540 },  /* UTC+9    */
    { "Sydney",    600 },  /* UTC+10   */
};
#define NCITIES ((int)(sizeof(kCities) / sizeof(kCities[0])))

/* Append "UTC+9" / "UTC-5" / "UTC+5:30" for an offset given in minutes. */
static unsigned append_offset(char* dst, unsigned n, int offset_min)
{
    unsigned hours, mins;
    n = copy_text(dst, n, "UTC");
    if (offset_min < 0) { dst[n++] = '-'; offset_min = -offset_min; }
    else                dst[n++] = '+';
    hours = (unsigned)(offset_min / 60);
    mins  = (unsigned)(offset_min % 60);
    n = append_uint(dst, n, hours);
    if (mins)
        {
        dst[n++] = ':';
        n = append_2(dst, n, mins);
        }
    return n;
}

/* UTC time-of-day in seconds [0,86400) from the genuine exec 68 RTC, or -1
 * when the executive fails.  time_now() is seconds since 2000-01-01; adding
 * 946684800 (Unix 1970->2000) yields Unix seconds, modulo one day. */
static int utc_now(void)
{
    int secs = time_now();
    if (secs <= 0) return -1;
    return (int)((((unsigned)secs + 946684800u) % 86400u));
}

static unsigned local_seconds(int utc, int offset_min)
{
    int local = utc + offset_min * 60;
    while (local < 0)     local += 86400;
    while (local >= 86400) local -= 86400;
    return (unsigned)local;
}

/* Emit: WT: <city> UTC<+/-N> = <HH:MM> */
static void emit_city(const struct City* c, int utc)
{
    char line[128];
    unsigned n = 0;
    unsigned local = local_seconds(utc, c->offset_min);
    n = copy_text(line, n, "WT: ");
    n = copy_text(line, n, c->name);
    n = copy_text(line, n, " ");
    n = append_offset(line, n, c->offset_min);
    n = copy_text(line, n, " = ");
    n = append_2(line, n, local / 3600u);
    line[n++] = ':';
    n = append_2(line, n, (local / 60u) % 60u);
    line[n] = 0;
    marker(line);
}

static void list_cities(void)
{
    char line[256];
    unsigned n = 0;
    int i;
    n = copy_text(line, n, "WT: cities:");
    for (i = 0; i < NCITIES; ++i)
        {
        n = copy_text(line, n, " ");
        n = copy_text(line, n, kCities[i].name);
        n = copy_text(line, n, "(");
        n = append_offset(line, n, kCities[i].offset_min);
        n = copy_text(line, n, ")");
        }
    line[n] = 0;
    marker(line);
}

/* Match a city by full name, first word or last word (e.g. "new york",
 * "york", "tokyo").  Returns its index, or -1 when nothing matches. */
static int find_city(const char* arg)
{
    char first[32], last[32];
    const char* p = arg;
    int i;
    take_word(&p, first, sizeof(first));
    last_word(arg, last, sizeof(last));
    if (!first[0]) return -1;
    for (i = 0; i < NCITIES; ++i)
        {
        char cfirst[32], clast[32];
        const char* q = kCities[i].name;
        take_word(&q, cfirst, sizeof(cfirst));
        last_word(kCities[i].name, clast, sizeof(clast));
        if (name_is(arg, kCities[i].name)
            || word_is(first, cfirst) || word_is(last, clast))
            return i;
        }
    return -1;
}

static void do_now(const char* arg)
{
    int utc, idx;
    while (*arg == ' ') ++arg;
    if (!*arg)         { marker("WT: now: no city"); return; }
    idx = find_city(arg);
    if (idx < 0)       { marker("WT: now: unknown city"); return; }
    utc = utc_now();
    if (utc < 0)       { marker("WT: time unavailable (EExecTimeNow 68)"); return; }
    emit_city(&kCities[idx], utc);
}

/* Deterministic suite through the very same formatter used by `now`: the
 * London, Tokyo and Mumbai local times from one UTC snapshot. */
static void selftest(void)
{
    int utc = utc_now();
    if (utc < 0) { marker("WT: time unavailable (EExecTimeNow 68)"); return; }
    emit_city(&kCities[1], utc);   /* London */
    emit_city(&kCities[6], utc);   /* Tokyo  */
    emit_city(&kCities[5], utc);   /* Mumbai */
    marker("WT: selftest OK");
}

static void help(void)
{
    marker("WT: commands: list  now <city>  selftest  help  quit");
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    char word[32];
    const char* p = input;
    take_word(&p, word, sizeof(word));
    if      (word_is(word, "list"))      list_cities();
    else if (word_is(word, "cities"))    list_cities();
    else if (word_is(word, "now"))       do_now(p);
    else if (word_is(word, "selftest"))  selftest();
    else if (word_is(word, "help") || word_is(word, "?"))
        help();
    else if (word_is(word, "quit") || word_is(word, "q"))
        return 1;
    else marker("WT: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("WT: console busy"); return 1; }

    fn = *(unsigned*)(void*)&worldtime_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    marker("WT: WORLD CLOCK");
    marker(s_bound ? "WT: EUSER ord 915 bound"
                   : "WT: using DebugPrint marker path");
    list_cities();

    for (;;)
        {
        print("WT> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("WT: DONE");
    con_release();
    return 0;
}

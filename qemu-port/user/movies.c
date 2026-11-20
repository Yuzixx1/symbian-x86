/*
 * movies.c -- a small MOVIE DATABASE end-user app for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...) (euseru.def).  ld defines
 * the IAT slot at codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915
 * emits the PE2 import section and the loader overwrites the slot with the
 * resolved export address before the process resumes.  Markers go through
 * that ordinal, with a DebugPrint fallback so the trace is always visible
 * on the serial console.
 *
 * The database is a built-in table of eight classic films (title, year,
 * director, genre).  Console commands (list / info <title-substring> /
 * genre <name> / selftest / help / quit) arrive through PortCon read_line().
 * The built-in selftest lists the whole table and queries one genre, so a
 * scripted "selftest" command exercises lookup and display without input.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*movies_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&movies_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------ small string helpers (freestanding) ------- */

static unsigned append_str(char* out, unsigned n, const char* text)
{
    while (*text) out[n++] = *text++;
    out[n] = 0;
    return n;
}

static unsigned append_uint(char* out, unsigned n, unsigned value)
{
    char digits[12];
    int d = 0;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10); value /= 10; }
    while (d) out[n++] = digits[--d];
    out[n] = 0;
    return n;
}

static char lower_char(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* Case-insensitive compare of a command word to a literal. */
static int word_is(const char* word, unsigned length, const char* literal)
{
    unsigned i = 0;
    while (i < length && literal[i])
        {
        if (lower_char(word[i]) != lower_char(literal[i])) return 0;
        ++i;
        }
    return (i == length && literal[i] == 0) ? 1 : 0;
}

/* Case-insensitive equality of two NUL-terminated strings. */
static int str_eq_ci(const char* a, const char* b)
{
    while (*a && *b)
        {
        if (lower_char(*a++) != lower_char(*b++)) return 0;
        }
    return (*a == 0 && *b == 0) ? 1 : 0;
}

/* Case-insensitive substring search (for "info <title-substring>"). */
static int contains_ci(const char* haystack, const char* needle)
{
    if (!*needle) return 1;
    while (*haystack)
        {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && lower_char(*h) == lower_char(*n)) { ++h; ++n; }
        if (!*n) return 1;
        ++haystack;
        }
    return 0;
}

static const char* skip_spaces(const char* p)
{
    while (*p == ' ') ++p;
    return p;
}

/* Whole argument (may contain spaces), with trailing blanks removed. */
static void copy_arg(char* out, unsigned cap, const char* text)
{
    unsigned n = 0;
    text = skip_spaces(text);
    while (*text && n + 1 < cap) out[n++] = *text++;
    while (n && out[n - 1] == ' ') --n;
    out[n] = 0;
}

/* ------------------------------ the movie table --------------------------- */

struct Movie {
    const char* title;
    unsigned year;
    const char* director;
    const char* genre;
};

static const struct Movie kMovies[] = {
    { "Casablanca",            1942, "Michael Curtiz",       "Drama"   },
    { "Citizen Kane",          1941, "Orson Welles",         "Drama"   },
    { "The Godfather",         1972, "Francis Ford Coppola", "Crime"   },
    { "Singin' in the Rain",   1952, "Gene Kelly",           "Musical" },
    { "2001: A Space Odyssey", 1968, "Stanley Kubrick",      "Sci-Fi"  },
    { "Psycho",                1960, "Alfred Hitchcock",     "Thriller"},
    { "The Wizard of Oz",      1939, "Victor Fleming",       "Fantasy" },
    { "Star Wars",             1977, "George Lucas",         "Sci-Fi"  }
};
#define NMOVIES ((unsigned)(sizeof(kMovies) / sizeof(kMovies[0])))

static char line[128];

/* Format one entry as "MV: <title> (<year>) - <director> [<genre>]". */
static void print_movie(unsigned i)
{
    char msg[128];
    unsigned n = append_str(msg, 0, "MV: ");
    n = append_str(msg, n, kMovies[i].title);
    n = append_str(msg, n, " (");
    n = append_uint(msg, n, kMovies[i].year);
    n = append_str(msg, n, ") - ");
    n = append_str(msg, n, kMovies[i].director);
    n = append_str(msg, n, " [");
    n = append_str(msg, n, kMovies[i].genre);
    append_str(msg, n, "]");
    marker(msg);
}

/* Display every film; return the number displayed. */
static unsigned list_movies(void)
{
    unsigned i;
    for (i = 0; i < NMOVIES; ++i) print_movie(i);
    return NMOVIES;
}

/* Display the films whose title contains the given (case-insensitive) text. */
static unsigned find_title(const char* needle)
{
    unsigned i, count = 0;
    for (i = 0; i < NMOVIES; ++i)
        if (contains_ci(kMovies[i].title, needle)) { print_movie(i); ++count; }
    return count;
}

/* Display the films in the named genre (case-insensitive exact match). */
static unsigned find_genre(const char* name)
{
    unsigned i, count = 0;
    for (i = 0; i < NMOVIES; ++i)
        if (str_eq_ci(kMovies[i].genre, name)) { print_movie(i); ++count; }
    return count;
}

static void marker_count(const char* tag, unsigned value)
{
    char msg[64];
    unsigned n = append_str(msg, 0, tag);
    n = append_uint(msg, n, value);
    append_str(msg, n, " found");
    marker(msg);
}

/* ------------------------------ commands ---------------------------------- */

static void do_info(const char* arg)
{
    char needle[64];
    copy_arg(needle, sizeof(needle), arg);
    if (!*needle) { marker("MV: usage: info <title-substring>"); return; }
    unsigned count = find_title(needle);
    marker_count("MV: info ", count);
}

static void do_genre(const char* arg)
{
    char name[32];
    copy_arg(name, sizeof(name), arg);
    if (!*name) { marker("MV: usage: genre <name>"); return; }
    unsigned count = find_genre(name);
    marker_count("MV: genre ", count);
}

/* List the whole database, then query one genre whose two entries are known. */
static void do_selftest(void)
{
    marker("MV: selftest start");
    unsigned all = list_movies();
    marker("MV: selftest query Sci-Fi");
    unsigned scifi = find_genre("Sci-Fi");
    marker_count("MV: genre ", scifi);
    int ok = (all == 8 && scifi == 2);
    marker(ok ? "MV: selftest OK" : "MV: selftest FAIL");
}

static void do_help(void)
{
    print("MV: commands: list  info <title-substring>  genre <name>"
          "  selftest  help  quit");
}

/* Parse "command [arguments]" from a console line.  1 means quit. */
static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if      (word_is(input, length, "list"))       list_movies();
    else if (word_is(input, length, "info"))       do_info(arg);
    else if (word_is(input, length, "genre"))      do_genre(arg);
    else if (word_is(input, length, "selftest") ||
             word_is(input, length, "test"))       do_selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))          do_help();
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))          return 1;
    else marker("MV: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("MV: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&movies_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("MV: MOVIE DATABASE");
    marker(s_bound ? "MV: EUSER ord 915 bound"
                   : "MV: using DebugPrint marker path");

    for (;;)
        {
        print("MV> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error -> done */
        if (run_command(line)) break;   /* quit */
        }

    marker("MV: DONE");
    con_release();
    return 0;
}

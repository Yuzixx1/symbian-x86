/*
 * primes.c -- a PRIME NUMBER tool for the Sybian REAL port (PE2 E32, "primes").
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/calc.c, user/filemgr.c and user/units.c).
 *
 * This is a genuine prime-number program, not a table of canned answers.  A
 * single trial-division predicate is_prime() drives every command: it rejects
 * 0/1 and evens, then tests odd divisors d with d <= n/d so the square test
 * cannot overflow a 32-bit word.  Commands are read from PortCon via
 * read_line():
 *   list <n>          print "PR: first <n>: 2 3 5 7 ..."
 *   check <num>       print "PR: <num> is prime" / "is composite"
 *   primes <lo> <hi>  print "PR: range <lo>-<hi>: <count> primes"
 *   selftest          verify first 10 primes, 97 prime and 1-30 = 10 primes
 *   help              list commands
 *   quit              exit
 * The built-in selftest prints the same markers as the live commands, so the
 * QEMU fixture can assert them over the serial console without guest input
 * beyond `selftest`.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk (like calc.c/units.c). */
extern void (*primes_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&primes_printf_slot;
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
    while (value)
        {
        digits[d++] = (char)('0' + (value % 10u));
        value /= 10u;
        }
    while (d) dst[n++] = digits[--d];
    return n;
}

static int text_equal(const char* a, const char* b)
{
    while (*a && *b)
        {
        if (*a++ != *b++) return 0;
        }
    return *a == *b;
}

/* ------------------------------------------------------------ prime engine */
static int is_prime(unsigned n)
{
    unsigned d;
    if (n < 2u) return 0;             /* 0 and 1 are not prime */
    if (n == 2u) return 1;
    if ((n & 1u) == 0u) return 0;     /* all other evens are composite */
    for (d = 3u; d <= n / d; d += 2u)
        {
        if (n % d == 0u) return 0;
        }
    return 1;
}

/* Count primes in the inclusive range [lo, hi] by probing each candidate. */
static unsigned range_count(unsigned lo, unsigned hi)
{
    unsigned count = 0u;
    unsigned n = lo;
    for (;;)
        {
        if (is_prime(n)) ++count;
        if (n == hi || n == 0xFFFFFFFFu) break;   /* stop at hi, guard wrap */
        ++n;
        }
    return count;
}

/* Render the first n primes as a space-separated list into out.  Returns the
 * number actually emitted (bounded by cap). */
static unsigned first_primes(unsigned n, char* out, unsigned cap)
{
    unsigned candidate = 2u;
    unsigned found = 0u;
    unsigned len = 0u;
    if (!cap) return 0u;
    while (found < n && len + 12u < cap)
        {
        if (is_prime(candidate))
            {
            if (found) out[len++] = ' ';
            len = append_uint(out, len, candidate);
            ++found;
            }
        ++candidate;
        }
    out[len] = 0;
    return found;
}

/* ------------------------------------------------------------ command words */
/* Case-insensitive compare of a command word to a literal. */
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

/* Parse a decimal unsigned value; *ok is cleared when no digit was found. */
static unsigned parse_uint(const char** pp, int* ok)
{
    const char* p = *pp;
    unsigned value = 0u;
    int any = 0;
    while (*p == ' ') ++p;
    while (*p >= '0' && *p <= '9')
        {
        any = 1;
        value = value * 10u + (unsigned)(*p - '0');
        ++p;
        }
    *pp = p;
    if (!any) *ok = 0;
    return value;
}

/* ------------------------------------------------------------ reports */
/* "PR: first <n>: 2 3 5 7 ..." */
static void report_first(unsigned n)
{
    char list[512];
    char line[600];
    unsigned len = 0;
    if (n > 100u) n = 100u;           /* keep the rendered list bounded */
    first_primes(n, list, sizeof(list));
    len = copy_text(line, len, "PR: first ");
    len = append_uint(line, len, n);
    len = copy_text(line, len, ": ");
    len = copy_text(line, len, list);
    line[len] = 0;
    marker(line);
}

/* "PR: <num> is prime" / "PR: <num> is composite" */
static void report_check(unsigned num)
{
    char line[96];
    unsigned len = 0;
    len = copy_text(line, len, "PR: ");
    len = append_uint(line, len, num);
    len = copy_text(line, len, is_prime(num) ? " is prime" : " is composite");
    line[len] = 0;
    marker(line);
}

/* "PR: range <lo>-<hi>: <count> primes" */
static void report_range(unsigned lo, unsigned hi)
{
    char line[128];
    unsigned len = 0;
    len = copy_text(line, len, "PR: range ");
    len = append_uint(line, len, lo);
    len = copy_text(line, len, "-");
    len = append_uint(line, len, hi);
    len = copy_text(line, len, ": ");
    len = append_uint(line, len, range_count(lo, hi));
    len = copy_text(line, len, " primes");
    line[len] = 0;
    marker(line);
}

/* Built-in suite: exercises the same three predicates/reports as live input.
 * It asserts first-10 = 2..29, 97 prime and 1-30 = 10 primes before saying OK. */
static void selftest(void)
{
    char list[512];
    int ok = 1;
    first_primes(10u, list, sizeof(list));
    if (!text_equal(list, "2 3 5 7 11 13 17 19 23 29")) ok = 0;
    if (!is_prime(97u)) ok = 0;
    if (range_count(1u, 30u) != 10u) ok = 0;

    report_first(10u);                 /* PR: first 10: 2 3 ... 29 */
    report_check(97u);                 /* PR: 97 is prime */
    report_range(1u, 30u);             /* PR: range 1-30: 10 primes */
    marker(ok ? "PR: selftest OK" : "PR: selftest FAIL");
}

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    const char* word;
    const char* arg;
    unsigned length;
    int ok = 1;

    while (*input == ' ') ++input;
    word = input;
    while (*word && *word != ' ') ++word;
    length = (unsigned)(word - input);
    arg = word;
    while (*arg == ' ') ++arg;

    if (word_is(input, length, "list"))
        {
        unsigned n = parse_uint(&arg, &ok);
        if (ok) report_first(n); else marker("PR: list: bad argument");
        }
    else if (word_is(input, length, "check"))
        {
        unsigned v = parse_uint(&arg, &ok);
        if (ok) report_check(v); else marker("PR: check: bad argument");
        }
    else if (word_is(input, length, "primes"))
        {
        unsigned lo = parse_uint(&arg, &ok);
        unsigned hi = parse_uint(&arg, &ok);
        if (ok) report_range(lo, hi); else marker("PR: primes: bad arguments");
        }
    else if (word_is(input, length, "selftest"))
        selftest();
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        marker("PR: commands: list <n>  check <num>  primes <lo> <hi>  selftest  help  quit");
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("PR: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    if (con_acquire() < 0) { marker("PR: console busy"); return 1; }

    marker("PR: PRIME TOOL");

    for (;;)
        {
        print("PR> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("PR: DONE");
    con_release();
    return 0;
}

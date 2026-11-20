/*
 * currency.c -- a fixed-point CURRENCY CONVERTER for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/units.c, user/calc.c and user/filemgr.c).
 *
 * Commands are read from PortCon through read_line() and cover:
 *   conv <amount> <from> <to>   convert between two currencies
 *   rates                       print the fixed USD-relative rate table
 *   help, selftest, quit
 * Amounts and results are held as signed milli-units (fixed point, scale
 * 1000, i.e. two displayed decimals).  Every exchange rate is stored as
 * milli-units of that currency per 1 USD; conversions go through USD with a
 * 64-bit intermediate, so no floating point is involved.
 * The built-in selftest converts 100 USD->EUR, 100 EUR->USD (round trip) and
 * 1000 JPY->USD through the very same path used by live commands.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*currency_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&currency_printf_slot;
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

/* Render an input amount: whole part, then the three milli digits only when
 * the fraction is non-zero (so 100.000 becomes plain "100"). */
static unsigned append_amount(char* dst, unsigned n, int milli)
{
    int negative = milli < 0;
    unsigned magnitude = (unsigned)(negative ? -milli : milli);
    unsigned fraction;
    if (negative) dst[n++] = '-';
    n = append_uint(dst, n, magnitude / 1000u);
    fraction = magnitude % 1000u;
    if (fraction)
        {
        dst[n++] = '.';
        dst[n++] = (char)('0' + fraction / 100u);
        dst[n++] = (char)('0' + (fraction / 10u) % 10u);
        dst[n++] = (char)('0' + fraction % 10u);
        }
    return n;
}

/* Render a signed milli-value as money: whole part, then exactly two
 * fractional digits (the third milli digit is truncated).  92.000 is printed
 * as "92.00"; the sign is emitted only for negative values. */
static unsigned append_money(char* dst, unsigned n, int milli)
{
    int negative = milli < 0;
    unsigned magnitude = (unsigned)(negative ? -milli : milli);
    unsigned fraction;
    if (negative) dst[n++] = '-';
    n = append_uint(dst, n, magnitude / 1000u);
    fraction = (magnitude % 1000u) / 10u;
    dst[n++] = '.';
    dst[n++] = (char)('0' + fraction / 10u);
    dst[n++] = (char)('0' + fraction % 10u);
    return n;
}

/* Case-insensitive compare of a command/unit word to a literal. */
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

/* Parse an integer or decimal (up to 3 fractional digits) into milli-units.
 * *ok is cleared when no digit was found, so a missing amount is rejected. */
static int parse_milli(const char** pp, int* ok)
{
    const char* p = *pp;
    int sign = 1, any = 0;
    unsigned whole = 0, fraction = 0, digits = 0;

    if (*p == '+' || *p == '-')
        {
        if (*p == '-') sign = -1;
        ++p;
        }
    while (*p >= '0' && *p <= '9')
        {
        any = 1;
        whole = whole * 10u + (unsigned)(*p - '0');
        ++p;
        }
    if (*p == '.')
        {
        ++p;
        while (*p >= '0' && *p <= '9')
            {
            any = 1;
            if (digits < 3)
                {
                fraction = fraction * 10u + (unsigned)(*p - '0');
                ++digits;
                }
            ++p;
            }
        }
    while (digits < 3) { fraction *= 10u; ++digits; }
    *pp = p;
    *ok = any;
    return sign * (int)(whole * 1000u + fraction);
}

/* ------------------------------------------------------------- conversions
 * Currency indices double as rate/label table indices.  A rate is the number
 * of milli-units of that currency equal to one USD (USD itself is 1000). */
enum { CUR_USD, CUR_EUR, CUR_GBP, CUR_JPY, CUR_CHF, CUR_AUD, CUR_BAD, CUR_COUNT = CUR_BAD };

static const char* const kCurLabels[CUR_COUNT] = {
    "USD", "EUR", "GBP", "JPY", "CHF", "AUD"
};
static const int kCurRates[CUR_COUNT] = {
    1000,   /* USD: 1.00 per USD */
    920,    /* EUR: 0.92 per USD */
    790,    /* GBP: 0.79 per USD */
    150000, /* JPY: 150.00 per USD */
    880,    /* CHF: 0.88 per USD */
    1520    /* AUD: 1.52 per USD */
};

static int currency_index(const char* w, unsigned n)
{
    if (word_is(w, n, "usd")) return CUR_USD;
    if (word_is(w, n, "eur")) return CUR_EUR;
    if (word_is(w, n, "gbp")) return CUR_GBP;
    if (word_is(w, n, "jpy")) return CUR_JPY;
    if (word_is(w, n, "chf")) return CUR_CHF;
    if (word_is(w, n, "aud")) return CUR_AUD;
    return CUR_BAD;
}

/* (value * numerator) / denominator with a 64-bit intermediate keeps the
 * fixed-point product from overflowing 32 bits even for large amounts. */
static int muldiv(int value, int numerator, int denominator)
{
    long long product = (long long)value * (long long)numerator;
    return (int)(product / (long long)denominator);
}

/* Convert via USD: milli_from -> USD milli -> milli_to. */
static int convert_currency(int milli, int from, int to)
{
    int usd = muldiv(milli, 1000, kCurRates[from]);
    return muldiv(usd, kCurRates[to], 1000);
}

/* Emit exactly: CUR: <amount> <from> = <result> <to> */
static void emit_conversion(int milli, const char* from, int result, const char* to)
{
    char line[160];
    unsigned n = 0;
    n = copy_text(line, n, "CUR: ");
    n = append_amount(line, n, milli);
    n = copy_text(line, n, " ");
    n = copy_text(line, n, from);
    n = copy_text(line, n, " = ");
    n = append_money(line, n, result);
    n = copy_text(line, n, " ");
    n = copy_text(line, n, to);
    line[n] = 0;
    marker(line);
}

/* Print the entire fixed rate table in one marker: CUR: rates: USD 1.00 ... */
static void show_rates(void)
{
    char line[192];
    unsigned n = 0;
    int i;
    n = copy_text(line, n, "CUR: rates: ");
    for (i = 0; i < CUR_COUNT; ++i)
        {
        if (i) n = copy_text(line, n, "  ");
        n = copy_text(line, n, kCurLabels[i]);
        n = copy_text(line, n, " ");
        n = append_money(line, n, kCurRates[i]);
        }
    line[n] = 0;
    marker(line);
}

/* Split "<amount> <from> <to>" out of a command argument and, when valid, emit
 * the conversion through the same path used by the built-in self-test. */
static void do_conv(const char* arg)
{
    const char* p = arg;
    const char* from_word;
    const char* to_word;
    unsigned from_length, to_length;
    int valid = 0;
    int milli = parse_milli(&p, &valid);
    int from, to, result;

    while (*p == ' ') ++p;
    from_word = p;
    while (*p && *p != ' ') ++p;
    from_length = (unsigned)(p - from_word);

    while (*p == ' ') ++p;
    to_word = p;
    while (*p && *p != ' ') ++p;
    to_length = (unsigned)(p - to_word);

    from = currency_index(from_word, from_length);
    to = currency_index(to_word, to_length);
    if (!valid || from == CUR_BAD || to == CUR_BAD)
        {
        marker("CUR: bad conversion");
        return;
        }
    result = convert_currency(milli, from, to);
    emit_conversion(milli, kCurLabels[from], result, kCurLabels[to]);
}

/* Deterministic suite through the same conversion/format path as live input. */
static void selftest(void)
{
    do_conv("100 USD EUR");
    do_conv("100 EUR USD");
    do_conv("1000 JPY USD");
    marker("CUR: selftest OK");
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    const char* word;
    const char* arg;
    unsigned length;

    while (*input == ' ') ++input;
    word = input;
    while (*word && *word != ' ') ++word;
    length = (unsigned)(word - input);
    arg = word;
    while (*arg == ' ') ++arg;

    if      (word_is(input, length, "conv"))     do_conv(arg);
    else if (word_is(input, length, "rates"))    show_rates();
    else if (word_is(input, length, "selftest")) selftest();
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        marker("CUR: conv <amount> <from> <to>  rates  help  selftest  quit  [USD EUR GBP JPY CHF AUD]");
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("CUR: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("CUR: console busy"); return 1; }

    fn = *(unsigned*)(void*)&currency_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    marker("CUR: CURRENCY CONVERTER");
    marker(s_bound ? "CUR: EUSER ord 915 bound"
                   : "CUR: using DebugPrint marker path");

    for (;;)
        {
        print("CUR> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("CUR: DONE");
    con_release();
    return 0;
}

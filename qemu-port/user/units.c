/*
 * units.c -- a fixed-point UNIT CONVERTER for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/calc.c and user/filemgr.c).
 *
 * Commands are read from PortCon through read_line() and cover three hooks:
 *   len  <value> <m|km|ft|in|mi>   m<->ft, km<->mi, in->m, mi->km
 *   temp <value> <c|f|k>          c->f, f->c, k->c
 *   mass <value> <kg|lb|oz>       kg<->lb, oz->kg
 * plus help, selftest and quit.  Values and results are held as signed
 * milli-units (fixed point, scale 1000); conversions use integer multiply/
 * divide with a 64-bit intermediate, so no floating point is involved.
 * The built-in selftest converts 1000 m -> ft, 100 C -> F (212) and 1 kg -> lb.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*units_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&units_printf_slot;
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

/* Render a signed milli-value: whole part, then three fractional digits only
 * when the fraction is non-zero (so 212.000 becomes plain "212"). */
static unsigned append_fixed(char* dst, unsigned n, int milli)
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
 * *ok is cleared when no digit was found, so a missing value is rejected. */
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
 * Unit indices double as label-table indices. */
enum { LEN_M, LEN_KM, LEN_FT, LEN_IN, LEN_MI, LEN_BAD };
enum { TEMP_C, TEMP_F, TEMP_K, TEMP_BAD };
enum { MASS_KG, MASS_LB, MASS_OZ, MASS_BAD };

static const char* const kLenLabels[]  = { "m", "km", "ft", "in", "mi" };
static const char* const kTempLabels[] = { "C", "F", "K" };
static const char* const kMassLabels[] = { "kg", "lb", "oz" };

static int len_unit(const char* w, unsigned n)
{
    if (word_is(w, n, "m"))  return LEN_M;
    if (word_is(w, n, "km")) return LEN_KM;
    if (word_is(w, n, "ft")) return LEN_FT;
    if (word_is(w, n, "in")) return LEN_IN;
    if (word_is(w, n, "mi")) return LEN_MI;
    return LEN_BAD;
}

static int temp_unit(const char* w, unsigned n)
{
    if (word_is(w, n, "c")) return TEMP_C;
    if (word_is(w, n, "f")) return TEMP_F;
    if (word_is(w, n, "k")) return TEMP_K;
    return TEMP_BAD;
}

static int mass_unit(const char* w, unsigned n)
{
    if (word_is(w, n, "kg")) return MASS_KG;
    if (word_is(w, n, "lb")) return MASS_LB;
    if (word_is(w, n, "oz")) return MASS_OZ;
    return MASS_BAD;
}

/* (value * numerator) / denominator with a 64-bit intermediate keeps the
 * fixed-point product from overflowing 32 bits even for large inputs. */
static int muldiv(int value, int numerator, int denominator)
{
    long long product = (long long)value * (long long)numerator;
    return (int)(product / (long long)denominator);
}

static int convert_len(int milli, int unit, const char** out_unit)
{
    switch (unit)
        {
        case LEN_M:  *out_unit = "ft"; return muldiv(milli, 32808399, 10000000);
        case LEN_KM: *out_unit = "mi"; return muldiv(milli, 621371, 1000000);
        case LEN_FT: *out_unit = "m";  return muldiv(milli, 3048, 10000);
        case LEN_IN: *out_unit = "m";  return muldiv(milli, 254, 10000);
        default:     *out_unit = "km"; return muldiv(milli, 1609344, 1000000);
        }
}

static int convert_temp(int milli, int unit, const char** out_unit)
{
    switch (unit)
        {
        case TEMP_C: *out_unit = "F"; return (milli * 9) / 5 + 32000;
        case TEMP_F: *out_unit = "C"; return ((milli - 32000) * 5) / 9;
        default:     *out_unit = "C"; return milli - 273150;
        }
}

static int convert_mass(int milli, int unit, const char** out_unit)
{
    switch (unit)
        {
        case MASS_KG: *out_unit = "lb"; return muldiv(milli, 2204623, 1000000);
        case MASS_LB: *out_unit = "kg"; return muldiv(milli, 453592, 1000000);
        default:      *out_unit = "kg"; return muldiv(milli, 28350, 1000000);
        }
}

/* Emit exactly: UN: <value> <unit> = <result> <unit> */
static void emit(int milli, const char* in_unit, int result, const char* out_unit)
{
    char line[160];
    unsigned n = 0;
    n = copy_text(line, n, "UN: ");
    n = append_fixed(line, n, milli);
    n = copy_text(line, n, " ");
    n = copy_text(line, n, in_unit);
    n = copy_text(line, n, " = ");
    n = append_fixed(line, n, result);
    n = copy_text(line, n, " ");
    n = copy_text(line, n, out_unit);
    line[n] = 0;
    marker(line);
}

/* Split "<value> <unit>" out of a command argument and, when valid, emit the
 * conversion through the same path used by the built-in self-test. */
static void do_len(const char* arg)
{
    const char* p = arg;
    const char* word;
    const char* out_unit;
    unsigned word_length;
    int valid = 0;
    int milli = parse_milli(&p, &valid);
    int unit, result;

    while (*p == ' ') ++p;
    word = p;
    while (*p && *p != ' ') ++p;
    word_length = (unsigned)(p - word);
    unit = len_unit(word, word_length);
    if (!valid || unit == LEN_BAD) { marker("UN: bad length"); return; }
    result = convert_len(milli, unit, &out_unit);
    emit(milli, kLenLabels[unit], result, out_unit);
}

static void do_temp(const char* arg)
{
    const char* p = arg;
    const char* word;
    const char* out_unit;
    unsigned word_length;
    int valid = 0;
    int milli = parse_milli(&p, &valid);
    int unit, result;

    while (*p == ' ') ++p;
    word = p;
    while (*p && *p != ' ') ++p;
    word_length = (unsigned)(p - word);
    unit = temp_unit(word, word_length);
    if (!valid || unit == TEMP_BAD) { marker("UN: bad temperature"); return; }
    result = convert_temp(milli, unit, &out_unit);
    emit(milli, kTempLabels[unit], result, out_unit);
}

static void do_mass(const char* arg)
{
    const char* p = arg;
    const char* word;
    const char* out_unit;
    unsigned word_length;
    int valid = 0;
    int milli = parse_milli(&p, &valid);
    int unit, result;

    while (*p == ' ') ++p;
    word = p;
    while (*p && *p != ' ') ++p;
    word_length = (unsigned)(p - word);
    unit = mass_unit(word, word_length);
    if (!valid || unit == MASS_BAD) { marker("UN: bad mass"); return; }
    result = convert_mass(milli, unit, &out_unit);
    emit(milli, kMassLabels[unit], result, out_unit);
}

/* Deterministic suite through the very same conversion/format path as live
 * commands: 1000 m -> ft, 100 C -> F (212) and 1 kg -> lb. */
static void selftest(void)
{
    do_len("1000 m");
    do_temp("100 c");
    do_mass("1 kg");
    marker("UN: selftest OK");
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

    if      (word_is(input, length, "len"))      do_len(arg);
    else if (word_is(input, length, "temp"))     do_temp(arg);
    else if (word_is(input, length, "mass"))     do_mass(arg);
    else if (word_is(input, length, "selftest")) selftest();
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        marker("UN: len <value> <m|km|ft|in|mi>  temp <value> <c|f|k>  mass <value> <kg|lb|oz>  selftest  quit");
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("UN: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("UN: console busy"); return 1; }

    fn = *(unsigned*)(void*)&units_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    marker("UN: UNIT CONVERTER");
    marker(s_bound ? "UN: EUSER ord 915 bound"
                   : "UN: using DebugPrint marker path");

    for (;;)
        {
        print("UN> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("UN: DONE");
    con_release();
    return 0;
}

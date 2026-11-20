/*
 * cal.c -- end-user CALENDAR utility for the Sybian REAL port.
 *
 * A genuine PE2 E32 program.  All serial markers go through EUSER.DLL ordinal
 * 915 (RDebug::Printf) imported through a PE2 IAT slot at codeBase+0x8000,
 * with the port's DebugPrint executive as a fallback so the trace is always
 * visible on the serial console (same pattern as user/clock.c and calc.c).
 *
 * The date is derived from the genuine EExecTimeNow executive (exec 68), which
 * returns TTime microseconds plus a UTC offset.  Symbian TTime counts from
 * 0000-01-01 ("0AD", astronomical year numbering: year 0 is a leap year), a
 * proleptic Gregorian epoch; the port's runtime.c confirms that anchor
 * (2000-01-01 = day 730485, i.e. 1970-01-01 = day 719528).  Because no
 * authoritative epoch is exported to userland, the raw microsecond reading and
 * the assumed epoch are printed alongside the converted date (the QEMU ROM's
 * iTime anchor in variant/qemu_variant_rom.cpp uses this same 0AD epoch and
 * nominally renders 2026-10-05).  A
 * civil-from-days algorithm (Gregorian 400-year eras, leap years 4/100/400)
 * converts the day number to Y/M/D; the weekday is anchored 1970-01-01 =
 * Thursday.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*cal_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define EEXEC_TIME_NOW 68u

/* Days from 0000-01-01 (0AD) to 1970-01-01 in the proleptic Gregorian
 * calendar; matches the 730485 anchor for 2000-01-01 used by runtime.c. */
#define DAYS_0AD_TO_1970 719528u

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&cal_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker directly into a buffer, no hosted libc. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

static unsigned append_int(char* dst, unsigned n, int value)
{
    unsigned magnitude = (unsigned)value;
    if (value < 0) { dst[n++] = '-'; magnitude = 0u - magnitude; }
    char digits[12];
    int d = 0;
    if (!magnitude) digits[d++] = '0';
    while (magnitude)
        {
        digits[d++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
        }
    while (d) dst[n++] = digits[--d];
    return n;
}

static unsigned append_u64(char* dst, unsigned n, unsigned long long value)
{
    char digits[24];
    int d = 0;
    if (!value) digits[d++] = '0';
    while (value)
        {
        digits[d++] = (char)('0' + (unsigned)(value % 10ULL));
        value /= 10ULL;                       /* libgcc __udivdi3 */
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

/* Two-column cell: a leading blank then two digits (calendar columns are 3). */
static unsigned append_cell(char* dst, unsigned n, unsigned day)
{
    dst[n++] = ' ';
    return append_2(dst, n, day);
}

/* ------------------------------------------------------------ Gregorian date
 * is_leap / days_in_month follow the standard 4-100-400 Gregorian rule. */
static unsigned is_leap(unsigned year)
{
    return (year % 4u == 0u) &&
           ((year % 100u != 0u) || (year % 400u == 0u));
}

static unsigned month_days(unsigned year, unsigned month)
{
    static const unsigned char mdays[12] =
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1u || month > 12u) return 30u;
    return mdays[month - 1u] + ((month == 2u && is_leap(year)) ? 1u : 0u);
}

struct Date { unsigned year, month, day; };

/* Convert a day count relative to 1970-01-01 into a Gregorian civil date.
 * Era arithmetic (146097 days / 400 years, eras begin 03-01) avoids any
 * per-year loop and handles the 100/400 year leap rules exactly. */
static struct Date civil_from_days(int z)
{
    struct Date out;
    unsigned era, doe, yoe, y, doy, mp;
    z += 719468;                              /* shift era origin to 0000-03-01 */
    era = (unsigned)((z >= 0 ? z : z - 146096) / 146097);
    doe = (unsigned)z - era * 146097u;        /* [0, 146096] */
    yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    y = yoe + era * 400u;
    doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    mp = (5u * doy + 2u) / 153u;              /* [0, 11], March-based */
    out.day = doy - (153u * mp + 2u) / 5u + 1u;
    out.month = (mp < 10u) ? mp + 3u : mp - 9u;
    y += (out.month <= 2u) ? 1u : 0u;
    out.year = y;
    return out;
}

/* Weekday with Sunday == 0: 1970-01-01 was a Thursday (4). */
static unsigned weekday_from_days(int z)
{
    int w = (z + 4) % 7;
    if (w < 0) w += 7;
    return (unsigned)w;
}

int app_main(void)
{
    char line[128];
    unsigned n;
    long long us = 0;
    int off = 0;
    unsigned long long raw;
    unsigned day_number;
    int z;
    struct Date today;
    unsigned first_wd, mdays, d, col;

    marker("CAL: CALENDAR");

    if (exec2(EEXEC_TIME_NOW, (unsigned)&us, (unsigned)&off) != 0)
        {
        marker("CAL: EExecTimeNow (exec 68) unavailable");
        marker("CAL: DONE");
        return 0;
        }
    raw = (unsigned long long)us;
    n = copy_text(line, 0, "CAL: raw exec68 us=");
    n = append_u64(line, n, raw);
    line[n] = 0;
    marker(line);
    marker("CAL: epoch assumed 0000-01-01 (0AD proleptic Gregorian UTC)");

    /* Whole days since the assumed epoch: 86400 s/day * 1e6 us/s.
     * The sub-day fraction is discarded. */
    day_number = (unsigned)(raw / 86400000000ULL);
    z = (int)(day_number - DAYS_0AD_TO_1970);
    today = civil_from_days(z);

    n = copy_text(line, 0, "CAL: today ");
    n = append_2(line, n, today.year / 100u);
    n = append_2(line, n, today.year % 100u);
    line[n++] = '-';
    n = append_2(line, n, today.month);
    line[n++] = '-';
    n = append_2(line, n, today.day);
    n = copy_text(line, n, " (day ");
    n = append_int(line, n, (int)day_number);
    line[n++] = ')';
    line[n] = 0;
    marker(line);

    /* Month grid: Sunday-first weekday header, then 3-column day cells. */
    marker("CAL: Su Mo Tu We Th Fr Sa");
    first_wd = weekday_from_days(z - (int)today.day + 1);
    mdays = month_days(today.year, today.month);
    n = copy_text(line, 0, "CAL: ");
    for (col = 0; col < first_wd; ++col)
        {
        line[n++] = ' ';
        line[n++] = ' ';
        line[n++] = ' ';
        }
    for (d = 1; d <= mdays; ++d)
        {
        n = append_cell(line, n, d);
        if (((first_wd + d) % 7u) == 0u || d == mdays)
            {
            line[n] = 0;
            marker(line);
            n = copy_text(line, 0, "CAL: ");
            }
        }

    marker("CAL: DONE");
    return 0;
}

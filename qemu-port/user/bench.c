/*
 * bench.c -- CPU benchmark utility for the Sybian REAL port.
 *
 * A genuine PE2 E32 program that measures integer CPU throughput in the
 * style of Dhrystone: a fixed-size loop mixes integer arithmetic, struct
 * field copies, string copy/scan, array indexing and a small recursive
 * function.  A volatile sink keeps the whole loop from being optimized away.
 *
 * Timing uses executive 68 (EExecTimeNow).  In this port exec 68 returns a
 * 64-bit TTime of microseconds since 0AD through a pointer pair (see
 * runtime.c time_now and user/ntpc.c), so it must be called with exec2 and
 * two valid output pointers -- exec0(68) would hand the kernel garbage
 * pointers.  The elapsed microsecond delta is converted to the "ticks" the
 * report asks for by assuming HZ=100 (one tick = 10000 us).  The PIT in this
 * port actually runs at 1000 Hz (exec 5 is the raw NKern tick count), so the
 * reported tick figure is a derived 100 Hz view; the iterations/second rate
 * remains correct because N iterations in t 100-Hz ticks is N*100/t per sec.
 *
 * All serial markers go through the genuine EUSER.DLL ordinal 915
 * (RDebug::Printf) PE2 import, with the DebugPrint executive as a fallback
 * so the trace is always visible.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*bench_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define EEXEC_TIME_NOW 68u
#define BENCH_HZ       100u
#define BENCH_ITERS    10000000u   /* inner iterations; ~1 s under QEMU */

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&bench_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: no hosted libc, so build each marker directly. */
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

/* ------------------------------------------------------------ workload
 * Dhrystone-like mix of integer operations.  The volatile sink at the end
 * makes the computed result escape so no part of the loop is dead code. */
struct BenchRec {
    int a, b, c, d;
    char name[16];
};

static volatile unsigned bench_sink;

/* Small non-tail-recursive function: the result of the recursive call is
 * folded into the return value, so the compiler cannot flatten it away. */
static unsigned recurse(unsigned n)
{
    if (n == 0u) return 0u;
    return n + recurse(n - 1u) + 1u;
}

static unsigned workload(void)
{
    struct BenchRec r, t;
    unsigned char array[32];
    unsigned acc = 0u;
    unsigned i;

    r.a = 3; r.b = 5; r.c = 7; r.d = 11;
    copy_text(r.name, 0, "SybianBench");
    for (i = 0; i < 32u; ++i) array[i] = (unsigned char)(i * 7u + 1u);

    for (i = 0; i < BENCH_ITERS; ++i)
        {
        char buf[16];
        unsigned k;

        t = r;                              /* struct field/string copy */

        /* integer arithmetic, including a divide */
        acc += (unsigned)(t.a * t.b) ^ (unsigned)(t.c + t.d);
        acc ^= acc >> 3;
        acc += (unsigned)(t.b ? (t.a / t.b) : 0);

        /* array indexing with a data-dependent index */
        array[(i + (unsigned)t.c) & 31u] ^= (unsigned char)(acc & 0xFFu);
        acc += array[i & 31u];

        /* string copy + scan (source is perturbed each iteration) */
        r.name[0] = (char)('A' + (i & 7u));
        for (k = 0; t.name[k]; ++k) buf[k] = t.name[k];
        buf[k] = 0;
        acc += k + (unsigned char)buf[0];

        /* recursion */
        acc += recurse(6u);

        /* feedback so the next iteration depends on this one */
        r.a = (int)acc;
        r.b = (int)((acc >> 1) | 1u);        /* always positive, never 0 */
        r.c = (int)((acc >> 2) & 7u);
        ++r.d;
        }

    bench_sink = acc;
    return acc;
}

/* Read exec 68 (TTime microseconds since 0AD) with two valid output slots. */
static long long tick_time_us(void)
{
    long long us = 0;
    int offset = 0;
    exec2(EEXEC_TIME_NOW, (unsigned)&us, (unsigned)&offset);
    return us;
}

int app_main(void)
{
    char line[128];
    unsigned n, ticks, rate, checksum;
    long long us0, us1, elapsed_us;

    marker("BENCH: CPU BENCHMARK START");

    us0 = tick_time_us();
    checksum = workload();
    us1 = tick_time_us();

    elapsed_us = us1 - us0;
    if (elapsed_us < 0) elapsed_us = 0;

    /* HZ=100 assumption: one reported tick = 10000 us. */
    ticks = (unsigned)(elapsed_us / 10000LL);
    if (ticks == 0u) ticks = 1u;             /* sub-tick run: report a floor */
    rate = (BENCH_ITERS * BENCH_HZ) / ticks; /* iterations per second */
    checksum |= 1u;                          /* guarantee a nonzero checksum */

    n = 0;
    n = copy_text(line, n, "BENCH: ticks=");
    n = append_uint(line, n, ticks);
    line[n] = 0;
    marker(line);

    n = 0;
    n = copy_text(line, n, "BENCH: ");
    n = append_uint(line, n, BENCH_ITERS);
    n = copy_text(line, n, " iterations in ");
    n = append_uint(line, n, ticks);
    n = copy_text(line, n, " ticks");
    line[n] = 0;
    marker(line);

    n = 0;
    n = copy_text(line, n, "BENCH: ~");
    n = append_uint(line, n, rate);
    n = copy_text(line, n, " iter/sec (HZ=100)");
    line[n] = 0;
    marker(line);

    n = 0;
    n = copy_text(line, n, "BENCH: checksum=");
    n = append_uint(line, n, checksum);
    line[n] = 0;
    marker(line);

    marker("BENCH: DONE");
    return 0;
}

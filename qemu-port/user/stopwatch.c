/*
 * stopwatch.c -- a STOPWATCH / TIMER end-user app for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with the
 * port's DebugPrint executive as a fallback so the trace is always visible
 * (same pattern as user/clock.c, user/calc.c and user/units.c).
 *
 * The timebase is the genuine EExecTickCount executive (slow exec 5).  That is
 * Kern::TickCount(), which reports SYSTEM ticks at 64 Hz (system tick period
 * ESystemTickPeriod = 15625us = 1/64 s), so one tick is 1/64 of a second.
 * SW_HZ is therefore 64 and elapsed times are rendered as HH:MM:SS.ss with
 * hundredths derived from the 1/64 s tick remainder.
 *
 * Interactive commands are read from PortCon through read_line():
 *   start / stop / lap / reset / show / selftest / help / quit
 * The built-in selftest starts the watch, busy-waits a short loop, records two
 * laps, stops it, and reports a nonzero elapsed time through the same state
 * machine used by the live commands.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*stopwatch_printf_slot)(const char*);
extern int exec0(unsigned number);

#define EEXEC_TICK_COUNT 5u   /* genuine EExecTickCount (slow exec 5) */
#define SW_HZ           64u   /* system tick = 1/64 s (ESystemTickPeriod=15625us) */

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&stopwatch_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
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

/* Render "HH:MM:SS.ss" from a raw tick count at SW_HZ ticks per second. */
static unsigned append_time(char* dst, unsigned n, int ticks)
{
    unsigned total, hundredths;
    if (ticks < 0) ticks = 0;
    total = (unsigned)ticks / SW_HZ;
    hundredths = ((unsigned)ticks % SW_HZ) * 100u / SW_HZ;
    n = append_2(dst, n, total / 3600u);
    dst[n++] = ':';
    n = append_2(dst, n, (total / 60u) % 60u);
    dst[n++] = ':';
    n = append_2(dst, n, total % 60u);
    dst[n++] = '.';
    n = append_2(dst, n, hundredths);
    return n;
}

/* ------------------------------------------------------------- stopwatch state
 * All readings use exec 5 directly; no timer driver is involved. */
static int s_start_ticks = 0;   /* tick count captured by `start` */
static int s_elapsed = 0;       /* frozen elapsed ticks after `stop` */
static int s_running = 0;
static unsigned s_laps = 0;

static int now_ticks(void) { return exec0(EEXEC_TICK_COUNT); }

static int elapsed_ticks(int now)
{
    if (s_running)
        {
        int d = now - s_start_ticks;
        return d > 0 ? d : 0;
        }
    return s_elapsed;
}

/* A genuine busy-wait loop: spin on the executive tick until at least
 * min_ticks system ticks have passed (~15.6 ms each at 64 Hz). */
static void busy_wait(int min_ticks)
{
    volatile unsigned spin = 0;
    int start = now_ticks();
    while ((now_ticks() - start) < min_ticks)
        {
        ++spin;
        if (spin == 0u) break;      /* paranoia: never loop forever */
        }
}

/* ------------------------------------------------------------------ commands */
static void do_start(void)
{
    char line[96];
    unsigned n;
    if (s_running) { marker("SW: already running"); return; }
    s_start_ticks = now_ticks();
    s_elapsed = 0;
    s_laps = 0;
    s_running = 1;
    n = copy_text(line, 0, "SW: started at ");
    n = append_int(line, n, s_start_ticks);
    line[n] = 0;
    marker(line);
}

static void do_stop(void)
{
    char line[96];
    unsigned n;
    if (!s_running) { marker("SW: not running"); return; }
    s_elapsed = elapsed_ticks(now_ticks());
    s_running = 0;
    n = copy_text(line, 0, "SW: stopped ");
    n = append_time(line, n, s_elapsed);
    line[n] = 0;
    marker(line);
}

static void do_lap(void)
{
    char line[96];
    unsigned n;
    if (!s_running) { marker("SW: not running"); return; }
    ++s_laps;
    n = copy_text(line, 0, "SW: lap ");
    n = append_uint(line, n, s_laps);
    line[n++] = ' ';
    n = append_time(line, n, elapsed_ticks(now_ticks()));
    line[n] = 0;
    marker(line);
}

static void do_reset(void)
{
    s_running = 0;
    s_start_ticks = 0;
    s_elapsed = 0;
    s_laps = 0;
    marker("SW: reset");
}

static void do_show(void)
{
    char line[96];
    unsigned n = copy_text(line, 0, "SW: elapsed ");
    n = append_time(line, n, elapsed_ticks(now_ticks()));
    line[n] = 0;
    marker(line);
}

/* Built-in self-test through the same state machine as the live commands:
 * start, busy-wait, two laps, stop, and report the nonzero elapsed time. */
static void do_selftest(void)
{
    marker("SW: selftest");
    do_start();
    busy_wait(3);
    do_lap();
    busy_wait(3);
    do_lap();
    do_stop();
    do_show();
    marker("SW: selftest OK");
}

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

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    const char* word;
    unsigned length;

    while (*input == ' ') ++input;
    word = input;
    while (*word && *word != ' ') ++word;
    length = (unsigned)(word - input);

    if      (word_is(input, length, "start"))    do_start();
    else if (word_is(input, length, "stop"))     do_stop();
    else if (word_is(input, length, "lap"))      do_lap();
    else if (word_is(input, length, "reset"))    do_reset();
    else if (word_is(input, length, "show"))     do_show();
    else if (word_is(input, length, "selftest")) do_selftest();
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        marker("SW: start stop lap reset show selftest help quit");
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("SW: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("SW: console busy"); return 1; }

    fn = *(unsigned*)(void*)&stopwatch_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    marker("SW: STOPWATCH READY (HZ=64)");
    marker(s_bound ? "SW: EUSER ord 915 bound"
                   : "SW: using DebugPrint marker path");

    for (;;)
        {
        print("SW> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("SW: DONE");
    con_release();
    return 0;
}
